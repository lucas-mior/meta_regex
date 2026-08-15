#define CBASE_IMPLEMENT
#include "cbase.h"

#include <regex.h>

#include "meta_regex.h"
#include "meta_match.c"
#include "gen/main_bench_regexes2.h"

typedef enum BenchInputLengthClass {
    BENCH_INPUT_LEN_0_16,
    BENCH_INPUT_LEN_17_32,
    BENCH_INPUT_LEN_33_64,
    BENCH_INPUT_LEN_65_128,
    BENCH_INPUT_LEN_129_256,
    BENCH_INPUT_LEN_257_512,
    BENCH_INPUT_LEN_513_1024,
    BENCH_INPUT_LEN_1025_2048,
    BENCH_INPUT_LEN_2049_4096,
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
    case BENCH_INPUT_LEN_0_16:
        return "0_16";
    case BENCH_INPUT_LEN_17_32:
        return "17_32";
    case BENCH_INPUT_LEN_33_64:
        return "33_64";
    case BENCH_INPUT_LEN_65_128:
        return "65_128";
    case BENCH_INPUT_LEN_129_256:
        return "129_256";
    case BENCH_INPUT_LEN_257_512:
        return "257_512";
    case BENCH_INPUT_LEN_513_1024:
        return "513_1024";
    case BENCH_INPUT_LEN_1025_2048:
        return "1025_2048";
    case BENCH_INPUT_LEN_2049_4096:
        return "2049_4096";
    case BENCH_INPUT_LEN_LAST:
    default:
        return "unknown_input_length";
    }
}

static int32
bench_input_length_class_min(enum BenchInputLengthClass c) {
    switch (c) {
    case BENCH_INPUT_LEN_0_16:
        return 0;
    case BENCH_INPUT_LEN_17_32:
        return 17;
    case BENCH_INPUT_LEN_33_64:
        return 33;
    case BENCH_INPUT_LEN_65_128:
        return 65;
    case BENCH_INPUT_LEN_129_256:
        return 129;
    case BENCH_INPUT_LEN_257_512:
        return 257;
    case BENCH_INPUT_LEN_513_1024:
        return 513;
    case BENCH_INPUT_LEN_1025_2048:
        return 1025;
    case BENCH_INPUT_LEN_2049_4096:
        return 2049;
    case BENCH_INPUT_LEN_LAST:
    default:
        return 0;
    }
}

static int32
bench_input_length_class_max(enum BenchInputLengthClass c) {
    switch (c) {
    case BENCH_INPUT_LEN_0_16:
        return 16;
    case BENCH_INPUT_LEN_17_32:
        return 32;
    case BENCH_INPUT_LEN_33_64:
        return 64;
    case BENCH_INPUT_LEN_65_128:
        return 128;
    case BENCH_INPUT_LEN_129_256:
        return 256;
    case BENCH_INPUT_LEN_257_512:
        return 512;
    case BENCH_INPUT_LEN_513_1024:
        return 1024;
    case BENCH_INPUT_LEN_1025_2048:
        return 2048;
    case BENCH_INPUT_LEN_2049_4096:
        return 4096;
    case BENCH_INPUT_LEN_LAST:
    default:
        return 0;
    }
}

#if !defined(error2)
#define error2(...) error2(__VA_ARGS__)
#endif

#if !defined(META_BENCH_ITERATIONS)
#define META_BENCH_ITERATIONS 100
#endif

#if !defined(META_BENCH_WARMUP_ITERATIONS)
#define META_BENCH_WARMUP_ITERATIONS 16
#endif

#define BENCH_MAX_MATCHES 16
#define BENCH_MAIN_REGEX_BUCKET_MAX BENCH_LEN_LAST
#define BENCH_RANDOM_INPUT_ATTEMPTS 500
#define BENCH_RANDOM_INPUT_MAX_LEN 4096

#if !defined(META_BENCH_MAX_INPUT_LEN)
#define META_BENCH_MAX_INPUT_LEN 1024
#endif

#if !defined(ENABLE_BTNFA)
#define ENABLE_BTNFA 1
#endif
#if !defined(ENABLE_TNFA)
#define ENABLE_TNFA 0
#endif
#if !defined(ENABLE_TDFA)
#define ENABLE_TDFA 1
#endif
#if !defined(ENABLE_LAZY_DFA)
#define ENABLE_LAZY_DFA 1
#endif
#if !defined(ENABLE_STATIC_DFA)
#define ENABLE_STATIC_DFA 1
#endif

static enum Matcher bench_matchers[] = {
    MATCHER_BTNFA,    MATCHER_TNFA,       MATCHER_TDFA,
    MATCHER_LAZY_DFA, MATCHER_STATIC_DFA,
};

static volatile int32 bench_sink_result;
static volatile int64 bench_sink_offsets;

static double
bench_timediff(struct timespec a, struct timespec b) {
    return (double)(b.tv_sec - a.tv_sec)
           + ((double)(b.tv_nsec - a.tv_nsec) / 1000000000.0);
}

static void
bench_csv_string(FILE *out, char *s) {
    fputc('"', out);
    if (s != NULL) {
        for (char *p = s; *p != '\0'; p += 1) {
            if (*p == '"') {
                fputc('"', out);
                fputc('"', out);
            } else if (*p == '\n') {
                fputs("\\n", out);
            } else if (*p == '\t') {
                fputs("\\t", out);
            } else {
                fputc(*p, out);
            }
        }
    }
    fputc('"', out);
    return;
}

static int32
bench_matcher_compile_enabled(enum Matcher matcher) {
    switch (matcher) {
    case MATCHER_BTNFA:
        return ENABLE_BTNFA;
    case MATCHER_TNFA:
        return ENABLE_TNFA;
    case MATCHER_TDFA:
        return ENABLE_TDFA;
    case MATCHER_LAZY_DFA:
        return ENABLE_LAZY_DFA;
    case MATCHER_STATIC_DFA:
        return ENABLE_STATIC_DFA;
    case MATCHER_LAST:
    case MATCHER_NONE:
    default:
        return 0;
    }
}

static int32
bench_matcher_supports_regex(MetaRegex *regex, enum Matcher matcher,
                             bool extract) {
    if (!bench_matcher_compile_enabled(matcher)) {
        return 0;
    }
    if (regex == NULL) {
        return 0;
    }

    switch (matcher) {
    case MATCHER_BTNFA:
        break;
    case MATCHER_TNFA:
        if (regex->tnfa == NULL) {
            return 0;
        }
        break;
    case MATCHER_TDFA:
        if (regex->tdfa == NULL) {
            return 0;
        }
        break;
    case MATCHER_LAZY_DFA:
        break;
    case MATCHER_STATIC_DFA:
        if (regex->static_dfa == NULL) {
            return 0;
        }
        break;
    case MATCHER_LAST:
    case MATCHER_NONE:
    default:
        return 0;
    }

    if (extract && !matchers[matcher]->extracts) {
        return 0;
    }
    if ((regex->used_ops & ~matchers[matcher]->supports) != 0) {
        return 0;
    }
    return 1;
}

static void
bench_clear_pmatch(regmatch_t *pmatch, int32 pmatch_len) {
    if (pmatch == NULL) {
        return;
    }
    for (int32 i = 0; i < pmatch_len; i += 1) {
        pmatch[i].rm_so = -1;
        pmatch[i].rm_eo = -1;
    }
    return;
}

static int32
bench_pmatch_mismatch(regmatch_t *reference, regmatch_t *actual,
                      int32 pmatch_len) {
    for (int32 i = 0; i < pmatch_len; i += 1) {
        if (reference[i].rm_so != actual[i].rm_so
            || reference[i].rm_eo != actual[i].rm_eo) {
            return i;
        }
    }
    return -1;
}

static int32
bench_result_mismatch(int32 reference_result, int32 actual_result,
                      regmatch_t *reference, regmatch_t *actual,
                      int32 pmatch_len, bool extract) {
    if (reference_result != actual_result) {
        return 1;
    }
    if (reference_result == 0 && extract) {
        return bench_pmatch_mismatch(reference, actual, pmatch_len) >= 0;
    }
    return 0;
}

static void
bench_report_mismatch(char *block, BenchRegexBucket *regex_bucket,
                      BenchRegexCase *regex_case,
                      BenchInputBucket *input_bucket,
                      BenchInputCase *input_case, char *engine,
                      enum Matcher matcher, int32 reference_result,
                      int32 actual_result, regmatch_t *reference,
                      regmatch_t *actual, int32 pmatch_len, bool extract) {
    error2("%s mismatch in regex bucket %s, input bucket %s, input case %s, "
           "engine %s",
           block, regex_bucket->name, input_bucket->name, input_case->name,
           engine);
    if (matcher != MATCHER_NONE) {
        char *matcher_name = MATCHER_str(matcher);

        error2(" (%s)", matcher_name);
        MATCHER_str_free(matcher_name);
    }
    error2("\n");
    error2("input " RED("\"%s\"") " against regex " BLUE("\"%s\"") "\n",
           input_case->input, regex_case->regex->string);
    error2("libc result: %d, actual result: %d\n", reference_result,
           actual_result);

    if (reference_result == 0 && actual_result == 0 && extract) {
        int32 group = bench_pmatch_mismatch(reference, actual, pmatch_len);
        if (group >= 0) {
            error2("capture group %d: libc[%d, %d], actual[%d, %d]\n", group,
                   (int32)reference[group].rm_so, (int32)reference[group].rm_eo,
                   (int32)actual[group].rm_so, (int32)actual[group].rm_eo);
        }
    }
    return;
}

static int32
bench_run_libc_one(regex_t *compiled, char *input, regmatch_t *pmatch,
                   int32 pmatch_len, bool extract) {
    regmatch_t *pmatch_ptr = NULL;
    int64 nmatch = 0;

    if (extract) {
        bench_clear_pmatch(pmatch, pmatch_len);
        pmatch_ptr = pmatch;
        nmatch = pmatch_len;
    }

    return regexec(compiled, input, (size_t)nmatch, pmatch_ptr, 0);
}

static int32
bench_run_meta_dispatch_one(MetaRegex *regex, char *input, int32 input_len,
                            enum Matcher enabled, regmatch_t *pmatch,
                            int32 pmatch_len, bool extract) {
    regmatch_t *pmatch_ptr = NULL;
    int32 nmatch = 0;

    if (extract) {
        bench_clear_pmatch(pmatch, pmatch_len);
        pmatch_ptr = pmatch;
        nmatch = pmatch_len;
    }

    return meta_regex_match(regex, (uint8 *)input, input_len, pmatch_ptr,
                            nmatch, enabled);
}

static int32
bench_run_meta_matcher_one(MetaRegex *regex, char *input, int32 input_len,
                           enum Matcher matcher, regmatch_t *pmatch,
                           int32 pmatch_len, bool extract) {
    regmatch_t *pmatch_ptr = NULL;
    int32 nmatch = 0;

    if (extract) {
        bench_clear_pmatch(pmatch, pmatch_len);
        pmatch_ptr = pmatch;
        nmatch = pmatch_len;
    }

    return meta_regex_match_with_algorithm(regex, (uint8 *)input, input_len,
                                           pmatch_ptr, nmatch, matcher);
}

static void
bench_absorb_result(int32 result, regmatch_t *pmatch, bool extract) {
    bench_sink_result += (result == 0);
    if (extract && pmatch != NULL) {
        bench_sink_offsets += pmatch[0].rm_so + (int64)pmatch[0].rm_eo;
    }
    return;
}

static void
bench_write_engine_row(FILE *csv, char *test_name, char *variant,
                       BenchRegexBucket *regex_bucket,
                       BenchInputBucket *input_bucket, char *engine,
                       char *matcher_name, char *selected_matcher_name,
                       int32 run_pair_count, double seconds, int32 matches) {
    int32 pair_count = regex_bucket->count;
    int64 total_iterations = META_BENCH_ITERATIONS*(int64)run_pair_count;
    double ns_per_match = 0.0;
    char *feature_name = regex_bucket->is_backref ? "with_backreferences"
                                                  : "no_backreferences";

    if (total_iterations > 0) {
        ns_per_match = (seconds*1000000000.0) / (double)total_iterations;
    }

    bench_csv_string(csv, test_name);
    fputc(',', csv);
    bench_csv_string(csv, variant);
    fputc(',', csv);
    bench_csv_string(csv, bench_length_class_name(regex_bucket->length_class));
    fprintf(csv, ",%d,", regex_bucket->max_regex_len);
    bench_csv_string(csv, feature_name);
    fputc(',', csv);
    bench_csv_string(csv, regex_bucket->name);
    fputc(',', csv);
    bench_csv_string(csv,
                     bench_input_length_class_name(input_bucket->length_class));
    fprintf(csv, ",%d,", input_bucket->max_input_len);
    bench_csv_string(csv, input_bucket->name);
    fputc(',', csv);
    bench_csv_string(csv, engine);
    fputc(',', csv);
    bench_csv_string(csv, matcher_name);
    fputc(',', csv);
    bench_csv_string(csv, selected_matcher_name);
    fprintf(csv, ",%d,%d,%d,%d,%d,%lld,%f,%f,%d\n", regex_bucket->count,
            input_bucket->count, pair_count, run_pair_count,
            META_BENCH_ITERATIONS, total_iterations, seconds,
            ns_per_match, matches);
    return;
}

static void
bench_generate_random_input(char *out, enum BenchInputLengthClass c) {
    static char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789 "
                             "_-./:;,@[](){}";
    int32 min_len = bench_input_length_class_min(c);
    int32 max_len = bench_input_length_class_max(c);
    int32 span = max_len - min_len + 1;
    int32 len = min_len;

    if (span > 1) {
        len += (int32)((uint32)rand() % (uint32)span);
    }

    for (int32 j = 0; j < len; j += 1) {
        uint32 r = (uint32)rand();
        out[j] = alphabet[r % (SIZEOF(alphabet) - 1)];
    }
    out[len] = '\0';
    return;
}

static void
bench_run_pairwise_variant(FILE *csv, char *test_name,
                           BenchRegexBucket *regex_bucket,
                           BenchInputBucket *input_bucket, regex_t *compiled,
                           enum Matcher enabled, bool extract) {
    char *variant = extract ? "extract" : "no_extract";
    int32 matches = 0;
    double seconds;
    struct timespec t0;
    struct timespec t1;
    regmatch_t pmatch[BENCH_MAX_MATCHES];

    printf("\n== %s (%s) ==\n", test_name, variant);

    for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
        BenchRegexCase *rc = &regex_bucket->cases[ri];
        BenchInputCase *ic = &input_bucket->cases[ri];
        regmatch_t ref_pm[BENCH_MAX_MATCHES];
        regmatch_t actual_pm[BENCH_MAX_MATCHES];
        int32 input_len = strlen32(ic->input);
        bool needs_extraction = (rc->regex->re_nsub > 0 && extract);
        enum Matcher selected = meta_choose_matcher(rc->regex, input_len,
                                                    needs_extraction, enabled);
        int32 ref_result = bench_run_libc_one(&compiled[ri], ic->input, ref_pm,
                                              LENGTH(ref_pm), extract);
        int32 actual_result = bench_run_meta_dispatch_one(
            rc->regex, ic->input, input_len, enabled, actual_pm,
            LENGTH(actual_pm), extract);

        if (bench_result_mismatch(ref_result, actual_result, ref_pm, actual_pm,
                                  LENGTH(ref_pm), extract)) {
            bench_report_mismatch("libc_vs_dispatch_pairwise", regex_bucket, rc,
                                  input_bucket, ic, "META_DISPATCH", selected,
                                  ref_result, actual_result, ref_pm, actual_pm,
                                  LENGTH(ref_pm), extract);
            exit(EXIT_FAILURE);
        }
    }

    for (int32 w = 0; w < META_BENCH_WARMUP_ITERATIONS; w += 1) {
        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            int32 result = bench_run_libc_one(&compiled[ri],
                                              input_bucket->cases[ri].input,
                                              pmatch, LENGTH(pmatch), extract);
            bench_absorb_result(result, pmatch, extract);
        }
    }

    matches = 0;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    for (int32 it = 0; it < META_BENCH_ITERATIONS; it += 1) {
        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            int32 result = bench_run_libc_one(&compiled[ri],
                                              input_bucket->cases[ri].input,
                                              pmatch, LENGTH(pmatch), extract);
            if (result == 0) {
                matches += 1;
            }
            bench_absorb_result(result, pmatch, extract);
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    seconds = bench_timediff(t0, t1);
    bench_write_engine_row(csv, test_name, variant, regex_bucket, input_bucket,
                           "LIBC", "LIBC", "LIBC", regex_bucket->count, seconds,
                           matches);

    for (int32 w = 0; w < META_BENCH_WARMUP_ITERATIONS; w += 1) {
        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            BenchRegexCase *rc = &regex_bucket->cases[ri];
            BenchInputCase *ic = &input_bucket->cases[ri];
            int32 input_len = strlen32(ic->input);
            int32 result = bench_run_meta_dispatch_one(
                rc->regex, ic->input, input_len, enabled, pmatch,
                LENGTH(pmatch), extract);
            bench_absorb_result(result, pmatch, extract);
        }
    }

    matches = 0;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    for (int32 it = 0; it < META_BENCH_ITERATIONS; it += 1) {
        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            BenchRegexCase *rc = &regex_bucket->cases[ri];
            BenchInputCase *ic = &input_bucket->cases[ri];
            int32 input_len = strlen32(ic->input);
            int32 result = bench_run_meta_dispatch_one(
                rc->regex, ic->input, input_len, enabled, pmatch,
                LENGTH(pmatch), extract);
            if (result == 0) {
                matches += 1;
            }
            bench_absorb_result(result, pmatch, extract);
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    seconds = bench_timediff(t0, t1);
    bench_write_engine_row(csv, test_name, variant, regex_bucket, input_bucket,
                           "META_DISPATCH", "DISPATCH", "mixed",
                           regex_bucket->count, seconds, matches);

    for (int32 mi = 0; mi < LENGTH(bench_matchers); mi += 1) {
        enum Matcher matcher = bench_matchers[mi];
        int32 run_pairs = 0;

        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            BenchRegexCase *rc = &regex_bucket->cases[ri];
            BenchInputCase *ic = &input_bucket->cases[ri];
            regmatch_t ref_pm[BENCH_MAX_MATCHES];
            regmatch_t actual_pm[BENCH_MAX_MATCHES];
            int32 input_len;
            int32 ref_result;
            int32 actual_result;

            if (!bench_matcher_supports_regex(rc->regex, matcher, extract)) {
                continue;
            }

            input_len = strlen32(ic->input);
            ref_result = bench_run_libc_one(&compiled[ri], ic->input, ref_pm,
                                            LENGTH(ref_pm), extract);
            actual_result = bench_run_meta_matcher_one(
                rc->regex, ic->input, input_len, matcher, actual_pm,
                LENGTH(actual_pm), extract);

            if (bench_result_mismatch(ref_result, actual_result, ref_pm,
                                      actual_pm, LENGTH(ref_pm), extract)) {
                char *matcher_name = MATCHER_str(matcher);

                bench_report_mismatch(
                    "meta_matchers_pairwise", regex_bucket, rc, input_bucket,
                    ic, matcher_name, matcher, ref_result, actual_result, ref_pm,
                    actual_pm, LENGTH(ref_pm), extract);
                MATCHER_str_free(matcher_name);
                exit(EXIT_FAILURE);
            }
            run_pairs += 1;
        }

        if (run_pairs == 0) {
            continue;
        }

        for (int32 w = 0; w < META_BENCH_WARMUP_ITERATIONS; w += 1) {
            for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
                BenchRegexCase *rc = &regex_bucket->cases[ri];
                BenchInputCase *ic = &input_bucket->cases[ri];
                int32 input_len;
                int32 result;

                if (!bench_matcher_supports_regex(rc->regex, matcher,
                                                  extract)) {
                    continue;
                }

                input_len = strlen32(ic->input);
                result = bench_run_meta_matcher_one(rc->regex, ic->input,
                                                    input_len, matcher, pmatch,
                                                    LENGTH(pmatch), extract);
                bench_absorb_result(result, pmatch, extract);
            }
        }

        matches = 0;
        clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
        for (int32 it = 0; it < META_BENCH_ITERATIONS; it += 1) {
            for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
                BenchRegexCase *rc = &regex_bucket->cases[ri];
                BenchInputCase *ic = &input_bucket->cases[ri];
                int32 input_len;
                int32 result;

                if (!bench_matcher_supports_regex(rc->regex, matcher,
                                                  extract)) {
                    continue;
                }

                input_len = strlen32(ic->input);
                result = bench_run_meta_matcher_one(rc->regex, ic->input,
                                                    input_len, matcher, pmatch,
                                                    LENGTH(pmatch), extract);
                if (result == 0) {
                    matches += 1;
                }
                bench_absorb_result(result, pmatch, extract);
            }
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
        seconds = bench_timediff(t0, t1);
        {
            char *matcher_name = MATCHER_str(matcher);

            bench_write_engine_row(csv, test_name, variant, regex_bucket,
                                   input_bucket, "META", matcher_name,
                                   matcher_name, run_pairs, seconds, matches);
            MATCHER_str_free(matcher_name);
        }
    }
    return;
}

static void
bench_process_regex_array(BenchRegexCase *array, int32 array_len,
                          char *array_name, int32 is_backref, llong now,
                          int32 max_input_len, enum Matcher enabled) {
    BenchRegexCase *bucket_cases;
    BenchRegexBucket regex_buckets[BENCH_LEN_LAST];
    char regex_bucket_names[BENCH_LEN_LAST][128];
    int32 counts[BENCH_LEN_LAST];
    BenchInputCase *input_cases;
    char (*input_names)[64];
    char (*input_storage)[BENCH_RANDOM_INPUT_MAX_LEN + 1];
    char csv_file[1024];
    FILE *csv;

    memset64(counts, 0, SIZEOF(counts));
    memset64(regex_buckets, 0, SIZEOF(regex_buckets));
    memset64(regex_bucket_names, 0, SIZEOF(regex_bucket_names));

    bucket_cases = malloc2(SIZEOF(*bucket_cases)*array_len * BENCH_LEN_LAST);
    input_cases = malloc2(SIZEOF(*input_cases)*array_len);
    input_names = malloc2(SIZEOF(*input_names)*array_len);
    input_storage = malloc2(SIZEOF(*input_storage)*array_len);

    for (int32 i = 0; i < array_len; i += 1) {
        BenchRegexCase c = array[i];
        int32 op_count = 0;
        enum BenchRegexLengthClass length_class;
        int32 index;

        if (c.regex != NULL) {
            for (op_count = 0; op_count < META_MAX_OPS; op_count += 1) {
                if (c.regex->ops[op_count].type == META_OP_END) {
                    break;
                }
            }
        }

        if (op_count <= 8) {
            length_class = BENCH_LEN_1_8;
        } else if (op_count <= 16) {
            length_class = BENCH_LEN_9_16;
        } else if (op_count <= 32) {
            length_class = BENCH_LEN_17_32;
        } else if (op_count <= 64) {
            length_class = BENCH_LEN_33_64;
        } else {
            length_class = BENCH_LEN_LAST;
        }

        if (length_class == BENCH_LEN_LAST) {
            error2("Skipping regex at index %d from %s with %d ops; no "
                   "benchmark length bucket exists above 64 ops: " BLUE(
                       "\"%s\"") "\n",
                   i, array_name, op_count,
                   c.regex != NULL ? c.regex->string : "(null)");
            continue;
        }

        c.regex_len = op_count;
        c.length_class = length_class;

        index = (int32)length_class*array_len + counts[length_class];
        bucket_cases[index] = c;
        counts[length_class] += 1;
    }

    SNPRINTF(csv_file, "benchmarks/%s-%lld.csv", array_name, now);
    csv = fopen(csv_file, "w");
    if (csv == NULL) {
        error("Error opening %s for writing: %s.\n", csv_file, strerror(errno));
        exit(EXIT_FAILURE);
    }
    fprintf(csv,
            "block,variant,regex_length_class,regex_max_len,feature_class,"
            "regex_bucket,input_length_class,input_max_len,input_bucket,engine,"
            "matcher,selected_matcher,regex_cases,input_cases,pair_count,"
            "run_pair_count,iterations_per_pair,total_iterations,seconds,"
            "ns_per_match,matches\n");

    for (enum BenchRegexLengthClass l = 0; l < BENCH_LEN_LAST; l += 1) {
        regex_t *compiled;

        if (counts[l] <= 0) {
            continue;
        }

        SNPRINTF(regex_bucket_names[l], "%s_ops_%s", array_name,
                 bench_length_class_name(l));

        regex_buckets[l].name = regex_bucket_names[l];
        regex_buckets[l].length_class = l;
        regex_buckets[l].is_backref = is_backref;
        regex_buckets[l].max_regex_len = bench_length_class_max(l);
        regex_buckets[l].cases = bucket_cases + (int32)l*array_len;
        regex_buckets[l].count = counts[l];
        ASSERT_MORE(counts[l], 10);

        compiled = malloc2(SIZEOF(*compiled)*regex_buckets[l].count);
        for (int32 i = 0; i < regex_buckets[l].count; i += 1) {
            int32 compiled_result;
            BenchRegexCase *c = &regex_buckets[l].cases[i];

            compiled_result
                = regcomp(&compiled[i], c->regex->string, REG_EXTENDED);
            if (compiled_result != 0) {
                char error_message[256];
                regerror(compiled_result, &compiled[i], error_message,
                         SIZEOF(error_message));
                error("regcomp failed for " BLUE("\"%s\"") ": %s\n",
                      c->regex->string, error_message);
                exit(EXIT_FAILURE);
            }
        }

        for (enum BenchInputLengthClass ii = 0; ii < BENCH_INPUT_LEN_LAST;
             ii += 1) {
            BenchInputBucket input_bucket;
            char test_name[256];
            int32 max_len = bench_input_length_class_max(ii);
            int32 not_matched = 0;
            int32 yes_matched = 0;

            if (max_len > max_input_len) {
                continue;
            }

            for (int32 ri = 0; ri < regex_buckets[l].count; ri += 1) {
                int32 matched = 0;

                SNPRINTF(input_names[ri], "random_%s_%d",
                         bench_input_length_class_name(ii), ri);

                for (int32 k = 0; k < BENCH_RANDOM_INPUT_ATTEMPTS; k += 1) {
                    bench_generate_random_input(input_storage[ri], ii);
                    if (bench_run_libc_one(&compiled[ri], input_storage[ri],
                                           NULL, 0, false)
                        == 0) {
                        matched = 1;
                        break;
                    }
                }

                if (matched) {
                    yes_matched += 1;
                } else {
                    not_matched += 1;
                    if (input_storage[ri][0] == '\0'
                        && bench_input_length_class_min(ii) > 0) {
                        bench_generate_random_input(input_storage[ri], ii);
                    }
                }

                input_cases[ri].name = input_names[ri];
                input_cases[ri].input = input_storage[ri];
                input_cases[ri].length_class = ii;
            }

            printf("%s / %s: matching_inputs=%d fallback_inputs=%d\n",
                   regex_buckets[l].name, bench_input_length_class_name(ii),
                   yes_matched, not_matched);

            input_bucket.name = bench_input_length_class_name(ii);
            input_bucket.length_class = ii;
            input_bucket.max_input_len = max_len;
            input_bucket.cases = input_cases;
            input_bucket.count = regex_buckets[l].count;

            SNPRINTF(test_name, "%s_ops_%s_input_%s", array_name,
                     bench_length_class_name(l),
                     bench_input_length_class_name(ii));

#if 1
            bench_run_pairwise_variant(csv, test_name, &regex_buckets[l],
                                       &input_bucket, compiled, enabled, false);
#endif
#if 1
            bench_run_pairwise_variant(csv, test_name, &regex_buckets[l],
                                       &input_bucket, compiled, enabled, true);
#endif
        }

        for (int32 i = 0; i < regex_buckets[l].count; i += 1) {
            regfree(&compiled[i]);
        }
        free2(compiled, SIZEOF(*compiled)*regex_buckets[l].count);
    }

    if (fclose(csv)) {
        error("Error closing %s: %s.\n", csv_file, strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("Wrote %s\n", csv_file);

    free2(input_storage, SIZEOF(*input_storage)*array_len);
    free2(input_names, SIZEOF(*input_names)*array_len);
    free2(input_cases, SIZEOF(*input_cases)*array_len);
    free2(bucket_cases, SIZEOF(*bucket_cases)*array_len * BENCH_LEN_LAST);
    return;
}

#define BENCH_PROCESS_ARRAY(arr, is_backref) \
    bench_process_regex_array((arr), LENGTH(arr), #arr, (is_backref), now, \
                              max_input_len, enabled)

static int32
bench_parse_input_len_cap(char *s, int32 *out) {
    char *end = NULL;
    int64 value;

    if (s == NULL || *s == '\0') {
        return 0;
    }

    errno = 0;
    value = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') {
        return 0;
    }
    if (value < 16 || value > BENCH_RANDOM_INPUT_MAX_LEN) {
        return 0;
    }

    *out = (int32)value;
    return 1;
}

static noreturn void
bench_usage(char *argv0) {
    fprintf(stderr,
            "Usage: %s [--max-input-len N]\n"
            "       %s [--max-input-len=N]\n"
            "       %s [N]\n"
            "\n"
            "N must be between 16 and %d. The benchmark runs only random "
            "input buckets whose max length is <= N.\n",
            argv0, argv0, argv0, BENCH_RANDOM_INPUT_MAX_LEN);
    exit(EXIT_FAILURE);
}

int32
main(int32 argc, char **argv) {
    int64 now;
    int32 max_input_len = META_BENCH_MAX_INPUT_LEN;
    enum Matcher enabled = MATCHER_NONE;
    (void)argc;
    (void)argv;

    srand(44u);

    setlocale(LC_ALL, "C");
    mkdir("benchmarks", 0777);

    for (int32 i = 0; i < LENGTH(bench_matchers); i += 1) {
        if (bench_matcher_compile_enabled(bench_matchers[i])) {
            enabled |= bench_matchers[i];
        }
    }
    if (enabled == MATCHER_NONE) {
        enabled = MATCHER_BTNFA;
    }

    printf("bench_max_input_len=%d\n", max_input_len);

    now = time(NULL);
    BENCH_PROCESS_ARRAY(bench_regex_cases, 0);
    BENCH_PROCESS_ARRAY(bench_regex_backref_cases, 1);

    printf("bench_sink_result=%d bench_sink_offsets=%lld\n",
           bench_sink_result, bench_sink_offsets);

    return 0;
}

#undef BENCH_PROCESS_ARRAY
