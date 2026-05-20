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
        return "no_backreferences";
    case BENCH_FEATURE_ALL:
        return "with_backreferences";
    case BENCH_FEATURE_LAST:
    default:
        return "unknown_features";
    }
}

static BenchRegexCase bench_regex_cases[] = {
    { R("a0") },
    { R("[a-z]") },
    { R("[0-9]{3}") },
    { R("abd?") },
    { R("x[a-z]4") },
    { R("a5") },
    { R("[a-z]+") },
    { R("[0-9]{2}") },
    { R("abi?") },
    { R("x[a-z]9") },
    { R("a0") },
    { R("[a-z]") },
    { R("[0-9]{1}") },
    { R("abn?") },
    { R("x[a-z]4") },
    { R("a5") },
    { R("[a-z]") },
    { R("[0-9]{3}") },
    { R("abs?") },
    { R("x[a-z]9") },
    { R("a0") },
    { R("[a-z]+") },
    { R("[0-9]{2}") },
    { R("abx?") },
    { R("x[a-z]4") },
    { R("a5") },
    { R("[a-z]") },
    { R("[0-9]{1}") },
    { R("abc?") },
    { R("x[a-z]9") },
    { R("a0") },
    { R("[a-z]") },
    { R("[0-9]{3}") },
    { R("abh?") },
    { R("x[a-z]4") },
    { R("a5") },
    { R("[a-z]+") },
    { R("[0-9]{2}") },
    { R("abm?") },
    { R("x[a-z]9") },
    { R("a0") },
    { R("[a-z]") },
    { R("[0-9]{1}") },
    { R("abr?") },
    { R("x[a-z]4") },
    { R("a5") },
    { R("[a-z]") },
    { R("[0-9]{3}") },
    { R("abw?") },
    { R("x[a-z]9") },
    { R("a0") },
    { R("[a-z]+") },
    { R("[0-9]{2}") },
    { R("abb?") },
    { R("x[a-z]4") },
    { R("a5") },
    { R("[a-z]") },
    { R("[0-9]{1}") },
    { R("abg?") },
    { R("x[a-z]9") },
    { R("a0") },
    { R("[a-z]") },
    { R("[0-9]{3}") },
    { R("abl?") },
    { R("x[a-z]4") },
    { R("a5") },
    { R("[a-z]+") },
    { R("[0-9]{2}") },
    { R("abq?") },
    { R("x[a-z]9") },
    { R("a0") },
    { R("[a-z]") },
    { R("[0-9]{1}") },
    { R("abv?") },
    { R("x[a-z]4") },
    { R("a5") },
    { R("[a-z]") },
    { R("[0-9]{3}") },
    { R("aba?") },
    { R("x[a-z]9") },
    { R("a0") },
    { R("[a-z]+") },
    { R("[0-9]{2}") },
    { R("abf?") },
    { R("x[a-z]4") },
    { R("a5") },
    { R("[a-z]") },
    { R("[0-9]{1}") },
    { R("abk?") },
    { R("x[a-z]9") },
    { R("a0") },
    { R("[a-z]") },
    { R("[0-9]{3}") },
    { R("abp?") },
    { R("x[a-z]4") },
    { R("a5") },
    { R("[a-z]+") },
    { R("[0-9]{2}") },
    { R("abu?") },
    { R("x[a-z]9") },
    { R("(foo|baz)") },
    { R("(bar|qux|zap)[0-9]?") },
    { R("[A-Za-z]+(baz|zip)") },
    { R("(qux)+") },
    { R("(zip|[0-9]{2})") },
    { R("(zap|bar)") },
    { R("(foo|baz|zip)[0-9]?") },
    { R("[A-Za-z]+(bar|qux)") },
    { R("(baz)+") },
    { R("(qux|[0-9]{1})") },
    { R("(zip|foo)") },
    { R("(zap|bar|qux)[0-9]?") },
    { R("[A-Za-z]+(foo|baz)") },
    { R("(bar)+") },
    { R("(baz|[0-9]{3})") },
    { R("(qux|zap)") },
    { R("(zip|foo|baz)[0-9]?") },
    { R("[A-Za-z]+(zap|bar)") },
    { R("(foo)+") },
    { R("(bar|[0-9]{2})") },
    { R("(baz|zip)") },
    { R("(qux|zap|bar)[0-9]?") },
    { R("[A-Za-z]+(zip|foo)") },
    { R("(zap)+") },
    { R("(foo|[0-9]{1})") },
    { R("(bar|qux)") },
    { R("(baz|zip|foo)[0-9]?") },
    { R("[A-Za-z]+(qux|zap)") },
    { R("(zip)+") },
    { R("(zap|[0-9]{3})") },
    { R("(foo|baz)") },
    { R("(bar|qux|zap)[0-9]?") },
    { R("[A-Za-z]+(baz|zip)") },
    { R("(qux)+") },
    { R("(zip|[0-9]{2})") },
    { R("(zap|bar)") },
    { R("(foo|baz|zip)[0-9]?") },
    { R("[A-Za-z]+(bar|qux)") },
    { R("(baz)+") },
    { R("(qux|[0-9]{1})") },
    { R("(zip|foo)") },
    { R("(zap|bar|qux)[0-9]?") },
    { R("[A-Za-z]+(foo|baz)") },
    { R("(bar)+") },
    { R("(baz|[0-9]{3})") },
    { R("(qux|zap)") },
    { R("(zip|foo|baz)[0-9]?") },
    { R("[A-Za-z]+(zap|bar)") },
    { R("(foo)+") },
    { R("(bar|[0-9]{2})") },
    { R("(baz|zip)") },
    { R("(qux|zap|bar)[0-9]?") },
    { R("[A-Za-z]+(zip|foo)") },
    { R("(zap)+") },
    { R("(foo|[0-9]{1})") },
    { R("(bar|qux)") },
    { R("(baz|zip|foo)[0-9]?") },
    { R("[A-Za-z]+(qux|zap)") },
    { R("(zip)+") },
    { R("(zap|[0-9]{3})") },
    { R("(foo|baz)") },
    { R("(bar|qux|zap)[0-9]?") },
    { R("[A-Za-z]+(baz|zip)") },
    { R("(qux)+") },
    { R("(zip|[0-9]{2})") },
    { R("(zap|bar)") },
    { R("(foo|baz|zip)[0-9]?") },
    { R("[A-Za-z]+(bar|qux)") },
    { R("(baz)+") },
    { R("(qux|[0-9]{1})") },
    { R("(zip|foo)") },
    { R("(zap|bar|qux)[0-9]?") },
    { R("[A-Za-z]+(foo|baz)") },
    { R("(bar)+") },
    { R("(baz|[0-9]{3})") },
    { R("(qux|zap)") },
    { R("(zip|foo|baz)[0-9]?") },
    { R("[A-Za-z]+(zap|bar)") },
    { R("(foo)+") },
    { R("(bar|[0-9]{2})") },
    { R("(baz|zip)") },
    { R("(qux|zap|bar)[0-9]?") },
    { R("[A-Za-z]+(zip|foo)") },
    { R("(zap)+") },
    { R("(foo|[0-9]{1})") },
    { R("(bar|qux)") },
    { R("(baz|zip|foo)[0-9]?") },
    { R("[A-Za-z]+(qux|zap)") },
    { R("(zip)+") },
    { R("(zap|[0-9]{3})") },
    { R("(foo|baz)") },
    { R("(bar|qux|zap)[0-9]?") },
    { R("[A-Za-z]+(baz|zip)") },
    { R("(qux)+") },
    { R("(zip|[0-9]{2})") },
    { R("(zap|bar)") },
    { R("(foo|baz|zip)[0-9]?") },
    { R("[A-Za-z]+(bar|qux)") },
    { R("(baz)+") },
    { R("(qux|[0-9]{1})") },
    { R("^foo[0-9]*$") },
    { R("\\bbar\\b") },
    { R("\\<baz[0-9]?\\>") },
    { R("^[[:space:]]*[[:alpha:]][[:space:]]*$") },
    { R("\\B[a-z]{2}[0-9]?\\B") },
    { R("^foo[0-9]*$") },
    { R("\\bbar\\b") },
    { R("\\<baz[0-9]?\\>") },
    { R("^[[:space:]]*[[:alpha:]][[:space:]]*$") },
    { R("\\B[a-z]{3}[0-9]?\\B") },
    { R("^foo[0-9]*$") },
    { R("\\bbar\\b") },
    { R("\\<baz[0-9]?\\>") },
    { R("^[[:space:]]*[[:alpha:]][[:space:]]*$") },
    { R("\\B[a-z]{4}[0-9]?\\B") },
    { R("^foo[0-9]*$") },
    { R("\\bbar\\b") },
    { R("\\<baz[0-9]?\\>") },
    { R("^[[:space:]]*[[:alpha:]][[:space:]]*$") },
    { R("\\B[a-z]{5}[0-9]?\\B") },
    { R("^foo[0-9]*$") },
    { R("\\bbar\\b") },
    { R("\\<baz[0-9]?\\>") },
    { R("^[[:space:]]*[[:alpha:]][[:space:]]*$") },
    { R("\\B[a-z]{2}[0-9]?\\B") },
    { R("^foo[0-9]*$") },
    { R("\\bbar\\b") },
    { R("\\<baz[0-9]?\\>") },
    { R("^[[:space:]]*[[:alpha:]][[:space:]]*$") },
    { R("\\B[a-z]{3}[0-9]?\\B") },
    { R("^foo[0-9]*$") },
    { R("\\bbar\\b") },
    { R("\\<baz[0-9]?\\>") },
    { R("^[[:space:]]*[[:alpha:]][[:space:]]*$") },
    { R("\\B[a-z]{4}[0-9]?\\B") },
    { R("^foo[0-9]*$") },
    { R("\\bbar\\b") },
    { R("\\<baz[0-9]?\\>") },
    { R("^[[:space:]]*[[:alpha:]][[:space:]]*$") },
    { R("\\B[a-z]{5}[0-9]?\\B") },
    { R("^foo[0-9]*$") },
    { R("\\bbar\\b") },
    { R("\\<baz[0-9]?\\>") },
    { R("^[[:space:]]*[[:alpha:]][[:space:]]*$") },
    { R("\\B[a-z]{2}[0-9]?\\B") },
    { R("^foo[0-9]*$") },
    { R("\\bbar\\b") },
    { R("\\<baz[0-9]?\\>") },
    { R("^[[:space:]]*[[:alpha:]][[:space:]]*$") },
    { R("\\B[a-z]{3}[0-9]?\\B") },
};

#endif /* META_BENCH_REGEXES_H */
