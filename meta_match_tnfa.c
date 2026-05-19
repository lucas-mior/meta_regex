#if !defined(META_MATCH_TNFA_C)
#define META_MATCH_TNFA_C

#include <regex.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "meta.h"

// clang-format off
static const MatcherFeatures match_features_tnfa = {
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

typedef struct MetaTnfaConfig {
    int32 state;
} MetaTnfaConfig;

static int32
match_tnfa_is_word_char(int32 c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '_');
}

static int32
match_tnfa_char_at(uint8 *input, int32 input_len, int32 pos) {
    if (pos < 0 || pos >= input_len) {
        return -1;
    }
    if (input[pos] == '\0') {
        return -1;
    }
    return input[pos];
}

static int32
match_tnfa_at_end(uint8 *input, int32 input_len, int32 pos) {
    return (pos >= input_len || input[pos] == '\0');
}

static int32
match_tnfa_assertion_matches(enum MetaTnfaTransitionKind kind, uint8 *input,
                             int32 input_len, int32 pos) {
    int32 prev = match_tnfa_char_at(input, input_len, pos - 1);
    int32 curr = match_tnfa_char_at(input, input_len, pos);
    int32 prev_is_w = (prev >= 0 && match_tnfa_is_word_char(prev));
    int32 curr_is_w = (curr >= 0 && match_tnfa_is_word_char(curr));

    if (kind == META_TNFA_TRANS_WORD_START) {
        return (!prev_is_w && curr_is_w);
    }
    if (kind == META_TNFA_TRANS_WORD_END) {
        return (prev_is_w && !curr_is_w);
    }
    if (kind == META_TNFA_TRANS_WORD_BOUNDARY) {
        return (prev_is_w != curr_is_w);
    }
    if (kind == META_TNFA_TRANS_NON_WORD_BOUNDARY) {
        return (prev_is_w == curr_is_w);
    }
    return 0;
}

static int32
match_tnfa_transition_is_zero_width(MetaTnfaTransition *tr) {
    return (tr->kind == META_TNFA_TRANS_EPSILON
            || tr->kind == META_TNFA_TRANS_WORD_START
            || tr->kind == META_TNFA_TRANS_WORD_END
            || tr->kind == META_TNFA_TRANS_WORD_BOUNDARY
            || tr->kind == META_TNFA_TRANS_NON_WORD_BOUNDARY);
}

static int32
match_tnfa_zero_width_enabled(MetaTnfaTransition *tr, uint8 *input,
                              int32 input_len, int32 pos) {
    if (tr->kind == META_TNFA_TRANS_EPSILON) {
        return 1;
    }
    return match_tnfa_assertion_matches(tr->kind, input, input_len, pos);
}

static int32
match_tnfa_can_use_state_slices(MetaTnfa *tnfa) {
    int32 covered = 0;

    if (tnfa->states == NULL) {
        return 0;
    }

    for (int32 s = 0; s < tnfa->num_states; s += 1) {
        MetaTnfaState *state = &tnfa->states[s];

        if (state->first_transition < 0) {
            if (state->transition_count != 0) {
                return 0;
            }
            continue;
        }

        if (state->transition_count < 0
            || state->first_transition > tnfa->num_transitions
            || state->transition_count
                   > tnfa->num_transitions - state->first_transition) {
            return 0;
        }

        for (int32 i = 0; i < state->transition_count; i += 1) {
            int32 tr_index = state->first_transition + i;
            if (tnfa->transitions[tr_index].from != s) {
                return 0;
            }
        }

        covered += state->transition_count;
    }

    return (covered == tnfa->num_transitions);
}

static void
match_tnfa_transition_range(MetaTnfa *tnfa, int32 use_state_slices, int32 state,
                            int32 *first, int32 *last) {
    if (use_state_slices) {
        MetaTnfaState *st = &tnfa->states[state];
        *first = st->first_transition;
        *last = st->first_transition + st->transition_count;
    } else {
        *first = 0;
        *last = tnfa->num_transitions;
    }
    return;
}

static int32 *
match_tnfa_tag_row(int32 *tag_storage, int32 tag_count, int32 index) {
    return tag_storage + ((int64)index*tag_count);
}

static void
match_tnfa_copy_tags(int32 *dst, int32 *src, int32 tag_count) {
    memcpy64(dst, src, SIZEOF(*dst)*tag_count);
    return;
}

static void
match_tnfa_apply_tag(int32 *tag_values, int32 tag_count, int32 tag, int32 pos) {
    int32 id;

    if (tag == META_TNFA_TAG_NONE) {
        return;
    }

    id = META_TNFA_TAG_ID(tag);
    if (id <= 0 || id >= tag_count) {
        return;
    }

    if (META_TNFA_TAG_IS_NEGATIVE(tag)) {
        tag_values[id] = -1;
    } else {
        tag_values[id] = pos;
    }
    return;
}

static void
match_tnfa_init_tags(int32 *tag_values, int32 tag_count) {
    for (int32 i = 0; i < tag_count; i += 1) {
        tag_values[i] = -1;
    }
    return;
}

static int32
match_tnfa_collect_zero_width_edges(MetaTnfa *tnfa, int32 use_state_slices,
                                    int32 state, uint8 *input, int32 input_len,
                                    int32 pos, int32 *edge_indices) {
    int32 edge_count = 0;
    int32 first;
    int32 last;

    match_tnfa_transition_range(tnfa, use_state_slices, state, &first, &last);

    for (int32 i = first; i < last; i += 1) {
        MetaTnfaTransition *tr = &tnfa->transitions[i];

        if (!use_state_slices && tr->from != state) {
            continue;
        }
        if (!match_tnfa_transition_is_zero_width(tr)) {
            continue;
        }
        if (!match_tnfa_zero_width_enabled(tr, input, input_len, pos)) {
            continue;
        }

        edge_indices[edge_count] = i;
        edge_count += 1;
    }

    for (int32 i = 1; i < edge_count; i += 1) {
        int32 edge = edge_indices[i];
        int32 prio = tnfa->transitions[edge].priority;
        int32 j = i - 1;

        while (j >= 0 && tnfa->transitions[edge_indices[j]].priority > prio) {
            edge_indices[j + 1] = edge_indices[j];
            j -= 1;
        }
        edge_indices[j + 1] = edge;
    }

    return edge_count;
}

static int32
match_tnfa_push_config(MetaTnfaConfig *dst_configs, int32 *dst_tags,
                       int32 dst_index, MetaTnfaConfig *src_configs,
                       int32 *src_tags, int32 src_index, int32 tag_count) {
    dst_configs[dst_index] = src_configs[src_index];
    match_tnfa_copy_tags(match_tnfa_tag_row(dst_tags, tag_count, dst_index),
                         match_tnfa_tag_row(src_tags, tag_count, src_index),
                         tag_count);
    return 0;
}

static int32
match_tnfa_epsilon_closure(MetaTnfa *tnfa, int32 use_state_slices, uint8 *input,
                           int32 input_len, int32 pos,
                           MetaTnfaConfig *input_configs, int32 *input_tags,
                           int32 input_count, MetaTnfaConfig *output_configs,
                           int32 *output_tags, MetaTnfaConfig *stack,
                           int32 *stack_tags, int32 stack_cap,
                           uint8 *closed_seen, int32 *edge_indices,
                           int32 *work_tags, int32 tag_count) {
    int32 stack_count = 0;
    int32 output_count = 0;

    memset64(closed_seen, 0, tnfa->num_states);

    /*
        Follow the TNFA simulation ordering from the paper: push the input
        configurations in reverse order, but mark a state as closed only when
        the configuration is popped and appended to the closure.

        Marking a state as seen when it is merely pushed is too early. In a
        pattern such as ((a|aa)*) on input "aa", a lower-priority pending
        configuration for the end of the inner group can otherwise block the
        higher-priority path that reaches the same state after the second
        repetition. That loses the final capture of group 2.
    */
    for (int32 i = input_count - 1; i >= 0; i -= 1) {
        int32 state = input_configs[i].state;
        if (state < 0 || state >= tnfa->num_states) {
            continue;
        }
        if (stack_count >= stack_cap) {
            return -1;
        }
        match_tnfa_push_config(stack, stack_tags, stack_count, input_configs,
                               input_tags, i, tag_count);
        stack_count += 1;
    }

    while (stack_count > 0) {
        MetaTnfaConfig cfg;
        int32 edge_count;
        int32 selected_count = 0;
        int32 *cfg_tags;

        stack_count -= 1;
        cfg = stack[stack_count];
        match_tnfa_copy_tags(
            work_tags, match_tnfa_tag_row(stack_tags, tag_count, stack_count),
            tag_count);
        cfg_tags = work_tags;

        if (cfg.state < 0 || cfg.state >= tnfa->num_states) {
            continue;
        }
        if (closed_seen[cfg.state]) {
            continue;
        }
        closed_seen[cfg.state] = 1;

        if (output_count >= tnfa->num_states) {
            return -1;
        }
        output_configs[output_count] = cfg;
        match_tnfa_copy_tags(
            match_tnfa_tag_row(output_tags, tag_count, output_count), cfg_tags,
            tag_count);
        output_count += 1;

        edge_count = match_tnfa_collect_zero_width_edges(
            tnfa, use_state_slices, cfg.state, input, input_len, pos,
            edge_indices);

        for (int32 i = 0; i < edge_count; i += 1) {
            MetaTnfaTransition *tr = &tnfa->transitions[edge_indices[i]];
            if (tr->to < 0 || tr->to >= tnfa->num_states
                || closed_seen[tr->to]) {
                continue;
            }
            edge_indices[selected_count] = edge_indices[i];
            selected_count += 1;
        }

        for (int32 i = selected_count - 1; i >= 0; i -= 1) {
            MetaTnfaTransition *tr = &tnfa->transitions[edge_indices[i]];
            int32 *next_tags;

            if (stack_count >= stack_cap) {
                return -1;
            }

            stack[stack_count] = cfg;
            stack[stack_count].state = tr->to;
            next_tags = match_tnfa_tag_row(stack_tags, tag_count, stack_count);
            match_tnfa_copy_tags(next_tags, cfg_tags, tag_count);

            if (tr->kind == META_TNFA_TRANS_EPSILON) {
                match_tnfa_apply_tag(next_tags, tag_count, tr->tag, pos);
            }

            stack_count += 1;
        }
    }

    return output_count;
}

static int32
match_tnfa_symbol_transition_matches(MetaTnfaTransition *tr, int32 c) {
    if (c < 0) {
        return 0;
    }

    if (tr->kind == META_TNFA_TRANS_LITERAL) {
        return (c == tr->value);
    }
    if (tr->kind == META_TNFA_TRANS_CLASS) {
        return ((tr->mask[c / 32] & (1u << (c % 32))) != 0);
    }
    if (tr->kind == META_TNFA_TRANS_ANY) {
        return (c != '\0');
    }
    return 0;
}

static int32
match_tnfa_step(MetaTnfa *tnfa, int32 use_state_slices, uint8 *input,
                int32 input_len, int32 pos, MetaTnfaConfig *closed_configs,
                int32 *closed_tags, int32 closed_count,
                MetaTnfaConfig *next_configs, int32 *next_tags, uint8 *seen,
                int32 tag_count) {
    int32 c = match_tnfa_char_at(input, input_len, pos);
    int32 next_count = 0;

    memset64(seen, 0, tnfa->num_states);

    for (int32 i = 0; i < closed_count; i += 1) {
        MetaTnfaConfig *cfg = &closed_configs[i];
        int32 first;
        int32 last;

        match_tnfa_transition_range(tnfa, use_state_slices, cfg->state, &first,
                                    &last);

        for (int32 j = first; j < last; j += 1) {
            MetaTnfaTransition *tr = &tnfa->transitions[j];

            if (!use_state_slices && tr->from != cfg->state) {
                continue;
            }
            if (!match_tnfa_symbol_transition_matches(tr, c)) {
                continue;
            }
            if (tr->to < 0 || tr->to >= tnfa->num_states || seen[tr->to]) {
                continue;
            }
            if (next_count >= tnfa->num_states) {
                return -1;
            }

            next_configs[next_count].state = tr->to;
            match_tnfa_copy_tags(
                match_tnfa_tag_row(next_tags, tag_count, next_count),
                match_tnfa_tag_row(closed_tags, tag_count, i), tag_count);

            next_count += 1;
            seen[tr->to] = 1;
        }
    }

    return next_count;
}

static int32
match_tnfa_find_accept(MetaTnfa *tnfa, MetaTnfaConfig *configs,
                       int32 config_count) {
    for (int32 i = 0; i < config_count; i += 1) {
        if (configs[i].state == tnfa->final_state) {
            return i;
        }
    }
    return -1;
}

static void
match_tnfa_save_accept(int32 *accept_tags, int32 *saved_tags, int32 tag_count) {
    match_tnfa_copy_tags(saved_tags, accept_tags, tag_count);
    return;
}

static void
match_tnfa_fill_pmatch(MetaRegex *regex, int32 start_pos, int32 end_pos,
                       int32 *saved_tags, regmatch_t *pmatch,
                       int32 pmatch_len) {
    MetaTnfa *tnfa = regex->tnfa;

    if (pmatch == NULL || pmatch_len <= 0) {
        return;
    }

    pmatch[0].rm_so = start_pos;
    pmatch[0].rm_eo = end_pos;

    for (int32 i = 0; i < tnfa->num_tags; i += 1) {
        MetaTnfaTag *tag = &tnfa->tags[i];
        int32 group = tag->group;
        int32 value;

        if (tag->id <= 0 || tag->id > tnfa->num_tags) {
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
match_tnfa(MetaRegex *regex, uint8 *input, int32 input_len, int32 start_pos,
           regmatch_t *pmatch, int32 pmatch_len) {
    MetaTnfa *tnfa;
    MetaTnfaConfig *configs_a = NULL;
    MetaTnfaConfig *configs_b = NULL;
    MetaTnfaConfig *stack = NULL;
    int32 *tags_a = NULL;
    int32 *tags_b = NULL;
    int32 *stack_tags = NULL;
    int32 *edge_indices = NULL;
    int32 *saved_tags = NULL;
    int32 *work_tags = NULL;
    uint8 *closed_seen = NULL;
    uint8 *seen = NULL;
    MetaTnfaConfig *current = NULL;
    MetaTnfaConfig *closed = NULL;
    int32 *current_tags = NULL;
    int32 *closed_tags = NULL;
    int32 current_count = 0;
    int32 closed_count = 0;
    int32 pos = start_pos;
    int32 accepted = 0;
    int32 accepted_end = -1;
    int32 result = REG_NOMATCH;
    int32 state_count;
    int32 tag_count;
    int32 edge_count;
    int32 stack_cap;
    int32 use_state_slices = 0;

    if (regex == NULL || regex->tnfa == NULL) {
        return REG_NOMATCH;
    }

    tnfa = regex->tnfa;
    if (tnfa->num_states <= 0 || tnfa->num_states > META_MAX_TNFA_STATES
        || tnfa->num_tags < 0 || tnfa->num_tags > META_MAX_TNFA_TAGS
        || tnfa->num_transitions < 0
        || (tnfa->num_transitions > 0 && tnfa->transitions == NULL)
        || tnfa->start_state < 0 || tnfa->start_state >= tnfa->num_states
        || tnfa->final_state < 0 || tnfa->final_state >= tnfa->num_states) {
        return REG_NOMATCH;
    }

    state_count = tnfa->num_states;
    tag_count = tnfa->num_tags + 1;
    edge_count = (tnfa->num_transitions > 0 ? tnfa->num_transitions : 1);
    stack_cap = state_count + edge_count + 1;
    use_state_slices = match_tnfa_can_use_state_slices(tnfa);

    configs_a = malloc2(SIZEOF(*configs_a)*state_count);
    configs_b = malloc2(SIZEOF(*configs_b)*state_count);
    stack = malloc2(SIZEOF(*stack)*stack_cap);
    tags_a = malloc2(SIZEOF(*tags_a)*state_count * tag_count);
    tags_b = malloc2(SIZEOF(*tags_b)*state_count * tag_count);
    stack_tags = malloc2(SIZEOF(*stack_tags)*stack_cap * tag_count);
    edge_indices = malloc2(SIZEOF(*edge_indices)*edge_count);
    saved_tags = malloc2(SIZEOF(*saved_tags)*tag_count);
    work_tags = malloc2(SIZEOF(*work_tags)*tag_count);
    closed_seen = malloc2(SIZEOF(*closed_seen)*state_count);
    seen = malloc2(SIZEOF(*seen)*state_count);

    if (configs_a == NULL || configs_b == NULL || stack == NULL
        || tags_a == NULL || tags_b == NULL || stack_tags == NULL
        || edge_indices == NULL || saved_tags == NULL || work_tags == NULL
        || closed_seen == NULL || seen == NULL) {
        goto cleanup;
    }

    match_tnfa_init_tags(saved_tags, tag_count);

    configs_a[0].state = tnfa->start_state;
    match_tnfa_init_tags(match_tnfa_tag_row(tags_a, tag_count, 0), tag_count);

    current = configs_a;
    current_tags = tags_a;
    closed = configs_b;
    closed_tags = tags_b;
    current_count = 1;

    closed_count = match_tnfa_epsilon_closure(
        tnfa, use_state_slices, input, input_len, pos, current, current_tags,
        current_count, closed, closed_tags, stack, stack_tags, stack_cap,
        closed_seen, edge_indices, work_tags, tag_count);
    if (closed_count < 0) {
        goto cleanup;
    }

    {
        int32 accept_index = match_tnfa_find_accept(tnfa, closed, closed_count);
        if (accept_index >= 0
            && (!regex->has_end_anchor
                || match_tnfa_at_end(input, input_len, pos))) {
            accepted = 1;
            accepted_end = pos;
            match_tnfa_save_accept(
                match_tnfa_tag_row(closed_tags, tag_count, accept_index),
                saved_tags, tag_count);
        }
    }

    while (!match_tnfa_at_end(input, input_len, pos)) {
        int32 next_count;
        int32 accept_index;
        MetaTnfaConfig *tmp_configs;
        int32 *tmp_tags;

        next_count = match_tnfa_step(tnfa, use_state_slices, input, input_len,
                                     pos, closed, closed_tags, closed_count,
                                     current, current_tags, seen, tag_count);
        if (next_count < 0) {
            goto cleanup;
        }
        if (next_count == 0) {
            break;
        }

        pos += 1;

        tmp_configs = current;
        current = closed;
        closed = tmp_configs;

        tmp_tags = current_tags;
        current_tags = closed_tags;
        closed_tags = tmp_tags;

        closed_count = match_tnfa_epsilon_closure(
            tnfa, use_state_slices, input, input_len, pos, closed, closed_tags,
            next_count, current, current_tags, stack, stack_tags, stack_cap,
            closed_seen, edge_indices, work_tags, tag_count);
        if (closed_count < 0) {
            goto cleanup;
        }

        tmp_configs = current;
        current = closed;
        closed = tmp_configs;

        tmp_tags = current_tags;
        current_tags = closed_tags;
        closed_tags = tmp_tags;

        accept_index = match_tnfa_find_accept(tnfa, closed, closed_count);
        if (accept_index >= 0
            && (!regex->has_end_anchor
                || match_tnfa_at_end(input, input_len, pos))) {
            accepted = 1;
            accepted_end = pos;
            match_tnfa_save_accept(
                match_tnfa_tag_row(closed_tags, tag_count, accept_index),
                saved_tags, tag_count);
        }
    }

    if (accepted) {
        match_tnfa_fill_pmatch(regex, start_pos, accepted_end, saved_tags,
                               pmatch, pmatch_len);
        result = 0;
    }

cleanup:
    if (configs_a != NULL) {
        free2(configs_a, SIZEOF(*configs_a)*state_count);
    }
    if (configs_b != NULL) {
        free2(configs_b, SIZEOF(*configs_b)*state_count);
    }
    if (stack != NULL) {
        free2(stack, SIZEOF(*stack)*stack_cap);
    }
    if (tags_a != NULL) {
        free2(tags_a, SIZEOF(*tags_a)*state_count * tag_count);
    }
    if (tags_b != NULL) {
        free2(tags_b, SIZEOF(*tags_b)*state_count * tag_count);
    }
    if (stack_tags != NULL) {
        free2(stack_tags, SIZEOF(*stack_tags)*stack_cap * tag_count);
    }
    if (edge_indices != NULL) {
        free2(edge_indices, SIZEOF(*edge_indices)*edge_count);
    }
    if (saved_tags != NULL) {
        free2(saved_tags, SIZEOF(*saved_tags)*tag_count);
    }
    if (work_tags != NULL) {
        free2(work_tags, SIZEOF(*work_tags)*tag_count);
    }
    if (closed_seen != NULL) {
        free2(closed_seen, SIZEOF(*closed_seen)*state_count);
    }
    if (seen != NULL) {
        free2(seen, SIZEOF(*seen)*state_count);
    }
    return result;
}

#endif /* META_MATCH_TNFA_C */
