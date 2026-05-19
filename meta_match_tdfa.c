#if !defined(META_MATCH_TDFA_C)
#define META_MATCH_TDFA_C

#include <regex.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "meta.h"

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
    ),
    .extracts = true,
};
// clang-format on

static int32
match_tdfa_char_at(uint8 *input, int32 input_len, int32 pos) {
    if (pos < 0 || pos >= input_len) {
        return -1;
    }
    if (input[pos] == '\0') {
        return -1;
    }
    return input[pos];
}

static int32
match_tdfa_at_end(uint8 *input, int32 input_len, int32 pos) {
    return (pos >= input_len || input[pos] == '\0');
}

static int32
match_tdfa_valid_reg(MetaTdfa *tdfa, int32 reg) {
    return (reg > 0 && reg <= tdfa->num_registers);
}

static int32
match_tdfa_exec_ops(MetaTdfa *tdfa, int32 *regs, int32 first_op,
                    int32 op_count, int32 pos) {
    if (op_count < 0) {
        return 0;
    }
    if (op_count == 0) {
        return 1;
    }
    if (first_op < 0 || first_op + op_count > tdfa->num_ops) {
        return 0;
    }

    for (int32 i = 0; i < op_count; i += 1) {
        MetaTdfaRegOp *op = &tdfa->ops[first_op + i];

        if (!match_tdfa_valid_reg(tdfa, op->dst)) {
            return 0;
        }

        if (op->kind == META_TDFA_REGOP_SET_NIL) {
            regs[op->dst] = -1;
        } else if (op->kind == META_TDFA_REGOP_SET_POS) {
            regs[op->dst] = pos;
        } else if (op->kind == META_TDFA_REGOP_COPY) {
            if (!match_tdfa_valid_reg(tdfa, op->src)) {
                return 0;
            }
            regs[op->dst] = regs[op->src];
        } else {
            return 0;
        }
    }

    return 1;
}

static MetaTdfaTransition *
match_tdfa_find_transition(MetaTdfa *tdfa, int32 state_id, int32 c) {
    MetaTdfaState *state;

    if (c < 0 || state_id < 0 || state_id >= tdfa->num_states) {
        return NULL;
    }

    state = &tdfa->states[state_id];
    if (state->first_transition >= 0 && state->transition_count > 0) {
        int32 first = state->first_transition;
        int32 count = state->transition_count;

        if (first >= 0 && first + count <= tdfa->num_transitions) {
            for (int32 i = 0; i < count; i += 1) {
                MetaTdfaTransition *tr = &tdfa->transitions[first + i];
                if (tr->symbol == c) {
                    return tr;
                }
                if (tr->symbol > c) {
                    break;
                }
            }
        }
    }

    /* Defensive fallback for malformed or old emitted tables. */
    for (int32 i = 0; i < tdfa->num_transitions; i += 1) {
        MetaTdfaTransition *tr = &tdfa->transitions[i];
        if (tr->from == state_id && tr->symbol == c) {
            return tr;
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
        int32 reg = tdfa->final_register_base + t - 1;
        if (!match_tdfa_valid_reg(tdfa, reg)) {
            return 0;
        }
        saved_tags[t] = regs[reg];
    }

    return 1;
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

        value = saved_tags[tag->id];
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
    int32 *regs = NULL;
    int32 *saved_tags = NULL;
    int32 state_id;
    int32 pos = start_pos;
    int32 accepted = 0;
    int32 accepted_end = -1;
    int32 result = REG_NOMATCH;

    if (regex == NULL || regex->tdfa == NULL) {
        return REG_NOMATCH;
    }

    tdfa = regex->tdfa;
    if (tdfa->num_states <= 0 || tdfa->start_state < 0
        || tdfa->start_state >= tdfa->num_states || tdfa->num_tags < 0
        || tdfa->num_registers < tdfa->num_tags
        || tdfa->final_register_base <= 0
        || tdfa->final_register_base + tdfa->num_tags - 1
               > tdfa->num_registers
        || tdfa->num_transitions < 0 || tdfa->num_ops < 0
        || (tdfa->num_tags > 0 && tdfa->tags == NULL)
        || (tdfa->num_states > 0 && tdfa->states == NULL)
        || (tdfa->num_transitions > 0 && tdfa->transitions == NULL)
        || (tdfa->num_ops > 0 && tdfa->ops == NULL)) {
        return REG_NOMATCH;
    }

    regs = malloc2(SIZEOF(*regs)*(tdfa->num_registers + 1));
    saved_tags = malloc2(SIZEOF(*saved_tags)*(tdfa->num_tags + 1));
    if (regs == NULL || saved_tags == NULL) {
        goto cleanup;
    }

    for (int32 i = 0; i <= tdfa->num_registers; i += 1) {
        regs[i] = -1;
    }
    for (int32 i = 0; i <= tdfa->num_tags; i += 1) {
        saved_tags[i] = -1;
    }

    state_id = tdfa->start_state;

    while (true) {
        MetaTdfaState *state;
        MetaTdfaTransition *tr;
        int32 c;

        if (state_id < 0 || state_id >= tdfa->num_states) {
            goto cleanup;
        }

        state = &tdfa->states[state_id];
        if (state->is_accepting
            && (!regex->has_end_anchor
                || match_tdfa_at_end(input, input_len, pos))) {
            if (!match_tdfa_save_accept(tdfa, state, regs, saved_tags, pos)) {
                goto cleanup;
            }
            accepted = 1;
            accepted_end = pos;
        }

        if (match_tdfa_at_end(input, input_len, pos)) {
            break;
        }

        c = match_tdfa_char_at(input, input_len, pos);
        tr = match_tdfa_find_transition(tdfa, state_id, c);
        if (tr == NULL) {
            break;
        }
        if (tr->to < 0 || tr->to >= tdfa->num_states) {
            goto cleanup;
        }

        if (!match_tdfa_exec_ops(tdfa, regs, tr->first_op, tr->op_count, pos)) {
            goto cleanup;
        }

        state_id = tr->to;
        pos += 1;
    }

    if (accepted) {
        match_tdfa_fill_pmatch(regex, start_pos, accepted_end, saved_tags,
                               pmatch, pmatch_len);
        result = 0;
    }

cleanup:
    if (regs != NULL) {
        free2(regs, SIZEOF(*regs)*(tdfa->num_registers + 1));
    }
    if (saved_tags != NULL) {
        free2(saved_tags, SIZEOF(*saved_tags)*(tdfa->num_tags + 1));
    }
    return result;
}

#endif /* META_MATCH_TDFA_C */
