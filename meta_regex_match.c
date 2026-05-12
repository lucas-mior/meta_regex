#if !defined(META_REGEX_MATCH_C)
#define META_REGEX_MATCH_C

#include <regex.h>
#include "meta_regex.h"
#include "util.c"

static int
match_at(MetaOp *ops, char *string) {
    int32 i = 0;

    while (ops[i].type != META_OP_END) {
        if (string[i] == '\0') {
            return 0;
        }
        if (ops[i].type == META_OP_ANY) {
            // Do nothing
        } else if (ops[i].type == META_OP_LITERAL) {
            if (string[i] != ops[i].value) {
                return 0;
            }
        } else if (ops[i].type == META_OP_DIGIT) {
            if (string[i] < '0') {
                return 0;
            }
            if (string[i] > '9') {
                return 0;
            }
        } else if (ops[i].type == META_OP_ALPHA_LOWER) {
            if (string[i] < 'a') {
                return 0;
            }
            if (string[i] > 'z') {
                return 0;
            }
        } else if (ops[i].type == META_OP_ALPHA_UPPER) {
            if (string[i] < 'A') {
                return 0;
            }
            if (string[i] > 'Z') {
                return 0;
            }
        }
        i += 1;
    }
    return 1;
}

static int
meta_regex_match(MetaRegex regex, char *string) {
    if (regex.has_start_anchor) {
        if (match_at(regex.ops, string)) {
            if (regex.has_end_anchor) {
                int32 len = 0;

                while (regex.ops[len].type != META_OP_END) {
                    len += 1;
                }
                if (string[len] == '\0') {
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
        if (match_at(regex.ops, &string[j])) {
            if (regex.has_end_anchor) {
                int32 len = 0;

                while (regex.ops[len].type != META_OP_END) {
                    len += 1;
                }
                if (string[j + len] == '\0') {
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
