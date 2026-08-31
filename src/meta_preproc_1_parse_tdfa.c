#include "cbase.h"
#include "meta_preproc.h"

/* Tagged DFA construction helpers. */

typedef struct TdfaBuildConfig {
    int32 tnfa_state;
    int32 look[PREPROC_MAX_TNFA_TAGS + 1];
} TdfaBuildConfig;

typedef struct TdfaWorkConfig {
    int32 tnfa_state;
    int32 source_config;
    int32 h[PREPROC_MAX_TNFA_TAGS + 1];
    int32 look[PREPROC_MAX_TNFA_TAGS + 1];
} TdfaWorkConfig;

typedef struct TdfaBuildState {
    int32 prev_is_w;
    int32 curr_is_w;
    int32 config_count;
    TdfaBuildConfig *configs;

    /* Flattened [config][tag] register table. */
    int32 *regs;
} TdfaBuildState;

static int32
tdfa_is_word_char(int32 c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '_');
}

static void
tdfa_init_tag_vector(int32 *v, int32 tag_count) {
    for (int32 i = 0; i < tag_count; i += 1) {
        v[i] = -1;
    }
    return;
}

static void
tdfa_copy_tag_vector(int32 *dst, int32 *src, int32 tag_count) {
    for (int32 i = 0; i < tag_count; i += 1) {
        dst[i] = src[i];
    }
    return;
}

static int32
tdfa_state_reg(TdfaBuildState *state, int32 config, int32 tag_count,
               int32 tag) {
    return state->regs[config*tag_count + tag];
}

static int32
tdfa_tag_is_fixed(ParsedTdfa *tdfa, int32 tag) {
    return (tag > 0 && tag <= tdfa->num_tags
            && tdfa->tags[tag - 1].fixed_base_tag > 0);
}

static void
tdfa_normalize_ops(ParsedTdfa *tdfa, int32 first_op) {
    int32 write = first_op;

    if (first_op < 0 || first_op > tdfa->num_ops) {
        return;
    }

    for (int32 read = first_op; read < tdfa->num_ops; read += 1) {
        MetaTdfaRegOp op = tdfa->ops[read];
        int32 duplicate = 0;

        if (op.kind == META_TDFA_REGOP_COPY && op.dst == op.src) {
            continue;
        }

        for (int32 i = first_op; i < write; i += 1) {
            MetaTdfaRegOp *prev = &tdfa->ops[i];
            if (prev->kind == op.kind && prev->dst == op.dst
                && prev->src == op.src) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        tdfa->ops[write] = op;
        write += 1;
    }

    tdfa->num_ops = write;
    return;
}

static bool
tdfa_add_regop(ParsedTdfa *tdfa, enum MetaTdfaRegOpKind kind,
               int32 dst, int32 src) {
    MetaTdfaRegOp *op;

    if (kind == META_TDFA_REGOP_COPY && dst == src) {
        return true;
    }

    if (tdfa->num_ops >= preproc_config.max_tdfa_regops) {
        return false;
    }

    op = &tdfa->ops[tdfa->num_ops];
    op->kind = kind;
    op->dst = dst;
    op->src = src;
    tdfa->num_ops += 1;

    return true;
}

static bool
tdfa_add_set_from_tag(ParsedTdfa *tdfa, int32 dst, int32 tag_value) {
    if (tag_value == 0) {
        return tdfa_add_regop(tdfa, META_TDFA_REGOP_SET_NIL, dst, 0);
    }
    return tdfa_add_regop(tdfa, META_TDFA_REGOP_SET_POS, dst, 0);
}

static bool
tdfa_tnfa_has_context_assertions(ParsedTnfa *tnfa) {
    for (int32 i = 0; i < tnfa->num_transitions; i += 1) {
        MetaTnfaTransition *tr = &tnfa->transitions[i];
        if (tr->kind == META_TNFA_TRANS_WORD_START
            || tr->kind == META_TNFA_TRANS_WORD_END
            || tr->kind == META_TNFA_TRANS_WORD_BOUNDARY
            || tr->kind == META_TNFA_TRANS_NON_WORD_BOUNDARY) {
            return true;
        }
    }
    return false;
}

static int32
tdfa_assertion_matches(enum MetaTnfaTransitionKind kind, int32 prev_is_w,
                       int32 curr_is_w) {
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
tdfa_zero_width_transition_enabled(MetaTnfaTransition *tr, int32 prev_is_w,
                                   int32 curr_is_w) {
    if (tr->kind == META_TNFA_TRANS_EPSILON) {
        return 1;
    }
    return tdfa_assertion_matches(tr->kind, prev_is_w, curr_is_w);
}

static int32
tdfa_transition_is_zero_width(MetaTnfaTransition *tr) {
    return (tr->kind == META_TNFA_TRANS_EPSILON
            || tr->kind == META_TNFA_TRANS_WORD_START
            || tr->kind == META_TNFA_TRANS_WORD_END
            || tr->kind == META_TNFA_TRANS_WORD_BOUNDARY
            || tr->kind == META_TNFA_TRANS_NON_WORD_BOUNDARY);
}

static bool
tdfa_symbol_transition_matches(MetaTnfaTransition *tr, int32 c) {
    if (tr->kind == META_TNFA_TRANS_LITERAL) {
        return c == tr->value;
    }
    if (tr->kind == META_TNFA_TRANS_CLASS) {
        return ((tr->mask[c / 32] & (1u << (c % 32))) != 0);
    }
    if (tr->kind == META_TNFA_TRANS_ANY) {
        return c != '\0';
    }
    return false;
}

static int32
tdfa_collect_zero_width_edges(ParsedTnfa *tnfa, int32 state, int32 prev_is_w,
                              int32 curr_is_w, int32 *edge_indices) {
    int32 edge_count = 0;

    for (int32 i = 0; i < tnfa->num_transitions; i += 1) {
        MetaTnfaTransition *tr = &tnfa->transitions[i];
        if (tr->from != state) {
            continue;
        }
        if (!tdfa_transition_is_zero_width(tr)) {
            continue;
        }
        if (!tdfa_zero_width_transition_enabled(tr, prev_is_w, curr_is_w)) {
            continue;
        }
        edge_indices[edge_count] = i;
        edge_count += 1;
    }

    for (int32 i = 1; i < edge_count; i += 1) {
        int32 edge = edge_indices[i];
        int32 priority = tnfa->transitions[edge].priority;
        int32 j = i - 1;

        while (j >= 0
               && tnfa->transitions[edge_indices[j]].priority > priority) {
            edge_indices[j + 1] = edge_indices[j];
            j -= 1;
        }
        edge_indices[j + 1] = edge;
    }

    return edge_count;
}

static void
tdfa_apply_lookahead_tag(TdfaWorkConfig *cfg, int32 tag) {
    int32 id;

    if (tag == META_TNFA_TAG_NONE) {
        return;
    }

    id = META_TNFA_TAG_ID(tag);
    if (id <= 0 || id > PREPROC_MAX_TNFA_TAGS) {
        return;
    }

    cfg->look[id] = META_TNFA_TAG_IS_NEGATIVE(tag) ? 0 : 1;
    return;
}

static int32
tdfa_epsilon_closure(ParsedTnfa *tnfa, int32 tag_count,
                     TdfaWorkConfig *input_configs, int32 input_count,
                     TdfaWorkConfig *output_configs, TdfaWorkConfig *stack,
                     int32 *edge_indices, int32 work_capacity, int32 prev_is_w,
                     int32 curr_is_w) {
    uint8 closed_seen[PREPROC_MAX_TNFA_STATES];
    int32 stack_count = 0;
    int32 output_count = 0;

    for (int32 i = 0; i < tnfa->num_states; i += 1) {
        closed_seen[i] = 0;
    }

    for (int32 i = input_count - 1; i >= 0; i -= 1) {
        if (stack_count >= work_capacity) {
            return -1;
        }
        stack[stack_count] = input_configs[i];
        stack_count += 1;
    }

    while (stack_count > 0) {
        TdfaWorkConfig cfg;
        int32 edge_count;
        int32 selected_count = 0;

        stack_count -= 1;
        cfg = stack[stack_count];

        if (cfg.tnfa_state < 0 || cfg.tnfa_state >= tnfa->num_states) {
            continue;
        }
        if (closed_seen[cfg.tnfa_state]) {
            continue;
        }
        closed_seen[cfg.tnfa_state] = 1;

        if (output_count >= work_capacity) {
            return -1;
        }
        output_configs[output_count] = cfg;
        output_count += 1;

        edge_count = tdfa_collect_zero_width_edges(
            tnfa, cfg.tnfa_state, prev_is_w, curr_is_w, edge_indices);

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
            TdfaWorkConfig next = cfg;

            next.tnfa_state = tr->to;
            if (tr->kind == META_TNFA_TRANS_EPSILON) {
                tdfa_apply_lookahead_tag(&next, tr->tag);
            }

            if (stack_count >= work_capacity) {
                return -1;
            }
            stack[stack_count] = next;
            stack_count += 1;
        }
    }

    (void)tag_count;
    return output_count;
}

static int32
tdfa_step_on_symbol(ParsedTnfa *tnfa, TdfaBuildState *source, int32 symbol,
                    int32 tag_count, TdfaWorkConfig *work,
                    int32 work_capacity) {
    int32 work_count = 0;

    for (int32 i = 0; i < source->config_count; i += 1) {
        TdfaBuildConfig *cfg = &source->configs[i];

        for (int32 j = 0; j < tnfa->num_transitions; j += 1) {
            MetaTnfaTransition *tr = &tnfa->transitions[j];
            TdfaWorkConfig *dst;

            if (tr->from != cfg->tnfa_state) {
                continue;
            }
            if (!tdfa_symbol_transition_matches(tr, symbol)) {
                continue;
            }
            if (tr->to < 0 || tr->to >= tnfa->num_states) {
                continue;
            }
            if (work_count >= work_capacity) {
                return -1;
            }

            dst = &work[work_count];
            dst->tnfa_state = tr->to;
            dst->source_config = i;
            tdfa_copy_tag_vector(dst->h, cfg->look, tag_count);
            tdfa_init_tag_vector(dst->look, tag_count);
            work_count += 1;
        }
    }

    return work_count;
}

static bool
tdfa_same_signature(ParsedTdfa *tdfa, TdfaBuildState *state,
                    TdfaWorkConfig *configs, int32 config_count,
                    int32 tag_count, int32 prev_is_w, int32 curr_is_w) {
    if (state->prev_is_w != prev_is_w || state->curr_is_w != curr_is_w) {
        return false;
    }
    if (state->config_count != config_count) {
        return false;
    }

    for (int32 i = 0; i < config_count; i += 1) {
        if (state->configs[i].tnfa_state != configs[i].tnfa_state) {
            return false;
        }
        for (int32 t = 1; t < tag_count; t += 1) {
            if (tdfa_tag_is_fixed(tdfa, t)) {
                continue;
            }
            if (state->configs[i].look[t] != configs[i].look[t]) {
                return false;
            }
        }
    }

    return true;
}

static int32
tdfa_find_state(TdfaBuildState *states, ParsedTdfa *tdfa,
                TdfaWorkConfig *configs, int32 config_count, int32 tag_count,
                int32 prev_is_w, int32 curr_is_w) {
    for (int32 i = 0; i < tdfa->num_states; i += 1) {
        if (tdfa_same_signature(tdfa, &states[i], configs, config_count,
                                tag_count, prev_is_w, curr_is_w)) {
            return i;
        }
    }
    return -1;
}

static bool
tdfa_add_final_ops(ParsedTdfa *tdfa, ParsedTnfa *tnfa,
                   TdfaBuildState *build_state, int32 state_id,
                   int32 tag_count) {
    MetaTdfaState *state = &tdfa->states[state_id];

    for (int32 i = 0; i < build_state->config_count; i += 1) {
        TdfaBuildConfig *cfg = &build_state->configs[i];

        if (cfg->tnfa_state != tnfa->final_state) {
            continue;
        }

        state->is_accepting = 1;
        state->first_final_op = tdfa->num_ops;

        for (int32 t = 1; t < tag_count; t += 1) {
            int32 dst = tdfa->final_register_base + t - 1;

            if (tdfa_tag_is_fixed(tdfa, t)) {
                continue;
            }

            if (cfg->look[t] != -1) {
                if (!tdfa_add_set_from_tag(tdfa, dst, cfg->look[t])) {
                    return false;
                }
            } else {
                int32 src = tdfa_state_reg(build_state, i, tag_count, t);
                if (!tdfa_add_regop(tdfa, META_TDFA_REGOP_COPY, dst, src)) {
                    return false;
                }
            }
        }

        tdfa_normalize_ops(tdfa, state->first_final_op);
        state->final_op_count = tdfa->num_ops - state->first_final_op;
        return true;
    }

    return true;
}

static int32
tdfa_add_state(TdfaBuildState *states, ParsedTdfa *tdfa, ParsedTnfa *tnfa,
               TdfaWorkConfig *configs, int32 config_count, int32 tag_count,
               int32 *next_register, int32 prev_is_w, int32 curr_is_w) {
    int32 state_id;
    TdfaBuildState *build_state;
    MetaTdfaState *state;

    if (tdfa->num_states >= preproc_config.max_tdfa_states) {
        return -1;
    }

    state_id = tdfa->num_states;
    tdfa->num_states += 1;

    state = &tdfa->states[state_id];
    state->is_accepting = 0;
    state->first_transition = -1;
    state->transition_count = 0;
    state->first_final_op = -1;
    state->final_op_count = 0;

    if (tdfa->transition_index_stride > 0) {
        int32 base = state_id*tdfa->transition_index_stride;
        int32 end = base + tdfa->transition_index_stride;
        if (end > preproc_config.max_tdfa_transition_index_entries) {
            return -1;
        }
        for (int32 i = base; i < end; i += 1) {
            tdfa->transition_index[i] = -1;
        }
        if (tdfa->transition_index_count < end) {
            tdfa->transition_index_count = end;
        }
    }

    build_state = &states[state_id];
    build_state->prev_is_w = prev_is_w;
    build_state->curr_is_w = curr_is_w;
    build_state->config_count = config_count;
    build_state->configs = NULL;
    build_state->regs = NULL;

    if (config_count > 0) {
        build_state->configs
            = malloc2(SIZEOF(*build_state->configs)*config_count);
        build_state->regs
            = malloc2(SIZEOF(*build_state->regs)*config_count * tag_count);
        if (build_state->configs == NULL || build_state->regs == NULL) {
            return -1;
        }
    }

    for (int32 i = 0; i < config_count; i += 1) {
        build_state->configs[i].tnfa_state = configs[i].tnfa_state;
        tdfa_copy_tag_vector(build_state->configs[i].look, configs[i].look,
                             tag_count);

        for (int32 t = 0; t < tag_count; t += 1) {
            if (t == 0 || tdfa_tag_is_fixed(tdfa, t)) {
                build_state->regs[i*tag_count + t] = 0;
            } else {
                if (*next_register > preproc_config.max_tdfa_registers) {
                    return -1;
                }
                build_state->regs[i*tag_count + t] = *next_register;
                *next_register += 1;
            }
        }
    }

    if (!tdfa_add_final_ops(tdfa, tnfa, build_state, state_id, tag_count)) {
        return -1;
    }

    return state_id;
}

static bool
tdfa_emit_transition_ops(ParsedTdfa *tdfa, TdfaBuildState *source,
                         TdfaBuildState *target, TdfaWorkConfig *closed,
                         int32 closed_count, int32 tag_count) {
    for (int32 i = 0; i < closed_count; i += 1) {
        int32 source_config = closed[i].source_config;

        if (source_config < 0 || source_config >= source->config_count) {
            return false;
        }

        for (int32 t = 1; t < tag_count; t += 1) {
            int32 dst;

            if (tdfa_tag_is_fixed(tdfa, t)) {
                continue;
            }

            dst = tdfa_state_reg(target, i, tag_count, t);

            if (closed[i].h[t] != -1 && closed[i].look[t] == -1) {
                if (!tdfa_add_set_from_tag(tdfa, dst, closed[i].h[t])) {
                    return false;
                }
            } else {
                int32 src = tdfa_state_reg(source, source_config, tag_count, t);
                if (src != dst) {
                    if (!tdfa_add_regop(tdfa, META_TDFA_REGOP_COPY, dst, src)) {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

static bool
tdfa_add_transition(ParsedTdfa *tdfa, int32 from, int32 to, int32 symbol,
                    int32 next_is_word, int32 first_op, int32 op_count) {
    int32 transition_id;
    MetaTdfaTransition *tr;

    if (tdfa->num_transitions >= preproc_config.max_tdfa_transitions) {
        return false;
    }

    transition_id = tdfa->num_transitions;
    tr = &tdfa->transitions[transition_id];
    tr->from = from;
    tr->to = to;
    tr->symbol = symbol;
    tr->next_is_word = next_is_word;
    tr->first_op = first_op;
    tr->op_count = op_count;

    if (tdfa->transition_index_stride > 0) {
        int32 context_offset = 0;
        int32 index;
        if (tdfa->uses_context) {
            if (next_is_word < 0) {
                return false;
            }
            context_offset = next_is_word*META_ALPHABET_SIZE;
        }
        index = from*tdfa->transition_index_stride + context_offset + symbol;
        if (index < 0
            || index >= preproc_config.max_tdfa_transition_index_entries) {
            return false;
        }
        tdfa->transition_index[index] = transition_id;
    }

    tdfa->num_transitions += 1;
    return true;
}

static bool
build_tdfa_from_tnfa(ParsedTdfa *tdfa, ParsedTnfa *tnfa) {
    int32 tag_count;
    int32 next_register;
    int32 work_capacity;
    int32 uses_context;
    TdfaBuildState *build_states = NULL;
    TdfaWorkConfig *work = NULL;
    TdfaWorkConfig *closed = NULL;
    TdfaWorkConfig *stack = NULL;
    int32 *edge_indices = NULL;
    TdfaWorkConfig initial;

    if (tdfa == NULL || tnfa == NULL) {
        return false;
    }
    if (tnfa->num_tags < 0 || tnfa->num_tags > preproc_config.max_tnfa_tags
        || tnfa->num_states <= 0
        || tnfa->num_states > preproc_config.max_tnfa_states
        || tnfa->num_transitions < 0) {
        return false;
    }

    memset64(tdfa, 0, SIZEOF(*tdfa));

    tdfa->num_tags = tnfa->num_tags;
    tdfa->start_state = 0;
    tdfa->start_state_nw_nw = -1;
    tdfa->start_state_nw_w = -1;
    tdfa->start_state_w_nw = -1;
    tdfa->start_state_w_w = -1;
    tdfa->final_register_base = 1;
    for (int32 i = 0; i < tnfa->num_tags; i += 1) {
        tdfa->tags[i] = tnfa->tags[i];
    }

    if (tnfa->num_tags > preproc_config.max_tdfa_registers) {
        return false;
    }

    tag_count = tnfa->num_tags + 1;
    next_register = tnfa->num_tags + 1;
    work_capacity = tnfa->num_states + tnfa->num_transitions + 1;
    uses_context = tdfa_tnfa_has_context_assertions(tnfa);
    tdfa->uses_context = uses_context;
    tdfa->transition_index_stride
        = preproc_config.emit_tdfa_transition_index
              ? (uses_context ? META_ALPHABET_SIZE*2 : META_ALPHABET_SIZE)
              : 0;
    tdfa->transition_index_count = 0;
    if (work_capacity <= 0 || work_capacity > PREPROC_MAX_TDFA_WORK_CONFIGS) {
        return false;
    }

    build_states
        = malloc2(SIZEOF(*build_states)*preproc_config.max_tdfa_states);
    work = malloc2(SIZEOF(*work)*work_capacity);
    closed = malloc2(SIZEOF(*closed)*work_capacity);
    stack = malloc2(SIZEOF(*stack)*work_capacity);
    edge_indices
        = malloc2(SIZEOF(*edge_indices)
                  * (tnfa->num_transitions > 0 ? tnfa->num_transitions : 1));
    if (build_states == NULL || work == NULL || closed == NULL || stack == NULL
        || edge_indices == NULL) {
        return false;
    }
    memset64(build_states, 0,
             SIZEOF(*build_states)*preproc_config.max_tdfa_states);

    initial.tnfa_state = tnfa->start_state;
    initial.source_config = -1;
    tdfa_init_tag_vector(initial.h, tag_count);
    tdfa_init_tag_vector(initial.look, tag_count);

    if (uses_context) {
        for (int32 prev = 0; prev <= 1; prev += 1) {
            for (int32 curr = 0; curr <= 1; curr += 1) {
                int32 initial_count = tdfa_epsilon_closure(
                    tnfa, tag_count, &initial, 1, closed, stack, edge_indices,
                    work_capacity, prev, curr);
                int32 state_id;

                if (initial_count < 0) {
                    return false;
                }

                state_id
                    = tdfa_find_state(build_states, tdfa, closed, initial_count,
                                      tag_count, prev, curr);
                if (state_id < 0) {
                    state_id = tdfa_add_state(build_states, tdfa, tnfa, closed,
                                              initial_count, tag_count,
                                              &next_register, prev, curr);
                    if (state_id < 0) {
                        return false;
                    }
                }

                if (!prev && !curr) {
                    tdfa->start_state_nw_nw = state_id;
                } else if (!prev && curr) {
                    tdfa->start_state_nw_w = state_id;
                } else if (prev && !curr) {
                    tdfa->start_state_w_nw = state_id;
                } else {
                    tdfa->start_state_w_w = state_id;
                }
            }
        }
        tdfa->start_state = tdfa->start_state_nw_nw;
    } else {
        int32 initial_count
            = tdfa_epsilon_closure(tnfa, tag_count, &initial, 1, closed, stack,
                                   edge_indices, work_capacity, 0, 0);
        if (initial_count < 0) {
            return false;
        }
        if (tdfa_add_state(build_states, tdfa, tnfa, closed, initial_count,
                           tag_count, &next_register, 0, 0)
            != 0) {
            return false;
        }
        tdfa->start_state = 0;
        tdfa->start_state_nw_nw = 0;
        tdfa->start_state_nw_w = 0;
        tdfa->start_state_w_nw = 0;
        tdfa->start_state_w_w = 0;
    }

    for (int32 state_id = 0; state_id < tdfa->num_states; state_id += 1) {
        TdfaBuildState *source = &build_states[state_id];
        MetaTdfaState *state = &tdfa->states[state_id];

        state->first_transition = tdfa->num_transitions;

        for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
            int32 work_count;
            int32 next_min = 0;
            int32 next_max = uses_context ? 1 : 0;

            if (uses_context && tdfa_is_word_char(c) != source->curr_is_w) {
                continue;
            }

            work_count = tdfa_step_on_symbol(tnfa, source, c, tag_count, work,
                                             work_capacity);
            if (work_count < 0) {
                return false;
            }
            if (work_count == 0) {
                continue;
            }

            for (int32 next_curr = next_min; next_curr <= next_max;
                 next_curr += 1) {
                int32 prev = uses_context ? tdfa_is_word_char(c) : 0;
                int32 curr = uses_context ? next_curr : 0;
                int32 next_is_word = uses_context ? next_curr : -1;
                int32 closed_count;
                int32 target_id;
                int32 first_op;
                int32 op_count;

                closed_count = tdfa_epsilon_closure(
                    tnfa, tag_count, work, work_count, closed, stack,
                    edge_indices, work_capacity, prev, curr);
                if (closed_count < 0) {
                    return false;
                }

                target_id
                    = tdfa_find_state(build_states, tdfa, closed, closed_count,
                                      tag_count, prev, curr);
                if (target_id < 0) {
                    target_id = tdfa_add_state(build_states, tdfa, tnfa, closed,
                                               closed_count, tag_count,
                                               &next_register, prev, curr);
                    if (target_id < 0) {
                        return false;
                    }
                }

                first_op = tdfa->num_ops;
                if (!tdfa_emit_transition_ops(tdfa, source,
                                              &build_states[target_id], closed,
                                              closed_count, tag_count)) {
                    return false;
                }
                tdfa_normalize_ops(tdfa, first_op);
                op_count = tdfa->num_ops - first_op;

                if (!tdfa_add_transition(tdfa, state_id, target_id, c,
                                         next_is_word, first_op, op_count)) {
                    return false;
                }
            }
        }

        state->transition_count
            = tdfa->num_transitions - state->first_transition;
        if (state->transition_count == 0) {
            state->first_transition = -1;
        }
    }

    tdfa->num_registers = next_register - 1;
    return true;
}
