#if !defined(META_REGEX_MATCH_C)
#define META_REGEX_MATCH_C

#include <regex.h>
#include "meta_regex.h"
#include "util.c"

static int
matches_char(MetaOp op, char c) {
    if (c == '\0') {
        return 0;
    }
    if (op.type == META_OP_ANY) {
        return 1;
    } else if (op.type == META_OP_LITERAL) {
        if (c == op.value) {
            return 1;
        }
    } else if (op.type == META_OP_DIGIT) {
        if (c >= '0') {
            if (c <= '9') {
                return 1;
            }
        }
    } else if (op.type == META_OP_ALPHA_LOWER) {
        if (c >= 'a') {
            if (c <= 'z') {
                return 1;
            }
        }
    } else if (op.type == META_OP_ALPHA_UPPER) {
        if (c >= 'A') {
            if (c <= 'Z') {
                return 1;
            }
        }
    }
    return 0;
}

static int match_at_recursive(MetaOp *ops, char *orig, char *curr, size_t nmatch, regmatch_t pmatch[]);

static int
eval_choice_point(MetaOp *ops, char *orig, char *curr, size_t nmatch, regmatch_t pmatch[]) {
    MetaOp *alt_start = ops;
    regmatch_t pmatch_backup[32];
    
    while (1) {
        int32 res;

        if (pmatch != NULL) {
            size_t copy_size = nmatch;
            if (copy_size > 32) {
                copy_size = 32;
            }
            for (size_t i = 0; i < copy_size; i += 1) {
                pmatch_backup[i] = pmatch[i];
            }
        }

        res = match_at_recursive(alt_start, orig, curr, nmatch, pmatch);
        if (res >= 0) {
            return res;
        }

        if (pmatch != NULL) {
            size_t copy_size = nmatch;
            if (copy_size > 32) {
                copy_size = 32;
            }
            for (size_t i = 0; i < copy_size; i += 1) {
                pmatch[i] = pmatch_backup[i];
            }
        }

        int32 depth = 0;
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
match_at_recursive(MetaOp *ops, char *original_string, char *current_string, size_t nmatch, regmatch_t pmatch[]) {
    if (ops[0].type == META_OP_END) {
        return (int32)(current_string - original_string);
    }

    if (ops[0].type == META_OP_ALTERNATION) {
        MetaOp *end_op = ops;
        int32 depth = 0;

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
        return match_at_recursive(end_op, original_string, current_string, nmatch, pmatch);
    }

    if (ops[0].type == META_OP_GROUP_START) {
        int32 group_id = ops[0].value;
        int32 old_so = -1;
        int32 res;

        if (pmatch != NULL) {
            if (group_id >= 0) {
                if ((size_t)group_id < nmatch) {
                    old_so = pmatch[group_id].rm_so;
                    pmatch[group_id].rm_so = (int32)(current_string - original_string);
                }
            }
        }

        res = eval_choice_point(ops + 1, original_string, current_string, nmatch, pmatch);
        if (res >= 0) {
            return res;
        }

        if (pmatch != NULL) {
            if (group_id >= 0) {
                if ((size_t)group_id < nmatch) {
                    pmatch[group_id].rm_so = old_so;
                }
            }
        }
        return -1;
    }

    if (ops[0].type == META_OP_GROUP_END) {
        int32 group_id = ops[0].value;
        int32 old_eo = -1;
        int32 res;

        if (pmatch != NULL) {
            if (group_id >= 0) {
                if ((size_t)group_id < nmatch) {
                    old_eo = pmatch[group_id].rm_eo;
                    pmatch[group_id].rm_eo = (int32)(current_string - original_string);
                }
            }
        }

        res = match_at_recursive(ops + 1, original_string, current_string, nmatch, pmatch);
        if (res >= 0) {
            return res;
        }

        if (pmatch != NULL) {
            if (group_id >= 0) {
                if ((size_t)group_id < nmatch) {
                    pmatch[group_id].rm_eo = old_eo;
                }
            }
        }
        return -1;
    }

    {
        int32 is_star = (ops[1].type == META_OP_STAR);
        int32 is_plus = (ops[1].type == META_OP_PLUS);
        int32 is_opt  = (ops[1].type == META_OP_OPTIONAL);

        if (is_star || is_plus || is_opt) {
            MetaOp token = ops[0];
            MetaOp *next_ops = ops + 2;
            char *s = current_string;
            char *max_s;
            regmatch_t pmatch_backup[32];

            if (is_plus) {
                if (!matches_char(token, *s)) {
                    return -1;
                }
                s += 1;
            }

            max_s = s;
            if (!is_opt) {
                while (matches_char(token, *max_s)) {
                    max_s += 1;
                }
            } else {
                if (matches_char(token, *max_s)) {
                    max_s += 1;
                }
            }

            if (pmatch != NULL) {
                size_t copy_size = nmatch;
                if (copy_size > 32) {
                    copy_size = 32;
                }
                for (size_t i = 0; i < copy_size; i += 1) {
                    pmatch_backup[i] = pmatch[i];
                }
            }

            while (max_s >= s) {
                int32 res = match_at_recursive(next_ops, original_string, max_s, nmatch, pmatch);
                if (res >= 0) {
                    return res;
                }
                if (pmatch != NULL) {
                    size_t copy_size = nmatch;
                    if (copy_size > 32) {
                        copy_size = 32;
                    }
                    for (size_t i = 0; i < copy_size; i += 1) {
                        pmatch[i] = pmatch_backup[i];
                    }
                }
                max_s -= 1;
            }
            return -1;
        }
    }

    if (matches_char(ops[0], *current_string)) {
        return match_at_recursive(ops + 1, original_string, current_string + 1, nmatch, pmatch);
    }

    return -1;
}

static int
meta_regex_match(MetaRegex regex, char *string, size_t nmatch, regmatch_t pmatch[]) {
    if (regex.has_start_anchor) {
        int32 match_len;

        if (pmatch != NULL) {
            for (size_t k = 0; k < nmatch; k += 1) {
                pmatch[k].rm_so = -1;
                pmatch[k].rm_eo = -1;
            }
        }
        match_len = eval_choice_point(regex.ops, string, string, nmatch, pmatch);
        if (match_len >= 0) {
            if (regex.has_end_anchor) {
                if (string[match_len] != '\0') {
                    return REG_NOMATCH;
                }
            }
            if (pmatch != NULL) {
                if (nmatch > 0) {
                    pmatch[0].rm_so = 0;
                    pmatch[0].rm_eo = match_len;
                }
            }
            return 0;
        }
        return REG_NOMATCH;
    }

    for (int32 j = 0; ; j += 1) {
        int32 match_len;

        if (pmatch != NULL) {
            for (size_t k = 0; k < nmatch; k += 1) {
                pmatch[k].rm_so = -1;
                pmatch[k].rm_eo = -1;
            }
        }
        match_len = eval_choice_point(regex.ops, string, &string[j], nmatch, pmatch);
        if (match_len >= 0) {
            int32 is_valid = 1;

            if (regex.has_end_anchor) {
                if (string[match_len] != '\0') {
                    is_valid = 0;
                }
            }
            
            if (is_valid) {
                if (pmatch != NULL) {
                    if (nmatch > 0) {
                        pmatch[0].rm_so = j;
                        pmatch[0].rm_eo = match_len;
                    }
                }
                return 0;
            }
        }

        if (string[j] == '\0') {
            break;
        }
    }
    return REG_NOMATCH;
}

#endif /* META_REGEX_MATCH_C */
