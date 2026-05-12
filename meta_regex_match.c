#if !defined(META_REGEX_MATCH_C)
#define META_REGEX_MATCH_C

#include "meta_regex.h"
#include "util.c"

static int
meta_regex_match(MetaRegex regex, char *string) {
    if (regex.type == META_REGEX_DIGIT) {
        for (int32 j = 0; string[j] != '\0'; j += 1) {
            if (string[j] >= '0') {
                if (string[j] <= '9') {
                    return 0;
                }
            }
        }
    }
    return 1;
}

#endif /* META_REGEX_MATCH_C */
