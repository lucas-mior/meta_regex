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

#include "meta_tests.h"
#include "gen/meta_tests_array2.h"

int
main(void) {
    struct timespec t0_posix;
    struct timespec t1_posix;
    struct timespec t0_meta;
    struct timespec t1_meta;

    RegexTest *tests_posix = xmemdup(ascii_tests, SIZEOF(ascii_tests));
    RegexTest *tests_meta = xmemdup(ascii_tests, SIZEOF(ascii_tests));

    setlocale(LC_ALL, "C");  // Only test against this kind of posix regex
    srand((uint)42);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_posix);
    for (int32 i = 0; i < LENGTH(ascii_tests); i += 1) {
        regex_t compiled_regex;
        char *input = tests_posix[i].input;
        char *regex = tests_posix[i].meta_regex->string;
        int32 compiled;

        compiled = regcomp(&compiled_regex, regex, REG_EXTENDED);
        if (compiled != 0) {
            char error_message[256];
            regerror(compiled, &compiled_regex,
                     error_message, sizeof(error_message));
            error("Regex compilation failed: %s\n", error_message);
            exit(EXIT_FAILURE);
        }
        tests_posix[i].result = regexec(&compiled_regex, input,
                                        MAX_MATCHES, tests_posix[i].pmatch, 0);
        regfree(&compiled_regex);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_posix);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_meta);
    for (int32 i = 0; i < LENGTH(ascii_tests); i += 1) {
        char *input = tests_meta[i].input;
        MetaRegex *meta_regex = tests_meta[i].meta_regex;

        tests_meta[i].result
            = meta_regex_match(meta_regex, input,
                               MAX_MATCHES, tests_meta[i].pmatch);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_meta);

    for (int32 i = 0; i < LENGTH(ascii_tests); i += 1) {
        RegexTest test_posix = tests_posix[i];
        RegexTest test_meta = tests_meta[i];
        char *regex = ascii_tests[i].meta_regex->string;
        char *input = ascii_tests[i].input;

        printf(RED("%15s")" against "BLUE("%18s")": %d\n",
               input, regex, test_posix.result);
        if (test_posix.result != test_meta.result) {
            error("Error: result mismatch for input "
                  RED("\"%s\"")" against regex "BLUE("\"%s\"")"\n",
                  input, regex);
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

                error("Mismatch:\n"
                      "input "RED("%s")" against regex "BLUE("%s")" (group %d)\n",
                      input, regex, m);

                error("posix: rm_so=%d, rm_eo=%d\n",
                      posix_match.rm_so, posix_match.rm_eo);
                error("meta: rm_so=%d, rm_eo=%d\n",
                      meta_match.rm_so, meta_match.rm_eo);
            }
        }
    }

    PRINT_TIMINGS(LENGTH(ascii_tests), t0_posix, t1_posix, "posix tests");
    PRINT_TIMINGS(LENGTH(ascii_tests), t0_meta, t1_meta, "meta tests");

    free2(tests_posix, SIZEOF(ascii_tests));
    free2(tests_meta, SIZEOF(ascii_tests));

    printf("\n----- Starting Fuzzy Testing (ASCII input) -----\n");
    {
        int32 fuzzy_len = 1000;
        FuzzyTest *fuzzy = malloc2(SIZEOF(*fuzzy)*fuzzy_len);
        FILE *mismatches;

        if ((mismatches = fopen("mismatches_ascii.txt", "w")) == NULL) {
            error("Error opening file: %s.\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        for (int32 i = 0; i < fuzzy_len; i += 1) {
            fuzzy[i].string_len = 1 + (rand() % 4096);
            fuzzy[i].input = malloc2(fuzzy[i].string_len + 1);
            ascii_random_string(fuzzy[i].input, fuzzy[i].string_len);
            fuzzy[i].regex_idx = rand() % LENGTH(ascii_tests);
        }

        clock_gettime(CLOCK_MONOTONIC_RAW, &t0_posix);
        for (int32 i = 0; i < fuzzy_len; i += 1) {
            regex_t compiled;
            int32 comp_error;
            char *pattern_str;

            pattern_str = ascii_tests[fuzzy[i].regex_idx].meta_regex->string;
            if ((comp_error = regcomp(&compiled, pattern_str, REG_EXTENDED))) {
                error("Error compiling %s: %s.\n", pattern_str, strerror(errno));
                fatal(EXIT_FAILURE);
            }
            fuzzy[i].result_posix
                = regexec(&compiled, fuzzy[i].input,
                          MAX_MATCHES, fuzzy[i].pmatch_posix, 0);
            regfree(&compiled);
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1_posix);

        clock_gettime(CLOCK_MONOTONIC_RAW, &t0_meta);
        for (int32 i = 0; i < fuzzy_len; i += 1) {
            MetaRegex *meta_pattern = ascii_tests[fuzzy[i].regex_idx].meta_regex;

            fuzzy[i].result_meta
                = meta_regex_match(meta_pattern,
                                   fuzzy[i].input,
                                   MAX_MATCHES,
                                   fuzzy[i].pmatch_meta);
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1_meta);

        for (int32 i = 0; i < fuzzy_len; i += 1) {
            int32 result_posix = fuzzy[i].result_posix;
            int32 result_meta = fuzzy[i].result_meta;
            if (result_posix != result_meta) {
                char *input = fuzzy[i].input;
                char *regex = ascii_tests[fuzzy[i].regex_idx].meta_regex->string;

                error("Mismatch:\n"
                      "input "RED("\"%s\"")" against regex "BLUE("\"%s\"")"\n",
                      input, regex);
                error("posix: %d, meta: %d\n", result_posix, result_meta);
                fprintf(mismatches, "%s against %s [posix=%d][meta=%d]\n",
                                    input, regex, result_posix, result_meta);
            }
        }
        PRINT_TIMINGS(fuzzy_len, t0_posix, t1_posix, "fuzzy ascii posix");
        PRINT_TIMINGS(fuzzy_len, t0_meta, t1_meta, "fuzzy ascii meta");

        for (int32 i = 0; i < fuzzy_len; i += 1) {
            free2(fuzzy[i].input, fuzzy[i].string_len + 1);
        }
        free2(fuzzy, SIZEOF(*fuzzy)*fuzzy_len);
        fclose(mismatches);
    }

    exit(EXIT_SUCCESS);
}
