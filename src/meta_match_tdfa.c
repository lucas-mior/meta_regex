#if !defined(META_MATCH_TDFA_C)
#define META_MATCH_TDFA_C

#include "cbase.h"

#include <regex.h>
#include "meta_regex.h"
#include "meta_util.c"

// clang-format off
static const MatcherFeatures match_features_tdfa = {
    .supports = (enum MetaOpType)(
        META_OP_END
        | META_OP_LITERAL
        | META_OP_ANY
        | META_OP_CLASS
        | META_OP_GROUP_START
        | META_OP_GROUP_END
        | META_OP_STAR
        | META_OP_PLUS
        | META_OP_OPTIONAL
        | META_OP_ALTERNATION
        | META_OP_BOUNDED
        | META_OP_SPLIT
        | META_OP_JUMP
        | META_OP_WORD_START
        | META_OP_WORD_END
        | META_OP_WORD_BOUNDARY
        | META_OP_NON_WORD_BOUNDARY
    ),
    .extracts = true,
};
// clang-format on

static int32
match_tdfa_valid_reg(MetaTdfa *tdfa, int32 reg) {
    return (reg > 0 && reg <= tdfa->num_registers);
}

static int32
match_tdfa_exec_ops(MetaTdfa *tdfa, int32 *regs, int32 first_op, int32 op_count,
                    int32 pos) {
    if (op_count <= 0) {
        return 1;
    }

    for (int32 i = 0; i < op_count; i += 1) {
        MetaTdfaRegOp *op = &tdfa->ops[first_op + i];

        switch (op->kind) {
        case META_TDFA_REGOP_SET_NIL:
            regs[op->dst] = -1;
            break;
        case META_TDFA_REGOP_SET_POS:
            regs[op->dst] = pos;
            break;
        case META_TDFA_REGOP_COPY:
            regs[op->dst] = regs[op->src];
            break;
        default:
            break;
        }
    }

    return 1;
}
static MetaTdfaTransition *
match_tdfa_find_transition(MetaTdfa *tdfa, int32 state_id, int32 c,
                           int32 next_is_word) {
    MetaTdfaState *state;
    int32 first;
    int32 count;

    if (c < 0 || state_id < 0 || state_id >= tdfa->num_states) {
        return NULL;
    }

    if (tdfa->transition_index && (tdfa->transition_index_stride > 0)) {
        int32 context_offset = 0;
        int32 index;
        int32 transition_id;

        if (tdfa->uses_context) {
            if (next_is_word < 0) {
                return NULL;
            }
            context_offset = next_is_word*META_ALPHABET_SIZE;
        }

        index = state_id*tdfa->transition_index_stride + context_offset + c;
        transition_id = tdfa->transition_index[index];
        if (transition_id < 0) {
            return NULL;
        }
        return &tdfa->transitions[transition_id];
    }

    state = &tdfa->states[state_id];
    first = state->first_transition;
    count = state->transition_count;
    if (first < 0 || count <= 0) {
        return NULL;
    }

    for (int32 i = 0; i < count; i += 1) {
        MetaTdfaTransition *tr = &tdfa->transitions[first + i];
        if (tr->symbol == c
            && (tr->next_is_word < 0 || tr->next_is_word == next_is_word)) {
            return tr;
        }
        if (tr->symbol > c) {
            break;
        }
    }

    return NULL;
}
static int32
match_tdfa_save_accept(MetaTdfa *tdfa, MetaTdfaState *state, int32 *regs,
                       int32 *saved_tags, int32 pos) {
    if (!match_tdfa_exec_ops(tdfa, regs, state->first_final_op,
                             state->final_op_count, pos)) {
        return 0;
    }

    for (int32 t = 1; t <= tdfa->num_tags; t += 1) {
        MetaTnfaTag *tag = &tdfa->tags[t - 1];
        int32 reg = tdfa->final_register_base + t - 1;

        if (tag->fixed_base_tag > 0) {
            continue;
        }
        saved_tags[t] = regs[reg];
    }

    return 1;
}

static int32
match_tdfa_saved_tag_value(MetaTdfa *tdfa, int32 *saved_tags, int32 tag_id,
                           int32 depth) {
    MetaTnfaTag *tag;
    int32 base;

    if (tag_id <= 0 || tag_id > tdfa->num_tags || depth > tdfa->num_tags) {
        return -1;
    }

    tag = &tdfa->tags[tag_id - 1];
    if (tag->fixed_base_tag <= 0) {
        return saved_tags[tag_id];
    }

    base = match_tdfa_saved_tag_value(tdfa, saved_tags, tag->fixed_base_tag,
                                      depth + 1);
    if (base < 0) {
        return -1;
    }
    return base + tag->fixed_offset;
}

static void
match_tdfa_fill_pmatch(MetaRegex *regex, int32 start_pos, int32 end_pos,
                       int32 *saved_tags, regmatch_t *pmatch,
                       int32 pmatch_len) {
    MetaTdfa *tdfa = regex->tdfa;

    if (pmatch == NULL || pmatch_len <= 0) {
        return;
    }

    pmatch[0].rm_so = start_pos;
    pmatch[0].rm_eo = end_pos;

    for (int32 i = 0; i < tdfa->num_tags; i += 1) {
        MetaTnfaTag *tag = &tdfa->tags[i];
        int32 group = tag->group;
        int32 value;

        if (tag->id <= 0 || tag->id > tdfa->num_tags) {
            continue;
        }
        if (group <= 0 || group >= pmatch_len) {
            continue;
        }

        value = match_tdfa_saved_tag_value(tdfa, saved_tags, tag->id, 0);
        if (tag->role == META_TNFA_TAG_GROUP_START) {
            pmatch[group].rm_so = value;
        } else if (tag->role == META_TNFA_TAG_GROUP_END) {
            pmatch[group].rm_eo = value;
        }
    }
    return;
}

static int32
match_tdfa(MetaRegex *regex, uint8 *input, int32 input_len, int32 start_pos,
           regmatch_t *pmatch, int32 pmatch_len) {
    MetaTdfa *tdfa;
    static int32 *regs = NULL;
    static int32 *saved_tags = NULL;
    static int32 nregs = 0;
    static int32 nsaved_tags = 0;
    int32 state_id;
    int32 pos = start_pos;
    int32 accepted = 0;
    int32 accepted_end = -1;
    int32 result = REG_NOMATCH;
    int32 extract;

    (void)input_len;

    if (DEBUGGING) {
        ASSERT(regex);
        ASSERT(regex->tdfa);
    }

    tdfa = regex->tdfa;
    if (tdfa->num_states <= 0 || tdfa->start_state < 0
        || tdfa->start_state >= tdfa->num_states || tdfa->num_tags < 0
        || tdfa->start_state_nw_nw < 0
        || tdfa->start_state_nw_nw >= tdfa->num_states
        || tdfa->start_state_nw_w < 0
        || tdfa->start_state_nw_w >= tdfa->num_states
        || tdfa->start_state_w_nw < 0
        || tdfa->start_state_w_nw >= tdfa->num_states
        || tdfa->start_state_w_w < 0
        || tdfa->start_state_w_w >= tdfa->num_states
        || tdfa->num_registers < tdfa->num_tags
        || tdfa->final_register_base <= 0
        || tdfa->final_register_base + tdfa->num_tags - 1 > tdfa->num_registers
        || tdfa->num_transitions < 0 || tdfa->num_ops < 0
        || (tdfa->num_tags > 0 && tdfa->tags == NULL)
        || (tdfa->num_states > 0 && tdfa->states == NULL)
        || (tdfa->num_transitions > 0 && tdfa->transitions == NULL)
        || (tdfa->transition_index_stride < 0)
        || (tdfa->transition_index_stride > 0
            && tdfa->transition_index_stride < META_ALPHABET_SIZE)
        || (tdfa->num_ops > 0 && tdfa->ops == NULL)) {
        return REG_NOMATCH;
    }

    extract = pmatch && (pmatch_len > 1) && (tdfa->num_tags > 0);
    if (extract) {
        if ((regs == NULL) || nregs < (tdfa->num_registers + 1)) {
            int32 new_regs = (tdfa->num_registers + 1)*2;
            regs = realloc2(regs, nregs, new_regs, SIZEOF(*regs));
            nregs = new_regs;
        }
        if ((saved_tags == NULL) || nsaved_tags < (tdfa->num_tags + 1)) {
            int32 new_tags = (tdfa->num_tags + 1)*2;
            saved_tags = realloc2(saved_tags, nsaved_tags, new_tags,
                                  SIZEOF(*saved_tags));
            nsaved_tags = new_tags;
        }

        for (int32 i = 0; i <= tdfa->num_registers; i += 1) {
            regs[i] = -1;
        }
        for (int32 i = 0; i <= tdfa->num_tags; i += 1) {
            saved_tags[i] = -1;
        }
    }

    if (!tdfa->uses_context) {
        state_id = tdfa->start_state;
    } else {
        int32 prev_is_w = 0;
        int32 curr_is_w = 0;

        if (start_pos > 0) {
            prev_is_w = word_table[input[start_pos - 1]];
        }
        curr_is_w = word_table[input[start_pos]];

        if (!prev_is_w && !curr_is_w) {
            state_id = tdfa->start_state_nw_nw;
        } else if (!prev_is_w && curr_is_w) {
            state_id = tdfa->start_state_nw_w;
        } else if (prev_is_w && !curr_is_w) {
            state_id = tdfa->start_state_w_nw;
        } else {
            state_id = tdfa->start_state_w_w;
        }
    }

    while (true) {
        MetaTdfaState *state;
        MetaTdfaTransition *tr;
        int32 c;
        int32 at_end;
        int32 next_is_word;

        if (state_id < 0 || state_id >= tdfa->num_states) {
            goto cleanup;
        }

        state = &tdfa->states[state_id];
        c = input[pos];
        at_end = (c == '\0');
        if (state->is_accepting && (!regex->has_end_anchor || at_end)) {
            if (extract
                && !match_tdfa_save_accept(tdfa, state, regs, saved_tags,
                                           pos)) {
                goto cleanup;
            }
            accepted = 1;
            accepted_end = pos;
        }

        if (at_end) {
            break;
        }

        if (tdfa->uses_context) {
            next_is_word = word_table[input[pos + 1]];
        } else {
            next_is_word = -1;
        }
        tr = match_tdfa_find_transition(tdfa, state_id, c, next_is_word);
        if (tr == NULL) {
            break;
        }

        if (extract
            && !match_tdfa_exec_ops(tdfa, regs, tr->first_op, tr->op_count,
                                    pos)) {
            goto cleanup;
        }

        state_id = tr->to;
        pos += 1;
    }

    if (accepted) {
        if (extract) {
            match_tdfa_fill_pmatch(regex, start_pos, accepted_end, saved_tags,
                                   pmatch, pmatch_len);
        } else if (pmatch && (pmatch_len > 0)) {
            pmatch[0].rm_so = start_pos;
            pmatch[0].rm_eo = accepted_end;
        }
        result = 0;
    }

cleanup:
    return result;
}

#endif /* META_MATCH_TDFA_C */
