#include <assert.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>

#include "util.c"
#include "meta_regex.h"
#include "meta_regex_match.c"

#include "meta_tests.h"
#include "gen/meta_tests_array2.h"
#include "utf8.h"

#if !defined(error2)
#define error2(...) fprintf(stderr, __VA_ARGS__)
#endif

#if !defined(BENCHMARK)
#define BENCHMARK 0
#endif

static void
run_posix_vs_meta(RegexTest *tests, int32 count, char *description) {
    struct timespec t0_posix;
    struct timespec t1_posix;
    struct timespec t0_meta;
    struct timespec t1_meta;
    RegexTest *tests_posix = xmemdup(tests, count * SIZEOF(RegexTest));
    RegexTest *tests_meta = xmemdup(tests, count * SIZEOF(RegexTest));

    printf("\n----- Running %s (POSIX vs Meta) -----\n", description);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_posix);
    for (int32 i = 0; i < count; i += 1) {
        regex_t compiled_regex;
        char *input = tests_posix[i].input;
        char *regex = tests_posix[i].meta_regex->string;
        int32 compiled = regcomp(&compiled_regex, regex, REG_EXTENDED);

        if (compiled != 0) {
            char error_message[256];
            regerror(compiled, &compiled_regex, error_message,
                     SIZEOF(error_message));
            error("Regex compilation failed: %s\n", error_message);
            exit(EXIT_FAILURE);
        }
        tests_posix[i].result = regexec(&compiled_regex, input, MAX_MATCHES,
                                        tests_posix[i].pmatch, 0);
        regfree(&compiled_regex);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_posix);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_meta);
    for (int32 i = 0; i < count; i += 1) {
        char *input = tests_meta[i].input;
        MetaRegex *meta_regex = tests_meta[i].meta_regex;

        tests_meta[i].result = meta_regex_match(meta_regex, input, MAX_MATCHES,
                                                tests_meta[i].pmatch);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_meta);

    for (int32 i = 0; i < count; i += 1) {
        RegexTest tp = tests_posix[i];
        RegexTest tm = tests_meta[i];
        char *regex = tests[i].meta_regex->string;
        char *input = tests[i].input;

        if (tp.result != tm.result) {
            error2("Error: result mismatch for input " RED(
                       "\"%s\"") " against regex " BLUE("\"%s\"") "\n",
                   input, regex);
            error2("posix: %d, meta: %d\n", tp.result, tm.result);
        } else if (tp.result == 0) {
            for (int32 m = 0; m < MAX_MATCHES; m += 1) {
                regmatch_t p_m = tp.pmatch[m];
                regmatch_t m_m = tm.pmatch[m];

                if (p_m.rm_so != m_m.rm_so || p_m.rm_eo != m_m.rm_eo) {
                    error2("Mismatch in capture group %d:\ninput " RED(
                               "%s") " against regex " BLUE("%s") "\n",
                           m, input, regex);
                    error2("posix: rm_so=%d, rm_eo=%d\n", p_m.rm_so, p_m.rm_eo);
                    error2("meta:  rm_so=%d, rm_eo=%d\n", m_m.rm_so, m_m.rm_eo);
                }
            }
        }
    }

    PRINT_TIMINGS(count, t0_posix, t1_posix, "posix");
    PRINT_TIMINGS(count, t0_meta, t1_meta, "meta");

    free2(tests_posix, count * SIZEOF(RegexTest));
    free2(tests_meta, count * SIZEOF(RegexTest));
    return;
}

static void
run_meta_only(RegexTest *tests, int32 count, char *description) {
    struct timespec t0;
    struct timespec t1;
    printf("\n----- Running %s (Meta Only) -----\n", description);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    for (int32 i = 0; i < count; i += 1) {
        char *input = tests[i].input;
        MetaRegex *meta_regex = tests[i].meta_regex;
        int32 result;
        bool matched;

        result
            = meta_regex_match(meta_regex, input, MAX_MATCHES, tests[i].pmatch);
        matched = !result;
        bool expected = (bool)tests[i].result;

        if (matched != expected) {
            error2("Error: expectation mismatch for input " RED(
                       "\"%s\"") " against regex " BLUE("\"%s\"") "\n",
                   input, meta_regex->string);
            error2("expected: %s, got: %s\n", expected ? "MATCH" : "NOMATCH",
                   matched ? "MATCH" : "NOMATCH");
        } else {
            printf(RED("%15s") " against " BLUE("%18s") ": %s (OK)\n", input,
                   meta_regex->string, matched ? "MATCH" : "NOMATCH");
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

    PRINT_TIMINGS(count, t0, t1, "meta (exclusive)");
    return;
}

static void
run_fuzzy_tests(int32 max_str_size, int32 ntests) {
    printf("\n----- Starting Fuzzy Testing (ASCII input) -----\n");
    printf("----- max_str_size: %d\n", max_str_size);
    int32 fuzzy_len;
    FuzzyTest *fuzzy;
    FILE *mismatches;
    struct timespec t0_posix;
    struct timespec t1_posix;
    struct timespec t0_meta;
    struct timespec t1_meta;

    fuzzy_len = ntests;
    fuzzy = malloc2(SIZEOF(*fuzzy) * fuzzy_len);

    if ((mismatches = fopen("mismatches_ascii.txt", "w")) == NULL) {
        error("Error opening file: %s.\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    for (int32 i = 0; i < fuzzy_len; i += 1) {
        fuzzy[i].string_len = 1 + (rand() % max_str_size);
        fuzzy[i].input = malloc2(fuzzy[i].string_len + 1);
        ascii_random_string(fuzzy[i].input, fuzzy[i].string_len);
        fuzzy[i].regex_idx = rand() % LENGTH(ascii_against_ascii);
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_posix);
    for (int32 i = 0; i < fuzzy_len; i += 1) {
        regex_t compiled;
        char *pattern_str;
        pattern_str = 
            ascii_against_ascii[fuzzy[i].regex_idx].meta_regex->string;
        if (regcomp(&compiled, pattern_str, REG_EXTENDED) == 0) {
            fuzzy[i].result_posix = regexec(&compiled, fuzzy[i].input, 
                                            MAX_MATCHES, 
                                            fuzzy[i].pmatch_posix, 0);
            regfree(&compiled);
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_posix);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_meta);
    for (int32 i = 0; i < fuzzy_len; i += 1) {
        MetaRegex *meta_pattern;
        meta_pattern = ascii_against_ascii[fuzzy[i].regex_idx].meta_regex;
        fuzzy[i].result_meta = meta_regex_match(meta_pattern, fuzzy[i].input, 
                                                MAX_MATCHES, 
                                                fuzzy[i].pmatch_meta);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_meta);

    for (int32 i = 0; i < fuzzy_len; i += 1) {
        char *input;
        char *regex;
        int32 mismatch;

        input = fuzzy[i].input;
        regex = ascii_against_ascii[fuzzy[i].regex_idx].meta_regex->string;
        mismatch = 0;

        if (fuzzy[i].result_posix != fuzzy[i].result_meta) {
            mismatch = 1;
        } else if (fuzzy[i].result_posix == 0) {
            for (int32 m = 0; m < MAX_MATCHES; m += 1) {
                if (fuzzy[i].pmatch_posix[m].rm_so != 
                    fuzzy[i].pmatch_meta[m].rm_so ||
                    fuzzy[i].pmatch_posix[m].rm_eo != 
                    fuzzy[i].pmatch_meta[m].rm_eo) {
                    mismatch = 1;
                    break;
                }
            }
        }

        if (mismatch) {
            fprintf(mismatches, "Error: mismatch for input \"%s\" "
                    "against regex \"%s\"\n", input, regex);
            fprintf(mismatches, "posix res: %d, meta res: %d\n",
                    fuzzy[i].result_posix, fuzzy[i].result_meta);

            if (fuzzy[i].result_posix == 0 && fuzzy[i].result_meta == 0) {
                for (int32 m = 0; m < MAX_MATCHES; m += 1) {
                    regmatch_t p_m;
                    regmatch_t m_m;

                    p_m = fuzzy[i].pmatch_posix[m];
                    m_m = fuzzy[i].pmatch_meta[m];

                    if (p_m.rm_so != m_m.rm_so || p_m.rm_eo != m_m.rm_eo) {
                        fprintf(mismatches, "  Group %d: posix[%d, %d], "
                                "meta[%d, %d]\n", m, (int32)p_m.rm_so, 
                                (int32)p_m.rm_eo, (int32)m_m.rm_so, 
                                (int32)m_m.rm_eo);
                    }
                }
            }
            fprintf(mismatches, "------------------------------------\n");
        }
    }

    {
        PRINT_TIMINGS(fuzzy_len, t0_posix, t1_posix, "fuzzy ascii posix");
        PRINT_TIMINGS(fuzzy_len, t0_meta, t1_meta, "fuzzy ascii meta");
    }

    for (int32 i = 0; i < fuzzy_len; i += 1) {
        free2(fuzzy[i].input, fuzzy[i].string_len + 1);
    }
    free2(fuzzy, SIZEOF(*fuzzy) * fuzzy_len);
    fclose(mismatches);
    return;
}

int
main(void) {
    setlocale(LC_ALL, "C");
    srand((uint)42);

#if !BENCHMARK
    run_posix_vs_meta(ascii_against_ascii, LENGTH(ascii_against_ascii),
                      "ASCII vs ASCII");
    run_posix_vs_meta(utf8_against_ascii, LENGTH(utf8_against_ascii),
                      "UTF8 vs ASCII");
    run_meta_only(utf8_against_utf8, LENGTH(utf8_against_utf8), "UTF8 vs UTF8");
#endif

    /* run_fuzzy_tests( 16,  1000); */
    /* run_fuzzy_tests( 32,  1000); */
    /* run_fuzzy_tests( 64,  1000); */
    /* run_fuzzy_tests(128,  1000); */
    /* run_fuzzy_tests(256,  1000); */
    /* run_fuzzy_tests(512,  1000); */
    /* run_fuzzy_tests(1024, 1000); */
    /* run_fuzzy_tests(2048, 1000); */
    run_fuzzy_tests(4096, 10000);
    exit(EXIT_SUCCESS);
}
