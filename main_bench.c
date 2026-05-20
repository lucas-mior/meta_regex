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
#include "main_bench_inputs.h"

#if !defined(error2)
#define error2(...) error2(__VA_ARGS__)
#endif

#if !defined(META_BENCH_ITERATIONS)
#define META_BENCH_ITERATIONS 100
#endif

#if !defined(META_BENCH_WARMUP_ITERATIONS)
#define META_BENCH_WARMUP_ITERATIONS 64
#endif

#define BENCH_MAX_MATCHES 16

#if !defined(ENABLE_BTNFA)
#define ENABLE_BTNFA 1
#endif
#if !defined(ENABLE_TNFA)
#define ENABLE_TNFA 1
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
    error2("%s mismatch in regex bucket %s, input bucket %s, regex case %s, "
           "input case %s, engine %s",
           block, regex_bucket->name, input_bucket->name, regex_case->name,
           input_case->name, engine);
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
        bench_sink_offsets += (int64)pmatch[0].rm_so + (int64)pmatch[0].rm_eo;
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
bench_write_row(FILE *csv, char *block, char *variant,
                BenchRegexBucket *regex_bucket, BenchInputBucket *input_bucket,
                char *engine, char *matcher_name, char *selected_matcher_name,
                int32 regex_cases, int32 input_cases, int32 run_pair_count,
                int32 iterations_per_pair, double seconds, int32 matches) {
    int64 total_iterations = (int64)iterations_per_pair*(int64)run_pair_count;
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
            regex_cases*input_cases, run_pair_count, iterations_per_pair,
            (llong)total_iterations, seconds, ns_per_match, matches);
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
                       BenchInputBucket *input_bucket, regex_t *compiled,
                       bool extract) {
    enum Matcher enabled = bench_enabled_matcher_mask();
    char *variant = extract ? "extract" : "no_extract";
    int32 pair_count = regex_bucket->count*input_bucket->count;
    int32 matches = 0;
    double seconds;

    printf("\n== libc vs meta dispatcher: %s / %s (%s) ==\n",
           regex_bucket->name, input_bucket->name, variant);

    bench_validate_dispatch(regex_bucket, input_bucket, compiled, extract);

    seconds = bench_time_libc_bucket(regex_bucket, input_bucket, compiled,
                                     extract, META_BENCH_ITERATIONS, &matches);
    bench_write_row(csv, "libc_vs_dispatch", variant, regex_bucket,
                    input_bucket, "LIBC", "LIBC", "LIBC", regex_bucket->count,
                    input_bucket->count, pair_count, META_BENCH_ITERATIONS,
                    seconds, matches);

    seconds
        = bench_time_dispatch_bucket(regex_bucket, input_bucket, extract,
                                     enabled, META_BENCH_ITERATIONS, &matches);
    bench_write_row(csv, "libc_vs_dispatch", variant, regex_bucket,
                    input_bucket, "META_DISPATCH", "DISPATCH", "mixed",
                    regex_bucket->count, input_bucket->count, pair_count,
                    META_BENCH_ITERATIONS, seconds, matches);
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

int32
main(void) {
    llong now;

    setlocale(LC_ALL, "C");
    mkdir("benchmarks", 0777);

    now = (llong)time(NULL);

    for (int32 bi = 0; bi < BENCH_REGEX_BUCKET_COUNT; bi += 1) {
        BenchRegexBucket *regex_bucket = &bench_regex_buckets[bi];
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
                 "benchmarks/%s-regex_%s-libc_vs_meta-%lld.csv", feature_name,
                 regex_len_name, now);
        SNPRINTF(matchers_csv_file,
                 "benchmarks/%s-regex_%s-meta_matchers-%lld.csv", feature_name,
                 regex_len_name, now);

        dispatch_csv = bench_open_csv(dispatch_csv_file);
        matchers_csv = bench_open_csv(matchers_csv_file);
        compiled = bench_compile_regex_bucket(regex_bucket);

        for (int32 ii = 0; ii < BENCH_INPUT_BUCKET_COUNT; ii += 1) {
            BenchInputBucket *input_bucket = &bench_input_buckets[ii];

            bench_libc_vs_dispatch(dispatch_csv, regex_bucket, input_bucket,
                                   compiled, false);
            bench_libc_vs_dispatch(dispatch_csv, regex_bucket, input_bucket,
                                   compiled, true);
            bench_meta_matchers(matchers_csv, regex_bucket, input_bucket,
                                compiled, false);
            bench_meta_matchers(matchers_csv, regex_bucket, input_bucket,
                                compiled, true);
        }

        bench_free_regex_bucket(compiled, regex_bucket);
        bench_close_csv(dispatch_csv, dispatch_csv_file);
        bench_close_csv(matchers_csv, matchers_csv_file);
    }

    printf("bench_sink_result=%d bench_sink_offsets=%lld\n", bench_sink_result,
           (llong)bench_sink_offsets);

    return 0;
}
