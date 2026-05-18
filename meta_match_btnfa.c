#if !defined(META_MATCH_BTNFA)
#define META_MATCH_BTNFA

typedef struct BtnfaState {
    MetaOp *pc;
    uint8 *input;
    regmatch_t pmatch[32];
    uint32 visited_empty[META_PC_WORDS];
} BtnfaState;

static int32
btnfa_quick_lookahead_fails(MetaOp *next_op, uint8 *curr_str) {
    if (next_op->type == META_OP_LITERAL || next_op->type == META_OP_CLASS) {
        if (next_op[1].type == META_OP_STAR
            || next_op[1].type == META_OP_OPTIONAL
            || (next_op[1].type == META_OP_BOUNDED && next_op[1].min == 0)) {
            return 0;
        }

        if (next_op->type == META_OP_LITERAL) {
            if (*curr_str == '\0') {
                return 1;
            }
            if ((int32)*curr_str != next_op->value) {
                return 1;
            }
        } else if (next_op->type == META_OP_CLASS) {
            uint8 fb = *curr_str;

            if (fb == '\0') {
                return 1;
            }
            if ((next_op->mask[fb / 32] & (1u << (fb % 32))) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int32
try_match_btnfa(MetaRegex *regex, uint8 *string, int32 string_len, int32 offset,
                int64 nmatch, regmatch_t pmatch[]) {
    uint8 *search_ptr = &string[offset];
    int32 match_len = -1;
    static int32 stack_cap = 8192;
    static BtnfaState *stack = NULL;
    int32 stack_ptr = 0;
    int64 copy_size = (nmatch > 32) ? 32 : nmatch;
    regmatch_t best_pmatch[32];
    uint32 *memo = NULL;
    int32 memo_size = 0;
    int32 step_count = 0;
    int32 is_catastrophic = 0;

    if (stack == NULL) {
        stack = realloc2(NULL, 0, stack_cap, SIZEOF(*stack));
    }

    if (regex->has_alternation) {
        MetaOp *alts[128];
        int32 num_alts = 0;
        alts[num_alts] = regex->ops;
        num_alts += 1;

        int32 depth = 0;
        MetaOp *scan = regex->ops;
        while (scan->type != META_OP_END) {
            if (scan->type == META_OP_GROUP_START) {
                depth += 1;
            } else if (scan->type == META_OP_GROUP_END) {
                if (depth == 0) {
                    break;
                }
                depth -= 1;
            } else if (scan->type == META_OP_ALTERNATION && depth == 0) {
                alts[num_alts] = scan + 1;
                num_alts += 1;
            }
            scan += 1;
        }

        for (int32 i = num_alts - 1; i >= 0; i -= 1) {
            if (!btnfa_quick_lookahead_fails(alts[i], search_ptr)) {
                stack[stack_ptr].pc = alts[i];
                stack[stack_ptr].input = search_ptr;
                if (pmatch != NULL) {
                    for (int64 k = 0; k < copy_size; k += 1) {
                        stack[stack_ptr].pmatch[k] = pmatch[k];
                    }
                }
                stack_ptr += 1;
            }
        }
    } else {
        if (!btnfa_quick_lookahead_fails(regex->ops, search_ptr)) {
            stack[stack_ptr].pc = regex->ops;
            stack[stack_ptr].input = search_ptr;
            if (pmatch != NULL) {
                for (int64 k = 0; k < copy_size; k += 1) {
                    stack[stack_ptr].pmatch[k] = pmatch[k];
                }
            }
            stack_ptr += 1;
        }
    }

    while (stack_ptr > 0) {
        MetaOp *pc;
        uint8 *input;
        regmatch_t current_pmatch[32];
        uint32 visited_empty[META_PC_WORDS];
        int32 pmatch_copy_valid;

        step_count += 1;
        if (step_count > 4096 && !is_catastrophic) {
            is_catastrophic = 1;
            if (!regex->has_backref) {
                memo_size = (string_len + 1)*META_PC_WORDS;
                memo = malloc2(memo_size*SIZEOF(*memo));
                for (int32 i = 0; i < memo_size; i += 1) {
                    memo[i] = 0;
                }
            }
            for (int32 i = 0; i < stack_ptr; i += 1) {
                for (int32 w = 0; w < META_PC_WORDS; w += 1) {
                    stack[i].visited_empty[w] = 0;
                }
            }
        }

        stack_ptr -= 1;
        pc = stack[stack_ptr].pc;
        input = stack[stack_ptr].input;
        pmatch_copy_valid = (pmatch != NULL);

        if (is_catastrophic) {
            for (int32 w = 0; w < META_PC_WORDS; w += 1) {
                visited_empty[w] = stack[stack_ptr].visited_empty[w];
            }
        }

        if (pmatch_copy_valid) {
            for (int64 k = 0; k < copy_size; k += 1) {
                current_pmatch[k] = stack[stack_ptr].pmatch[k];
            }
        }

        while (1) {
            if (is_catastrophic) {
                int32 pc_idx = (int32)(pc - regex->ops);
                if ((visited_empty[pc_idx / 32] & (1u << (pc_idx % 32))) != 0) {
                    break;
                }
                visited_empty[pc_idx / 32] |= (1u << (pc_idx % 32));

                if (memo != NULL) {
                    int32 in_idx = (int32)(input - string);
                    if (in_idx >= 0 && in_idx <= string_len) {
                        int32 word_idx = in_idx*META_PC_WORDS + (pc_idx / 32);
                        int32 bit_idx = pc_idx % 32;

                        if ((memo[word_idx] & (1u << bit_idx)) != 0) {
                            break;
                        }
                        memo[word_idx] |= (1u << bit_idx);
                    }
                }
            }

            if (pc->type == META_OP_END) {
                int32 curr_match_len = (int32)(input - string);
                if (curr_match_len > match_len) {
                    match_len = curr_match_len;
                    if (pmatch_copy_valid) {
                        for (int64 k = 0; k < copy_size; k += 1) {
                            best_pmatch[k] = current_pmatch[k];
                        }
                    }
                }
                break;
            }

            if (pc->type == META_OP_WORD_START) {
                int32 curr_is_word = is_word_char(*input);
                int32 prev_is_word = 0;
                if (input > string) {
                    prev_is_word = is_word_char(*(input - 1));
                }
                if (curr_is_word && !prev_is_word) {
                    pc += 1;
                    continue;
                }
                break;
            }

            if (pc->type == META_OP_WORD_END) {
                int32 curr_is_word = is_word_char(*input);
                int32 prev_is_word = 0;
                if (input > string) {
                    prev_is_word = is_word_char(*(input - 1));
                }
                if (!curr_is_word && prev_is_word) {
                    pc += 1;
                    continue;
                }
                break;
            }

            if (pc->type == META_OP_WORD_BOUNDARY) {
                int32 curr_is_word = is_word_char(*input);
                int32 prev_is_word = 0;
                if (input > string) {
                    prev_is_word = is_word_char(*(input - 1));
                }
                if (curr_is_word != prev_is_word) {
                    pc += 1;
                    continue;
                }
                break;
            }

            if (pc->type == META_OP_NON_WORD_BOUNDARY) {
                int32 curr_is_word = is_word_char(*input);
                int32 prev_is_word = 0;
                if (input > string) {
                    prev_is_word = is_word_char(*(input - 1));
                }
                if (curr_is_word == prev_is_word) {
                    pc += 1;
                    continue;
                }
                break;
            }

            if (pc->type == META_OP_BACKREF) {
                int32 group_id = pc->value;
                int32 backref_len;
                uint8 *backref_ptr;

                if (pmatch_copy_valid && group_id < nmatch
                    && current_pmatch[group_id].rm_so != -1) {
                    backref_len = current_pmatch[group_id].rm_eo
                                  - current_pmatch[group_id].rm_so;
                    backref_ptr = string + current_pmatch[group_id].rm_so;

                    if (strncmp32((char *)input, (char *)backref_ptr,
                                  backref_len)
                        == 0) {
                        input += backref_len;
                        pc += 1;
                        if (is_catastrophic && backref_len > 0) {
                            for (int32 w = 0; w < META_PC_WORDS; w += 1) {
                                visited_empty[w] = 0;
                            }
                        }
                        continue;
                    }
                }
                break;
            }

            if (pc->type == META_OP_ALTERNATION) {
                int32 depth = 0;
                MetaOp *end_op = pc;
                while (end_op->type != META_OP_END) {
                    if (end_op->type == META_OP_GROUP_START) {
                        depth += 1;
                    } else if (end_op->type == META_OP_GROUP_END) {
                        if (depth == 0) {
                            break;
                        }
                        depth -= 1;
                    }
                    end_op += 1;
                }
                pc = end_op;
                continue;
            }

            if (pc->type == META_OP_SPLIT) {
                int32 skip_1
                    = btnfa_quick_lookahead_fails(pc + pc->value, input);
                int32 skip_2 = btnfa_quick_lookahead_fails(pc + pc->min, input);

                if (!skip_2) {
                    if (stack_ptr >= stack_cap) {
                        int32 new_cap = stack_cap*2;
                        stack = realloc2(stack, stack_cap, new_cap,
                                         SIZEOF(*stack));
                        stack_cap = new_cap;
                    }
                    stack[stack_ptr].pc = pc + pc->min;
                    stack[stack_ptr].input = input;
                    if (is_catastrophic) {
                        for (int32 w = 0; w < META_PC_WORDS; w += 1) {
                            stack[stack_ptr].visited_empty[w]
                                = visited_empty[w];
                        }
                    }
                    if (pmatch_copy_valid) {
                        for (int64 k = 0; k < copy_size; k += 1) {
                            stack[stack_ptr].pmatch[k] = current_pmatch[k];
                        }
                    }
                    stack_ptr += 1;
                }

                if (!skip_1) {
                    if (stack_ptr >= stack_cap) {
                        int32 new_cap = stack_cap*2;
                        stack = realloc2(stack, stack_cap, new_cap,
                                         SIZEOF(*stack));
                        stack_cap = new_cap;
                    }
                    stack[stack_ptr].pc = pc + pc->value;
                    stack[stack_ptr].input = input;
                    if (is_catastrophic) {
                        for (int32 w = 0; w < META_PC_WORDS; w += 1) {
                            stack[stack_ptr].visited_empty[w]
                                = visited_empty[w];
                        }
                    }
                    if (pmatch_copy_valid) {
                        for (int64 k = 0; k < copy_size; k += 1) {
                            stack[stack_ptr].pmatch[k] = current_pmatch[k];
                        }
                    }
                    stack_ptr += 1;
                }
                break;
            }

            if (pc->type == META_OP_JUMP) {
                pc += pc->value;
                continue;
            }

            if (pc->type == META_OP_GROUP_START) {
                int32 group_id = pc->value;
                if (pmatch_copy_valid && group_id < nmatch) {
                    current_pmatch[group_id].rm_so = (int32)(input - string);
                }

                int32 has_alt = 0;
                int32 depth = 0;
                MetaOp *scan = pc + 1;
                while (scan->type != META_OP_END) {
                    if (scan->type == META_OP_GROUP_START) {
                        depth += 1;
                    } else if (scan->type == META_OP_GROUP_END) {
                        if (depth == 0) {
                            break;
                        }
                        depth -= 1;
                    } else if (scan->type == META_OP_ALTERNATION
                               && depth == 0) {
                        has_alt = 1;
                        break;
                    }
                    scan += 1;
                }

                if (has_alt) {
                    MetaOp *alts[128];
                    int32 num_alts = 0;
                    alts[num_alts] = pc + 1;
                    num_alts += 1;

                    depth = 0;
                    scan = pc + 1;
                    while (scan->type != META_OP_END) {
                        if (scan->type == META_OP_GROUP_START) {
                            depth += 1;
                        } else if (scan->type == META_OP_GROUP_END) {
                            if (depth == 0) {
                                break;
                            }
                            depth -= 1;
                        } else if (scan->type == META_OP_ALTERNATION
                                   && depth == 0) {
                            alts[num_alts] = scan + 1;
                            num_alts += 1;
                        }
                        scan += 1;
                    }

                    for (int32 i = num_alts - 1; i >= 0; i -= 1) {
                        if (!btnfa_quick_lookahead_fails(alts[i], input)) {
                            if (stack_ptr >= stack_cap) {
                                int32 new_cap = stack_cap*2;
                                stack = realloc2(stack, stack_cap, new_cap,
                                                 SIZEOF(*stack));
                                stack_cap = new_cap;
                            }
                            stack[stack_ptr].pc = alts[i];
                            stack[stack_ptr].input = input;
                            if (is_catastrophic) {
                                for (int32 w = 0; w < META_PC_WORDS; w += 1) {
                                    stack[stack_ptr].visited_empty[w]
                                        = visited_empty[w];
                                }
                            }
                            if (pmatch_copy_valid) {
                                for (int64 k = 0; k < copy_size; k += 1) {
                                    stack[stack_ptr].pmatch[k]
                                        = current_pmatch[k];
                                }
                            }
                            stack_ptr += 1;
                        }
                    }
                    break;
                } else {
                    pc += 1;
                    continue;
                }
            }

            if (pc->type == META_OP_GROUP_END) {
                int32 group_id = pc->value;
                if (pmatch_copy_valid && group_id < nmatch) {
                    current_pmatch[group_id].rm_eo = (int32)(input - string);
                }
                pc += 1;
                continue;
            }

            {
                int32 is_star = (pc[1].type == META_OP_STAR);
                int32 is_plus = (pc[1].type == META_OP_PLUS);
                int32 is_opt = (pc[1].type == META_OP_OPTIONAL);
                int32 is_bound = (pc[1].type == META_OP_BOUNDED);

                if (is_star || is_plus || is_opt || is_bound) {
                    MetaOp token = pc[0];
                    MetaOp *next_ops = pc + 2;
                    uint8 *s = input;
                    int32 min_req = 0;
                    int32 max_req = -1;
                    int32 count = 0;

                    if (is_star) {
                        min_req = 0;
                        max_req = -1;
                    } else if (is_plus) {
                        min_req = 1;
                        max_req = -1;
                    } else if (is_opt) {
                        min_req = 0;
                        max_req = 1;
                    } else if (is_bound) {
                        min_req = pc[1].min;
                        max_req = pc[1].max;
                    }

                    if (token.type == META_OP_ANY) {
                        while (max_req == -1 || count < max_req) {
                            if (s[count] == '\0') {
                                break;
                            }
                            count += 1;
                        }
                    } else if (token.type == META_OP_LITERAL) {
                        while (max_req == -1 || count < max_req) {
                            if (s[count] == '\0') {
                                break;
                            }
                            if ((int32)s[count] != token.value) {
                                break;
                            }
                            count += 1;
                        }
                    } else if (token.type == META_OP_CLASS) {
                        while (max_req == -1 || count < max_req) {
                            uint8 fb = s[count];

                            if (fb == '\0') {
                                break;
                            }
                            if ((token.mask[fb / 32] & (1u << (fb % 32)))
                                == 0) {
                                break;
                            }
                            count += 1;
                        }
                    }

                    if (count >= min_req) {
                        uint8 *min_s = s + min_req;
                        uint8 *max_s_ptr = s + count;

                        for (uint8 *p = min_s; p <= max_s_ptr; p += 1) {
                            if (!btnfa_quick_lookahead_fails(next_ops, p)) {
                                if (stack_ptr >= stack_cap) {
                                    int32 new_cap = stack_cap*2;
                                    stack = realloc2(stack, stack_cap, new_cap,
                                                     SIZEOF(*stack));
                                    stack_cap = new_cap;
                                }
                                stack[stack_ptr].pc = next_ops;
                                stack[stack_ptr].input = p;
                                if (is_catastrophic) {
                                    if (p > input) {
                                        for (int32 w = 0; w < META_PC_WORDS;
                                             w += 1) {
                                            stack[stack_ptr].visited_empty[w]
                                                = 0;
                                        }
                                    } else {
                                        for (int32 w = 0; w < META_PC_WORDS;
                                             w += 1) {
                                            stack[stack_ptr].visited_empty[w]
                                                = visited_empty[w];
                                        }
                                    }
                                }
                                if (pmatch_copy_valid) {
                                    for (int64 k = 0; k < copy_size; k += 1) {
                                        stack[stack_ptr].pmatch[k]
                                            = current_pmatch[k];
                                    }
                                }
                                stack_ptr += 1;
                            }
                        }
                    }
                    break;
                } else {
                    int32 is_match = 0;
                    uint8 fb = input[0];
                    int32 consumed = 0;

                    if (fb == '\0') {
                        is_match = 0;
                    } else {
                        consumed = 1;
                        if (pc->type == META_OP_ANY) {
                            is_match = 1;
                        } else if (pc->type == META_OP_LITERAL) {
                            is_match = ((int32)fb == pc->value);
                        } else if (pc->type == META_OP_CLASS) {
                            is_match = ((pc->mask[fb / 32] & (1u << (fb % 32)))
                                        != 0);
                        } else {
                            is_match = 0;
                        }
                    }
                    if (is_match) {
                        pc += 1;
                        input += consumed;
                        if (is_catastrophic && consumed > 0) {
                            for (int32 w = 0; w < META_PC_WORDS; w += 1) {
                                visited_empty[w] = 0;
                            }
                        }
                        continue;
                    } else {
                        break;
                    }
                }
            }
        }
    }

    if (memo) {
        free2(memo, memo_size*SIZEOF(*memo));
    }

    if (match_len >= 0) {
        if (!regex->has_end_anchor || string[match_len] == '\0') {
            if (pmatch != NULL && nmatch > 0) {
                pmatch[0].rm_so = offset;
                pmatch[0].rm_eo = match_len;
                for (int64 k = 1; k < copy_size; k += 1) {
                    pmatch[k] = best_pmatch[k];
                }
            }
            return 0;
        }
    }

    return -1;
}

#endif /* META_MATCH_BTNFA */
