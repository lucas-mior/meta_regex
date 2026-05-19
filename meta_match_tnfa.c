#if !defined(META_MATCH_TNFA_C)
#define META_MATCH_TNFA_C

#include <regex.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "meta.h"

#define META_TNFA_MAX_ACTIVE_CONFIGS META_MAX_TNFA_STATES
#define META_TNFA_MAX_TAG_VALUES (META_MAX_TNFA_TAGS + 1)

static const MatcherFeatures match_features_tnfa = {
    .supports = (enum MetaOpType)(META_OP_END
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
                                  | META_OP_NON_WORD_BOUNDARY),
    .extracts = true,
};

typedef struct MetaTnfaConfig {
    int32 state;
    int32 tag_values[META_TNFA_MAX_TAG_VALUES];
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

static void
match_tnfa_apply_tag(MetaTnfaConfig *cfg, int32 tag, int32 pos) {
    int32 id;

    if (tag == META_TNFA_TAG_NONE) {
        return;
    }

    id = META_TNFA_TAG_ID(tag);
    if (id <= 0 || id > META_MAX_TNFA_TAGS) {
        return;
    }

    if (META_TNFA_TAG_IS_NEGATIVE(tag)) {
        cfg->tag_values[id] = -1;
    } else {
        cfg->tag_values[id] = pos;
    }
    return;
}

static void
match_tnfa_init_tags(MetaTnfaConfig *cfg) {
    for (int32 i = 0; i < META_TNFA_MAX_TAG_VALUES; i += 1) {
        cfg->tag_values[i] = -1;
    }
    return;
}

static int32
match_tnfa_collect_zero_width_edges(MetaTnfa *tnfa, int32 state, uint8 *input,
                                    int32 input_len, int32 pos,
                                    int32 *edge_indices) {
    int32 edge_count = 0;

    for (int32 i = 0; i < tnfa->num_transitions; i += 1) {
        MetaTnfaTransition *tr = &tnfa->transitions[i];
        if (tr->from != state) {
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
match_tnfa_epsilon_closure(MetaTnfa *tnfa, uint8 *input, int32 input_len,
                           int32 pos, MetaTnfaConfig *input_configs,
                           int32 input_count, MetaTnfaConfig *output_configs,
                           MetaTnfaConfig *stack, int32 *edge_indices) {
    uint8 closed_seen[META_TNFA_MAX_ACTIVE_CONFIGS];
    int32 stack_count = 0;
    int32 output_count = 0;

    if (tnfa->num_states > META_TNFA_MAX_ACTIVE_CONFIGS) {
        return -1;
    }

    for (int32 i = 0; i < META_TNFA_MAX_ACTIVE_CONFIGS; i += 1) {
        closed_seen[i] = 0;
    }

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
        if (stack_count >= META_TNFA_MAX_ACTIVE_CONFIGS) {
            return -1;
        }
        stack[stack_count] = input_configs[i];
        stack_count += 1;
    }

    while (stack_count > 0) {
        MetaTnfaConfig cfg;
        int32 edge_count;
        int32 selected_count = 0;

        stack_count -= 1;
        cfg = stack[stack_count];

        if (cfg.state < 0 || cfg.state >= tnfa->num_states) {
            continue;
        }
        if (closed_seen[cfg.state]) {
            continue;
        }
        closed_seen[cfg.state] = 1;

        if (output_count >= META_TNFA_MAX_ACTIVE_CONFIGS) {
            return -1;
        }
        output_configs[output_count] = cfg;
        output_count += 1;

        edge_count = match_tnfa_collect_zero_width_edges(tnfa, cfg.state,
                                                         input, input_len,
                                                         pos, edge_indices);

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
            MetaTnfaConfig next = cfg;

            next.state = tr->to;
            if (tr->kind == META_TNFA_TRANS_EPSILON) {
                match_tnfa_apply_tag(&next, tr->tag, pos);
            }

            if (stack_count >= META_TNFA_MAX_ACTIVE_CONFIGS) {
                return -1;
            }
            stack[stack_count] = next;
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
match_tnfa_step(MetaTnfa *tnfa, uint8 *input, int32 input_len, int32 pos,
                MetaTnfaConfig *closed_configs, int32 closed_count,
                MetaTnfaConfig *next_configs) {
    uint8 seen[META_TNFA_MAX_ACTIVE_CONFIGS];
    int32 c = match_tnfa_char_at(input, input_len, pos);
    int32 next_count = 0;

    if (tnfa->num_states > META_TNFA_MAX_ACTIVE_CONFIGS) {
        return -1;
    }

    for (int32 i = 0; i < META_TNFA_MAX_ACTIVE_CONFIGS; i += 1) {
        seen[i] = 0;
    }

    for (int32 i = 0; i < closed_count; i += 1) {
        MetaTnfaConfig *cfg = &closed_configs[i];

        for (int32 j = 0; j < tnfa->num_transitions; j += 1) {
            MetaTnfaTransition *tr = &tnfa->transitions[j];
            if (tr->from != cfg->state) {
                continue;
            }
            if (!match_tnfa_symbol_transition_matches(tr, c)) {
                continue;
            }
            if (tr->to < 0 || tr->to >= tnfa->num_states || seen[tr->to]) {
                continue;
            }
            if (next_count >= META_TNFA_MAX_ACTIVE_CONFIGS) {
                return -1;
            }
            next_configs[next_count] = *cfg;
            next_configs[next_count].state = tr->to;
            next_count += 1;
            seen[tr->to] = 1;
        }
    }

    return next_count;
}

static MetaTnfaConfig *
match_tnfa_find_accept(MetaTnfa *tnfa, MetaTnfaConfig *configs,
                       int32 config_count) {
    for (int32 i = 0; i < config_count; i += 1) {
        if (configs[i].state == tnfa->final_state) {
            return &configs[i];
        }
    }
    return NULL;
}

static void
match_tnfa_save_accept(MetaTnfa *tnfa, MetaTnfaConfig *accept_config,
                       int32 *saved_tags) {
    for (int32 i = 0; i <= tnfa->num_tags && i <= META_MAX_TNFA_TAGS; i += 1) {
        saved_tags[i] = accept_config->tag_values[i];
    }
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

        if (tag->id <= 0 || tag->id > META_MAX_TNFA_TAGS) {
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
    int32 *edge_indices = NULL;
    MetaTnfaConfig initial = {0};
    MetaTnfaConfig *current = NULL;
    MetaTnfaConfig *closed = NULL;
    int32 current_count = 0;
    int32 closed_count = 0;
    int32 pos = start_pos;
    int32 accepted = 0;
    int32 accepted_end = -1;
    int32 saved_tags[META_TNFA_MAX_TAG_VALUES];
    int32 result = REG_NOMATCH;

    if (regex == NULL || regex->tnfa == NULL) {
        return REG_NOMATCH;
    }

    tnfa = regex->tnfa;
    if (tnfa->num_states <= 0 || tnfa->num_states > META_TNFA_MAX_ACTIVE_CONFIGS
        || tnfa->num_tags < 0 || tnfa->num_tags > META_MAX_TNFA_TAGS
        || tnfa->num_transitions < 0
        || (tnfa->num_transitions > 0 && tnfa->transitions == NULL)) {
        return REG_NOMATCH;
    }

    configs_a = malloc(SIZEOF(*configs_a)*META_TNFA_MAX_ACTIVE_CONFIGS);
    configs_b = malloc(SIZEOF(*configs_b)*META_TNFA_MAX_ACTIVE_CONFIGS);
    stack = malloc(SIZEOF(*stack)*META_TNFA_MAX_ACTIVE_CONFIGS);
    edge_indices = malloc(SIZEOF(*edge_indices)
                          *(tnfa->num_transitions > 0
                                ? tnfa->num_transitions
                                : 1));

    if (configs_a == NULL || configs_b == NULL || stack == NULL
        || edge_indices == NULL) {
        goto cleanup;
    }

    for (int32 i = 0; i < META_TNFA_MAX_TAG_VALUES; i += 1) {
        saved_tags[i] = -1;
    }

    initial.state = tnfa->start_state;
    match_tnfa_init_tags(&initial);
    configs_a[0] = initial;
    current = configs_a;
    closed = configs_b;
    current_count = 1;

    closed_count = match_tnfa_epsilon_closure(tnfa, input, input_len, pos,
                                              current, current_count, closed,
                                              stack, edge_indices);
    if (closed_count < 0) {
        goto cleanup;
    }

    {
        MetaTnfaConfig *accept = match_tnfa_find_accept(tnfa, closed,
                                                        closed_count);
        if (accept != NULL
            && (!regex->has_end_anchor
                || match_tnfa_at_end(input, input_len, pos))) {
            accepted = 1;
            accepted_end = pos;
            match_tnfa_save_accept(tnfa, accept, saved_tags);
        }
    }

    while (!match_tnfa_at_end(input, input_len, pos)) {
        int32 next_count;
        MetaTnfaConfig *tmp;
        MetaTnfaConfig *accept;

        next_count = match_tnfa_step(tnfa, input, input_len, pos, closed,
                                     closed_count, current);
        if (next_count < 0) {
            goto cleanup;
        }
        if (next_count == 0) {
            break;
        }

        pos += 1;

        tmp = current;
        current = closed;
        closed = tmp;

        closed_count = match_tnfa_epsilon_closure(tnfa, input, input_len, pos,
                                                  closed, next_count, current,
                                                  stack, edge_indices);
        if (closed_count < 0) {
            goto cleanup;
        }

        tmp = current;
        current = closed;
        closed = tmp;

        accept = match_tnfa_find_accept(tnfa, closed, closed_count);
        if (accept != NULL
            && (!regex->has_end_anchor
                || match_tnfa_at_end(input, input_len, pos))) {
            accepted = 1;
            accepted_end = pos;
            match_tnfa_save_accept(tnfa, accept, saved_tags);
        }
    }

    if (accepted) {
        match_tnfa_fill_pmatch(regex, start_pos, accepted_end, saved_tags,
                               pmatch, pmatch_len);
        result = 0;
    }

cleanup:
    if (configs_a != NULL) {
        free(configs_a);
    }
    if (configs_b != NULL) {
        free(configs_b);
    }
    if (stack != NULL) {
        free(stack);
    }
    if (edge_indices != NULL) {
        free(edge_indices);
    }
    return result;
}

#endif /* META_MATCH_TNFA_C */
