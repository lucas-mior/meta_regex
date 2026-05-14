#if !defined(META_TESTS_H)
#define META_TESTS_H

#include "meta_regex.h"
#include <regex.h>
#define MAX_MATCHES 10

typedef struct RegexTest {
    char *string;
    MetaRegex *meta_regex;
    int32 result;
    regmatch_t pmatch[MAX_MATCHES];
} RegexTest;

typedef struct FuzzyTest {
    char *string;
    int32 string_size;
    int32 regex_idx;
    int32 result_posix;
    regmatch_t pmatch_posix[MAX_MATCHES];
    int32 result_meta;
    regmatch_t pmatch_meta[MAX_MATCHES];
} FuzzyTest;

#endif /* META_TESTS_H */
