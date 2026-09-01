#include "cbase.h"
#include "meta_preproc.h"

static int32
is_word_char(int32 c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '_');
}

static int32
static_dfa_uses_word_context(ExtractedRegex *regex) {
    enum MetaOpType context_ops = (enum MetaOpType)(
        META_OP_WORD_START | META_OP_WORD_END | META_OP_WORD_BOUNDARY
        | META_OP_NON_WORD_BOUNDARY);

    return (regex->used_ops & context_ops) != 0;
}

static void
compute_epsilon_closure(DfaSet *set, ParsedOp *ops, int32 ops_count,
                        int32 prev_is_w, int32 curr_is_w, int32 *is_accepting) {
    int32 stack[PREPROC_MAX_TEMP_OPS];
    int32 stack_ptr = 0;

    for (int32 i = 0; i <= ops_count; i += 1) {
        if ((set->bits[i / 32] & (1u << (i % 32))) != 0) {
            stack[stack_ptr] = i;
            stack_ptr += 1;
        }
    }

    while (stack_ptr > 0) {
        int32 pc;
        ParsedOp *op;

        stack_ptr -= 1;
        pc = stack[stack_ptr];

        if (pc == ops_count) {
            *is_accepting = 1;
            continue;
        }

        op = &ops[pc];

        if (op->type == META_OP_SPLIT) {
            int32 t1 = pc + op->value;
            int32 t2 = pc + op->min;
            if (t1 >= 0 && t1 <= ops_count
                && !(set->bits[t1 / 32] & (1u << (t1 % 32)))) {
                set->bits[t1 / 32] |= (1u << (t1 % 32));
                stack[stack_ptr] = t1;
                stack_ptr += 1;
            }
            if (t2 >= 0 && t2 <= ops_count
                && !(set->bits[t2 / 32] & (1u << (t2 % 32)))) {
                set->bits[t2 / 32] |= (1u << (t2 % 32));
                stack[stack_ptr] = t2;
                stack_ptr += 1;
            }
        } else if (op->type == META_OP_JUMP) {
            int32 t1 = pc + op->value;
            if (t1 >= 0 && t1 <= ops_count
                && !(set->bits[t1 / 32] & (1u << (t1 % 32)))) {
                set->bits[t1 / 32] |= (1u << (t1 % 32));
                stack[stack_ptr] = t1;
                stack_ptr += 1;
            }
        } else if (op->type == META_OP_WORD_BOUNDARY) {
            if (prev_is_w != curr_is_w) {
                int32 t1 = pc + 1;
                if (t1 <= ops_count
                    && !(set->bits[t1 / 32] & (1u << (t1 % 32)))) {
                    set->bits[t1 / 32] |= (1u << (t1 % 32));
                    stack[stack_ptr] = t1;
                    stack_ptr += 1;
                }
            }
        } else if (op->type == META_OP_NON_WORD_BOUNDARY) {
            if (prev_is_w == curr_is_w) {
                int32 t1 = pc + 1;
                if (t1 <= ops_count
                    && !(set->bits[t1 / 32] & (1u << (t1 % 32)))) {
                    set->bits[t1 / 32] |= (1u << (t1 % 32));
                    stack[stack_ptr] = t1;
                    stack_ptr += 1;
                }
            }
        } else if (op->type == META_OP_WORD_START) {
            if (!prev_is_w && curr_is_w) {
                int32 t1 = pc + 1;
                if (t1 <= ops_count
                    && !(set->bits[t1 / 32] & (1u << (t1 % 32)))) {
                    set->bits[t1 / 32] |= (1u << (t1 % 32));
                    stack[stack_ptr] = t1;
                    stack_ptr += 1;
                }
            }
        } else if (op->type == META_OP_WORD_END) {
            if (prev_is_w && !curr_is_w) {
                int32 t1 = pc + 1;
                if (t1 <= ops_count
                    && !(set->bits[t1 / 32] & (1u << (t1 % 32)))) {
                    set->bits[t1 / 32] |= (1u << (t1 % 32));
                    stack[stack_ptr] = t1;
                    stack_ptr += 1;
                }
            }
        } else if (op->type == META_OP_GROUP_START) {
            int32 depth = 0;
            int32 t1 = pc + 1;
            if (t1 <= ops_count && !(set->bits[t1 / 32] & (1u << (t1 % 32)))) {
                set->bits[t1 / 32] |= (1u << (t1 % 32));
                stack[stack_ptr] = t1;
                stack_ptr += 1;
            }
            for (int32 i = pc + 1; i < ops_count; i += 1) {
                if (ops[i].type == META_OP_GROUP_START) {
                    depth += 1;
                } else if (ops[i].type == META_OP_GROUP_END) {
                    if (depth == 0) {
                        break;
                    }
                    depth -= 1;
                } else if (ops[i].type == META_OP_ALTERNATION && depth == 0) {
                    int32 t2 = i + 1;
                    if (t2 <= ops_count
                        && !(set->bits[t2 / 32] & (1u << (t2 % 32)))) {
                        set->bits[t2 / 32] |= (1u << (t2 % 32));
                        stack[stack_ptr] = t2;
                        stack_ptr += 1;
                    }
                }
            }
        } else if (op->type == META_OP_ALTERNATION) {
            int32 depth = 0;
            int32 i = pc + 1;
            while (i < ops_count) {
                if (ops[i].type == META_OP_GROUP_START) {
                    depth += 1;
                } else if (ops[i].type == META_OP_GROUP_END) {
                    if (depth == 0) {
                        break;
                    }
                    depth -= 1;
                }
                i += 1;
            }
            if (i <= ops_count && !(set->bits[i / 32] & (1u << (i % 32)))) {
                set->bits[i / 32] |= (1u << (i % 32));
                stack[stack_ptr] = i;
                stack_ptr += 1;
            }
        } else if (op->type == META_OP_GROUP_END) {
            int32 t1 = pc + 1;
            if (t1 <= ops_count && !(set->bits[t1 / 32] & (1u << (t1 % 32)))) {
                set->bits[t1 / 32] |= (1u << (t1 % 32));
                stack[stack_ptr] = t1;
                stack_ptr += 1;
            }
        }

        if (op->type == META_OP_LITERAL || op->type == META_OP_CLASS
            || op->type == META_OP_ANY) {
            if (pc + 1 < ops_count) {
                ParsedOp *next_op = &ops[pc + 1];
                if (next_op->type == META_OP_STAR
                    || next_op->type == META_OP_OPTIONAL
                    || (next_op->type == META_OP_BOUNDED
                        && next_op->min == 0)) {
                    int32 t1 = pc + 2;
                    if (t1 <= ops_count
                        && !(set->bits[t1 / 32] & (1u << (t1 % 32)))) {
                        set->bits[t1 / 32] |= (1u << (t1 % 32));
                        stack[stack_ptr] = t1;
                        stack_ptr += 1;
                    }
                }
            }
        }
    }
    return;
}

static void
compute_core_transitions(DfaSet *closed_set, ParsedOp *ops, int32 ops_count,
                         int32 c, DfaSet *next_core) {
    for (int32 i = 0; i < PREPROC_NFA_BITSET_WORDS; i += 1) {
        next_core->bits[i] = 0;
    }

    for (int32 i = 0; i < ops_count; i += 1) {
        if ((closed_set->bits[i / 32] & (1u << (i % 32))) != 0) {
            ParsedOp *op = &ops[i];
            int32 match = 0;

            if (op->type == META_OP_LITERAL) {
                if (c == op->value) {
                    match = 1;
                }
            } else if (op->type == META_OP_CLASS) {
                if (c >= 0 && c < META_ALPHABET_SIZE) {
                    if ((op->mask[c / 32] & (1u << (c % 32))) != 0) {
                        match = 1;
                    }
                }
            } else if (op->type == META_OP_ANY) {
                if (c != '\0') {
                    match = 1;
                }
            }

            if (match) {
                int32 t1 = i + 1;
                if (t1 < ops_count) {
                    ParsedOp *next_op = &ops[t1];
                    if (next_op->type == META_OP_STAR
                        || next_op->type == META_OP_PLUS) {
                        int32 t2 = i + 2;
                        next_core->bits[i / 32] |= (1u << (i % 32));
                        if (t2 <= ops_count) {
                            next_core->bits[t2 / 32] |= (1u << (t2 % 32));
                        }
                    } else if (next_op->type == META_OP_OPTIONAL
                               || next_op->type == META_OP_BOUNDED) {
                        int32 t2 = i + 2;
                        if (t2 <= ops_count) {
                            next_core->bits[t2 / 32] |= (1u << (t2 % 32));
                        }
                    } else {
                        next_core->bits[t1 / 32] |= (1u << (t1 % 32));
                    }
                } else {
                    next_core->bits[t1 / 32] |= (1u << (t1 % 32));
                }
            }
        }
    }
    return;
}

static void
emit_tnfa(ExtractedRegex *regex, StrBuilder *out) {
    ParsedTnfa *tnfa = regex->tnfa;

    if (!preproc_config.emit_tnfa || tnfa == NULL) {
        SB_APPEND(out, ", .tnfa = NULL");
        return;
    }

    SB_APPEND(out, ", .tnfa = &(MetaTnfa){ ");
    sb_printf(out, ".num_tags = %d, .num_states = %d, ",
                   tnfa->num_tags, tnfa->num_states);
    sb_printf(out, ".num_transitions = %d, .start_state = %d, ",
                   tnfa->num_transitions, tnfa->start_state);
    sb_printf(out, ".final_state = %d", tnfa->final_state);

    if (tnfa->num_tags > 0) {
        SB_APPEND(out, ", .tags = (MetaTnfaTag[]){\n");
        for (int32 i = 0; i < tnfa->num_tags; i += 1) {
            MetaTnfaTag *tag = &tnfa->tags[i];
            char *role = META_TNFA_TAG_str(tag->role);

            sb_printf(out, "{ .id = %d, .group = %d, .role = %s, ",
                           tag->id, tag->group, role);
            sb_printf(out, ".is_multivalued = %d, .fixed_base_tag = %d, ",
                           tag->is_multivalued, tag->fixed_base_tag);
            sb_printf(out, ".fixed_offset = %d },\n", tag->fixed_offset);

            META_TNFA_TAG_str_free(role);
        }
        SB_APPEND(out, "}");
    } else {
        SB_APPEND(out, ", .tags = NULL");
    }

    if (tnfa->num_states > 0) {
        SB_APPEND(out, ", .states = (MetaTnfaState[]){\n");
        for (int32 i = 0; i < tnfa->num_states; i += 1) {
            MetaTnfaState *state = &tnfa->states[i];

            sb_printf(out,
                      "{ .first_transition = %d, .transition_count = %d },\n",
                      state->first_transition, state->transition_count);
        }
        SB_APPEND(out, "}");
    } else {
        SB_APPEND(out, ", .states = NULL");
    }

    if (tnfa->num_transitions > 0) {
        SB_APPEND(out, ", .transitions = (MetaTnfaTransition[]){\n");
        for (int32 i = 0; i < tnfa->num_transitions; i += 1) {
            MetaTnfaTransition *tr = &tnfa->transitions[i];
            char *kind = META_TNFA_TRANS_str(tr->kind);
            uint32 *mask = tr->mask;

            SB_APPEND(out, "{ ");
            sb_printf(out, ".kind = %s, .from = %d, .to = %d, ",
                      kind, tr->from, tr->to);
            sb_printf(out, ".value = %d, .mask = ", tr->value);
            SB_APPEND(out, "{");
            sb_printf(out, "%u, %u, %u, %u, ",
                      mask[0], mask[1], mask[2], mask[3]);
            sb_printf(out, "%u, %u, %u, %u",
                      mask[4], mask[5], mask[6], mask[7]);
            SB_APPEND(out, "}, ");
            sb_printf(out, ".priority = %d, .tag = %d ",
                      tr->priority, tr->tag);
            SB_APPEND(out, "},\n");
            META_TNFA_TRANS_str_free(kind);
        }
        SB_APPEND(out, "}");
    } else {
        SB_APPEND(out, ", .transitions = NULL");
    }

    SB_APPEND(out, " }");
    return;
}

static void
emit_tdfa(ExtractedRegex *regex, StrBuilder *out) {
    ParsedTdfa *tdfa = regex->tdfa;

    if (!preproc_config.emit_tdfa || tdfa == NULL) {
        SB_APPEND(out, ", .tdfa = NULL");
        return;
    }

    SB_APPEND(out, ", .tdfa = &(MetaTdfa){ ");
    sb_printf(out, ".num_tags = %d, .num_states = %d, ",
                   tdfa->num_tags, tdfa->num_states);
    sb_printf(out, ".num_transitions = %d, .num_registers = %d, ",
                   tdfa->num_transitions, tdfa->num_registers);
    sb_printf(out, ".num_ops = %d, .start_state = %d, ",
                   tdfa->num_ops, tdfa->start_state);
    sb_printf(out, ".start_state_nw_nw = %d, .start_state_nw_w = %d, ",
                   tdfa->start_state_nw_nw, tdfa->start_state_nw_w);
    sb_printf(out, ".start_state_w_nw = %d, .start_state_w_w = %d, ",
                   tdfa->start_state_w_nw, tdfa->start_state_w_w);
    sb_printf(out, ".final_register_base = %d, .uses_context = %d, ",
                   tdfa->final_register_base, tdfa->uses_context);
    sb_printf(out, ".transition_index_stride = %d",
                   tdfa->transition_index_stride);

    if (tdfa->num_tags > 0) {
        SB_APPEND(out, ", .tags = (MetaTnfaTag[]){\n");
        for (int32 i = 0; i < tdfa->num_tags; i += 1) {
            MetaTnfaTag *tag = &tdfa->tags[i];
            char *role = META_TNFA_TAG_str(tag->role);

            sb_printf(out, "{ .id = %d, .group = %d, .role = %s, ",
                           tag->id, tag->group, role);
            sb_printf(out, ".is_multivalued = %d, .fixed_base_tag = %d, ",
                           tag->is_multivalued, tag->fixed_base_tag);
            sb_printf(out, ".fixed_offset = %d },\n", tag->fixed_offset);

            META_TNFA_TAG_str_free(role);
        }
        SB_APPEND(out, "}");
    } else {
        SB_APPEND(out, ", .tags = NULL");
    }

    if (tdfa->num_states > 0) {
        SB_APPEND(out, ", .states = (MetaTdfaState[]){\n");
        for (int32 i = 0; i < tdfa->num_states; i += 1) {
            MetaTdfaState *state = &tdfa->states[i];
            SB_APPEND(out, "{ ");
            sb_printf(out, ".is_accepting = %d, .first_transition = %d, ",
                      state->is_accepting, state->first_transition);
            sb_printf(out, ".transition_count = %d, .first_final_op = %d, ",
                      state->transition_count, state->first_final_op);
            sb_printf(out, ".final_op_count = %d ", state->final_op_count);
            SB_APPEND(out, "},\n");
        }
        SB_APPEND(out, "}");
    } else {
        SB_APPEND(out, ", .states = NULL");
    }

    if (tdfa->num_transitions > 0) {
        SB_APPEND(out, ", .transitions = (MetaTdfaTransition[]){\n");
        for (int32 i = 0; i < tdfa->num_transitions; i += 1) {
            MetaTdfaTransition *tr = &tdfa->transitions[i];
            SB_APPEND(out, "{ ");
            sb_printf(out, ".from = %d, .to = %d, .symbol = %d, ",
                      tr->from, tr->to, tr->symbol);
            sb_printf(out, ".next_is_word = %d, .first_op = %d, ",
                      tr->next_is_word, tr->first_op);
            sb_printf(out, ".op_count = %d ", tr->op_count);
            SB_APPEND(out, "},\n");
        }
        SB_APPEND(out, "}");
    } else {
        SB_APPEND(out, ", .transitions = NULL");
    }

    if (tdfa->transition_index_count > 0) {
        SB_APPEND(out, ", .transition_index = (int32[]){\n");
        for (int32 i = 0; i < tdfa->transition_index_count; i += 1) {
            sb_printf(out, "%d,", tdfa->transition_index[i]);
            if ((i + 1) % 16 == 0) {
                SB_APPEND(out, "\n");
            }
        }
        SB_APPEND(out, "}");
    } else {
        SB_APPEND(out, ", .transition_index = NULL");
    }

    if (tdfa->num_ops > 0) {
        SB_APPEND(out, ", .ops = (MetaTdfaRegOp[]){\n");
        for (int32 i = 0; i < tdfa->num_ops; i += 1) {
            MetaTdfaRegOp *op = &tdfa->ops[i];
            char *kind = META_TDFA_REGOP_str(op->kind);

            sb_printf(out, "{ .kind = %s, .dst = %d, .src = %d },\n",
                           kind, op->dst, op->src);
            META_TDFA_REGOP_str_free(kind);
        }
        SB_APPEND(out, "}");
    } else {
        SB_APPEND(out, ", .ops = NULL");
    }

    SB_APPEND(out, " }");
    return;
}

static void
static_dfa_try_generate(ExtractedRegex *regex, char *source, StrBuilder *out) {
    ParsedOp *temp_ops = regex->temp_ops;
    int32 temp_ops_count = regex->temp_ops_count;
    int32 original_string_length = regex->original_string_length;
    char *quote_start = source + regex->quote_start_offset;

    enum PreprocFailReason fail_reasons = 0;
    static int32 dfa_transitions[META_MAX_STATIC_DFA_STATES]
                                [META_ALPHABET_SIZE];
    static uint8 dfa_accept[META_MAX_STATIC_DFA_STATES][META_ALPHABET_SIZE];
    static DfaSet dfa_sets[META_MAX_STATIC_DFA_STATES];

    DfaSet start_set_base = {0};
    int32 dfa_count = 1;
    int32 start_dfa_w = 0;
    int32 start_dfa_nw = 0;
    int32 uses_word_context = static_dfa_uses_word_context(regex);
    int32 min_start_state_count = 2;

    if (uses_word_context) {
        min_start_state_count = 3;
    }
    if (!preproc_config.emit_static_dfa
        || preproc_config.max_static_dfa_states < min_start_state_count) {
        SB_APPEND(out, ", .static_dfa = NULL");
        return;
    }

    for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
        dfa_transitions[0][c] = 0;
        dfa_accept[0][c] = 0;
    }

    for (int32 i = 0; i < PREPROC_NFA_BITSET_WORDS; i += 1) {
        start_set_base.bits[i] = 0;
    }
    start_set_base.bits[0] = 1;

    {
        int32 depth = 0;
        for (int32 i = 0; i < temp_ops_count; i += 1) {
            if (temp_ops[i].type == META_OP_GROUP_START) {
                depth += 1;
            } else if (temp_ops[i].type == META_OP_GROUP_END) {
                depth -= 1;
            } else if (temp_ops[i].type == META_OP_ALTERNATION && depth == 0) {
                int32 t1 = i + 1;
                start_set_base.bits[t1 / 32] |= (1u << (t1 % 32));
            }
        }
    }

    start_set_base.prev_is_w = 0;
    dfa_sets[1] = start_set_base;
    start_dfa_nw = 1;

    if (uses_word_context) {
        DfaSet start_set_w = start_set_base;
        start_set_w.prev_is_w = 1;

        dfa_sets[2] = start_set_w;
        start_dfa_w = 2;
        dfa_count = 3;
    } else {
        start_dfa_w = start_dfa_nw;
        dfa_count = 2;
    }

    for (int32 d = 1; d < dfa_count; d += 1) {
        if (fail_reasons != 0) {
            break;
        }

        for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
            int32 curr_is_w = 0;
            int32 is_acc = 0;
            DfaSet accept_closure = dfa_sets[d];
            DfaSet next_kernel = {0};
            bool has_next = false;
            int32 match_id = -1;

            if (uses_word_context) {
                curr_is_w = is_word_char(c);
            }
            compute_epsilon_closure(&accept_closure, temp_ops, temp_ops_count,
                                    dfa_sets[d].prev_is_w, curr_is_w, &is_acc);

            dfa_accept[d][c] = (uint8)is_acc;

            next_kernel.prev_is_w = 0;
            if (uses_word_context) {
                next_kernel.prev_is_w = curr_is_w;
            }
            for (int32 i = 0; i < PREPROC_NFA_BITSET_WORDS; i += 1) {
                next_kernel.bits[i] = 0;
            }

            compute_core_transitions(&accept_closure, temp_ops, temp_ops_count,
                                     c, &next_kernel);

            for (int32 i = 0; i < PREPROC_NFA_BITSET_WORDS; i += 1) {
                if (next_kernel.bits[i] != 0) {
                    has_next = true;
                    break;
                }
            }

            if (has_next) {
                for (int32 i = 1; i < dfa_count; i += 1) {
                    bool bits_match;
                    if (dfa_sets[i].prev_is_w != next_kernel.prev_is_w) {
                        continue;
                    }

                    bits_match = true;
                    for (int32 k = 0; k < PREPROC_NFA_BITSET_WORDS; k += 1) {
                        if (dfa_sets[i].bits[k] != next_kernel.bits[k]) {
                            bits_match = false;
                            break;
                        }
                    }
                    if (bits_match) {
                        match_id = i;
                        break;
                    }
                }

                if (match_id != -1) {
                    dfa_transitions[d][c] = match_id;
                } else if (dfa_count < preproc_config.max_static_dfa_states) {
                    dfa_sets[dfa_count] = next_kernel;
                    for (int32 k = 0; k < META_ALPHABET_SIZE; k += 1) {
                        dfa_transitions[dfa_count][k] = 0;
                    }
                    dfa_transitions[d][c] = dfa_count;
                    dfa_count += 1;
                } else {
                    fail_reasons |= PREPROC_FAIL_DFA_STATES_EXCEEDED;
                    break;
                }
            } else {
                dfa_transitions[d][c] = 0;
            }
        }
    }

    error2("regex " BLUE("%.*s") " created %d states.\n",
           original_string_length, quote_start, dfa_count);

    if (fail_reasons) {
        char *fail_reason_names = PREPROC_FAIL_str(fail_reasons);

        error2("Warning: DFA conversion failed for %.*s",
               original_string_length, quote_start);
        error2(" because of %s.\n", fail_reason_names);
        PREPROC_FAIL_str_free(fail_reason_names);
        error2("static dfa will not be available at runtime.\n");
        SB_APPEND(out, ", .static_dfa = NULL");
    } else {
        SB_APPEND(out, ", .static_dfa = (StaticDfa *)&(struct {\n");
        SB_APPEND(out, "int32 num_states; int32 start_state_w; ");
        sb_printf(out, "int32 start_state_nw; StaticDfaState states[%d]; ",
                       dfa_count);
        sb_printf(out, "}){ .num_states = %d, .start_state_w = %d, ",
                       dfa_count, start_dfa_w);
        sb_printf(out, ".start_state_nw = %d, .states = { \n", start_dfa_nw);
        for (int32 i = 0; i < dfa_count; i += 1) {
            bool has_accepts = false;
            bool has_transitions = false;

            SB_APPEND(out, "{ .is_accepting = {");
            for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                if (dfa_accept[i][c] != 0) {
                    sb_printf(out, "[%d]=1,", c);
                    has_accepts = true;
                }
            }
            if (!has_accepts) {
                SB_APPEND(out, "0");
            }
            SB_APPEND(out, "}, .next = {");
            for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                if (dfa_transitions[i][c] != 0) {
                    sb_printf(out, "[%d]=%d,", c, dfa_transitions[i][c]);
                    has_transitions = true;
                }
            }
            if (!has_transitions) {
                SB_APPEND(out, "0");
            }
            SB_APPEND(out, "} },\n");
        }
        SB_APPEND(out, "} }");
    }
    return;
}

static void
generate_source_code(char *source, int64 source_len, RegexList *list,
                     FILE *out_file) {
    StrBuilder out = {0};
    int64 current_offset = 0;

    SB_APPEND(&out, "#if defined(__clang__) || defined(__GNUC__)\n");
    SB_APPEND(&out, "#pragma GCC diagnostic push\n");
    SB_APPEND(&out, "#pragma GCC diagnostic ignored "
                    "\"-Wmissing-field-initializers\"\n");
    SB_APPEND(&out, "#endif\n");

    for (int32 i = 0; i < list->count; i += 1) {
        ExtractedRegex *regex = &list->items[i];
        char *quote_start = source + regex->quote_start_offset;
        enum MetaRegexFlags submatch_flag = META_RE_NOSUB;
        int32 re_nsub = 0;

        // Print everything leading up to this macro natively
        int64 prefix_len = regex->source_start_offset - current_offset;
        SB_APPEND(&out, source + current_offset, prefix_len);

        if (regex->is_null_macro) {
            SB_APPEND(&out, "NULL");
            current_offset = regex->source_end_offset;
            continue;
        }

        if (regex->extract_submatches) {
            submatch_flag = META_RE_YESSUB;
            re_nsub = regex->group_counter;
        }

        // Emulate original printing structure
        sb_printf(&out, "&(MetaRegex){ .string = %.*s, ",
                        regex->original_string_length, quote_start);
        sb_printf(&out, ".ops = { %.*s }, ",
                        regex->op_buffer_len, regex->op_buffer);

        sb_printf(&out, ".has_start_anchor = %d, ", regex->has_start);
        sb_printf(&out, ".has_end_anchor = %d, ", regex->has_end);
        sb_printf(&out, ".re_nsub = %d, ", re_nsub);
        sb_printf(&out, ".can_be_null = %d, ", regex->can_be_null);
        sb_printf(&out, ".min_match_len = %d, ", regex->min_match_len);

        SB_APPEND(&out, ".flags = (enum MetaRegexFlags)((");
        if (regex->flags_buffer_len > 0) {
            SB_APPEND(&out, regex->flags_buffer, regex->flags_buffer_len);
        } else {
            SB_APPEND(&out, "0");
        }
        {
            char *submatch_flag_str = META_RE_str(submatch_flag);
            char *used_ops = META_OP_str((enum MetaOpType)regex->used_ops);

            sb_printf(&out, ") | %s), ", submatch_flag_str);
            sb_printf(&out, ".used_ops = (enum MetaOpType)(%s), ", used_ops);
            META_RE_str_free(submatch_flag_str);
            META_OP_str_free(used_ops);
        }
        SB_APPEND(&out, ".fastmap = {");

        for (int32 j = 0; j < META_FASTMAP_SIZE; j += 1) {
            char *sep = ", ";

            if (j == META_FASTMAP_SIZE - 1) {
                sep = "";
            }
            sb_printf(&out, "0x%02x%s", regex->fastmap[j], sep);
        }
        SB_APPEND(&out, "}");
        emit_tnfa(regex, &out);
        emit_tdfa(regex, &out);

        if (!preproc_config.emit_static_dfa) {
            SB_APPEND(&out, ", .static_dfa = NULL");
        } else if (regex->used_ops & META_OP_BACKREF) {
            error2("Warning: Regex " BLUE("%.*s") " has backreferences.\n",
                   regex->original_string_length, quote_start);
            error2("static dfa will not be available at runtime.\n");
            SB_APPEND(&out, ", .static_dfa = NULL");
        } else {
            static_dfa_try_generate(regex, source, &out);
        }
        SB_APPEND(&out, "}");

        // Move trailing cursor
        current_offset = regex->source_end_offset;
    }

    // Output any remaining trailing code from the original file
    if (current_offset < source_len) {
        SB_APPEND(&out, source + current_offset, source_len - current_offset);
    }

    SB_APPEND(&out, "\n#if defined(__clang__) || defined(__GNUC__)\n");
    SB_APPEND(&out, "#pragma GCC diagnostic pop\n");
    SB_APPEND(&out, "#endif\n");

    if (out.len > 0 && fwrite64(out.data, 1, out.len, out_file) != out.len) {
        error("Error writing generated source.\n");
        exit(EXIT_FAILURE);
    }
    sb_free(&out);
}
