#if !defined(META_REGEX_MATCH_C)
#define META_REGEX_MATCH_C

#include <regex.h>
#include "meta_regex.h"
#include "util.c"
#include "utf8.h"

static int
matches_char(MetaOp op, char *s, int32 *consumed, int32 is_regex_ascii) {
    uchar first_byte;
    int32 cp;
    int32 is_match;

    first_byte = (uchar)s[0];
    if (first_byte == '\0') {
        *consumed = 0;
        return 0;
    }

    if (first_byte < 0x80) {
        *consumed = 1;
        if (op.type == META_OP_ANY) {
            return 1;
        }
        if (op.type == META_OP_LITERAL) {
            is_match = (op.value < 128 && (int32)first_byte == op.value);
            return is_match;
        }
        if (op.type == META_OP_CLASS) {
            is_match = ((op.mask[first_byte / 32] & (1u << (first_byte % 32))) != 0);
            return is_match;
        }
        return 0;
    }

    cp = utf8_decode(s, consumed);
    if (*consumed == 0) {
        return 0;
    }
    
    if (op.type == META_OP_ANY) {
        return 1;
    }

    if (is_regex_ascii) {
        return 0;
    }

    if (op.type == META_OP_LITERAL) {
        if (cp == op.value) {
            return 1;
        }
    } else if (op.type == META_OP_CLASS) {
        if (cp < 256) {
            is_match = ((op.mask[cp / 32] & (1u << (cp % 32))) != 0);
            return is_match;
        }
        
        for (int32 i = 0; i < 16; i += 1) {
            if (op.high_codepoints[i] == 0) {
                break;
            }
            if (op.high_codepoints[i] == cp) {
                return 1;
            }
        }
    }
    return 0;
}

static int match_at_recursive(MetaOp *ops, char *orig, char *curr, size_t nmatch, regmatch_t pmatch[], int32 is_ascii);

static int
eval_choice_point(MetaOp *ops, char *orig, char *curr, size_t nmatch, regmatch_t pmatch[], int32 is_ascii) {
    MetaOp *alt_start = ops;
    regmatch_t pmatch_backup[32];
    while (1) {
        int32 res;
        if (pmatch != NULL) {
            size_t copy_size = (nmatch > 32) ? 32 : nmatch;
            for (size_t i = 0; i < copy_size; i += 1) pmatch_backup[i] = pmatch[i];
        }
        res = match_at_recursive(alt_start, orig, curr, nmatch, pmatch, is_ascii);
        if (res >= 0) return res;
        if (pmatch != NULL) {
            size_t copy_size = (nmatch > 32) ? 32 : nmatch;
            for (size_t i = 0; i < copy_size; i += 1) pmatch[i] = pmatch_backup[i];
        }
        int32 depth = 0;
        while (alt_start->type != META_OP_END) {
            if (alt_start->type == META_OP_GROUP_START) depth += 1;
            else if (alt_start->type == META_OP_GROUP_END) {
                if (depth == 0) break;
                depth -= 1;
            } else if (alt_start->type == META_OP_ALTERNATION && depth == 0) break;
            alt_start += 1;
        }
        if (alt_start->type == META_OP_ALTERNATION && depth == 0) alt_start += 1;
        else return -1;
    }
}

static int
match_at_recursive(MetaOp *ops, char *original_string, char *current_string, size_t nmatch, regmatch_t pmatch[], int32 is_ascii) {
    if (ops[0].type == META_OP_END) return (int32)(current_string - original_string);
    if (ops[0].type == META_OP_ALTERNATION) {
        MetaOp *end_op = ops;
        int32 depth = 0;
        while (end_op->type != META_OP_END) {
            if (end_op->type == META_OP_GROUP_START) depth += 1;
            else if (end_op->type == META_OP_GROUP_END) {
                if (depth == 0) break;
                depth -= 1;
            }
            end_op += 1;
        }
        return match_at_recursive(end_op, original_string, current_string, nmatch, pmatch, is_ascii);
    }
    if (ops[0].type == META_OP_GROUP_START) {
        int32 group_id = ops[0].value;
        int32 old_so = -1;
        int32 res;
        if (pmatch != NULL && (size_t)group_id < nmatch) {
            old_so = pmatch[group_id].rm_so;
            pmatch[group_id].rm_so = (int32)(current_string - original_string);
        }
        res = eval_choice_point(ops + 1, original_string, current_string, nmatch, pmatch, is_ascii);
        if (res >= 0) return res;
        if (pmatch != NULL && (size_t)group_id < nmatch) pmatch[group_id].rm_so = old_so;
        return -1;
    }
    if (ops[0].type == META_OP_GROUP_END) {
        int32 group_id = ops[0].value;
        int32 old_eo = -1;
        int32 res;
        if (pmatch != NULL && (size_t)group_id < nmatch) {
            old_eo = pmatch[group_id].rm_eo;
            pmatch[group_id].rm_eo = (int32)(current_string - original_string);
        }
        res = match_at_recursive(ops + 1, original_string, current_string, nmatch, pmatch, is_ascii);
        if (res >= 0) return res;
        if (pmatch != NULL && (size_t)group_id < nmatch) pmatch[group_id].rm_eo = old_eo;
        return -1;
    }
    {
        int32 is_star = (ops[1].type == META_OP_STAR);
        int32 is_plus = (ops[1].type == META_OP_PLUS);
        int32 is_opt  = (ops[1].type == META_OP_OPTIONAL);
        int32 is_bound = (ops[1].type == META_OP_BOUNDED);
        if (is_star || is_plus || is_opt || is_bound) {
            MetaOp token = ops[0];
            MetaOp *next_ops = ops + 2;
            char *s = current_string;
            char *max_s;
            regmatch_t pmatch_backup[32];
            int32 min_req = 0;
            int32 max_req = -1;
            int32 consumed = 0;
            if (is_star) { min_req = 0; max_req = -1; }
            else if (is_plus) { min_req = 1; max_req = -1; }
            else if (is_opt) { min_req = 0; max_req = 1; }
            else if (is_bound) { min_req = ops[1].min; max_req = ops[1].max; }
            int32 count = 0;
            while (count < min_req) {
                if (!matches_char(token, s, &consumed, is_ascii)) return -1;
                s += consumed;
                count += 1;
            }
            max_s = s;
            while ((max_req == -1 || count < max_req) && matches_char(token, max_s, &consumed, is_ascii)) {
                max_s += consumed;
                count += 1;
            }
            if (pmatch != NULL) {
                size_t copy_size = (nmatch > 32) ? 32 : nmatch;
                for (size_t i = 0; i < copy_size; i += 1) pmatch_backup[i] = pmatch[i];
            }
            while (max_s >= s) {
                int32 res = match_at_recursive(next_ops, original_string, max_s, nmatch, pmatch, is_ascii);
                if (res >= 0) return res;
                if (pmatch != NULL) {
                    size_t copy_size = (nmatch > 32) ? 32 : nmatch;
                    for (size_t i = 0; i < copy_size; i += 1) pmatch[i] = pmatch_backup[i];
                }
                if (max_s == s) break;
                if (is_ascii) {
                    max_s -= 1;
                } else {
                    char *prev = max_s - 1;
                    while (prev > s && (*(uchar *)prev & 0xC0) == 0x80) prev -= 1;
                    max_s = prev;
                }
            }
            return -1;
        }
    }
    int32 consumed = 0;
    if (matches_char(ops[0], current_string, &consumed, is_ascii)) return match_at_recursive(ops + 1, original_string, current_string + consumed, nmatch, pmatch, is_ascii);
    return -1;
}

static int
meta_regex_match(MetaRegex *regex, char *string, size_t nmatch, regmatch_t pmatch[]) {
    if (regex == NULL) return REG_NOMATCH;
    int32 is_ascii = regex->is_ascii;

    if (regex->has_start_anchor) {
        if (pmatch != NULL) {
            for (size_t k = 0; k < nmatch; k += 1) { 
                pmatch[k].rm_so = -1; 
                pmatch[k].rm_eo = -1; 
            }
        }
        int32 match_len = eval_choice_point(regex->ops, string, string, nmatch, pmatch, is_ascii);
        if (match_len >= 0) {
            if (regex->has_end_anchor && string[match_len] != '\0') return REG_NOMATCH;
            if (pmatch != NULL && nmatch > 0) { 
                pmatch[0].rm_so = 0; 
                pmatch[0].rm_eo = match_len; 
            }
            return 0;
        }
        return REG_NOMATCH;
    }
    for (int32 j = 0; ; ) {
        if (pmatch != NULL) {
            for (size_t k = 0; k < nmatch; k += 1) { 
                pmatch[k].rm_so = -1; 
                pmatch[k].rm_eo = -1; 
            }
        }
        int32 match_len = eval_choice_point(regex->ops, string, &string[j], nmatch, pmatch, is_ascii);
        if (match_len >= 0) {
            if (!regex->has_end_anchor || string[match_len] == '\0') {
                if (pmatch != NULL && nmatch > 0) { 
                    pmatch[0].rm_so = j; 
                    pmatch[0].rm_eo = match_len; 
                }
                return 0;
            }
        }
        if (string[j] == '\0') break;
        int32 consumed = 0; 
        if ((uchar)string[j] < 0x80) {
            j += 1;
        } else {
            utf8_decode(&string[j], &consumed);
            j += (consumed > 0) ? consumed : 1;
        }
    }
    return REG_NOMATCH;
}

#endif /* META_REGEX_MATCH_C */
