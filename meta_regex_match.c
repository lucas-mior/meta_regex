#if !defined(META_REGEX_MATCH_C)
#define META_REGEX_MATCH_C

#include <regex.h>
#include <string.h>
#include "meta_regex.h"
#include "util.c"

static int32
is_word_char(char c) {
    int32 match;
    match = 0;
    if (c >= 'a' && c <= 'z') {
        match = 1;
    } else if (c >= 'A' && c <= 'Z') {
        match = 1;
    } else if (c >= '0' && c <= '9') {
        match = 1;
    } else if (c == '_') {
        match = 1;
    }
    return match;
}

static int match_at_recursive(MetaOp *ops, char *orig, char *curr, 
                              size_t nmatch, regmatch_t pmatch[]);

static int
eval_choice_point(MetaOp *ops, char *orig, char *curr, 
                  size_t nmatch, regmatch_t pmatch[]) {
    MetaOp *alt_start;
    regmatch_t temp_pmatch[32];
    regmatch_t best_pmatch[32];
    regmatch_t *pass_pmatch;
    size_t copy_size;
    int32 longest_match;
    int32 res;
    int32 depth;

    if (nmatch > 32) {
        copy_size = 32;
    } else {
        copy_size = nmatch;
    }

    alt_start = ops;
    longest_match = -1;

    while (1) {
        pass_pmatch = NULL;
        if (pmatch != NULL) {
            for (size_t i = 0; i < copy_size; i += 1) {
                temp_pmatch[i] = pmatch[i];
            }
            pass_pmatch = temp_pmatch;
        }

        res = match_at_recursive(alt_start, orig, curr, nmatch, pass_pmatch);
        
        if (res >= 0) {
            if (pmatch == NULL) {
                return res;
            }
            if (res > longest_match) {
                longest_match = res;
                if (pmatch != NULL) {
                    for (size_t i = 0; i < copy_size; i += 1) {
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
            for (size_t i = 0; i < copy_size; i += 1) {
                pmatch[i] = best_pmatch[i];
            }
        }
    }

    return longest_match;
}

static int
match_at_recursive(MetaOp *ops, char *original_string, char *current_string, 
                   size_t nmatch, regmatch_t pmatch[]) {
    MetaOp *end_op;
    MetaOp *scan;
    MetaOp token;
    MetaOp *next_ops;
    regmatch_t temp_pmatch[32];
    regmatch_t best_pmatch[32];
    regmatch_t *pass_pmatch;
    char *s;
    char *max_s;
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
    int32 count;
    int32 has_alt;
    size_t copy_size;

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
            return match_at_recursive(ops + 1, original_string, current_string, 
                                      nmatch, pmatch);
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
            return match_at_recursive(ops + 1, original_string, current_string, 
                                      nmatch, pmatch);
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
            return match_at_recursive(ops + 1, original_string, current_string, 
                                      nmatch, pmatch);
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
            return match_at_recursive(ops + 1, original_string, current_string, 
                                      nmatch, pmatch);
        }
        return -1;
    }

    if (ops[0].type == META_OP_ALTERNATION) {
        end_op = ops;
        depth = 0;
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
        return match_at_recursive(end_op, original_string, current_string, 
                                  nmatch, pmatch);
    }

    if (ops[0].type == META_OP_SPLIT) {
        longest_match = -1;

        pass_pmatch = NULL;
        if (pmatch != NULL) {
            for (size_t i = 0; i < copy_size; i += 1) {
                temp_pmatch[i] = pmatch[i];
            }
            pass_pmatch = temp_pmatch;
        }
        res = match_at_recursive(ops + ops[0].value, original_string, 
                                 current_string, nmatch, pass_pmatch);
        if (res >= 0) {
            if (pmatch == NULL) {
                return res;
            }
            if (res > longest_match) {
                longest_match = res;
                for (size_t i = 0; i < copy_size; i += 1) {
                    best_pmatch[i] = pass_pmatch[i];
                }
            }
        }

        pass_pmatch = NULL;
        if (pmatch != NULL) {
            for (size_t i = 0; i < copy_size; i += 1) {
                temp_pmatch[i] = pmatch[i];
            }
            pass_pmatch = temp_pmatch;
        }
        res = match_at_recursive(ops + ops[0].min, original_string, 
                                 current_string, nmatch, pass_pmatch);
        if (res >= 0) {
            if (pmatch == NULL) {
                return res;
            }
            if (res > longest_match) {
                longest_match = res;
                for (size_t i = 0; i < copy_size; i += 1) {
                    best_pmatch[i] = pass_pmatch[i];
                }
            }
        }

        if (longest_match >= 0 && pmatch != NULL) {
            for (size_t i = 0; i < copy_size; i += 1) {
                pmatch[i] = best_pmatch[i];
            }
        }
        return longest_match;
    }

    if (ops[0].type == META_OP_JUMP) {
        return match_at_recursive(ops + ops[0].value, original_string, 
                                  current_string, nmatch, pmatch);
    }

    if (ops[0].type == META_OP_GROUP_START) {
        group_id = ops[0].value;
        old_so = -1;
        if (pmatch != NULL && (size_t)group_id < nmatch) {
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
            res = eval_choice_point(ops + 1, original_string, current_string, 
                                    nmatch, pmatch);
        } else {
            res = match_at_recursive(ops + 1, original_string, current_string, 
                                     nmatch, pmatch);
        }

        if (res >= 0) {
            return res;
        }

        if (pmatch != NULL && (size_t)group_id < nmatch) {
            pmatch[group_id].rm_so = old_so;
        }
        return -1;
    }

    if (ops[0].type == META_OP_GROUP_END) {
        group_id = ops[0].value;
        old_eo = -1;
        if (pmatch != NULL && (size_t)group_id < nmatch) {
            old_eo = pmatch[group_id].rm_eo;
            pmatch[group_id].rm_eo = (int32)(current_string - original_string);
        }
        res = match_at_recursive(ops + 1, original_string, current_string, 
                                 nmatch, pmatch);
        if (res >= 0) {
            return res;
        }
        if (pmatch != NULL && (size_t)group_id < nmatch) {
            pmatch[group_id].rm_eo = old_eo;
        }
        return -1;
    }

    is_star = (ops[1].type == META_OP_STAR);
    is_plus = (ops[1].type == META_OP_PLUS);
    is_opt  = (ops[1].type == META_OP_OPTIONAL);
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
                if ((int32)(unsigned char)s[count] != token.value) {
                    break;
                }
                count += 1;
            }
        } else if (token.type == META_OP_CLASS) {
            while (max_req == -1 || count < max_req) {
                unsigned char fb;
                fb = (unsigned char)s[count];
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
            pass_pmatch = NULL;
            if (pmatch != NULL) {
                for (size_t i = 0; i < copy_size; i += 1) {
                    temp_pmatch[i] = pmatch[i];
                }
                pass_pmatch = temp_pmatch;
            }

            res = match_at_recursive(next_ops, original_string, max_s, 
                                     nmatch, pass_pmatch);
            
            if (res >= 0) {
                if (pmatch == NULL) {
                    return res;
                }
                if (res > longest_match) {
                    longest_match = res;
                    if (pmatch != NULL) {
                        for (size_t i = 0; i < copy_size; i += 1) {
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

        if (longest_match >= 0) {
            if (pmatch != NULL) {
                for (size_t i = 0; i < copy_size; i += 1) {
                    pmatch[i] = best_pmatch[i];
                }
            }
        }

        return longest_match;
    }

    {
        int32 is_match;
        int32 consumed;
        unsigned char fb;

        consumed = 0;
        fb = (unsigned char)current_string[0];
        if (fb == '\0') {
            is_match = 0;
        } else {
            consumed = 1;
            if (ops[0].type == META_OP_ANY) {
                is_match = 1;
            } else if (ops[0].type == META_OP_LITERAL) {
                if ((int32)fb == ops[0].value) {
                    is_match = 1;
                } else {
                    is_match = 0;
                }
            } else if (ops[0].type == META_OP_CLASS) {
                if ((ops[0].mask[fb / 32] & (1u << (fb % 32))) != 0) {
                    is_match = 1;
                } else {
                    is_match = 0;
                }
            } else {
                is_match = 0;
            }
        }

        if (is_match) {
            return match_at_recursive(ops + 1, original_string, 
                                      current_string + consumed, nmatch, 
                                      pmatch);
        }
    }
    return -1;
}

static int
meta_regex_match(MetaRegex *regex, char *string, 
                 size_t nmatch, regmatch_t pmatch[]) {
    char *search_ptr;
    char *next;
    int32 match_len;
    int32 is_mandatory_lead;
    int32 res;

    if (regex == NULL) {
        return REG_NOMATCH;
    }

    if (regex->has_start_anchor) {
        if (pmatch != NULL) {
            for (size_t k = 0; k < nmatch; k += 1) { 
                pmatch[k].rm_so = -1; 
                pmatch[k].rm_eo = -1; 
            }
        }

        if (regex->has_alternation) {
            res = eval_choice_point(regex->ops, string, string, nmatch, pmatch);
        } else {
            res = match_at_recursive(regex->ops, string, string, 
                                     nmatch, pmatch);
        }

        if (res >= 0) {
            if (regex->has_end_anchor && string[res] != '\0') {
                return REG_NOMATCH;
            }
            if (pmatch != NULL && nmatch > 0) { 
                pmatch[0].rm_so = 0; 
                pmatch[0].rm_eo = res; 
            }
            return 0;
        }
        return REG_NOMATCH;
    }

    is_mandatory_lead = 0;
    if (!regex->has_alternation) {
        if (regex->ops[0].type == META_OP_LITERAL) {
            if (regex->ops[1].type != META_OP_STAR && 
                regex->ops[1].type != META_OP_OPTIONAL) {
                if (regex->ops[1].type != META_OP_BOUNDED || 
                    regex->ops[1].min > 0) {
                    is_mandatory_lead = 1;
                }
            }
        }
    }

    for (int32 j = 0; ; ) {
        search_ptr = &string[j];
        
        if (is_mandatory_lead) {
            next = strchr(search_ptr, (char)regex->ops[0].value);
            if (next == NULL) {
                break;
            }
            j = (int32)(next - string);
            search_ptr = next;
        }

        if (pmatch != NULL) {
            for (size_t k = 0; k < nmatch; k += 1) { 
                pmatch[k].rm_so = -1; 
                pmatch[k].rm_eo = -1; 
            }
        }

        if (regex->has_alternation) {
            match_len = eval_choice_point(regex->ops, string, search_ptr, 
                                          nmatch, pmatch);
        } else {
            match_len = match_at_recursive(regex->ops, string, search_ptr, 
                                           nmatch, pmatch);
        }

        if (match_len >= 0) {
            if (!regex->has_end_anchor || string[match_len] == '\0') {
                if (pmatch != NULL && nmatch > 0) { 
                    pmatch[0].rm_so = j; 
                    pmatch[0].rm_eo = match_len; 
                }
                return 0;
            }
        }
        
        if (string[j] == '\0') {
            break;
        }

        j += 1;
    }
    return REG_NOMATCH;
}

#endif /* META_REGEX_MATCH_C */
