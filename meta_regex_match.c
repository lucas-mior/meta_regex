#if !defined(META_REGEX_MATCH_C)
#define META_REGEX_MATCH_C

#include <regex.h>
#include <string.h>
#include "meta_regex.h"
#include "util.c"

static int
matches_char(MetaOp op, char *s, int32 *consumed) {
    unsigned char first_byte;
    int32 is_match;

    first_byte = (unsigned char)s[0];
    if (first_byte == '\0') {
        *consumed = 0;
        return 0;
    }

    *consumed = 1;

    if (op.type == META_OP_ANY) {
        return 1;
    }

    if (op.type == META_OP_LITERAL) {
        is_match = ((int32)first_byte == op.value);
        return is_match;
    } else if (op.type == META_OP_CLASS) {
        is_match = ((op.mask[first_byte / 32] & 
                     (1u << (first_byte % 32))) != 0);
        return is_match;
    }
    
    return 0;
}

static int match_at_recursive(MetaOp *ops, char *orig, char *curr, 
                              size_t nmatch, regmatch_t pmatch[]);

static int
eval_choice_point(MetaOp *ops, char *orig, char *curr, 
                  size_t nmatch, regmatch_t pmatch[]) {
    MetaOp *alt_start;
    regmatch_t pmatch_backup[32];
    size_t copy_size;
    int32 res;
    int32 depth;

    alt_start = ops;
    while (1) {
        if (pmatch != NULL) {
            copy_size = (nmatch > 32) ? 32 : nmatch;
            for (size_t i = 0; i < copy_size; i += 1) {
                pmatch_backup[i] = pmatch[i];
            }
        }

        res = match_at_recursive(alt_start, orig, curr, nmatch, pmatch);
        if (res >= 0) {
            return res;
        }

        if (pmatch != NULL) {
            copy_size = (nmatch > 32) ? 32 : nmatch;
            for (size_t i = 0; i < copy_size; i += 1) {
                pmatch[i] = pmatch_backup[i];
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
            return -1;
        }
    }
}

static int
match_at_recursive(MetaOp *ops, char *original_string, char *current_string, 
                   size_t nmatch, regmatch_t pmatch[]) {
    MetaOp *end_op;
    MetaOp *scan;
    MetaOp token;
    MetaOp *next_ops;
    regmatch_t pmatch_backup[32];
    char *s;
    char *max_s;
    int32 depth;
    int32 group_id;
    int32 old_so;
    int32 old_eo;
    int32 res;
    int32 is_star;
    int32 is_plus;
    int32 is_opt;
    int32 is_bound;
    int32 min_req;
    int32 max_req;
    int32 consumed;
    int32 count;
    int32 has_alt;
    size_t copy_size;

    if (ops[0].type == META_OP_END) {
        return (int32)(current_string - original_string);
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
        consumed = 0;
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

        while (count < min_req) {
            if (!matches_char(token, s, &consumed)) {
                return -1;
            }
            s += consumed;
            count += 1;
        }

        max_s = s;
        while ((max_req == -1 || count < max_req) && 
               matches_char(token, max_s, &consumed)) {
            max_s += consumed;
            count += 1;
        }

        if (pmatch != NULL) {
            copy_size = (nmatch > 32) ? 32 : nmatch;
            for (size_t i = 0; i < copy_size; i += 1) {
                pmatch_backup[i] = pmatch[i];
            }
        }

        while (max_s >= s) {
            res = match_at_recursive(next_ops, original_string, max_s, 
                                     nmatch, pmatch);
            if (res >= 0) {
                return res;
            }

            if (pmatch != NULL) {
                copy_size = (nmatch > 32) ? 32 : nmatch;
                for (size_t i = 0; i < copy_size; i += 1) {
                    pmatch[i] = pmatch_backup[i];
                }
            }

            if (max_s == s) {
                break;
            }

            max_s -= 1;
        }
        return -1;
    }

    consumed = 0;
    if (matches_char(ops[0], current_string, &consumed)) {
        return match_at_recursive(ops + 1, original_string, 
                                  current_string + consumed, nmatch, pmatch);
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

    is_mandatory_lead = (!regex->has_alternation &&
                         regex->ops[0].type == META_OP_LITERAL && 
                         regex->ops[1].type != META_OP_STAR && 
                         regex->ops[1].type != META_OP_OPTIONAL &&
                         (regex->ops[1].type != META_OP_BOUNDED || 
                          regex->ops[1].min > 0));

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
