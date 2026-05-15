#if !defined(META_REGEX_MATCH_C)
#define META_REGEX_MATCH_C

#include <regex.h>
#include <string.h>
#include "meta_regex.h"
#include "util.c"
#include "meta_util.c"

static int32 btnfa_match_at_recursive(MetaOp *ops, uchar *orig, uchar *curr,
                                      int64 nmatch, regmatch_t pmatch[]);

static int32
btnfa_quick_lookahead_fails(MetaOp *next_op, uchar *curr_str) {
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
            if ((int32)(uchar)*curr_str != next_op->value) {
                return 1;
            }
        } else if (next_op->type == META_OP_CLASS) {
            uchar fb;

            fb = (uchar)*curr_str;
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
btnfa_eval_choice_point(MetaOp *ops, uchar *orig, uchar *curr, int64 nmatch,
                        regmatch_t pmatch[]) {
    MetaOp *alt_start;
    regmatch_t temp_pmatch[32];
    regmatch_t best_pmatch[32];
    regmatch_t *pass_pmatch;
    int64 copy_size;
    int32 longest_match;
    int32 res;
    int32 depth;
    int32 skip;

    if (nmatch > 32) {
        copy_size = 32;
    } else {
        copy_size = nmatch;
    }

    alt_start = ops;
    longest_match = -1;

    while (1) {
        skip = btnfa_quick_lookahead_fails(alt_start, curr);
        if (!skip) {
            pass_pmatch = NULL;
            if (pmatch != NULL) {
                for (int64 i = 0; i < copy_size; i += 1) {
                    temp_pmatch[i] = pmatch[i];
                }
                pass_pmatch = temp_pmatch;
            }

            res = btnfa_match_at_recursive(alt_start, orig, curr, nmatch,
                                           pass_pmatch);

            if (res >= 0) {
                if (pmatch == NULL) {
                    return res;
                }
                if (res > longest_match) {
                    longest_match = res;
                    for (int64 i = 0; i < copy_size; i += 1) {
                        best_pmatch[i] = pass_pmatch[i];
                    }
                }
            }
        }

        depth = 0;
        while (alt_start->type != META_OP_END) {
            if (alt_start->type == META_OP_GROUP_START) {
                depth += 1;
            } else if (alt_start->type == META_OP_GROUP_END) {
                if (depth == 0) {
                    break;
                }
                depth -= 1;
            } else if (alt_start->type == META_OP_ALTERNATION && depth == 0) {
                break;
            }
            alt_start += 1;
        }

        if (alt_start->type == META_OP_ALTERNATION && depth == 0) {
            alt_start += 1;
        } else {
            break;
        }
    }

    if (longest_match >= 0) {
        if (pmatch != NULL) {
            for (int64 i = 0; i < copy_size; i += 1) {
                pmatch[i] = best_pmatch[i];
            }
        }
    }

    return longest_match;
}

static int32
btnfa_match_at_recursive(MetaOp *ops, uchar *original_string,
                         uchar *current_string, int64 nmatch,
                         regmatch_t pmatch[]) {
    MetaOp *end_op;
    MetaOp *scan;
    MetaOp token;
    MetaOp *next_ops;
    regmatch_t temp_pmatch[32];
    regmatch_t best_pmatch[32];
    regmatch_t *pass_pmatch;
    uchar *s;
    uchar *max_s;
    int32 depth;
    int32 group_id;
    int32 old_so;
    int32 old_eo;
    int32 res;
    int32 longest_match;
    int32 is_star;
    int32 is_plus;
    int32 is_opt;
    int32 is_bound;
    int32 min_req;
    int32 max_req;
    int32 consumed;
    int32 count;
    int32 has_alt;
    int32 skip_1;
    int32 skip_2;
    int32 skip_quant;
    int64 copy_size;

    if (nmatch > 32) {
        copy_size = 32;
    } else {
        copy_size = nmatch;
    }

    if (ops[0].type == META_OP_END) {
        return (int32)(current_string - original_string);
    }

    if (ops[0].type == META_OP_WORD_START) {
        int32 curr_is_word;
        int32 prev_is_word;

        curr_is_word = is_word_char(*current_string);
        prev_is_word = 0;
        if (current_string > original_string) {
            prev_is_word = is_word_char(*(current_string - 1));
        }
        if (curr_is_word && !prev_is_word) {
            return btnfa_match_at_recursive(ops + 1, original_string,
                                            current_string, nmatch, pmatch);
        }
        return -1;
    }

    if (ops[0].type == META_OP_WORD_END) {
        int32 curr_is_word;
        int32 prev_is_word;

        curr_is_word = is_word_char(*current_string);
        prev_is_word = 0;
        if (current_string > original_string) {
            prev_is_word = is_word_char(*(current_string - 1));
        }
        if (!curr_is_word && prev_is_word) {
            return btnfa_match_at_recursive(ops + 1, original_string,
                                            current_string, nmatch, pmatch);
        }
        return -1;
    }

    if (ops[0].type == META_OP_WORD_BOUNDARY) {
        int32 curr_is_word;
        int32 prev_is_word;

        curr_is_word = is_word_char(*current_string);
        prev_is_word = 0;
        if (current_string > original_string) {
            prev_is_word = is_word_char(*(current_string - 1));
        }
        if (curr_is_word != prev_is_word) {
            return btnfa_match_at_recursive(ops + 1, original_string,
                                            current_string, nmatch, pmatch);
        }
        return -1;
    }

    if (ops[0].type == META_OP_NON_WORD_BOUNDARY) {
        int32 curr_is_word;
        int32 prev_is_word;

        curr_is_word = is_word_char(*current_string);
        prev_is_word = 0;
        if (current_string > original_string) {
            prev_is_word = is_word_char(*(current_string - 1));
        }
        if (curr_is_word == prev_is_word) {
            return btnfa_match_at_recursive(ops + 1, original_string,
                                            current_string, nmatch, pmatch);
        }
        return -1;
    }

    if (ops[0].type == META_OP_BACKREF) {
        int32 backref_len;
        uchar *backref_ptr;

        group_id = ops[0].value;
        backref_len = 0;
        backref_ptr = NULL;
        if (pmatch != NULL && (int64)group_id < nmatch
            && pmatch[group_id].rm_so != -1) {
            backref_len = pmatch[group_id].rm_eo - pmatch[group_id].rm_so;
            backref_ptr = original_string + pmatch[group_id].rm_so;

            if (strncmp32((char*)current_string, (char*)backref_ptr,
                          backref_len) == 0) {
                return btnfa_match_at_recursive(ops + 1, original_string,
                                                current_string + backref_len,
                                                nmatch, pmatch);
            }
        }
        return -1;
    }

    if (ops[0].type == META_OP_ALTERNATION) {
        depth = 0;
        end_op = ops;
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
        return btnfa_match_at_recursive(end_op, original_string, current_string,
                                        nmatch, pmatch);
    }

    if (ops[0].type == META_OP_SPLIT) {
        longest_match = -1;
        skip_1 = btnfa_quick_lookahead_fails(ops + ops[0].value,
                                             current_string);
        if (!skip_1) {
            pass_pmatch = NULL;
            if (pmatch != NULL) {
                for (int64 i = 0; i < copy_size; i += 1) {
                    temp_pmatch[i] = pmatch[i];
                }
                pass_pmatch = temp_pmatch;
            }
            res = btnfa_match_at_recursive(ops + ops[0].value, original_string,
                                           current_string, nmatch, pass_pmatch);
            if (res >= 0) {
                if (pmatch == NULL) {
                    return res;
                }
                if (res > longest_match) {
                    longest_match = res;
                    for (int64 i = 0; i < copy_size; i += 1) {
                        best_pmatch[i] = pass_pmatch[i];
                    }
                }
            }
        }

        skip_2 = btnfa_quick_lookahead_fails(ops + ops[0].min, current_string);
        if (!skip_2) {
            pass_pmatch = NULL;
            if (pmatch != NULL) {
                for (int64 i = 0; i < copy_size; i += 1) {
                    temp_pmatch[i] = pmatch[i];
                }
                pass_pmatch = temp_pmatch;
            }
            res = btnfa_match_at_recursive(ops + ops[0].min, original_string,
                                           current_string, nmatch, pass_pmatch);
            if (res >= 0) {
                if (pmatch == NULL) {
                    return res;
                }
                if (res > longest_match) {
                    longest_match = res;
                    for (int64 i = 0; i < copy_size; i += 1) {
                        best_pmatch[i] = pass_pmatch[i];
                    }
                }
            }
        }

        if (longest_match >= 0 && pmatch != NULL) {
            for (int64 i = 0; i < copy_size; i += 1) {
                pmatch[i] = best_pmatch[i];
            }
        }
        return longest_match;
    }

    if (ops[0].type == META_OP_JUMP) {
        return btnfa_match_at_recursive(ops + ops[0].value, original_string,
                                        current_string, nmatch, pmatch);
    }

    if (ops[0].type == META_OP_GROUP_START) {
        group_id = ops[0].value;
        old_so = -1;
        if (pmatch != NULL && (int64)group_id < nmatch) {
            old_so = pmatch[group_id].rm_so;
            pmatch[group_id].rm_so = (int32)(current_string - original_string);
        }

        has_alt = 0;
        depth = 0;
        scan = ops + 1;
        while (scan->type != META_OP_END) {
            if (scan->type == META_OP_GROUP_START) {
                depth += 1;
            } else if (scan->type == META_OP_GROUP_END) {
                if (depth == 0) {
                    break;
                }
                depth -= 1;
            } else if (scan->type == META_OP_ALTERNATION && depth == 0) {
                has_alt = 1;
                break;
            }
            scan += 1;
        }

        if (has_alt) {
            res = btnfa_eval_choice_point(ops + 1, original_string,
                                          current_string, nmatch, pmatch);
        } else {
            res = btnfa_match_at_recursive(ops + 1, original_string,
                                           current_string, nmatch, pmatch);
        }

        if (res >= 0) {
            return res;
        }
        if (pmatch != NULL && (int64)group_id < nmatch) {
            pmatch[group_id].rm_so = old_so;
        }
        return -1;
    }

    if (ops[0].type == META_OP_GROUP_END) {
        group_id = ops[0].value;
        old_eo = -1;
        if (pmatch != NULL && (int64)group_id < nmatch) {
            old_eo = pmatch[group_id].rm_eo;
            pmatch[group_id].rm_eo = (int32)(current_string - original_string);
        }
        res = btnfa_match_at_recursive(ops + 1, original_string, current_string,
                                       nmatch, pmatch);
        if (res >= 0) {
            return res;
        }
        if (pmatch != NULL && (int64)group_id < nmatch) {
            pmatch[group_id].rm_eo = old_eo;
        }
        return -1;
    }

    is_star = (ops[1].type == META_OP_STAR);
    is_plus = (ops[1].type == META_OP_PLUS);
    is_opt = (ops[1].type == META_OP_OPTIONAL);
    is_bound = (ops[1].type == META_OP_BOUNDED);

    if (is_star || is_plus || is_opt || is_bound) {
        token = ops[0];
        next_ops = ops + 2;
        s = current_string;
        min_req = 0;
        max_req = -1;
        count = 0;

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
            min_req = ops[1].min;
            max_req = ops[1].max;
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
                if ((int32)(uchar)s[count] != token.value) {
                    break;
                }
                count += 1;
            }
        } else if (token.type == META_OP_CLASS) {
            while (max_req == -1 || count < max_req) {
                uchar fb;

                fb = (uchar)s[count];
                if (fb == '\0') {
                    break;
                }
                if ((token.mask[fb / 32] & (1u << (fb % 32))) == 0) {
                    break;
                }
                count += 1;
            }
        }

        if (count < min_req) {
            return -1;
        }

        max_s = s + count;
        s += min_req;
        longest_match = -1;

        while (max_s >= s) {
            skip_quant = btnfa_quick_lookahead_fails(next_ops, max_s);
            if (!skip_quant) {
                pass_pmatch = NULL;
                if (pmatch != NULL) {
                    for (int64 i = 0; i < copy_size; i += 1) {
                        temp_pmatch[i] = pmatch[i];
                    }
                    pass_pmatch = temp_pmatch;
                }
                res = btnfa_match_at_recursive(next_ops, original_string, max_s,
                                               nmatch, pass_pmatch);
                if (res >= 0) {
                    if (pmatch == NULL) {
                        return res;
                    }
                    if (res > longest_match) {
                        longest_match = res;
                        for (int64 i = 0; i < copy_size; i += 1) {
                            best_pmatch[i] = pass_pmatch[i];
                        }
                    }
                }
            }
            if (max_s == s) {
                break;
            }
            max_s -= 1;
        }
        if (longest_match >= 0 && pmatch != NULL) {
            for (int64 i = 0; i < copy_size; i += 1) {
                pmatch[i] = best_pmatch[i];
            }
        }
        return longest_match;
    }

    {
        int32 is_match;
        uchar fb;

        consumed = 0;
        fb = (uchar)current_string[0];
        if (fb == '\0') {
            is_match = 0;
        } else {
            consumed = 1;
            if (ops[0].type == META_OP_ANY) {
                is_match = 1;
            } else if (ops[0].type == META_OP_LITERAL) {
                is_match = ((int32)fb == ops[0].value);
            } else if (ops[0].type == META_OP_CLASS) {
                is_match = ((ops[0].mask[fb / 32] & (1u << (fb % 32))) != 0);
            } else {
                is_match = 0;
            }
        }
        if (is_match) {
            return btnfa_match_at_recursive(ops + 1, original_string,
                                            current_string + consumed, nmatch,
                                            pmatch);
        }
    }
    return -1;
}

static int32
try_match_btnfa(MetaRegex *regex, uchar *string, int32 offset, int64 nmatch,
                regmatch_t pmatch[]) {
    uchar *search_ptr;
    int32 match_len;

    search_ptr = &string[offset];
    match_len = -1;

    if (regex->has_alternation) {
        match_len = btnfa_eval_choice_point(regex->ops, string, search_ptr,
                                            nmatch, pmatch);
    } else {
        if (!btnfa_quick_lookahead_fails(regex->ops, search_ptr)) {
            match_len = btnfa_match_at_recursive(regex->ops, string, search_ptr,
                                                 nmatch, pmatch);
        }
    }

    if (match_len >= 0) {
        if (!regex->has_end_anchor || string[match_len] == '\0') {
            if (pmatch != NULL && nmatch > 0) {
                pmatch[0].rm_so = offset;
                pmatch[0].rm_eo = match_len;
            }
            return 0;
        }
    }

    return -1;
}

static int32
try_match_dfa(MetaRegex *regex, uchar *string, int32 offset, int64 nmatch,
              regmatch_t pmatch[]) {
    DfaState *states;
    DfaState *current_state_ptr;
    int32 last_accept;

    states = regex->dfa->states;
    current_state_ptr = &states[regex->dfa->start_state];
    last_accept = -1;

    for (int32 i = offset;; i += 1) {
        uchar b;
        int32 next_state_idx;
        int32 *next_table;

        b = (uchar)string[i];
        
        if (current_state_ptr->is_accepting) {
            last_accept = i;
        }

        if (b == '\0') {
            break;
        }

        next_table = current_state_ptr->next;
        next_state_idx = next_table[b];
        
        if (next_state_idx == 0) {
            break;
        }

        current_state_ptr = &states[next_state_idx];
    }

    if (last_accept >= 0) {
        if (!regex->has_end_anchor || string[last_accept] == '\0') {
            if (pmatch != NULL && nmatch > 0) {
                pmatch[0].rm_so = offset;
                pmatch[0].rm_eo = last_accept;
            }
            return 0;
        }
    }

    return -1;
}

static int32
meta_regex_match(MetaRegex *regex, uchar *string, int64 nmatch,
                 regmatch_t pmatch[]) {
    int32 result;
    int32 use_backtracking;

    if (regex == NULL) {
        return REG_NOMATCH;
    }

    if (pmatch != NULL) {
        for (int64 k = 0; k < nmatch; k += 1) {
            pmatch[k].rm_so = -1;
            pmatch[k].rm_eo = -1;
        }
    }

    use_backtracking = 0;
    if (regex->has_backref || regex->dfa == NULL) {
        use_backtracking = 1;
    }
    assert(!use_backtracking);
    assert(regex->dfa);
    use_backtracking = 1;

    if (use_backtracking) {
        if (regex->has_start_anchor) {
            result = try_match_btnfa(regex, string, 0, nmatch, pmatch);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uchar b;
            int32 bit_match;

            b = (uchar)string[j];
            bit_match = (regex->fastmap[b >> 3] & (1 << (b & 7)));

            if (bit_match || regex->can_be_null) {
                result = try_match_btnfa(regex, string, j, nmatch, pmatch);
                if (result == 0) {
                    return 0;
                }
            }

            if (b == '\0') {
                break;
            }
        }
    } else {
        if (regex->has_start_anchor) {
            result = try_match_dfa(regex, string, 0, nmatch, pmatch);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uchar b;
            int32 bit_match;

            b = (uchar)string[j];
            bit_match = (regex->fastmap[b >> 3] & (1 << (b & 7)));

            if (bit_match || regex->can_be_null) {
                result = try_match_dfa(regex, string, j, nmatch, pmatch);
                if (result == 0) {
                    return 0;
                }
            }

            if (b == '\0') {
                break;
            }
        }
    }

    return REG_NOMATCH;
}

#endif /* META_REGEX_MATCH_C */
