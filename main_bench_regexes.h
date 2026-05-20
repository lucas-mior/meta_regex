#if !defined(META_BENCH_REGEXES_H)
#define META_BENCH_REGEXES_H

#include "meta.h"

typedef enum BenchRegexLengthClass {
    BENCH_LEN_1_8,
    BENCH_LEN_9_16,
    BENCH_LEN_17_32,
    BENCH_LEN_33_64,
    BENCH_LEN_LAST,
} BenchRegexLengthClass;

typedef enum BenchRegexFeatureClass {
    BENCH_FEATURE_NO_BACKREFS,
    BENCH_FEATURE_ALL,
    BENCH_FEATURE_LAST,
} BenchRegexFeatureClass;

typedef struct BenchRegexCase {
    char *name;
    MetaRegex *regex;
    int32 regex_len;
    enum BenchRegexLengthClass length_class;
    enum BenchRegexFeatureClass feature_class;
} BenchRegexCase;

typedef struct BenchRegexBucket {
    char *name;
    enum BenchRegexLengthClass length_class;
    enum BenchRegexFeatureClass feature_class;
    int32 max_regex_len;
    BenchRegexCase *cases;
    int32 count;
} BenchRegexBucket;

static char *
bench_length_class_name(enum BenchRegexLengthClass c) {
    switch (c) {
    case BENCH_LEN_1_8: return "1_8";
    case BENCH_LEN_9_16: return "9_16";
    case BENCH_LEN_17_32: return "17_32";
    case BENCH_LEN_33_64: return "33_64";
    case BENCH_LEN_LAST:
    default: return "unknown_length";
    }
}

static int32
bench_length_class_max(enum BenchRegexLengthClass c) {
    switch (c) {
    case BENCH_LEN_1_8: return 8;
    case BENCH_LEN_9_16: return 16;
    case BENCH_LEN_17_32: return 32;
    case BENCH_LEN_33_64: return 64;
    case BENCH_LEN_LAST:
    default: return 0;
    }
}

static char *
bench_feature_class_name(enum BenchRegexFeatureClass c) {
    switch (c) {
    case BENCH_FEATURE_NO_BACKREFS:
        return "all_except_backreferences";
    case BENCH_FEATURE_ALL:
        return "all_features";
    case BENCH_FEATURE_LAST:
    default:
        return "unknown_features";
    }
}

static BenchRegexCase bench_regex_1_8_regular_no_backrefs[] = {
    { "word_start_a", R("\\<a"), 3, BENCH_LEN_1_8, BENCH_FEATURE_NO_BACKREFS },
    { "word_end_a", R("a\\>"), 3, BENCH_LEN_1_8, BENCH_FEATURE_NO_BACKREFS },
    { "non_word_aa", R("\\Baa"), 4, BENCH_LEN_1_8, BENCH_FEATURE_NO_BACKREFS },
};

static BenchRegexCase bench_regex_9_16_regular_no_backrefs[] = {
    { "whole_word_a_digit", R("\\<a+[0-9]\\>"), 12, BENCH_LEN_9_16, BENCH_FEATURE_NO_BACKREFS },
    { "word_boundary_foo", R("\\bfoo\\b"), 7, BENCH_LEN_9_16, BENCH_FEATURE_NO_BACKREFS },
    { "inner_non_boundary", R("\\Baa+[0-9]"), 10, BENCH_LEN_9_16, BENCH_FEATURE_NO_BACKREFS },
};

static BenchRegexCase bench_regex_17_32_regular_no_backrefs[] = {
    { "word_repeated_pair", R("\\<([a-z][0-9])+\\>"), 17, BENCH_LEN_17_32, BENCH_FEATURE_NO_BACKREFS },
    { "word_animal_alt", R("\\b(foo|bar|baz)\\b"), 17, BENCH_LEN_17_32, BENCH_FEATURE_NO_BACKREFS },
    { "inner_non_boundary", R("\\B[a-z]{2,4}[0-9]?\\B"), 22, BENCH_LEN_17_32, BENCH_FEATURE_NO_BACKREFS },
};

static BenchRegexCase bench_regex_33_64_regular_no_backrefs[] = {
    { "word_hyphen_optional", R("\\<([a-z]+|[0-9]{2,4})(-[a-z]+)?\\>"), 40, BENCH_LEN_33_64, BENCH_FEATURE_NO_BACKREFS },
    { "boundary_space_number", R("\\b(foo|bar|baz)[[:space:]]+[0-9]+\\b"), 38, BENCH_LEN_33_64, BENCH_FEATURE_NO_BACKREFS },
    { "non_boundary_alt_end", R("\\B([abc]{1,3}|[0-9]{1,2})+(end)?\\B"), 41, BENCH_LEN_33_64, BENCH_FEATURE_NO_BACKREFS },
};

static BenchRegexCase bench_regex_1_8_with_backrefs[] = {
    { "small_backref", R("(a)\\1"), 5, BENCH_LEN_1_8, BENCH_FEATURE_ALL },
    { "word_backref_start", R("\\<(a)\\1"), 7, BENCH_LEN_1_8, BENCH_FEATURE_ALL },
    { "alt_backref", R("(a|b)\\1"), 7, BENCH_LEN_1_8, BENCH_FEATURE_ALL },
};

static BenchRegexCase bench_regex_9_16_with_backrefs[] = {
    { "word_backref", R("\\<(a)\\1\\>"), 9, BENCH_LEN_9_16, BENCH_FEATURE_ALL },
    { "class_backref_plus", R("([ab])\\1+"), 9, BENCH_LEN_9_16, BENCH_FEATURE_ALL },
    { "digit_backref", R("^([0-9])\\1$"), 11, BENCH_LEN_9_16, BENCH_FEATURE_ALL },
};

static BenchRegexCase bench_regex_17_32_with_backrefs[] = {
    { "word_cap_backref", R("\\<([A-Z][a-z])\\1\\>"), 18, BENCH_LEN_17_32, BENCH_FEATURE_ALL },
    { "digits_backref_suffix", R("^([0-9]{2})-\\1([a-z]?)$"), 25, BENCH_LEN_17_32, BENCH_FEATURE_ALL },
    { "boundary_alt_backref", R("\\b(foo|bar) \\1(x?)\\b"), 22, BENCH_LEN_17_32, BENCH_FEATURE_ALL },
};

static BenchRegexCase bench_regex_33_64_with_backrefs[] = {
    { "word_alt_backref", R("\\<([a-z]+|[0-9]{2,4})[- ]\\1([_-][a-z0-9]{0,4})?\\>"), 62, BENCH_LEN_33_64, BENCH_FEATURE_ALL },
    { "name_space_backref", R("^([A-Z][a-z]+)[[:space:]]+\\1([.]?)$"), 40, BENCH_LEN_33_64, BENCH_FEATURE_ALL },
    { "boundary_alt_xx_backref", R("\\b([abc]{1,3}|[0-9]{1,2})xx\\1([z]?)\\b"), 45, BENCH_LEN_33_64, BENCH_FEATURE_ALL },
};

#define BENCH_REGEX_BUCKET(ARRAY, LENGTH_CLASS, FEATURE_CLASS, MAX_REGEX_LEN) \
    { \
        .name = #ARRAY, \
        .length_class = LENGTH_CLASS, \
        .feature_class = FEATURE_CLASS, \
        .max_regex_len = MAX_REGEX_LEN, \
        .cases = ARRAY, \
        .count = LENGTH(ARRAY), \
    }

static BenchRegexBucket bench_regex_buckets[] = {
    BENCH_REGEX_BUCKET(bench_regex_1_8_regular_no_backrefs,
                       BENCH_LEN_1_8, BENCH_FEATURE_NO_BACKREFS, 8),
    BENCH_REGEX_BUCKET(bench_regex_9_16_regular_no_backrefs,
                       BENCH_LEN_9_16, BENCH_FEATURE_NO_BACKREFS, 16),
    BENCH_REGEX_BUCKET(bench_regex_17_32_regular_no_backrefs,
                       BENCH_LEN_17_32, BENCH_FEATURE_NO_BACKREFS, 32),
    BENCH_REGEX_BUCKET(bench_regex_33_64_regular_no_backrefs,
                       BENCH_LEN_33_64, BENCH_FEATURE_NO_BACKREFS, 64),

    BENCH_REGEX_BUCKET(bench_regex_1_8_with_backrefs, BENCH_LEN_1_8,
                       BENCH_FEATURE_ALL, 8),
    BENCH_REGEX_BUCKET(bench_regex_9_16_with_backrefs, BENCH_LEN_9_16,
                       BENCH_FEATURE_ALL, 16),
    BENCH_REGEX_BUCKET(bench_regex_17_32_with_backrefs, BENCH_LEN_17_32,
                       BENCH_FEATURE_ALL, 32),
    BENCH_REGEX_BUCKET(bench_regex_33_64_with_backrefs, BENCH_LEN_33_64,
                       BENCH_FEATURE_ALL, 64),
};

#undef BENCH_REGEX_BUCKET
#define BENCH_REGEX_BUCKET_COUNT LENGTH(bench_regex_buckets)

#endif /* META_BENCH_REGEXES_H */
