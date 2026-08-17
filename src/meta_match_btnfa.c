#if !defined(META_MATCH_BTNFA)
#define META_MATCH_BTNFA

#include "cbase.h"

#include <regex.h>
#include "meta_regex.h"
#include "meta_util.c"

// clang-format off
static const MatcherFeatures match_features_btnfa = {
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
        | META_OP_BACKREF
    ),
    .extracts = true,
};
// clang-format on

typedef struct BtnfaState {
    MetaOp *pc;
    uint8 *input;
    MetaRegexMatch pmatch[32];
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
btnfa_stack_push(BtnfaState **stack_ptr_ref, int32 *stack_cap_ref,
                 int32 *stack_ptr_count, MetaOp *pc, uint8 *input,
                 MetaRegexMatch *pmatch, uint32 *visited_empty,
                 int32 is_catastrophic, int32 clear_empty_visited) {
    BtnfaState *stack = *stack_ptr_ref;
    int32 stack_cap = *stack_cap_ref;
    int32 stack_ptr = *stack_ptr_count;

    if (stack_ptr >= stack_cap) {
        int32 new_cap = stack_cap*2;
        stack = realloc2(stack, stack_cap, new_cap, SIZEOF(*stack));
        stack_cap = new_cap;
    }

    stack[stack_ptr].pc = pc;
    stack[stack_ptr].input = input;

    if (is_catastrophic) {
        if (clear_empty_visited) {
            for (int32 w = 0; w < META_PC_WORDS; w += 1) {
                stack[stack_ptr].visited_empty[w] = 0;
            }
        } else {
            for (int32 w = 0; w < META_PC_WORDS; w += 1) {
                stack[stack_ptr].visited_empty[w] = visited_empty[w];
            }
        }
    }

    for (int32 k = 0; k < 32; k += 1) {
        stack[stack_ptr].pmatch[k] = pmatch[k];
    }

    stack_ptr += 1;
    *stack_ptr_ref = stack;
    *stack_cap_ref = stack_cap;
    *stack_ptr_count = stack_ptr;
    return 1;
}

static int32
btnfa_get_backref(MetaRegexMatch *pmatch, int32 group_id, uint8 *string,
                  uint8 **backref_ptr, int32 *backref_len) {
    if (group_id < 0 || group_id >= 32) {
        return 0;
    }
    if (pmatch[group_id].rm_so == -1 || pmatch[group_id].rm_eo == -1) {
        return 0;
    }
    if (pmatch[group_id].rm_eo < pmatch[group_id].rm_so) {
        return 0;
    }

    *backref_len = pmatch[group_id].rm_eo - pmatch[group_id].rm_so;
    *backref_ptr = string + pmatch[group_id].rm_so;
    return 1;
}

static int32
btnfa_backref_matches_at(uint8 *input, uint8 *backref_ptr, int32 backref_len) {
    if (backref_len < 0) {
        return 0;
    }
    if (backref_len == 0) {
        return 1;
    }
    return strncmp32((char *)input, (char *)backref_ptr, backref_len) == 0;
}

static int32
match_btnfa(MetaRegex *regex, uint8 *string, int32 string_len, int32 offset,
            MetaRegexMatch *pmatch, int64 pmatch_len) {
    uint8 *search_ptr = &string[offset];
    int32 match_len = -1;
    static int32 stack_cap = 8192;
    static BtnfaState *stack = NULL;
    int32 stack_ptr = 0;
    MetaRegexMatch init_pmatch[32];
    MetaRegexMatch best_pmatch[32];
    uint32 *memo = NULL;
    int32 memo_size = 0;
    int32 step_count = 0;
    int32 is_catastrophic = 0;

    for (int32 k = 0; k < 32; k += 1) {
        init_pmatch[k].rm_so = -1;
        init_pmatch[k].rm_eo = -1;
        best_pmatch[k].rm_so = -1;
        best_pmatch[k].rm_eo = -1;
    }

    if (pmatch != NULL) {
        int64 ext_copy = (pmatch_len > 32) ? 32 : pmatch_len;
        for (int64 k = 0; k < ext_copy; k += 1) {
            init_pmatch[k] = pmatch[k];
        }
    }

    if (stack == NULL) {
        stack = realloc2(NULL, 0, stack_cap, SIZEOF(*stack));
    }

    if (regex->used_ops & META_OP_ALTERNATION) {
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
                btnfa_stack_push(&stack, &stack_cap, &stack_ptr, alts[i],
                                 search_ptr, init_pmatch, NULL, 0, 0);
            }
        }
    } else {
        if (!btnfa_quick_lookahead_fails(regex->ops, search_ptr)) {
            btnfa_stack_push(&stack, &stack_cap, &stack_ptr, regex->ops,
                             search_ptr, init_pmatch, NULL, 0, 0);
        }
    }

    while (stack_ptr > 0) {
        MetaOp *pc;
        uint8 *input;
        MetaRegexMatch current_pmatch[32];
        uint32 visited_empty[META_PC_WORDS];

        step_count += 1;
        if (step_count > 4096 && !is_catastrophic) {
            is_catastrophic = 1;
            if (!(regex->used_ops & META_OP_BACKREF)) {
                memo_size = (string_len + 1)*META_PC_WORDS;
                memo = malloc2(memo_size*SIZEOF(*memo));
                memset64(memo, 0, memo_size*SIZEOF(*memo));
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

        if (is_catastrophic) {
            for (int32 w = 0; w < META_PC_WORDS; w += 1) {
                visited_empty[w] = stack[stack_ptr].visited_empty[w];
            }
        }

        for (int32 k = 0; k < 32; k += 1) {
            current_pmatch[k] = stack[stack_ptr].pmatch[k];
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
                    for (int32 k = 0; k < 32; k += 1) {
                        best_pmatch[k] = current_pmatch[k];
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

            if (pc->type == META_OP_BACKREF
                && (pc[1].type == META_OP_STAR || pc[1].type == META_OP_PLUS
                    || pc[1].type == META_OP_OPTIONAL
                    || pc[1].type == META_OP_BOUNDED)) {
                uint8 *backref_ptr = NULL;
                int32 group_id = pc->value;
                int32 backref_len = 0;
                int32 min_req = 0;
                int32 max_req = -1;
                int32 count = 0;
                MetaOp *next_ops = pc + 2;

                if (!btnfa_get_backref(current_pmatch, group_id, string,
                                       &backref_ptr, &backref_len)) {
                    break;
                }

                if (pc[1].type == META_OP_STAR) {
                    min_req = 0;
                    max_req = -1;
                } else if (pc[1].type == META_OP_PLUS) {
                    min_req = 1;
                    max_req = -1;
                } else if (pc[1].type == META_OP_OPTIONAL) {
                    min_req = 0;
                    max_req = 1;
                } else if (pc[1].type == META_OP_BOUNDED) {
                    min_req = pc[1].min;
                    max_req = pc[1].max;
                }

                if (backref_len == 0) {
                    if (min_req >= 0) {
                        if (!btnfa_quick_lookahead_fails(next_ops, input)) {
                            btnfa_stack_push(&stack, &stack_cap, &stack_ptr,
                                             next_ops, input, current_pmatch,
                                             visited_empty, is_catastrophic, 0);
                        }
                    }
                    break;
                }

                while (max_req == -1 || count < max_req) {
                    uint8 *candidate = input + count*backref_len;
                    if (!btnfa_backref_matches_at(candidate, backref_ptr,
                                                  backref_len)) {
                        break;
                    }
                    count += 1;
                }

                if (count >= min_req) {
                    for (int32 n = min_req; n <= count; n += 1) {
                        uint8 *p = input + n*backref_len;
                        if (!btnfa_quick_lookahead_fails(next_ops, p)) {
                            btnfa_stack_push(&stack, &stack_cap, &stack_ptr,
                                             next_ops, p, current_pmatch,
                                             visited_empty, is_catastrophic,
                                             p > input);
                        }
                    }
                }
                break;
            }

            if (pc->type == META_OP_BACKREF) {
                int32 group_id = pc->value;
                int32 backref_len;
                uint8 *backref_ptr;

                if (btnfa_get_backref(current_pmatch, group_id, string,
                                      &backref_ptr, &backref_len)) {
                    if (btnfa_backref_matches_at(input, backref_ptr,
                                                 backref_len)) {
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
                    btnfa_stack_push(&stack, &stack_cap, &stack_ptr,
                                     pc + pc->min, input, current_pmatch,
                                     visited_empty, is_catastrophic, 0);
                }

                if (!skip_1) {
                    btnfa_stack_push(&stack, &stack_cap, &stack_ptr,
                                     pc + pc->value, input, current_pmatch,
                                     visited_empty, is_catastrophic, 0);
                }
                break;
            }

            if (pc->type == META_OP_JUMP) {
                pc += pc->value;
                continue;
            }

            if (pc->type == META_OP_GROUP_START) {
                int32 group_id = pc->value;
                if (group_id >= 0 && group_id < 32) {
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
                            btnfa_stack_push(&stack, &stack_cap, &stack_ptr,
                                             alts[i], input, current_pmatch,
                                             visited_empty, is_catastrophic, 0);
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
                if (group_id >= 0 && group_id < 32) {
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
                                btnfa_stack_push(&stack, &stack_cap, &stack_ptr,
                                                 next_ops, p, current_pmatch,
                                                 visited_empty, is_catastrophic,
                                                 p > input);
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
            if (pmatch != NULL && pmatch_len > 0) {
                pmatch[0].rm_so = offset;
                pmatch[0].rm_eo = match_len;
                int64 ext_copy = (pmatch_len > 32) ? 32 : pmatch_len;
                for (int64 k = 1; k < ext_copy; k += 1) {
                    pmatch[k] = best_pmatch[k];
                }
            }
            return 0;
        }
    }

    return -1;
}

#endif /* META_MATCH_BTNFA */
