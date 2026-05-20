#if !defined(META_BENCH_REGEXES_H)
#define META_BENCH_REGEXES_H

#include "meta.h"

typedef enum BenchRegexLengthClass {
    BENCH_LEN_1_16,
    BENCH_LEN_17_32,
    BENCH_LEN_33_64,
    BENCH_LEN_LAST,
} BenchRegexLengthClass;

typedef enum BenchRegexFeatureClass {
    BENCH_FEATURE_NO_WORD_BOUNDARY_NO_BACKREF,
    BENCH_FEATURE_NO_BACKREF,
    BENCH_FEATURE_ALL,
    BENCH_FEATURE_LAST,
} BenchRegexFeatureClass;

typedef struct BenchRegexCase {
    char *name;
    char *input;
    MetaRegex *regex;
    int32 regex_len;
    enum BenchRegexLengthClass length_class;
    enum BenchRegexFeatureClass feature_class;
} BenchRegexCase;

typedef struct BenchRegexBucket {
    char *name;
    enum BenchRegexLengthClass length_class;
    enum BenchRegexFeatureClass feature_class;
    BenchRegexCase *cases;
    int32 count;
} BenchRegexBucket;

static char *bench_length_class_name(enum BenchRegexLengthClass c) {
    switch (c) {
    case BENCH_LEN_1_16: return "1_16";
    case BENCH_LEN_17_32: return "17_32";
    case BENCH_LEN_33_64: return "33_64";
    case BENCH_LEN_LAST:
    default: return "unknown_length";
    }
}

static char *bench_feature_class_name(enum BenchRegexFeatureClass c) {
    switch (c) {
    case BENCH_FEATURE_NO_WORD_BOUNDARY_NO_BACKREF:
        return "all_except_word_boundaries_and_backreferences";
    case BENCH_FEATURE_NO_BACKREF:
        return "all_except_backreferences";
    case BENCH_FEATURE_ALL:
        return "all_features";
    case BENCH_FEATURE_LAST:
    default:
        return "unknown_features";
    }
}


static BenchRegexCase bench_regex_1_16_regular_no_word_boundary_no_backref[] = {
    { "literal_one", "a", R("a"), 1, BENCH_LEN_1_16, BENCH_FEATURE_NO_WORD_BOUNDARY_NO_BACKREF },
    { "small_class", "abcxyz", R("[a-z]+"), 6, BENCH_LEN_1_16, BENCH_FEATURE_NO_WORD_BOUNDARY_NO_BACKREF },
    { "small_alt", "abcdab", R("(ab|cd)+"), 8, BENCH_LEN_1_16, BENCH_FEATURE_NO_WORD_BOUNDARY_NO_BACKREF },
};

static BenchRegexCase bench_regex_17_32_regular_no_word_boundary_no_backref[] = {
    { "anchored_alt", "abcxyz", R("^([a-z]+|[0-9]+)$"), 17, BENCH_LEN_17_32, BENCH_FEATURE_NO_WORD_BOUNDARY_NO_BACKREF },
    { "capital_words", "HelloWorld", R("([A-Z][a-z]*){1,3}"), 18, BENCH_LEN_17_32, BENCH_FEATURE_NO_WORD_BOUNDARY_NO_BACKREF },
    { "dot_alt_digit", "axxfoo7", R("a.*(foo|bar)[0-9]"), 17, BENCH_LEN_17_32, BENCH_FEATURE_NO_WORD_BOUNDARY_NO_BACKREF },
};

static BenchRegexCase bench_regex_33_64_regular_no_word_boundary_no_backref[] = {
    { "name_or_number_dash", "Smith-ab", R("^([A-Z][a-z]+|[0-9]{2,4})(-[a-z]{1,3})?$"), 40, BENCH_LEN_33_64, BENCH_FEATURE_NO_WORD_BOUNDARY_NO_BACKREF },
    { "csv_words_numbers", "ab.cd,123,xy", R("([a-z]+\\.[a-z]+|[0-9]+)(,([a-z]+|[0-9]+))*"), 42, BENCH_LEN_33_64, BENCH_FEATURE_NO_WORD_BOUNDARY_NO_BACKREF },
    { "space_number_suffix", "foo 123A", R("^(foo|bar|baz)[[:space:]]+[0-9]{2,4}[A-Z]?$"), 43, BENCH_LEN_33_64, BENCH_FEATURE_NO_WORD_BOUNDARY_NO_BACKREF },
};

static BenchRegexCase bench_regex_1_16_regular_with_word_boundary_no_backref[] = {
    { "word_left_right", "a", R("\\<a\\>"), 5, BENCH_LEN_1_16, BENCH_FEATURE_NO_BACKREF },
    { "word_boundary", "a", R("\\ba\\b"), 5, BENCH_LEN_1_16, BENCH_FEATURE_NO_BACKREF },
    { "non_word_boundary", "baaa", R("\\Baa"), 4, BENCH_LEN_1_16, BENCH_FEATURE_NO_BACKREF },
};

static BenchRegexCase bench_regex_17_32_regular_with_word_boundary_no_backref[] = {
    { "word_repeated_pair", "a1b2", R("\\<([a-z][0-9])+\\>"), 17, BENCH_LEN_17_32, BENCH_FEATURE_NO_BACKREF },
    { "word_animal_alt", "bar", R("\\b(foo|bar|baz)\\b"), 17, BENCH_LEN_17_32, BENCH_FEATURE_NO_BACKREF },
    { "inner_non_boundary", "xaa7z", R("\\B[a-z]{2,4}[0-9]?\\B"), 20, BENCH_LEN_17_32, BENCH_FEATURE_NO_BACKREF },
};

static BenchRegexCase bench_regex_33_64_regular_with_word_boundary_no_backref[] = {
    { "word_hyphen_optional", "abc-def", R("\\<([a-z]+|[0-9]{2,4})(-[a-z]+)?\\>"), 33, BENCH_LEN_33_64, BENCH_FEATURE_NO_BACKREF },
    { "boundary_space_number", "foo 123", R("\\b(foo|bar|baz)[[:space:]]+[0-9]+\\b"), 35, BENCH_LEN_33_64, BENCH_FEATURE_NO_BACKREF },
    { "non_boundary_alt_end", "xaaendz", R("\\B([abc]{1,3}|[0-9]{1,2})+(end)?\\B"), 34, BENCH_LEN_33_64, BENCH_FEATURE_NO_BACKREF },
};

static BenchRegexCase bench_regex_1_16_all_features[] = {
    { "small_backref", "aa", R("(a)\\1"), 5, BENCH_LEN_1_16, BENCH_FEATURE_ALL },
    { "word_backref", "aa", R("\\<(a)\\1\\>"), 9, BENCH_LEN_1_16, BENCH_FEATURE_ALL },
    { "alt_backref", "bb", R("(a|b)\\1"), 7, BENCH_LEN_1_16, BENCH_FEATURE_ALL },
};

static BenchRegexCase bench_regex_17_32_all_features[] = {
    { "word_cap_backref", "AbAb", R("\\<([A-Z][a-z])\\1\\>"), 18, BENCH_LEN_17_32, BENCH_FEATURE_ALL },
    { "digits_backref_suffix", "12-12a", R("^([0-9]{2})-\\1([a-z]?)$"), 23, BENCH_LEN_17_32, BENCH_FEATURE_ALL },
    { "boundary_alt_backref", "foo foox", R("\\b(foo|bar) \\1(x?)\\b"), 20, BENCH_LEN_17_32, BENCH_FEATURE_ALL },
};

static BenchRegexCase bench_regex_33_64_all_features[] = {
    { "word_alt_backref", "abc-abc", R("\\<([a-z]+|[0-9]{2,4})[- ]\\1([_-][a-z0-9]{0,4})?\\>"), 49, BENCH_LEN_33_64, BENCH_FEATURE_ALL },
    { "name_space_backref", "Smith Smith.", R("^([A-Z][a-z]+)[[:space:]]+\\1([.]?)$"), 35, BENCH_LEN_33_64, BENCH_FEATURE_ALL },
    { "boundary_alt_xx_backref", "abcxxabcz", R("\\b([abc]{1,3}|[0-9]{1,2})xx\\1([z]?)\\b"), 37, BENCH_LEN_33_64, BENCH_FEATURE_ALL },
};

#define BENCH_REGEX_BUCKET(ARRAY, LENGTH_CLASS, FEATURE_CLASS) \
    { \
        .name = #ARRAY, \
        .length_class = LENGTH_CLASS, \
        .feature_class = FEATURE_CLASS, \
        .cases = ARRAY, \
        .count = LENGTH(ARRAY), \
    }

static BenchRegexBucket bench_regex_buckets[] = {
    BENCH_REGEX_BUCKET(bench_regex_1_16_regular_no_word_boundary_no_backref,
                       BENCH_LEN_1_16,
                       BENCH_FEATURE_NO_WORD_BOUNDARY_NO_BACKREF),
    BENCH_REGEX_BUCKET(bench_regex_17_32_regular_no_word_boundary_no_backref,
                       BENCH_LEN_17_32,
                       BENCH_FEATURE_NO_WORD_BOUNDARY_NO_BACKREF),
    BENCH_REGEX_BUCKET(bench_regex_33_64_regular_no_word_boundary_no_backref,
                       BENCH_LEN_33_64,
                       BENCH_FEATURE_NO_WORD_BOUNDARY_NO_BACKREF),

    BENCH_REGEX_BUCKET(bench_regex_1_16_regular_with_word_boundary_no_backref,
                       BENCH_LEN_1_16, BENCH_FEATURE_NO_BACKREF),
    BENCH_REGEX_BUCKET(bench_regex_17_32_regular_with_word_boundary_no_backref,
                       BENCH_LEN_17_32, BENCH_FEATURE_NO_BACKREF),
    BENCH_REGEX_BUCKET(bench_regex_33_64_regular_with_word_boundary_no_backref,
                       BENCH_LEN_33_64, BENCH_FEATURE_NO_BACKREF),

    BENCH_REGEX_BUCKET(bench_regex_1_16_all_features, BENCH_LEN_1_16,
                       BENCH_FEATURE_ALL),
    BENCH_REGEX_BUCKET(bench_regex_17_32_all_features, BENCH_LEN_17_32,
                       BENCH_FEATURE_ALL),
    BENCH_REGEX_BUCKET(bench_regex_33_64_all_features, BENCH_LEN_33_64,
                       BENCH_FEATURE_ALL),
};

#undef BENCH_REGEX_BUCKET
#define BENCH_REGEX_BUCKET_COUNT LENGTH(bench_regex_buckets)

#endif /* META_BENCH_REGEXES_H */
