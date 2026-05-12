#if !defined(META_REGEX_MATCH_C)
#define META_REGEX_MATCH_C

#include <regex.h>
#include "meta_regex.h"
#include "util.c"

static int
match_at(MetaOp *ops, char *original_string, char *current_string, size_t nmatch, regmatch_t pmatch[]) {
    int32 op_idx = 0;
    int32 str_idx = 0;

    while (ops[op_idx].type != META_OP_END) {
        if (ops[op_idx].type == META_OP_GROUP_START) {
            int32 group_id = ops[op_idx].value;

            if (pmatch != NULL) {
                if (group_id >= 0) {
                    if ((size_t)group_id < nmatch) {
                        pmatch[group_id].rm_so = (int32)((current_string + str_idx) - original_string);
                    }
                }
            }
            op_idx += 1;
            continue;
        }
        if (ops[op_idx].type == META_OP_GROUP_END) {
            int32 group_id = ops[op_idx].value;

            if (pmatch != NULL) {
                if (group_id >= 0) {
                    if ((size_t)group_id < nmatch) {
                        pmatch[group_id].rm_eo = (int32)((current_string + str_idx) - original_string);
                    }
                }
            }
            op_idx += 1;
            continue;
        }

        {
            char c = current_string[str_idx];

            if (c == '\0') {
                return -1;
            }
            if (ops[op_idx].type == META_OP_ANY) {
                // Do nothing
            } else if (ops[op_idx].type == META_OP_LITERAL) {
                if (c != ops[op_idx].value) {
                    return -1;
                }
            } else if (ops[op_idx].type == META_OP_DIGIT) {
                if (c < '0') {
                    return -1;
                }
                if (c > '9') {
                    return -1;
                }
            } else if (ops[op_idx].type == META_OP_ALPHA_LOWER) {
                if (c < 'a') {
                    return -1;
                }
                if (c > 'z') {
                    return -1;
                }
            } else if (ops[op_idx].type == META_OP_ALPHA_UPPER) {
                if (c < 'A') {
                    return -1;
                }
                if (c > 'Z') {
                    return -1;
                }
            }
        }
        str_idx += 1;
        op_idx += 1;
    }

    if (pmatch != NULL) {
        if (nmatch > 0) {
            pmatch[0].rm_so = (int32)(current_string - original_string);
            pmatch[0].rm_eo = (int32)((current_string + str_idx) - original_string);
        }
    }
    return str_idx;
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
        match_len = match_at(regex.ops, string, string, nmatch, pmatch);
        if (match_len >= 0) {
            if (regex.has_end_anchor) {
                if (string[match_len] == '\0') {
                    return 0;
                } else {
                    return REG_NOMATCH;
                }
            }
            return 0;
        }
        return REG_NOMATCH;
    }

    for (int32 j = 0; string[j] != '\0'; j += 1) {
        int32 match_len;

        if (pmatch != NULL) {
            for (size_t k = 0; k < nmatch; k += 1) {
                pmatch[k].rm_so = -1;
                pmatch[k].rm_eo = -1;
            }
        }
        match_len = match_at(regex.ops, string, &string[j], nmatch, pmatch);
        if (match_len >= 0) {
            if (regex.has_end_anchor) {
                if (string[j + match_len] == '\0') {
                    return 0;
                }
            } else {
                return 0;
            }
        }
    }
    return REG_NOMATCH;
}

#endif /* META_REGEX_MATCH_C */
