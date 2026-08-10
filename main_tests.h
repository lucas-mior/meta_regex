#if !defined(META_TESTS_H)
#define META_TESTS_H

#include "cbase.h"
#include "meta_regex.h"
#include <regex.h>
#define MAX_MATCHES 10

typedef struct RegexTest {
    char *input;
    MetaRegex *meta_regex;
    int32 result;
    regmatch_t pmatch[MAX_MATCHES];
    bool expected;
    int32 input_len;
} RegexTest;

typedef struct FuzzyTest {
    char *input;
    int32 input_len;
    int32 regex_idx;
    int32 result_libc;
    regmatch_t pmatch_libc[MAX_MATCHES];
    int32 result_meta;
    regmatch_t pmatch_meta[MAX_MATCHES];
} FuzzyTest;

#endif /* META_TESTS_H */
