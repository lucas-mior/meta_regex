#include <assert.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <time.h>
#include <sys/wait.h>

#include "util.c"
#include "meta_regex.h"
#include "meta_regex_match.c"

#define NFUZZY 1000

#include "meta_tests.h"
#include "gen/meta_tests_array2.h"

int
main(void) {
    struct timespec t0_posix;
    struct timespec t1_posix;
    struct timespec t0_meta;
    struct timespec t1_meta;
    RegexTest *tests_posix = xmemdup(regex_tests, SIZEOF(regex_tests));
    RegexTest *tests_meta = xmemdup(regex_tests, SIZEOF(regex_tests));
    /* setlocale(LC_ALL, ""); */
    srand((uint)42);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_posix);
    for (int32 i = 0; i < LENGTH(regex_tests); i += 1) {
        regex_t compiled_regex;
        char *string = tests_posix[i].string;
        char *regex = tests_posix[i].meta_regex->string;
        int32 compile_status;

        compile_status = regcomp(&compiled_regex, regex, REG_EXTENDED);
        if (compile_status != 0) {
            char error_message[256];
            regerror(compile_status, &compiled_regex,
                     error_message, sizeof(error_message));
            error("Regex compilation failed: %s\n", error_message);
            exit(EXIT_FAILURE);
        }
        tests_posix[i].result
            = regexec(&compiled_regex, string,
                      MAX_MATCHES, tests_posix[i].pmatch, 0);
        regfree(&compiled_regex);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_posix);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_meta);
    for (int32 i = 0; i < LENGTH(regex_tests); i += 1) {
        char *string = tests_meta[i].string;
        MetaRegex *meta_regex = tests_meta[i].meta_regex;

        tests_meta[i].result
            = meta_regex_match(meta_regex, string,
                               MAX_MATCHES, tests_meta[i].pmatch);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_meta);

    for (int32 i = 0; i < LENGTH(regex_tests); i += 1) {
        RegexTest test_posix = tests_posix[i];
        RegexTest test_meta = tests_meta[i];
        char *regex = regex_tests[i].meta_regex->string;
        char *string = regex_tests[i].string;

        printf(RED("%15s")" against "BLUE("%18s")": %d\n",
                string, regex, test_posix.result);
        if (test_posix.result != test_meta.result) {
            error("Error: result mismatch for regex "
                  RED("\"%s\"")" against "BLUE("\"%s\"")"\n", regex, string);
            error("posix: %d, meta: %d\n", test_posix.result, test_meta.result);
            exit(EXIT_FAILURE);
        }
        if (test_posix.result == 0) {
            for (int32 m = 0; m < MAX_MATCHES; m += 1) {
                regmatch_t posix_match = test_posix.pmatch[m];
                regmatch_t meta_match = test_meta.pmatch[m];

                if (posix_match.rm_so == meta_match.rm_so) {
                    if (posix_match.rm_eo == meta_match.rm_eo) {
                        continue;
                    }
                }

                error("Error: mismatch in "
                      RED("%s")" against "BLUE("%s")" (group %d)\n",
                      string, regex, m);

                error("posix: rm_so=%d, rm_eo=%d\n",
                      posix_match.rm_so, posix_match.rm_eo);
                error("meta: rm_so=%d, rm_eo=%d\n",
                      meta_match.rm_so, meta_match.rm_eo);
            }
        }
    }

    PRINT_TIMINGS(LENGTH(regex_tests), t0_posix, t1_posix, "posix tests");
    PRINT_TIMINGS(LENGTH(regex_tests), t0_meta, t1_meta, "meta tests");

    printf("\n--- Starting Fuzzy Testing (%d iterations) ---\n", NFUZZY);
    {
        FuzzyTest *fuzzy_cases = malloc2(SIZEOF(FuzzyTest)*NFUZZY);

        for (int32 i = 0; i < NFUZZY; i += 1) {
            int32 length = 1 + (rand() % 4096);
            fuzzy_cases[i].string_size = length + 1;
            fuzzy_cases[i].string = malloc2(fuzzy_cases[i].string_size);
            utf8_random_string(fuzzy_cases[i].string, length);
            fuzzy_cases[i].regex_idx = rand() % LENGTH(regex_tests);
        }

        clock_gettime(CLOCK_MONOTONIC_RAW, &t0_posix);
        for (int32 i = 0; i < NFUZZY; i += 1) {
            regex_t compiled;
            int32 comp_error;
            char *pattern_str;

            pattern_str = regex_tests[fuzzy_cases[i].regex_idx].meta_regex->string;
            if ((comp_error = regcomp(&compiled, pattern_str, REG_EXTENDED))) {
                error("Error compiling %s: %s.\n", pattern_str, strerror(errno));
                fatal(EXIT_FAILURE);
            }
            fuzzy_cases[i].result_posix
                = regexec(&compiled, fuzzy_cases[i].string,
                          MAX_MATCHES, fuzzy_cases[i].pmatch_posix, 0);
            regfree(&compiled);
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1_posix);

        clock_gettime(CLOCK_MONOTONIC_RAW, &t0_meta);
        for (int32 i = 0; i < NFUZZY; i += 1) {
            MetaRegex *meta_pattern = regex_tests[fuzzy_cases[i].regex_idx].meta_regex;

            fuzzy_cases[i].result_meta
                = meta_regex_match(meta_pattern,
                                   fuzzy_cases[i].string,
                                   MAX_MATCHES,
                                   fuzzy_cases[i].pmatch_meta);
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1_meta);

        for (int32 i = 0; i < NFUZZY; i += 1) {
            int32 result_posix = fuzzy_cases[i].result_posix;
            int32 result_meta = fuzzy_cases[i].result_meta;
            if (result_posix != result_meta) {
                char *string = fuzzy_cases[i].string;
                char *regex = regex_tests[fuzzy_cases[i].regex_idx].meta_regex->string;

                error("Error: result mismatch for regex "
                      RED("\"%s\"")" against "BLUE("\"%s\"")"\n", regex, string);
                error("posix: %d, meta: %d\n", result_posix, result_meta);
                exit(EXIT_FAILURE);
            }
        }

        PRINT_TIMINGS(NFUZZY, t0_posix, t1_posix, "fuzzy posix tests");
        PRINT_TIMINGS(NFUZZY, t0_meta, t1_meta, "fuzzy meta tests");

        for (int32 i = 0; i < NFUZZY; i += 1) {
            free2(fuzzy_cases[i].string, fuzzy_cases[i].string_size);
        }
        free2(fuzzy_cases, SIZEOF(FuzzyTest)*NFUZZY);
    }

    free2(tests_posix, SIZEOF(regex_tests));
    free2(tests_meta, SIZEOF(regex_tests));

    exit(EXIT_SUCCESS);
}
