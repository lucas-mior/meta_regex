#include "meta_preproc.h"

static int32
is_word_char(int32 c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '_');
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
                        next_core->bits[i / 32] |= (1u << (i % 32));
                        int32 t2 = i + 2;
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

static int32
get_branch_weight(ParsedOp *ops, int32 count) {
    int32 weight = 0;

    for (int32 i = 0; i < count; i += 1) {
        if (ops[i].type == META_OP_LITERAL || ops[i].type == META_OP_CLASS
            || ops[i].type == META_OP_ANY || ops[i].type == META_OP_BACKREF) {
            weight += 1;
        }
    }
    return weight;
}

static void
emit_tnfa(ExtractedRegex *regex, FILE *out) {
    ParsedTnfa *tnfa = regex->tnfa;

    if (tnfa == NULL) {
        fprintf(out, ", .tnfa = NULL");
        return;
    }

    fprintf(out,
            ", .tnfa = &(MetaTnfa){ .num_tags = %d, .num_states = %d, "
            ".num_transitions = %d, .start_state = %d, .final_state = %d",
            tnfa->num_tags, tnfa->num_states, tnfa->num_transitions,
            tnfa->start_state, tnfa->final_state);

    if (tnfa->num_tags > 0) {
        fprintf(out, ", .tags = (MetaTnfaTag[]){\n");
        for (int32 i = 0; i < tnfa->num_tags; i += 1) {
            MetaTnfaTag *tag = &tnfa->tags[i];
            char *role = "META_TNFA_TAG_GENERIC";
            if (tag->role == META_TNFA_TAG_GROUP_START) {
                role = "META_TNFA_TAG_GROUP_START";
            } else if (tag->role == META_TNFA_TAG_GROUP_END) {
                role = "META_TNFA_TAG_GROUP_END";
            } else if (tag->role == META_TNFA_TAG_POSIX_AUX) {
                role = "META_TNFA_TAG_POSIX_AUX";
            }
            fprintf(out,
                    "{ .id = %d, .group = %d, .role = %s, "
                    ".is_multivalued = %d, .fixed_base_tag = %d, "
                    ".fixed_offset = %d },\n",
                    tag->id, tag->group, role, tag->is_multivalued,
                    tag->fixed_base_tag, tag->fixed_offset);
        }
        fprintf(out, "}");
    } else {
        fprintf(out, ", .tags = NULL");
    }

    if (tnfa->num_states > 0) {
        fprintf(out, ", .states = (MetaTnfaState[]){\n");
        for (int32 i = 0; i < tnfa->num_states; i += 1) {
            fprintf(out,
                    "{ .first_transition = %d, .transition_count = %d },\n",
                    tnfa->states[i].first_transition,
                    tnfa->states[i].transition_count);
        }
        fprintf(out, "}");
    } else {
        fprintf(out, ", .states = NULL");
    }

    if (tnfa->num_transitions > 0) {
        fprintf(out, ", .transitions = (MetaTnfaTransition[]){\n");
        for (int32 i = 0; i < tnfa->num_transitions; i += 1) {
            MetaTnfaTransition *tr = &tnfa->transitions[i];
            char *kind = "META_TNFA_TRANS_EPSILON";
            if (tr->kind == META_TNFA_TRANS_LITERAL) {
                kind = "META_TNFA_TRANS_LITERAL";
            } else if (tr->kind == META_TNFA_TRANS_CLASS) {
                kind = "META_TNFA_TRANS_CLASS";
            } else if (tr->kind == META_TNFA_TRANS_ANY) {
                kind = "META_TNFA_TRANS_ANY";
            } else if (tr->kind == META_TNFA_TRANS_WORD_START) {
                kind = "META_TNFA_TRANS_WORD_START";
            } else if (tr->kind == META_TNFA_TRANS_WORD_END) {
                kind = "META_TNFA_TRANS_WORD_END";
            } else if (tr->kind == META_TNFA_TRANS_WORD_BOUNDARY) {
                kind = "META_TNFA_TRANS_WORD_BOUNDARY";
            } else if (tr->kind == META_TNFA_TRANS_NON_WORD_BOUNDARY) {
                kind = "META_TNFA_TRANS_NON_WORD_BOUNDARY";
            }

            fprintf(out,
                    "{ .kind = %s, .from = %d, .to = %d, .value = %d, "
                    ".mask = {%u, %u, %u, %u, %u, %u, %u, %u}, "
                    ".priority = %d, .tag = %d },\n",
                    kind, tr->from, tr->to, tr->value, tr->mask[0], tr->mask[1],
                    tr->mask[2], tr->mask[3], tr->mask[4], tr->mask[5],
                    tr->mask[6], tr->mask[7], tr->priority, tr->tag);
        }
        fprintf(out, "}");
    } else {
        fprintf(out, ", .transitions = NULL");
    }

    fprintf(out, " }");
    return;
}

static void
emit_tdfa(ExtractedRegex *regex, FILE *out) {
    ParsedTdfa *tdfa = regex->tdfa;

    if (tdfa == NULL) {
        fprintf(out, ", .tdfa = NULL");
        return;
    }

    fprintf(out,
            ", .tdfa = &(MetaTdfa){ .num_tags = %d, .num_states = %d, "
            ".num_transitions = %d, .num_registers = %d, .num_ops = %d, "
            ".start_state = %d, .start_state_nw_nw = %d, "
            ".start_state_nw_w = %d, .start_state_w_nw = %d, "
            ".start_state_w_w = %d, .final_register_base = %d, "
            ".uses_context = %d, .transition_index_stride = %d",
            tdfa->num_tags, tdfa->num_states, tdfa->num_transitions,
            tdfa->num_registers, tdfa->num_ops, tdfa->start_state,
            tdfa->start_state_nw_nw, tdfa->start_state_nw_w,
            tdfa->start_state_w_nw, tdfa->start_state_w_w,
            tdfa->final_register_base, tdfa->uses_context,
            tdfa->transition_index_stride);

    if (tdfa->num_tags > 0) {
        fprintf(out, ", .tags = (MetaTnfaTag[]){\n");
        for (int32 i = 0; i < tdfa->num_tags; i += 1) {
            MetaTnfaTag *tag = &tdfa->tags[i];
            char *role = "META_TNFA_TAG_GENERIC";
            if (tag->role == META_TNFA_TAG_GROUP_START) {
                role = "META_TNFA_TAG_GROUP_START";
            } else if (tag->role == META_TNFA_TAG_GROUP_END) {
                role = "META_TNFA_TAG_GROUP_END";
            } else if (tag->role == META_TNFA_TAG_POSIX_AUX) {
                role = "META_TNFA_TAG_POSIX_AUX";
            }
            fprintf(out,
                    "{ .id = %d, .group = %d, .role = %s, "
                    ".is_multivalued = %d, .fixed_base_tag = %d, "
                    ".fixed_offset = %d },\n",
                    tag->id, tag->group, role, tag->is_multivalued,
                    tag->fixed_base_tag, tag->fixed_offset);
        }
        fprintf(out, "}");
    } else {
        fprintf(out, ", .tags = NULL");
    }

    if (tdfa->num_states > 0) {
        fprintf(out, ", .states = (MetaTdfaState[]){\n");
        for (int32 i = 0; i < tdfa->num_states; i += 1) {
            MetaTdfaState *state = &tdfa->states[i];
            fprintf(out,
                    "{ .is_accepting = %d, .first_transition = %d, "
                    ".transition_count = %d, .first_final_op = %d, "
                    ".final_op_count = %d },\n",
                    state->is_accepting, state->first_transition,
                    state->transition_count, state->first_final_op,
                    state->final_op_count);
        }
        fprintf(out, "}");
    } else {
        fprintf(out, ", .states = NULL");
    }

    if (tdfa->num_transitions > 0) {
        fprintf(out, ", .transitions = (MetaTdfaTransition[]){\n");
        for (int32 i = 0; i < tdfa->num_transitions; i += 1) {
            MetaTdfaTransition *tr = &tdfa->transitions[i];
            fprintf(out,
                    "{ .from = %d, .to = %d, .symbol = %d, "
                    ".next_is_word = %d, .first_op = %d, "
                    ".op_count = %d },\n",
                    tr->from, tr->to, tr->symbol, tr->next_is_word,
                    tr->first_op, tr->op_count);
        }
        fprintf(out, "}");
    } else {
        fprintf(out, ", .transitions = NULL");
    }

    if (tdfa->transition_index_count > 0) {
        fprintf(out, ", .transition_index = (int32[]){\n");
        for (int32 i = 0; i < tdfa->transition_index_count; i += 1) {
            fprintf(out, "%d,", tdfa->transition_index[i]);
            if ((i + 1) % 16 == 0) {
                fprintf(out, "\n");
            }
        }
        fprintf(out, "}");
    } else {
        fprintf(out, ", .transition_index = NULL");
    }

    if (tdfa->num_ops > 0) {
        fprintf(out, ", .ops = (MetaTdfaRegOp[]){\n");
        for (int32 i = 0; i < tdfa->num_ops; i += 1) {
            MetaTdfaRegOp *op = &tdfa->ops[i];
            char *kind = "META_TDFA_REGOP_COPY";
            if (op->kind == META_TDFA_REGOP_SET_NIL) {
                kind = "META_TDFA_REGOP_SET_NIL";
            } else if (op->kind == META_TDFA_REGOP_SET_POS) {
                kind = "META_TDFA_REGOP_SET_POS";
            }
            fprintf(out, "{ .kind = %s, .dst = %d, .src = %d },\n", kind,
                    op->dst, op->src);
        }
        fprintf(out, "}");
    } else {
        fprintf(out, ", .ops = NULL");
    }

    fprintf(out, " }");
    return;
}

static void
static_dfa_try_generate(ExtractedRegex *regex, char *source, FILE *out) {
    ParsedOp *temp_ops = regex->temp_ops;
    int32 temp_ops_count = regex->temp_ops_count;
    int32 original_string_length = regex->original_string_length;
    char *quote_start = source + regex->quote_start_offset;

    enum PreprocFailReason fail_reasons = 0;
    static int32 dfa_transitions[META_MAX_STATIC_DFA_STATES]
                                [META_ALPHABET_SIZE];
    static uint8 dfa_accept[META_MAX_STATIC_DFA_STATES][META_ALPHABET_SIZE];
    static DfaSet dfa_sets[META_MAX_STATIC_DFA_STATES];

    int32 dfa_count = 1;
    int32 start_dfa_w = 0;
    int32 start_dfa_nw = 0;

    for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
        dfa_transitions[0][c] = 0;
        dfa_accept[0][c] = 0;
    }

    DfaSet start_set_base = {0};
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

    DfaSet start_set_nw = start_set_base;
    start_set_nw.prev_is_w = 0;

    DfaSet start_set_w = start_set_base;
    start_set_w.prev_is_w = 1;

    dfa_sets[1] = start_set_nw;
    start_dfa_nw = 1;
    dfa_sets[2] = start_set_w;
    start_dfa_w = 2;
    dfa_count = 3;

    for (int32 d = 1; d < dfa_count; d += 1) {
        if (fail_reasons != 0) {
            break;
        }

        for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
            int32 curr_is_w = is_word_char(c);
            int32 is_acc = 0;
            DfaSet accept_closure = dfa_sets[d];
            DfaSet next_kernel = {0};
            bool has_next = false;
            int32 match_id = -1;

            compute_epsilon_closure(&accept_closure, temp_ops, temp_ops_count,
                                    dfa_sets[d].prev_is_w, curr_is_w, &is_acc);

            dfa_accept[d][c] = (uint8)is_acc;

            next_kernel.prev_is_w = curr_is_w;
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
                    if (dfa_sets[i].prev_is_w != next_kernel.prev_is_w) {
                        continue;
                    }
                    bool bits_match = true;
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
                } else if (dfa_count < META_MAX_STATIC_DFA_STATES) {
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
        fprintf(stderr,
                "Warning: DFA conversion failed for %.*s because of %s.\n",
                original_string_length, quote_start,
                PREPROC_FAIL_str(fail_reasons));
        fprintf(stderr, "static dfa will not be available at runtime.\n");
        fprintf(out, ", .static_dfa = NULL");
    } else {
        fprintf(out,
                ", .static_dfa = &(StaticDfa){ .num_states = %d, "
                ".start_state_w = %d, .start_state_nw = %d, "
                ".states = (StaticDfaState[]){ \n",
                dfa_count, start_dfa_w, start_dfa_nw);
        for (int32 i = 0; i < dfa_count; i += 1) {
            bool has_accepts = false;
            bool has_transitions = false;

            fprintf(out, "{ .is_accepting = {");
            for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                if (dfa_accept[i][c] != 0) {
                    fprintf(out, "[%d]=1,", c);
                    has_accepts = true;
                }
            }
            if (!has_accepts) {
                fprintf(out, "0");
            }
            fprintf(out, "}, .next = {");
            for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                if (dfa_transitions[i][c] != 0) {
                    fprintf(out, "[%d]=%d,", c, dfa_transitions[i][c]);
                    has_transitions = true;
                }
            }
            if (!has_transitions) {
                fprintf(out, "0");
            }
            fprintf(out, "} },\n");
        }
        fprintf(out, "} }");
    }
    return;
}

void
generate_source_code(char *source, int64 source_len, RegexList *list,
                     FILE *out) {
    int64 current_offset = 0;

    for (int32 i = 0; i < list->count; i += 1) {
        ExtractedRegex *regex = &list->items[i];

        // Print everything leading up to this macro natively
        int64 prefix_len = regex->source_start_offset - current_offset;
        fprintf(out, "%.*s", (int32)prefix_len, source + current_offset);

        if (regex->is_null_macro) {
            fprintf(out, "NULL");
            current_offset = regex->source_end_offset;
            continue;
        }

        // Emulate original printing structure
        fprintf(out, "&(MetaRegex){ .string = %.*s, ",
                regex->original_string_length,
                source + regex->quote_start_offset);
        fprintf(out, ".ops = { %s }, ", regex->op_buffer);
        fprintf(out, ".has_start_anchor = %d, ", regex->has_start);
        fprintf(out, ".has_end_anchor = %d, ", regex->has_end);
        fprintf(out, ".re_nsub = %d, ", regex->group_counter);
        fprintf(out, ".can_be_null = %d, ", regex->can_be_null);
        fprintf(out, ".used_ops = (enum MetaOpType)%u, ", regex->used_ops);
        fprintf(out, ".fastmap = {");

        for (int32 j = 0; j < META_FASTMAP_SIZE; j += 1) {
            fprintf(out, "0x%02x%s", regex->fastmap[j],
                    (j == META_FASTMAP_SIZE - 1 ? "" : ", "));
        }
        fprintf(out, "}");
        emit_tnfa(regex, out);
        emit_tdfa(regex, out);

        if (regex->used_ops & META_OP_BACKREF) {
            fprintf(
                stderr,
                "Warning: Regex " BLUE(
                    "%.*s") " has backreferences.\n"
                            "static dfa will not be available at runtime.\n",
                regex->original_string_length,
                source + regex->quote_start_offset);
            fprintf(out, ", .static_dfa = NULL");
        } else {
            static_dfa_try_generate(regex, source, out);
        }
        fprintf(out, "}");

        // Move trailing cursor
        current_offset = regex->source_end_offset;
    }

    // Output any remaining trailing code from the original file
    if (current_offset < source_len) {
        fprintf(out, "%.*s", (int32)(source_len - current_offset),
                source + current_offset);
    }
}
