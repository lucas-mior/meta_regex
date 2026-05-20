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
        return "no_backreferences";
    case BENCH_FEATURE_ALL:
        return "with_backreferences";
    case BENCH_FEATURE_LAST:
    default:
        return "unknown_features";
    }
}

static BenchRegexCase bench_regex_cases[] = {
    { "bench_re_0000", R("a0"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0001", R("[a-z]"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0002", R("[0-9]{3}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0003", R("abd?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0004", R("x[abc]4"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0005", R("a5"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0006", R("[a-z]+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0007", R("[0-9]{2}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0008", R("abi?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0009", R("x[abc]9"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0010", R("a0"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0011", R("[a-z]"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0012", R("[0-9]{1}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0013", R("abn?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0014", R("x[abc]4"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0015", R("a5"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0016", R("[a-z]"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0017", R("[0-9]{3}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0018", R("abs?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0019", R("x[abc]9"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0020", R("a0"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0021", R("[a-z]+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0022", R("[0-9]{2}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0023", R("abx?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0024", R("x[abc]4"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0025", R("a5"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0026", R("[a-z]"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0027", R("[0-9]{1}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0028", R("abc?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0029", R("x[abc]9"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0030", R("a0"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0031", R("[a-z]"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0032", R("[0-9]{3}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0033", R("abh?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0034", R("x[abc]4"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0035", R("a5"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0036", R("[a-z]+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0037", R("[0-9]{2}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0038", R("abm?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0039", R("x[abc]9"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0040", R("a0"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0041", R("[a-z]"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0042", R("[0-9]{1}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0043", R("abr?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0044", R("x[abc]4"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0045", R("a5"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0046", R("[a-z]"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0047", R("[0-9]{3}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0048", R("abw?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0049", R("x[abc]9"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0050", R("a0"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0051", R("[a-z]+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0052", R("[0-9]{2}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0053", R("abb?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0054", R("x[abc]4"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0055", R("a5"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0056", R("[a-z]"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0057", R("[0-9]{1}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0058", R("abg?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0059", R("x[abc]9"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0060", R("a0"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0061", R("[a-z]"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0062", R("[0-9]{3}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0063", R("abl?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0064", R("x[abc]4"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0065", R("a5"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0066", R("[a-z]+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0067", R("[0-9]{2}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0068", R("abq?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0069", R("x[abc]9"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0070", R("a0"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0071", R("[a-z]"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0072", R("[0-9]{1}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0073", R("abv?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0074", R("x[abc]4"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0075", R("a5"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0076", R("[a-z]"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0077", R("[0-9]{3}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0078", R("aba?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0079", R("x[abc]9"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0080", R("a0"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0081", R("[a-z]+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0082", R("[0-9]{2}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0083", R("abf?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0084", R("x[abc]4"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0085", R("a5"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0086", R("[a-z]"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0087", R("[0-9]{1}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0088", R("abk?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0089", R("x[abc]9"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0090", R("a0"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0091", R("[a-z]"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0092", R("[0-9]{3}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0093", R("abp?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0094", R("x[abc]4"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0095", R("a5"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0096", R("[a-z]+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0097", R("[0-9]{2}"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0098", R("abu?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0099", R("x[abc]9"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0100", R("(foo|baz)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0101", R("(bar|qux|zap)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0102", R("[A-Za-z]+(baz|zip)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0103", R("(qux)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0104", R("(zip|[0-9]{2})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0105", R("(zap|bar)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0106", R("(foo|baz|zip)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0107", R("[A-Za-z]+(bar|qux)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0108", R("(baz)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0109", R("(qux|[0-9]{1})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0110", R("(zip|foo)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0111", R("(zap|bar|qux)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0112", R("[A-Za-z]+(foo|baz)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0113", R("(bar)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0114", R("(baz|[0-9]{3})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0115", R("(qux|zap)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0116", R("(zip|foo|baz)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0117", R("[A-Za-z]+(zap|bar)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0118", R("(foo)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0119", R("(bar|[0-9]{2})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0120", R("(baz|zip)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0121", R("(qux|zap|bar)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0122", R("[A-Za-z]+(zip|foo)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0123", R("(zap)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0124", R("(foo|[0-9]{1})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0125", R("(bar|qux)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0126", R("(baz|zip|foo)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0127", R("[A-Za-z]+(qux|zap)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0128", R("(zip)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0129", R("(zap|[0-9]{3})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0130", R("(foo|baz)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0131", R("(bar|qux|zap)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0132", R("[A-Za-z]+(baz|zip)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0133", R("(qux)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0134", R("(zip|[0-9]{2})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0135", R("(zap|bar)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0136", R("(foo|baz|zip)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0137", R("[A-Za-z]+(bar|qux)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0138", R("(baz)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0139", R("(qux|[0-9]{1})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0140", R("(zip|foo)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0141", R("(zap|bar|qux)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0142", R("[A-Za-z]+(foo|baz)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0143", R("(bar)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0144", R("(baz|[0-9]{3})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0145", R("(qux|zap)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0146", R("(zip|foo|baz)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0147", R("[A-Za-z]+(zap|bar)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0148", R("(foo)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0149", R("(bar|[0-9]{2})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0150", R("(baz|zip)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0151", R("(qux|zap|bar)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0152", R("[A-Za-z]+(zip|foo)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0153", R("(zap)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0154", R("(foo|[0-9]{1})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0155", R("(bar|qux)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0156", R("(baz|zip|foo)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0157", R("[A-Za-z]+(qux|zap)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0158", R("(zip)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0159", R("(zap|[0-9]{3})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0160", R("(foo|baz)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0161", R("(bar|qux|zap)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0162", R("[A-Za-z]+(baz|zip)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0163", R("(qux)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0164", R("(zip|[0-9]{2})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0165", R("(zap|bar)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0166", R("(foo|baz|zip)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0167", R("[A-Za-z]+(bar|qux)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0168", R("(baz)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0169", R("(qux|[0-9]{1})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0170", R("(zip|foo)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0171", R("(zap|bar|qux)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0172", R("[A-Za-z]+(foo|baz)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0173", R("(bar)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0174", R("(baz|[0-9]{3})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0175", R("(qux|zap)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0176", R("(zip|foo|baz)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0177", R("[A-Za-z]+(zap|bar)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0178", R("(foo)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0179", R("(bar|[0-9]{2})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0180", R("(baz|zip)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0181", R("(qux|zap|bar)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0182", R("[A-Za-z]+(zip|foo)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0183", R("(zap)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0184", R("(foo|[0-9]{1})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0185", R("(bar|qux)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0186", R("(baz|zip|foo)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0187", R("[A-Za-z]+(qux|zap)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0188", R("(zip)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0189", R("(zap|[0-9]{3})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0190", R("(foo|baz)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0191", R("(bar|qux|zap)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0192", R("[A-Za-z]+(baz|zip)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0193", R("(qux)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0194", R("(zip|[0-9]{2})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0195", R("(zap|bar)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0196", R("(foo|baz|zip)[0-9]?"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0197", R("[A-Za-z]+(bar|qux)"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0198", R("(baz)+"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0199", R("(qux|[0-9]{1})"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0200", R("^foo[0-9]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0201", R("\\bbar\\b"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0202", R("\\<baz[0-9]?\\>"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0203", R("^[[:space:]]*alpha[[:space:]]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0204", R("\\B[a-z]{2}[0-9]?\\B"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0205", R("^foo[0-9]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0206", R("\\bbar\\b"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0207", R("\\<baz[0-9]?\\>"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0208", R("^[[:space:]]*alpha[[:space:]]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0209", R("\\B[a-z]{3}[0-9]?\\B"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0210", R("^foo[0-9]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0211", R("\\bbar\\b"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0212", R("\\<baz[0-9]?\\>"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0213", R("^[[:space:]]*alpha[[:space:]]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0214", R("\\B[a-z]{4}[0-9]?\\B"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0215", R("^foo[0-9]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0216", R("\\bbar\\b"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0217", R("\\<baz[0-9]?\\>"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0218", R("^[[:space:]]*alpha[[:space:]]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0219", R("\\B[a-z]{5}[0-9]?\\B"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0220", R("^foo[0-9]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0221", R("\\bbar\\b"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0222", R("\\<baz[0-9]?\\>"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0223", R("^[[:space:]]*alpha[[:space:]]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0224", R("\\B[a-z]{2}[0-9]?\\B"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0225", R("^foo[0-9]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0226", R("\\bbar\\b"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0227", R("\\<baz[0-9]?\\>"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0228", R("^[[:space:]]*alpha[[:space:]]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0229", R("\\B[a-z]{3}[0-9]?\\B"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0230", R("^foo[0-9]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0231", R("\\bbar\\b"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0232", R("\\<baz[0-9]?\\>"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0233", R("^[[:space:]]*alpha[[:space:]]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0234", R("\\B[a-z]{4}[0-9]?\\B"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0235", R("^foo[0-9]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0236", R("\\bbar\\b"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0237", R("\\<baz[0-9]?\\>"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0238", R("^[[:space:]]*alpha[[:space:]]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0239", R("\\B[a-z]{5}[0-9]?\\B"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0240", R("^foo[0-9]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0241", R("\\bbar\\b"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0242", R("\\<baz[0-9]?\\>"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0243", R("^[[:space:]]*alpha[[:space:]]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0244", R("\\B[a-z]{2}[0-9]?\\B"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0245", R("^foo[0-9]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0246", R("\\bbar\\b"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0247", R("\\<baz[0-9]?\\>"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0248", R("^[[:space:]]*alpha[[:space:]]*$"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
    { "bench_re_0249", R("\\B[a-z]{3}[0-9]?\\B"), 0, BENCH_LEN_LAST, BENCH_FEATURE_LAST },
};

#endif /* META_BENCH_REGEXES_H */
