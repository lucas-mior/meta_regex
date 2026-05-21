#include <errno.h>
#include <locale.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "util.c"
#include "meta.h"
#include "meta_match.c"
#include "gen/main_bench_regexes2.h"
#include "gen/main_bench_patterns2.h"

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
#define META_BENCH_ITERATIONS 2
#endif

#if !defined(META_BENCH_WARMUP_ITERATIONS)
#define META_BENCH_WARMUP_ITERATIONS 16
#endif

#define BENCH_MAX_MATCHES 16

#if !defined(META_BENCH_ENABLE_EXTRACT_VARIANTS)
#define META_BENCH_ENABLE_EXTRACT_VARIANTS 0
#endif

#if !defined(ENABLE_BTNFA)
#define ENABLE_BTNFA 1
#endif
#if !defined(ENABLE_TNFA)
#define ENABLE_TNFA 0
#endif
#if !defined(ENABLE_TDFA)
#define ENABLE_TDFA 0
#endif
#if !defined(ENABLE_LAZY_DFA)
#define ENABLE_LAZY_DFA 0
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

static enum Matcher
bench_enabled_matcher_mask(void) {
    enum Matcher enabled = MATCHER_NONE;

    for (int32 i = 0; i < LENGTH(bench_matchers); i += 1) {
        if (bench_matcher_compile_enabled(bench_matchers[i])) {
            enabled |= bench_matchers[i];
        }
    }

    if (enabled == MATCHER_NONE) {
        enabled = MATCHER_BTNFA;
    }
    return enabled;
}

static int32
bench_matcher_storage_available(MetaRegex *regex, enum Matcher matcher) {
    if (regex == NULL) {
        return 0;
    }

    switch (matcher) {
    case MATCHER_BTNFA:
        return 1;
    case MATCHER_TNFA:
        return regex->tnfa != NULL;
    case MATCHER_TDFA:
        return regex->tdfa != NULL;
    case MATCHER_LAZY_DFA:
        return 1;
    case MATCHER_STATIC_DFA:
        return regex->static_dfa != NULL;
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
    if (!bench_matcher_storage_available(regex, matcher)) {
        return 0;
    }
    if (extract && !matchers[matcher].extracts) {
        return 0;
    }
    if ((regex->used_ops & ~matchers[matcher].supports) != 0) {
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
        error2(" (%s)", MATCHER_str(matcher));
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
bench_write_header(FILE *csv) {
    fprintf(csv,
            "block,variant,regex_length_class,regex_max_len,feature_class,"
            "regex_bucket,input_length_class,input_max_len,input_bucket,engine,"
            "matcher,selected_matcher,regex_cases,input_cases,pair_count,"
            "run_pair_count,iterations_per_pair,total_iterations,seconds,"
            "ns_per_match,matches\n");
    return;
}

static void
bench_write_row_with_pair_count(FILE *csv, char *block, char *variant,
                                BenchRegexBucket *regex_bucket,
                                BenchInputBucket *input_bucket, char *engine,
                                char *matcher_name, char *selected_matcher_name,
                                int32 regex_cases, int32 input_cases,
                                int32 pair_count, int32 run_pair_count,
                                int32 iterations_per_pair, double seconds,
                                int32 matches) {
    int64 total_iterations = iterations_per_pair*(int64)run_pair_count;
    double ns_per_match = 0.0;

    if (total_iterations > 0) {
        ns_per_match = (seconds*1000000000.0) / (double)total_iterations;
    }

    bench_csv_string(csv, block);
    fputc(',', csv);
    bench_csv_string(csv, variant);
    fputc(',', csv);
    bench_csv_string(csv, bench_length_class_name(regex_bucket->length_class));
    fprintf(csv, ",%d,", regex_bucket->max_regex_len);
    bench_csv_string(csv,
                     bench_feature_class_name(regex_bucket->feature_class));
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
    fprintf(csv, ",%d,%d,%d,%d,%d,%lld,%f,%f,%d\n", regex_cases, input_cases,
            pair_count, run_pair_count, iterations_per_pair,
            (llong)total_iterations, seconds, ns_per_match, matches);
    return;
}

static void
bench_write_row(FILE *csv, char *block, char *variant,
                BenchRegexBucket *regex_bucket, BenchInputBucket *input_bucket,
                char *engine, char *matcher_name, char *selected_matcher_name,
                int32 regex_cases, int32 input_cases, int32 run_pair_count,
                int32 iterations_per_pair, double seconds, int32 matches) {
    bench_write_row_with_pair_count(
        csv, block, variant, regex_bucket, input_bucket, engine, matcher_name,
        selected_matcher_name, regex_cases, input_cases,
        regex_cases*input_cases, run_pair_count, iterations_per_pair, seconds,
        matches);
    return;
}

static FILE *
bench_open_csv(char *path) {
    FILE *csv = fopen(path, "w");
    if (csv == NULL) {
        error("Error opening %s for writing: %s.\n", path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    bench_write_header(csv);
    return csv;
}

static void
bench_close_csv(FILE *csv, char *path) {
    if (fclose(csv)) {
        error("Error closing %s: %s.\n", path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("Wrote %s\n", path);
    return;
}

static regex_t *
bench_compile_regex_bucket(BenchRegexBucket *regex_bucket) {
    regex_t *compiled = malloc2(SIZEOF(*compiled)*regex_bucket->count);

    for (int32 i = 0; i < regex_bucket->count; i += 1) {
        int32 compiled_result;
        BenchRegexCase *c = &regex_bucket->cases[i];

        compiled_result = regcomp(&compiled[i], c->regex->string, REG_EXTENDED);
        if (compiled_result != 0) {
            char error_message[256];
            regerror(compiled_result, &compiled[i], error_message,
                     SIZEOF(error_message));
            error("regcomp failed for " BLUE("\"%s\"") ": %s\n",
                  c->regex->string, error_message);
            exit(EXIT_FAILURE);
        }
    }

    return compiled;
}

static void
bench_free_regex_bucket(regex_t *compiled, BenchRegexBucket *regex_bucket) {
    for (int32 i = 0; i < regex_bucket->count; i += 1) {
        regfree(&compiled[i]);
    }
    free2(compiled, SIZEOF(*compiled)*regex_bucket->count);
    return;
}

static void
bench_validate_dispatch(BenchRegexBucket *regex_bucket,
                        BenchInputBucket *input_bucket, regex_t *compiled,
                        bool extract) {
    enum Matcher enabled = bench_enabled_matcher_mask();

    for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
        BenchRegexCase *rc = &regex_bucket->cases[ri];
        for (int32 ii = 0; ii < input_bucket->count; ii += 1) {
            BenchInputCase *ic = &input_bucket->cases[ii];
            regmatch_t ref_pm[BENCH_MAX_MATCHES];
            regmatch_t actual_pm[BENCH_MAX_MATCHES];
            int32 ref_result;
            int32 actual_result;
            int32 input_len = strlen32(ic->input);
            bool needs_extraction = (rc->regex->re_nsub > 0 && extract);
            enum Matcher selected = meta_choose_matcher(
                rc->regex, input_len, needs_extraction, enabled);

            ref_result = bench_run_libc_one(&compiled[ri], ic->input, ref_pm,
                                            LENGTH(ref_pm), extract);
            actual_result = bench_run_meta_dispatch_one(
                rc->regex, ic->input, input_len, enabled, actual_pm,
                LENGTH(actual_pm), extract);

            if (bench_result_mismatch(ref_result, actual_result, ref_pm,
                                      actual_pm, LENGTH(ref_pm), extract)) {
                bench_report_mismatch(
                    "libc_vs_dispatch", regex_bucket, rc, input_bucket, ic,
                    "META_DISPATCH", selected, ref_result, actual_result,
                    ref_pm, actual_pm, LENGTH(ref_pm), extract);
                exit(EXIT_FAILURE);
            }
        }
    }
    return;
}

static void
bench_validate_matcher(BenchRegexBucket *regex_bucket,
                       BenchInputBucket *input_bucket, regex_t *compiled,
                       enum Matcher matcher, bool extract, int32 *run_pairs) {
    *run_pairs = 0;

    for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
        BenchRegexCase *rc = &regex_bucket->cases[ri];
        if (!bench_matcher_supports_regex(rc->regex, matcher, extract)) {
            continue;
        }

        for (int32 ii = 0; ii < input_bucket->count; ii += 1) {
            BenchInputCase *ic = &input_bucket->cases[ii];
            regmatch_t ref_pm[BENCH_MAX_MATCHES];
            regmatch_t actual_pm[BENCH_MAX_MATCHES];
            int32 ref_result;
            int32 actual_result;
            int32 input_len = strlen32(ic->input);

            ref_result = bench_run_libc_one(&compiled[ri], ic->input, ref_pm,
                                            LENGTH(ref_pm), extract);
            actual_result = bench_run_meta_matcher_one(
                rc->regex, ic->input, input_len, matcher, actual_pm,
                LENGTH(actual_pm), extract);

            if (bench_result_mismatch(ref_result, actual_result, ref_pm,
                                      actual_pm, LENGTH(ref_pm), extract)) {
                bench_report_mismatch(
                    "meta_matchers", regex_bucket, rc, input_bucket, ic,
                    MATCHER_str(matcher), matcher, ref_result, actual_result,
                    ref_pm, actual_pm, LENGTH(ref_pm), extract);
                exit(EXIT_FAILURE);
            }
            *run_pairs += 1;
        }
    }
    return;
}

static double
bench_time_libc_bucket(BenchRegexBucket *regex_bucket,
                       BenchInputBucket *input_bucket, regex_t *compiled,
                       bool extract, int32 iterations_per_pair,
                       int32 *matches) {
    struct timespec t0;
    struct timespec t1;
    regmatch_t pmatch[BENCH_MAX_MATCHES];
    int32 local_matches = 0;

    for (int32 w = 0; w < META_BENCH_WARMUP_ITERATIONS; w += 1) {
        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            for (int32 ii = 0; ii < input_bucket->count; ii += 1) {
                int32 result = bench_run_libc_one(
                    &compiled[ri], input_bucket->cases[ii].input, pmatch,
                    LENGTH(pmatch), extract);
                bench_absorb_result(result, pmatch, extract);
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    for (int32 it = 0; it < iterations_per_pair; it += 1) {
        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            for (int32 ii = 0; ii < input_bucket->count; ii += 1) {
                int32 result = bench_run_libc_one(
                    &compiled[ri], input_bucket->cases[ii].input, pmatch,
                    LENGTH(pmatch), extract);
                if (result == 0) {
                    local_matches += 1;
                }
                bench_absorb_result(result, pmatch, extract);
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

    *matches = local_matches;
    return bench_timediff(t0, t1);
}

static double
bench_time_dispatch_bucket(BenchRegexBucket *regex_bucket,
                           BenchInputBucket *input_bucket, bool extract,
                           enum Matcher enabled, int32 iterations_per_pair,
                           int32 *matches) {
    struct timespec t0;
    struct timespec t1;
    regmatch_t pmatch[BENCH_MAX_MATCHES];
    int32 local_matches = 0;

    for (int32 w = 0; w < META_BENCH_WARMUP_ITERATIONS; w += 1) {
        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            BenchRegexCase *rc = &regex_bucket->cases[ri];
            for (int32 ii = 0; ii < input_bucket->count; ii += 1) {
                BenchInputCase *ic = &input_bucket->cases[ii];
                int32 input_len = strlen32(ic->input);
                int32 result = bench_run_meta_dispatch_one(
                    rc->regex, ic->input, input_len, enabled, pmatch,
                    LENGTH(pmatch), extract);
                bench_absorb_result(result, pmatch, extract);
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    for (int32 it = 0; it < iterations_per_pair; it += 1) {
        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            BenchRegexCase *rc = &regex_bucket->cases[ri];
            for (int32 ii = 0; ii < input_bucket->count; ii += 1) {
                BenchInputCase *ic = &input_bucket->cases[ii];
                int32 input_len = strlen32(ic->input);
                int32 result = bench_run_meta_dispatch_one(
                    rc->regex, ic->input, input_len, enabled, pmatch,
                    LENGTH(pmatch), extract);
                if (result == 0) {
                    local_matches += 1;
                }
                bench_absorb_result(result, pmatch, extract);
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

    *matches = local_matches;
    return bench_timediff(t0, t1);
}

static double
bench_time_matcher_bucket(BenchRegexBucket *regex_bucket,
                          BenchInputBucket *input_bucket, enum Matcher matcher,
                          bool extract, int32 iterations_per_pair,
                          int32 *matches) {
    struct timespec t0;
    struct timespec t1;
    regmatch_t pmatch[BENCH_MAX_MATCHES];
    int32 local_matches = 0;

    for (int32 w = 0; w < META_BENCH_WARMUP_ITERATIONS; w += 1) {
        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            BenchRegexCase *rc = &regex_bucket->cases[ri];
            if (!bench_matcher_supports_regex(rc->regex, matcher, extract)) {
                continue;
            }
            for (int32 ii = 0; ii < input_bucket->count; ii += 1) {
                BenchInputCase *ic = &input_bucket->cases[ii];
                int32 input_len = strlen32(ic->input);
                int32 result = bench_run_meta_matcher_one(
                    rc->regex, ic->input, input_len, matcher, pmatch,
                    LENGTH(pmatch), extract);
                bench_absorb_result(result, pmatch, extract);
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    for (int32 it = 0; it < iterations_per_pair; it += 1) {
        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            BenchRegexCase *rc = &regex_bucket->cases[ri];
            if (!bench_matcher_supports_regex(rc->regex, matcher, extract)) {
                continue;
            }
            for (int32 ii = 0; ii < input_bucket->count; ii += 1) {
                BenchInputCase *ic = &input_bucket->cases[ii];
                int32 input_len = strlen32(ic->input);
                int32 result = bench_run_meta_matcher_one(
                    rc->regex, ic->input, input_len, matcher, pmatch,
                    LENGTH(pmatch), extract);
                if (result == 0) {
                    local_matches += 1;
                }
                bench_absorb_result(result, pmatch, extract);
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

    *matches = local_matches;
    return bench_timediff(t0, t1);
}

static void
bench_libc_vs_dispatch(FILE *csv, BenchRegexBucket *regex_bucket,
                       BenchInputBucket *input_bucket, regex_t *compiled) {
    enum Matcher enabled = bench_enabled_matcher_mask();
    int32 pair_count = regex_bucket->count*input_bucket->count;
    int32 matches = 0;
    double seconds;

    printf("\n== libc vs meta dispatcher: %s / %s ==\n", regex_bucket->name,
           input_bucket->name);

    bench_validate_dispatch(regex_bucket, input_bucket, compiled, false);
#if META_BENCH_ENABLE_EXTRACT_VARIANTS
    bench_validate_dispatch(regex_bucket, input_bucket, compiled, true);
#endif

    seconds = bench_time_libc_bucket(regex_bucket, input_bucket, compiled,
                                     false, META_BENCH_ITERATIONS, &matches);
    bench_write_row(csv, "libc_vs_dispatch", "no_extract", regex_bucket,
                    input_bucket, "LIBC", "LIBC", "LIBC", regex_bucket->count,
                    input_bucket->count, pair_count, META_BENCH_ITERATIONS,
                    seconds, matches);

#if META_BENCH_ENABLE_EXTRACT_VARIANTS
    seconds = bench_time_libc_bucket(regex_bucket, input_bucket, compiled, true,
                                     META_BENCH_ITERATIONS, &matches);
    bench_write_row(csv, "libc_vs_dispatch", "extract", regex_bucket,
                    input_bucket, "LIBC", "LIBC", "LIBC", regex_bucket->count,
                    input_bucket->count, pair_count, META_BENCH_ITERATIONS,
                    seconds, matches);
#endif

    seconds
        = bench_time_dispatch_bucket(regex_bucket, input_bucket, false, enabled,
                                     META_BENCH_ITERATIONS, &matches);
    bench_write_row(csv, "libc_vs_dispatch", "no_extract", regex_bucket,
                    input_bucket, "META_DISPATCH", "DISPATCH", "mixed",
                    regex_bucket->count, input_bucket->count, pair_count,
                    META_BENCH_ITERATIONS, seconds, matches);

#if META_BENCH_ENABLE_EXTRACT_VARIANTS
    seconds
        = bench_time_dispatch_bucket(regex_bucket, input_bucket, true, enabled,
                                     META_BENCH_ITERATIONS, &matches);
    bench_write_row(csv, "libc_vs_dispatch", "extract", regex_bucket,
                    input_bucket, "META_DISPATCH", "DISPATCH", "mixed",
                    regex_bucket->count, input_bucket->count, pair_count,
                    META_BENCH_ITERATIONS, seconds, matches);
#endif
    return;
}

static void
bench_meta_matchers(FILE *csv, BenchRegexBucket *regex_bucket,
                    BenchInputBucket *input_bucket, regex_t *compiled,
                    bool extract) {
    char *variant = extract ? "extract" : "no_extract";

    printf("\n== meta matchers only: %s / %s (%s) ==\n", regex_bucket->name,
           input_bucket->name, variant);

    for (int32 mi = 0; mi < LENGTH(bench_matchers); mi += 1) {
        enum Matcher matcher = bench_matchers[mi];
        int32 run_pairs = 0;
        int32 matches = 0;
        double seconds;

        bench_validate_matcher(regex_bucket, input_bucket, compiled, matcher,
                               extract, &run_pairs);
        if (run_pairs == 0) {
            continue;
        }

        seconds = bench_time_matcher_bucket(regex_bucket, input_bucket, matcher,
                                            extract, META_BENCH_ITERATIONS,
                                            &matches);
        bench_write_row(csv, "meta_matchers", variant, regex_bucket,
                        input_bucket, "META", MATCHER_str(matcher),
                        MATCHER_str(matcher), regex_bucket->count,
                        input_bucket->count, run_pairs, META_BENCH_ITERATIONS,
                        seconds, matches);
    }
    return;
}

static void
bench_validate_dispatch_pairwise(BenchRegexBucket *regex_bucket,
                                 BenchInputBucket *input_bucket,
                                 regex_t *compiled, bool extract) {
    enum Matcher enabled = bench_enabled_matcher_mask();

    for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
        BenchRegexCase *rc = &regex_bucket->cases[ri];
        BenchInputCase *ic = &input_bucket->cases[ri];
        regmatch_t ref_pm[BENCH_MAX_MATCHES];
        regmatch_t actual_pm[BENCH_MAX_MATCHES];
        int32 ref_result;
        int32 actual_result;
        int32 input_len = strlen32(ic->input);
        bool needs_extraction = (rc->regex->re_nsub > 0 && extract);
        enum Matcher selected = meta_choose_matcher(rc->regex, input_len,
                                                    needs_extraction, enabled);

        ref_result = bench_run_libc_one(&compiled[ri], ic->input, ref_pm,
                                        LENGTH(ref_pm), extract);
        actual_result = bench_run_meta_dispatch_one(
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
    return;
}

static void
bench_validate_matcher_pairwise(BenchRegexBucket *regex_bucket,
                                BenchInputBucket *input_bucket,
                                regex_t *compiled, enum Matcher matcher,
                                bool extract, int32 *run_pairs) {
    *run_pairs = 0;

    for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
        BenchRegexCase *rc = &regex_bucket->cases[ri];
        BenchInputCase *ic = &input_bucket->cases[ri];
        regmatch_t ref_pm[BENCH_MAX_MATCHES];
        regmatch_t actual_pm[BENCH_MAX_MATCHES];
        int32 ref_result;
        int32 actual_result;
        int32 input_len;

        if (!bench_matcher_supports_regex(rc->regex, matcher, extract)) {
            continue;
        }

        input_len = strlen32(ic->input);
        ref_result = bench_run_libc_one(&compiled[ri], ic->input, ref_pm,
                                        LENGTH(ref_pm), extract);
        actual_result = bench_run_meta_matcher_one(
            rc->regex, ic->input, input_len, matcher, actual_pm,
            LENGTH(actual_pm), extract);

        if (bench_result_mismatch(ref_result, actual_result, ref_pm, actual_pm,
                                  LENGTH(ref_pm), extract)) {
            bench_report_mismatch("meta_matchers_pairwise", regex_bucket, rc,
                                  input_bucket, ic, MATCHER_str(matcher),
                                  matcher, ref_result, actual_result, ref_pm,
                                  actual_pm, LENGTH(ref_pm), extract);
            exit(EXIT_FAILURE);
        }
        *run_pairs += 1;
    }
    return;
}

static double
bench_time_libc_bucket_pairwise(BenchRegexBucket *regex_bucket,
                                BenchInputBucket *input_bucket,
                                regex_t *compiled, bool extract,
                                int32 iterations_per_pair, int32 *matches) {
    struct timespec t0;
    struct timespec t1;
    regmatch_t pmatch[BENCH_MAX_MATCHES];
    int32 local_matches = 0;

    for (int32 w = 0; w < META_BENCH_WARMUP_ITERATIONS; w += 1) {
        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            int32 result = bench_run_libc_one(&compiled[ri],
                                              input_bucket->cases[ri].input,
                                              pmatch, LENGTH(pmatch), extract);
            bench_absorb_result(result, pmatch, extract);
        }
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    for (int32 it = 0; it < iterations_per_pair; it += 1) {
        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            int32 result = bench_run_libc_one(&compiled[ri],
                                              input_bucket->cases[ri].input,
                                              pmatch, LENGTH(pmatch), extract);
            if (result == 0) {
                local_matches += 1;
            }
            bench_absorb_result(result, pmatch, extract);
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

    *matches = local_matches;
    return bench_timediff(t0, t1);
}

static double
bench_time_dispatch_bucket_pairwise(BenchRegexBucket *regex_bucket,
                                    BenchInputBucket *input_bucket,
                                    bool extract, enum Matcher enabled,
                                    int32 iterations_per_pair, int32 *matches) {
    struct timespec t0;
    struct timespec t1;
    regmatch_t pmatch[BENCH_MAX_MATCHES];
    int32 local_matches = 0;

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

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    for (int32 it = 0; it < iterations_per_pair; it += 1) {
        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            BenchRegexCase *rc = &regex_bucket->cases[ri];
            BenchInputCase *ic = &input_bucket->cases[ri];
            int32 input_len = strlen32(ic->input);
            int32 result = bench_run_meta_dispatch_one(
                rc->regex, ic->input, input_len, enabled, pmatch,
                LENGTH(pmatch), extract);
            if (result == 0) {
                local_matches += 1;
            }
            bench_absorb_result(result, pmatch, extract);
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

    *matches = local_matches;
    return bench_timediff(t0, t1);
}

static double
bench_time_matcher_bucket_pairwise(BenchRegexBucket *regex_bucket,
                                   BenchInputBucket *input_bucket,
                                   enum Matcher matcher, bool extract,
                                   int32 iterations_per_pair, int32 *matches) {
    struct timespec t0;
    struct timespec t1;
    regmatch_t pmatch[BENCH_MAX_MATCHES];
    int32 local_matches = 0;

    for (int32 w = 0; w < META_BENCH_WARMUP_ITERATIONS; w += 1) {
        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            BenchRegexCase *rc = &regex_bucket->cases[ri];
            BenchInputCase *ic = &input_bucket->cases[ri];
            int32 input_len;
            int32 result;

            if (!bench_matcher_supports_regex(rc->regex, matcher, extract)) {
                continue;
            }

            input_len = strlen32(ic->input);
            result = bench_run_meta_matcher_one(rc->regex, ic->input, input_len,
                                                matcher, pmatch, LENGTH(pmatch),
                                                extract);
            bench_absorb_result(result, pmatch, extract);
        }
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    for (int32 it = 0; it < iterations_per_pair; it += 1) {
        for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
            BenchRegexCase *rc = &regex_bucket->cases[ri];
            BenchInputCase *ic = &input_bucket->cases[ri];
            int32 input_len;
            int32 result;

            if (!bench_matcher_supports_regex(rc->regex, matcher, extract)) {
                continue;
            }

            input_len = strlen32(ic->input);
            result = bench_run_meta_matcher_one(rc->regex, ic->input, input_len,
                                                matcher, pmatch, LENGTH(pmatch),
                                                extract);
            if (result == 0) {
                local_matches += 1;
            }
            bench_absorb_result(result, pmatch, extract);
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

    *matches = local_matches;
    return bench_timediff(t0, t1);
}

static void
bench_libc_vs_dispatch_pairwise(FILE *csv, BenchRegexBucket *regex_bucket,
                                BenchInputBucket *input_bucket,
                                regex_t *compiled) {
    enum Matcher enabled = bench_enabled_matcher_mask();
    int32 pair_count = regex_bucket->count;
    int32 matches = 0;
    double seconds;

    printf("\n== libc vs meta dispatcher pairwise: %s / %s ==\n",
           regex_bucket->name, input_bucket->name);

    bench_validate_dispatch_pairwise(regex_bucket, input_bucket, compiled,
                                     false);
#if META_BENCH_ENABLE_EXTRACT_VARIANTS
    bench_validate_dispatch_pairwise(regex_bucket, input_bucket, compiled,
                                     true);
#endif

    seconds = bench_time_libc_bucket_pairwise(regex_bucket, input_bucket,
                                              compiled, false,
                                              META_BENCH_ITERATIONS, &matches);
    bench_write_row_with_pair_count(csv, "libc_vs_dispatch_pairwise",
                                    "no_extract", regex_bucket, input_bucket,
                                    "LIBC", "LIBC", "LIBC", regex_bucket->count,
                                    input_bucket->count, pair_count, pair_count,
                                    META_BENCH_ITERATIONS, seconds, matches);

#if META_BENCH_ENABLE_EXTRACT_VARIANTS
    seconds = bench_time_libc_bucket_pairwise(regex_bucket, input_bucket,
                                              compiled, true,
                                              META_BENCH_ITERATIONS, &matches);
    bench_write_row_with_pair_count(
        csv, "libc_vs_dispatch_pairwise", "extract", regex_bucket, input_bucket,
        "LIBC", "LIBC", "LIBC", regex_bucket->count, input_bucket->count,
        pair_count, pair_count, META_BENCH_ITERATIONS, seconds, matches);
#endif

    seconds = bench_time_dispatch_bucket_pairwise(
        regex_bucket, input_bucket, false, enabled, META_BENCH_ITERATIONS,
        &matches);
    bench_write_row_with_pair_count(
        csv, "libc_vs_dispatch_pairwise", "no_extract", regex_bucket,
        input_bucket, "META_DISPATCH", "DISPATCH", "mixed", regex_bucket->count,
        input_bucket->count, pair_count, pair_count, META_BENCH_ITERATIONS,
        seconds, matches);

#if META_BENCH_ENABLE_EXTRACT_VARIANTS
    seconds = bench_time_dispatch_bucket_pairwise(
        regex_bucket, input_bucket, true, enabled, META_BENCH_ITERATIONS,
        &matches);
    bench_write_row_with_pair_count(csv, "libc_vs_dispatch_pairwise", "extract",
                                    regex_bucket, input_bucket, "META_DISPATCH",
                                    "DISPATCH", "mixed", regex_bucket->count,
                                    input_bucket->count, pair_count, pair_count,
                                    META_BENCH_ITERATIONS, seconds, matches);
#endif
    return;
}

static void
bench_meta_matchers_pairwise(FILE *csv, BenchRegexBucket *regex_bucket,
                             BenchInputBucket *input_bucket, regex_t *compiled,
                             bool extract) {
    char *variant = extract ? "extract" : "no_extract";
    int32 pair_count = regex_bucket->count;

    printf("\n== meta matchers only pairwise: %s / %s (%s) ==\n",
           regex_bucket->name, input_bucket->name, variant);

    for (int32 mi = 0; mi < LENGTH(bench_matchers); mi += 1) {
        enum Matcher matcher = bench_matchers[mi];
        int32 run_pairs = 0;
        int32 matches = 0;
        double seconds;

        bench_validate_matcher_pairwise(regex_bucket, input_bucket, compiled,
                                        matcher, extract, &run_pairs);
        if (run_pairs == 0) {
            continue;
        }

        seconds = bench_time_matcher_bucket_pairwise(
            regex_bucket, input_bucket, matcher, extract, META_BENCH_ITERATIONS,
            &matches);
        bench_write_row_with_pair_count(
            csv, "meta_matchers_pairwise", variant, regex_bucket, input_bucket,
            "META", MATCHER_str(matcher), MATCHER_str(matcher),
            regex_bucket->count, input_bucket->count, pair_count, run_pairs,
            META_BENCH_ITERATIONS, seconds, matches);
    }
    return;
}

#define BENCH_MAIN_REGEX_BUCKET_MAX (BENCH_FEATURE_LAST*BENCH_LEN_LAST)
#define BENCH_RANDOM_INPUT_ATTEMPTS 500
#define BENCH_RANDOM_INPUT_MAX_LEN 4096

#if !defined(META_BENCH_MAX_INPUT_LEN)
#define META_BENCH_MAX_INPUT_LEN 128
#endif

static int32 bench_max_input_len = META_BENCH_MAX_INPUT_LEN;

static BenchRegexCase bench_runtime_regex_cases[BENCH_FEATURE_LAST]
                                               [BENCH_LEN_LAST]
                                               [LENGTH(bench_regex_cases)];
static BenchRegexBucket
    bench_runtime_regex_buckets[BENCH_MAIN_REGEX_BUCKET_MAX];
static char bench_runtime_regex_bucket_names[BENCH_MAIN_REGEX_BUCKET_MAX][128];
static int32 bench_runtime_regex_bucket_count;

static BenchInputCase bench_pair_input_cases[LENGTH(bench_regex_cases)];
static char bench_pair_input_names[LENGTH(bench_regex_cases)][64];
static char bench_pair_input_storage[LENGTH(bench_regex_cases)]
                                    [BENCH_RANDOM_INPUT_MAX_LEN + 1];
static int32
bench_regex_op_count(MetaRegex *regex) {
    if (regex == NULL) {
        return 0;
    }

    for (int32 i = 0; i < META_MAX_OPS; i += 1) {
        if (regex->ops[i].type == META_OP_END) {
            return i;
        }
    }

    return META_MAX_OPS;
}

static enum BenchRegexLengthClass
bench_regex_length_class_from_ops(int32 op_count) {
    if (op_count <= 8) {
        return BENCH_LEN_1_8;
    }
    if (op_count <= 16) {
        return BENCH_LEN_9_16;
    }
    if (op_count <= 32) {
        return BENCH_LEN_17_32;
    }
    if (op_count <= 64) {
        return BENCH_LEN_33_64;
    }
    return BENCH_LEN_LAST;
}

static enum BenchRegexFeatureClass
bench_regex_feature_class_from_ops(MetaRegex *regex) {
    if (regex != NULL && (regex->used_ops & META_OP_BACKREF) != 0) {
        return BENCH_FEATURE_ALL;
    }
    return BENCH_FEATURE_NO_BACKREFS;
}

static void
bench_build_runtime_regex_buckets(void) {
    int32 counts[BENCH_FEATURE_LAST][BENCH_LEN_LAST];

    memset64(counts, 0, SIZEOF(counts));
    memset64(bench_runtime_regex_cases, 0, SIZEOF(bench_runtime_regex_cases));
    memset64(bench_runtime_regex_buckets, 0,
             SIZEOF(bench_runtime_regex_buckets));
    memset64(bench_runtime_regex_bucket_names, 0,
             SIZEOF(bench_runtime_regex_bucket_names));
    bench_runtime_regex_bucket_count = 0;

    for (int32 i = 0; i < LENGTH(bench_regex_cases); i += 1) {
        BenchRegexCase c = bench_regex_cases[i];
        int32 op_count = bench_regex_op_count(c.regex);
        enum BenchRegexLengthClass length_class
            = bench_regex_length_class_from_ops(op_count);
        enum BenchRegexFeatureClass feature_class
            = bench_regex_feature_class_from_ops(c.regex);

        if (length_class == BENCH_LEN_LAST) {
            error2("Skipping regex at index %d with %d ops; no benchmark "
                   "length bucket exists above 64 ops: " BLUE("\"%s\"") "\n",
                   i, op_count, c.regex->string);
            continue;
        }

        c.regex_len = op_count;
        c.length_class = length_class;
        c.feature_class = feature_class;

        bench_runtime_regex_cases[feature_class][length_class]
                                 [counts[feature_class][length_class]] = c;
        counts[feature_class][length_class] += 1;
    }

    for (int32 f = 0; f < BENCH_FEATURE_LAST; f += 1) {
        for (int32 l = 0; l < BENCH_LEN_LAST; l += 1) {
            int32 count = counts[f][l];
            int32 index;

            if (count <= 0) {
                continue;
            }

            index = bench_runtime_regex_bucket_count;
            SNPRINTF(bench_runtime_regex_bucket_names[index],
                     "runtime_%s_ops_%s", bench_feature_class_name(f),
                     bench_length_class_name(l));

            bench_runtime_regex_buckets[index].name
                = bench_runtime_regex_bucket_names[index];
            bench_runtime_regex_buckets[index].length_class = l;
            bench_runtime_regex_buckets[index].feature_class = f;
            bench_runtime_regex_buckets[index].max_regex_len
                = bench_length_class_max(l);
            bench_runtime_regex_buckets[index].cases
                = bench_runtime_regex_cases[f][l];
            bench_runtime_regex_buckets[index].count = count;
            bench_runtime_regex_bucket_count += 1;
        }
    }

    if (bench_runtime_regex_bucket_count == 0) {
        error("No generated main benchmark regexes fit the active length "
              "buckets.\n");
        exit(EXIT_FAILURE);
    }
    return;
}

static uint32
bench_random_next(uint32 *state) {
    *state = (*state*1664525u) + 1013904223u;
    return *state;
}

static char
bench_random_char(uint32 *state) {
    static char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789 "
                             "_-./:;,@[](){}";
    uint32 r = bench_random_next(state);
    return alphabet[r % (SIZEOF(alphabet) - 1)];
}

static int32
bench_random_input_len_for_class(enum BenchInputLengthClass c, uint32 *seed) {
    int32 min_len = bench_input_length_class_min(c);
    int32 max_len = bench_input_length_class_max(c);
    int32 span = max_len - min_len + 1;

    if (span <= 1) {
        return min_len;
    }
    return min_len + (int32)(bench_random_next(seed) % (uint32)span);
}

static void
bench_generate_random_input(char *out, enum BenchInputLengthClass c,
                            uint32 *seed) {
    int32 len = bench_random_input_len_for_class(c, seed);

    for (int32 j = 0; j < len; j += 1) {
        out[j] = bench_random_char(seed);
    }
    out[len] = '\0';
    return;
}

static void
bench_build_pair_input_bucket(BenchRegexBucket *regex_bucket, regex_t *compiled,
                              enum BenchInputLengthClass input_class,
                              uint32 *seed, BenchInputBucket *input_bucket) {
    int32 max_len = bench_input_length_class_max(input_class);
    int32 not_matched = 0;
    int32 yes_matched = 0;

    for (int32 ri = 0; ri < regex_bucket->count; ri += 1) {
        int32 matched = 0;

        SNPRINTF(bench_pair_input_names[ri], "random_%s_%d",
                 bench_input_length_class_name(input_class), ri);

        for (int32 k = 0; k < BENCH_RANDOM_INPUT_ATTEMPTS; k += 1) {
            bench_generate_random_input(bench_pair_input_storage[ri],
                                        input_class, seed);
            if (bench_run_libc_one(&compiled[ri], bench_pair_input_storage[ri],
                                   NULL, 0, false)
                == 0) {
                matched = 1;
                break;
            }
        }

        if (!matched) {
            not_matched += 1;
        } else {
            yes_matched += 1;
        }

        if (!matched && bench_pair_input_storage[ri][0] == '\0'
            && bench_input_length_class_min(input_class) > 0) {
            bench_generate_random_input(bench_pair_input_storage[ri],
                                        input_class, seed);
        }

        bench_pair_input_cases[ri].name = bench_pair_input_names[ri];
        bench_pair_input_cases[ri].input = bench_pair_input_storage[ri];
        bench_pair_input_cases[ri].length_class = input_class;
    }

    PRINTLN(not_matched);
    PRINTLN(yes_matched);

    input_bucket->name = bench_input_length_class_name(input_class);
    input_bucket->length_class = input_class;
    input_bucket->max_input_len = max_len;
    input_bucket->cases = bench_pair_input_cases;
    input_bucket->count = regex_bucket->count;
    return;
}

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

static void __attribute((noreturn))
bench_usage(char *argv0) {
    fprintf(stderr,
            "Usage: %s [--max-input-len N]\n"
            "       %s [N]\n"
            "\n"
            "N must be between 16 and %d. The benchmark runs only random "
            "input buckets whose max length is <= N.\n",
            argv0, argv0, BENCH_RANDOM_INPUT_MAX_LEN);
    exit(EXIT_FAILURE);
}

static void
bench_parse_args(int32 argc, char **argv) {
    for (int32 i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        char *value = NULL;
        int32 parsed = 0;

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            bench_usage(argv[0]);
        }

        if (strcmp(arg, "--max-input-len") == 0) {
            i += 1;
            if (i >= argc) {
                bench_usage(argv[0]);
            }
            value = argv[i];
        } else if (strncmp32(arg, "--max-input-len=", 16) == 0) {
            value = arg + 16;
        } else if (arg[0] != '-') {
            value = arg;
        } else {
            bench_usage(argv[0]);
        }

        if (!bench_parse_input_len_cap(value, &parsed)) {
            bench_usage(argv[0]);
        }
        bench_max_input_len = parsed;
    }

    return;
}

static void
bench_run_main_regex_buckets(llong now) {
    uint32 seed = 0xC0FFEEu;

    bench_build_runtime_regex_buckets();

    for (int32 bi = 0; bi < bench_runtime_regex_bucket_count; bi += 1) {
        BenchRegexBucket *regex_bucket = &bench_runtime_regex_buckets[bi];
        regex_t *compiled = NULL;
        char dispatch_csv_file[1024];
        char matchers_csv_file[1024];
        FILE *dispatch_csv;
        FILE *matchers_csv;
        char *feature_name
            = bench_feature_class_name(regex_bucket->feature_class);
        char *regex_len_name
            = bench_length_class_name(regex_bucket->length_class);

        SNPRINTF(dispatch_csv_file,
                 "benchmarks/%s-regex_ops_%s-libc_vs_meta-%lld.csv",
                 feature_name, regex_len_name, now);
        SNPRINTF(matchers_csv_file,
                 "benchmarks/%s-regex_ops_%s-meta_matchers-%lld.csv",
                 feature_name, regex_len_name, now);

        dispatch_csv = bench_open_csv(dispatch_csv_file);
        matchers_csv = bench_open_csv(matchers_csv_file);
        compiled = bench_compile_regex_bucket(regex_bucket);

        for (int32 ii = 0; ii < BENCH_INPUT_LEN_LAST; ii += 1) {
            BenchInputBucket input_bucket;

            if (bench_input_length_class_max(ii) > bench_max_input_len) {
                continue;
            }

            bench_build_pair_input_bucket(regex_bucket, compiled, ii, &seed,
                                          &input_bucket);
            bench_libc_vs_dispatch_pairwise(dispatch_csv, regex_bucket,
                                            &input_bucket, compiled);
            bench_meta_matchers_pairwise(matchers_csv, regex_bucket,
                                         &input_bucket, compiled, false);
#if META_BENCH_ENABLE_EXTRACT_VARIANTS
            bench_meta_matchers_pairwise(matchers_csv, regex_bucket,
                                         &input_bucket, compiled, true);
#endif
        }

        bench_free_regex_bucket(compiled, regex_bucket);
        bench_close_csv(dispatch_csv, dispatch_csv_file);
        bench_close_csv(matchers_csv, matchers_csv_file);
    }
    return;
}

typedef struct BenchLoadedInputBucket {
    BenchInputBucket bucket;
    BenchInputCase *cases;
    int32 count;
} BenchLoadedInputBucket;

static char *
bench_dup_cstr(char *s) {
    int32 len = strlen32(s);
    char *copy = malloc2(len + 1);
    memcpy64(copy, s, len + 1);
    return copy;
}

static char *
bench_make_generated_input_name(char *array_name, int32 index) {
    char name[512];

    SNPRINTF(name, "%s_input_%d", array_name, index);
    return bench_dup_cstr(name);
}

static char *
bench_dup_slice(char *s, int32 len) {
    char *copy = malloc2(len + 1);

    if (len > 0) {
        memcpy64(copy, s, len);
    }
    copy[len] = '\0';
    return copy;
}

static void __attribute((noreturn))
bench_fail_input_file(char *path, char *reason) {
    error("Error reading generated benchmark input file %s: %s.\n", path,
          reason);
    exit(EXIT_FAILURE);
}

static void
bench_load_generated_inputs(char *array_name, char *path,
                            BenchLoadedInputBucket *loaded) {
    FILE *file;
    int64 file_size;
    int64 read_size;
    char *storage;
    int32 input_len;
    int32 input_count = 0;
    int32 start = 0;
    int32 case_index = 0;
    int32 max_input_len = 0;

    memset64(loaded, 0, SIZEOF(*loaded));

    file = fopen(path, "rb");
    if (file == NULL) {
        bench_fail_input_file(path, strerror(errno));
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        bench_fail_input_file(path, strerror(errno));
    }

    file_size = ftell(file);
    if (file_size < 0) {
        bench_fail_input_file(path, strerror(errno));
    }
    if (file_size > 2147483646L) {
        bench_fail_input_file(path, "file is too large");
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        bench_fail_input_file(path, strerror(errno));
    }

    storage = malloc2(file_size + 1);
    read_size = fread64(storage, 1, file_size, file);
    if (read_size != file_size) {
        bench_fail_input_file(path, strerror(errno));
    }
    storage[file_size] = '\0';

    if (fclose(file) != 0) {
        bench_fail_input_file(path, strerror(errno));
    }

    input_len = (int32)file_size;
    if (input_len == 0) {
        bench_fail_input_file(path, "file contains no inputs");
    }

    for (int32 i = 0; i < input_len; i += 1) {
        if (storage[i] == '\n') {
            input_count += 1;
        }
    }
    if (storage[input_len - 1] != '\n') {
        input_count += 1;
    }

    loaded->cases = malloc2(SIZEOF(*loaded->cases)*input_count);
    loaded->count = input_count;

    for (int32 i = 0; i < input_len; i += 1) {
        if (storage[i] != '\n') {
            continue;
        }

        int32 len = i + 1 - start;
        loaded->cases[case_index].name
            = bench_make_generated_input_name(array_name, case_index);
        loaded->cases[case_index].input = bench_dup_slice(storage + start, len);
        if (len > max_input_len) {
            max_input_len = len;
        }
        case_index += 1;
        start = i + 1;
    }

    if (start < input_len) {
        int32 len = input_len - start;
        loaded->cases[case_index].name
            = bench_make_generated_input_name(array_name, case_index);
        loaded->cases[case_index].input = bench_dup_slice(storage + start, len);
        if (len > max_input_len) {
            max_input_len = len;
        }
        case_index += 1;
    }

    if (case_index != input_count) {
        bench_fail_input_file(path, "internal input splitting error");
    }

    loaded->bucket.name = array_name;
    loaded->bucket.length_class = 0;
    loaded->bucket.max_input_len = max_input_len;
    loaded->bucket.cases = loaded->cases;
    loaded->bucket.count = loaded->count;

    free2(storage, file_size + 1);
    return;
}

static void
bench_free_generated_inputs(BenchLoadedInputBucket *loaded) {
    if (loaded == NULL || loaded->cases == NULL) {
        return;
    }

    for (int32 i = 0; i < loaded->count; i += 1) {
        if (loaded->cases[i].name != NULL) {
            free2(loaded->cases[i].name, strlen32(loaded->cases[i].name) + 1);
        }
        if (loaded->cases[i].input != NULL) {
            free2(loaded->cases[i].input, strlen32(loaded->cases[i].input) + 1);
        }
    }

    free2(loaded->cases, SIZEOF(*loaded->cases)*loaded->count);
    memset64(loaded, 0, SIZEOF(*loaded));
    return;
}

static void
bench_run_generated_pattern_buckets(llong now) {
    for (int32 bi = 0; bi < GENERATED_BENCH_REGEX_BUCKET_COUNT; bi += 1) {
        GeneratedBenchRegexBucket *generated
            = &generated_bench_regex_buckets[bi];
        BenchRegexBucket *regex_bucket = &generated->regex_bucket;
        BenchLoadedInputBucket loaded_input;
        BenchInputBucket *input_bucket;
        regex_t *compiled = NULL;
        char dispatch_csv_file[1024];
        char matchers_csv_file[1024];
        FILE *dispatch_csv;
        FILE *matchers_csv;

        if (regex_bucket->count <= 0) {
            continue;
        }

        bench_load_generated_inputs(generated->array_name,
                                    generated->input_path, &loaded_input);
        input_bucket = &loaded_input.bucket;

        SNPRINTF(dispatch_csv_file,
                 "benchmarks/generated_%s-libc_vs_meta-%lld.csv",
                 generated->array_name, now);
        SNPRINTF(matchers_csv_file,
                 "benchmarks/generated_%s-meta_matchers-%lld.csv",
                 generated->array_name, now);

        dispatch_csv = bench_open_csv(dispatch_csv_file);
        matchers_csv = bench_open_csv(matchers_csv_file);
        compiled = bench_compile_regex_bucket(regex_bucket);

        bench_libc_vs_dispatch(dispatch_csv, regex_bucket, input_bucket,
                               compiled);
        bench_meta_matchers(matchers_csv, regex_bucket, input_bucket, compiled,
                            false);
#if META_BENCH_ENABLE_EXTRACT_VARIANTS
        bench_meta_matchers(matchers_csv, regex_bucket, input_bucket, compiled,
                            true);
#endif

        bench_free_regex_bucket(compiled, regex_bucket);
        bench_close_csv(dispatch_csv, dispatch_csv_file);
        bench_close_csv(matchers_csv, matchers_csv_file);
        bench_free_generated_inputs(&loaded_input);
    }
    return;
}

int32
main(int32 argc, char **argv) {
    llong now;

    bench_parse_args(argc, argv);
    setlocale(LC_ALL, "C");
    mkdir("benchmarks", 0777);

    printf("bench_max_input_len=%d\n", bench_max_input_len);
    now = (llong)time(NULL);

    bench_run_main_regex_buckets(now);

    /* bench_run_generated_pattern_buckets(now); */

    printf("bench_sink_result=%d bench_sink_offsets=%lld\n", bench_sink_result,
           (llong)bench_sink_offsets);

    return 0;
}
