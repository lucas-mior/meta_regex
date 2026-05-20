#if !defined(META_BENCH_INPUTS_H)
#define META_BENCH_INPUTS_H

#include "meta.h"

typedef enum BenchInputLengthClass {
    BENCH_INPUT_LEN_0_8,
    BENCH_INPUT_LEN_9_16,
    BENCH_INPUT_LEN_17_32,
    BENCH_INPUT_LEN_33_64,
    BENCH_INPUT_LEN_65_128,
    BENCH_INPUT_LEN_129_256,
    BENCH_INPUT_LEN_257_512,
    BENCH_INPUT_LEN_513_1024,
    BENCH_INPUT_LEN_LAST,
} BenchInputLengthClass;

typedef struct BenchInputCase {
    char *name;
    char *input;
    enum BenchInputLengthClass length_class;
} BenchInputCase;

typedef struct BenchInputBucket {
    char *name;
    enum BenchInputLengthClass length_class;
    int32 max_input_len;
    BenchInputCase *cases;
    int32 count;
} BenchInputBucket;

static char *
bench_input_length_class_name(enum BenchInputLengthClass c) {
    switch (c) {
    case BENCH_INPUT_LEN_0_8: return "0_8";
    case BENCH_INPUT_LEN_9_16: return "9_16";
    case BENCH_INPUT_LEN_17_32: return "17_32";
    case BENCH_INPUT_LEN_33_64: return "33_64";
    case BENCH_INPUT_LEN_65_128: return "65_128";
    case BENCH_INPUT_LEN_129_256: return "129_256";
    case BENCH_INPUT_LEN_257_512: return "257_512";
    case BENCH_INPUT_LEN_513_1024: return "513_1024";
    case BENCH_INPUT_LEN_LAST:
    default: return "unknown_input_length";
    }
}

static int32
bench_input_length_class_max(enum BenchInputLengthClass c) {
    switch (c) {
    case BENCH_INPUT_LEN_0_8: return 8;
    case BENCH_INPUT_LEN_9_16: return 16;
    case BENCH_INPUT_LEN_17_32: return 32;
    case BENCH_INPUT_LEN_33_64: return 64;
    case BENCH_INPUT_LEN_65_128: return 128;
    case BENCH_INPUT_LEN_129_256: return 256;
    case BENCH_INPUT_LEN_257_512: return 512;
    case BENCH_INPUT_LEN_513_1024: return 1024;
    case BENCH_INPUT_LEN_LAST:
    default: return 0;
    }
}

static BenchInputCase bench_inputs_0_8[] = {
    { "empty", "", BENCH_INPUT_LEN_0_8 },
    { "single_a", "a", BENCH_INPUT_LEN_0_8 },
    { "double_a", "aa", BENCH_INPUT_LEN_0_8 },
    { "small_alnum", "abc123", BENCH_INPUT_LEN_0_8 },
    { "small_words", "foo bar", BENCH_INPUT_LEN_0_8 },
};

static BenchInputCase bench_inputs_9_16[] = {
    { "alnum9", "abc123xyz", BENCH_INPUT_LEN_9_16 },
    { "words13", "hello foo bar", BENCH_INPUT_LEN_9_16 },
    { "mixed9", "AbAb-12_z", BENCH_INPUT_LEN_9_16 },
    { "words15", "foo bar baz 123", BENCH_INPUT_LEN_9_16 },
};

static BenchInputCase bench_inputs_17_32[] = {
    { "alnum24", "abc123XYZ_abc123XYZ_abc1", BENCH_INPUT_LEN_17_32 },
    { "words31", "foo bar foo bar foo bar foo bar", BENCH_INPUT_LEN_17_32 },
    { "mixed23", "Smith Smith. abc 123 zz", BENCH_INPUT_LEN_17_32 },
    { "pairs16", "a1b2c3d4e5f6g7h8", BENCH_INPUT_LEN_17_32 },
};

static BenchInputCase bench_inputs_33_64[] = {
    { "alnum48", "abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XY", BENCH_INPUT_LEN_33_64 },
    { "words63", "foo bar baz foo bar baz foo bar baz foo bar baz foo bar baz foo", BENCH_INPUT_LEN_33_64 },
    { "mixed52", "Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 ", BENCH_INPUT_LEN_33_64 },
    { "pairs64", "a1b2a1b2a1b2a1b2a1b2a1b2a1b2a1b2a1b2a1b2a1b2a1b2a1b2a1b2a1b2a1b2", BENCH_INPUT_LEN_33_64 },
};

static BenchInputCase bench_inputs_65_128[] = {
    { "alnum96", "abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123", BENCH_INPUT_LEN_65_128 },
    { "words127", "foo bar baz qux foo bar baz qux foo bar baz qux foo bar baz qux foo bar baz qux foo bar baz qux foo bar baz qux foo bar baz qux", BENCH_INPUT_LEN_65_128 },
    { "mixed104", "Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-", BENCH_INPUT_LEN_65_128 },
    { "pairs128", "a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4", BENCH_INPUT_LEN_65_128 },
};

static BenchInputCase bench_inputs_129_256[] = {
    { "alnum192", "abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_ab", BENCH_INPUT_LEN_129_256 },
    { "words255", "foo bar baz qux quux foo bar baz qux quux foo bar baz qux quux foo bar baz qux quux foo bar baz qux quux foo bar baz qux quux foo bar baz qux quux foo bar baz qux quux foo bar baz qux quux foo bar baz qux quux foo bar baz qux quux foo bar baz qux quux foo", BENCH_INPUT_LEN_129_256 },
    { "mixed208", "Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 ab", BENCH_INPUT_LEN_129_256 },
    { "pairs256", "a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4", BENCH_INPUT_LEN_129_256 },
};

static BenchInputCase bench_inputs_257_512[] = {
    { "alnum384", "abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc1", BENCH_INPUT_LEN_257_512 },
    { "words511", "foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corge foo bar baz qux quux corg", BENCH_INPUT_LEN_257_512 },
    { "mixed416", "Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 ", BENCH_INPUT_LEN_257_512 },
    { "pairs512", "a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4", BENCH_INPUT_LEN_257_512 },
};

static BenchInputCase bench_inputs_513_1024[] = {
    { "alnum768", "abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XYZ_abc123XY", BENCH_INPUT_LEN_513_1024 },
    { "words1023", "foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo bar baz qux quux corge grault foo", BENCH_INPUT_LEN_513_1024 },
    { "mixed832", "Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-123 abc_Smith-", BENCH_INPUT_LEN_513_1024 },
    { "pairs1024", "a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4a1b2c3d4", BENCH_INPUT_LEN_513_1024 },
};

#define BENCH_INPUT_BUCKET(ARRAY, LENGTH_CLASS, MAX_INPUT_LEN) \
    { \
        .name = #ARRAY, \
        .length_class = LENGTH_CLASS, \
        .max_input_len = MAX_INPUT_LEN, \
        .cases = ARRAY, \
        .count = LENGTH(ARRAY), \
    }

static BenchInputBucket bench_input_buckets[] = {
    BENCH_INPUT_BUCKET(bench_inputs_0_8, BENCH_INPUT_LEN_0_8, 8),
    BENCH_INPUT_BUCKET(bench_inputs_9_16, BENCH_INPUT_LEN_9_16, 16),
    BENCH_INPUT_BUCKET(bench_inputs_17_32, BENCH_INPUT_LEN_17_32, 32),
    BENCH_INPUT_BUCKET(bench_inputs_33_64, BENCH_INPUT_LEN_33_64, 64),
    BENCH_INPUT_BUCKET(bench_inputs_65_128, BENCH_INPUT_LEN_65_128, 128),
    BENCH_INPUT_BUCKET(bench_inputs_129_256, BENCH_INPUT_LEN_129_256, 256),
    BENCH_INPUT_BUCKET(bench_inputs_257_512, BENCH_INPUT_LEN_257_512, 512),
    BENCH_INPUT_BUCKET(bench_inputs_513_1024, BENCH_INPUT_LEN_513_1024, 1024),
};

#undef BENCH_INPUT_BUCKET
#define BENCH_INPUT_BUCKET_COUNT LENGTH(bench_input_buckets)

#endif /* META_BENCH_INPUTS_H */
