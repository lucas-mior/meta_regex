#include <assert.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>

#include "util.c"
#include "meta.h"
#include "meta_match.c"

#include "main_tests.h"
#include "gen/main_tests_array2.h"
#include "utf8.c"

#if !defined(error2)
#define error2(...) error2(__VA_ARGS__)
#endif

#if !defined(BENCHMARK)
#define BENCHMARK 0
#endif

static void run_posix_vs_meta(RegexTest *tests, int32 count, char *description);
static void run_fuzzy_tests(MetaRegex **patterns, int32 tests_len,
                            int32 max_str_size, int32 ntests);
static void run_meta_only(RegexTest *tests, int32 count, char *description);
static void run_file_fuzzy_tests(MetaRegex **tests, int32 tests_len);

#define RUN_POSIX_VS_META(ARRAY) \
    run_posix_vs_meta(ARRAY, LENGTH(ARRAY), #ARRAY)
#define RUN_FUZZY_TESTS(ARRAY, MAX_STR_SIZE, NTESTS) \
    run_fuzzy_tests(ARRAY, LENGTH(ARRAY), MAX_STR_SIZE, NTESTS)

#define FUZZY_PRECOMPILE_POSIX 1

int32
main(void) {
    setlocale(LC_ALL, "C");
    srand((uint32)42);

    RUN_POSIX_VS_META(ascii_no_group_no_backref);
    RUN_POSIX_VS_META(ascii_with_group_no_backref);
    RUN_POSIX_VS_META(ascii_with_group_and_backref);
    RUN_POSIX_VS_META(utf8_against_ascii);
    RUN_POSIX_VS_META(utf8_against_utf8);
    RUN_POSIX_VS_META(ascii_catastrophic_no_group_no_backref);
    RUN_POSIX_VS_META(ascii_catastrophic_with_group_no_backref);
    RUN_POSIX_VS_META(ascii_catastrophic_with_group_and_backref);
    /* exit(0); */

    run_meta_only(utf8_against_utf8, LENGTH(utf8_against_utf8), "utf8");

    /* printf("\n----- Starting Fuzzy Testing (ASCII input) -----\n"); */
    RUN_FUZZY_TESTS(fuzzy_patterns, 8, 200);
    RUN_FUZZY_TESTS(fuzzy_patterns, 32, 200);
    /* RUN_FUZZY_TESTS(fuzzy_patterns, 64, 200); */
    /* RUN_FUZZY_TESTS(fuzzy_patterns, 128, 200); */
    /* RUN_FUZZY_TESTS(fuzzy_patterns, 256, 200); */
    /* RUN_FUZZY_TESTS(fuzzy_patterns, 512, 200); */
    /* RUN_FUZZY_TESTS(fuzzy_patterns, 1024, 200); */
    /* RUN_FUZZY_TESTS(fuzzy_patterns, 2048, 200); */
    /* RUN_FUZZY_TESTS(fuzzy_patterns, 4096, 200); */
    /* RUN_FUZZY_TESTS(fuzzy_patterns, 8192, 100); */

    printf("\n----- Starting Fuzzy Testing (File input) -----\n");
    run_file_fuzzy_tests(fuzzy_patterns, LENGTH(fuzzy_patterns));

    exit(EXIT_SUCCESS);
}

#undef RUN_FUZZY_TESTS
#undef RUN_POSIX_VS_META

static void
run_posix_vs_meta(RegexTest *tests, int32 count, char *description) {
    struct timespec t0_posix;
    struct timespec t1_posix;
    struct timespec t0_meta;
    struct timespec t1_meta;
    RegexTest *tests_posix = xmemdup(tests, count*SIZEOF(RegexTest));
    RegexTest *tests_meta = xmemdup(tests, count*SIZEOF(RegexTest));
    bool failed = false;

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
            error("Regex compilation failed for" RED("\"%s\"") ": %s\n", regex,
                  error_message);
            exit(EXIT_FAILURE);
        }
        tests_posix[i].result = regexec(&compiled_regex, input, MAX_MATCHES,
                                        tests_posix[i].pmatch, 0);
        regfree(&compiled_regex);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_posix);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_meta);
    for (int32 i = 0; i < count; i += 1) {
        uint8 *input = (uint8 *)tests_meta[i].input;
        int32 input_len = strlen32((char *)input);
        MetaRegex *meta_regex = tests_meta[i].meta_regex;

        tests_meta[i].result = meta_regex_match(
            meta_regex, input, input_len, MAX_MATCHES, tests_meta[i].pmatch);
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
            failed = true;
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
                    failed = true;
                }
            }
        }
    }

    if (failed) {
        exit(EXIT_FAILURE);
    }

    PRINT_TIMINGS(count, t0_posix, t1_posix, "posix");
    PRINT_TIMINGS(count, t0_meta, t1_meta, "meta");

    free2(tests_posix, count*SIZEOF(RegexTest));
    free2(tests_meta, count*SIZEOF(RegexTest));
    return;
}

static void
run_meta_only(RegexTest *tests, int32 count, char *description) {
    struct timespec t0;
    struct timespec t1;
    printf("\n----- Running %s (Meta Only) -----\n", description);
    bool failed = false;

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    for (int32 i = 0; i < count; i += 1) {
        char *input = tests[i].input;
        int32 input_len = strlen32((char *)input);
        MetaRegex *meta_regex = tests[i].meta_regex;
        int32 result = meta_regex_match(meta_regex, (uint8 *)input, input_len,
                                        MAX_MATCHES, tests[i].pmatch);
        bool matched = !result;
        bool expected = (bool)tests[i].result;

        if (matched != expected) {
            error2("Error: expectation mismatch for input " RED(
                       "\"%s\"") " against regex " BLUE("\"%s\"") "\n",
                   input, meta_regex->string);
            error2("expected: %s, got: %s\n", expected ? "MATCH" : "NOMATCH",
                   matched ? "MATCH" : "NOMATCH");
            failed = true;
        } else {
            printf(RED("%15s") " against " BLUE("%18s") ": %s (OK)\n", input,
                   meta_regex->string, matched ? "MATCH" : "NOMATCH");
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

    if (failed) {
        exit(EXIT_FAILURE);
    }

    PRINT_TIMINGS(count, t0, t1, "meta (exclusive)");
    return;
}

static void
run_fuzzy_tests(MetaRegex **tests, int32 tests_len, int32 max_str_size,
                int32 ntests) {
    int32 fuzzy_len = ntests*tests_len;
    FuzzyTest *fuzzy = malloc2(SIZEOF(*fuzzy)*fuzzy_len);
    struct timespec t0_posix;
    struct timespec t1_posix;
    struct timespec t0_meta;
    struct timespec t1_meta;
    bool failed = false;
#if FUZZY_PRECOMPILE_POSIX
    regex_t *posix_regexes = malloc2(tests_len*SIZEOF(regex_t));
#endif

    for (int32 i = 0; i < ntests; i += 1) {
        int32 input_len = 1 + (rand() % max_str_size);
        char *input = malloc2(input_len + 1);

        random_ascii_string(input, input_len, 1);

        for (int32 j = 0; j < tests_len; j += 1) {
            int32 idx = i*tests_len + j;

            fuzzy[idx].input_len = input_len;
            fuzzy[idx].input = input;
            fuzzy[idx].regex_idx = j;
        }
    }

#if FUZZY_PRECOMPILE_POSIX
    for (int32 i = 0; i < tests_len; i += 1) {
        char *pattern_str = tests[i]->string;

        if (regcomp(&posix_regexes[i], pattern_str, REG_EXTENDED) != 0) {
            error("Pre-compilation failed for " BLUE("\"%s\"") "\n",
                  pattern_str);
            exit(EXIT_FAILURE);
        }
    }
#endif

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_posix);
    for (int32 i = 0; i < fuzzy_len; i += 1) {
#if FUZZY_PRECOMPILE_POSIX
        int32 idx = fuzzy[i].regex_idx;
        fuzzy[i].result_posix = regexec(&posix_regexes[idx], fuzzy[i].input,
                                        MAX_MATCHES, fuzzy[i].pmatch_posix, 0);
#else
        regex_t compiled;
        char *pattern_str = tests[fuzzy[i].regex_idx]->string;

        if (regcomp(&compiled, pattern_str, REG_EXTENDED)) {
            error("Pre-compilation failed for " BLUE("\"%s\"") "\n",
                  pattern_str);
            exit(EXIT_FAILURE);
        }
        fuzzy[i].result_posix = regexec(&compiled, fuzzy[i].input, MAX_MATCHES,
                                        fuzzy[i].pmatch_posix, 0);
        regfree(&compiled);
#endif
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_posix);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_meta);
    for (int32 i = 0; i < fuzzy_len; i += 1) {
        uint8 *input = (uint8 *)fuzzy[i].input;
        int32 input_len = fuzzy[i].input_len;
        MetaRegex *meta_pattern = tests[fuzzy[i].regex_idx];
        fuzzy[i].result_meta = meta_regex_match(
            meta_pattern, input, input_len, MAX_MATCHES, fuzzy[i].pmatch_meta);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_meta);

    for (int32 i = 0; i < fuzzy_len; i += 1) {
        char *input = fuzzy[i].input;
        char *regex = tests[fuzzy[i].regex_idx]->string;
        int32 mismatch = 0;

        if (fuzzy[i].result_posix != fuzzy[i].result_meta) {
            mismatch = 1;
        } else if (fuzzy[i].result_posix == 0) {
            for (int32 m = 0; m < MAX_MATCHES; m += 1) {
                if (fuzzy[i].pmatch_posix[m].rm_so
                        != fuzzy[i].pmatch_meta[m].rm_so
                    || fuzzy[i].pmatch_posix[m].rm_eo
                           != fuzzy[i].pmatch_meta[m].rm_eo) {
                    mismatch = 1;
                    break;
                }
            }
        }

        if (mismatch) {
            failed = true;
            error2("Error: mismatch for input " RED(
                       "\"%s\"") " against regex " BLUE("\"%s\"") "\n",
                   input, regex);
            error2("posix result: %d, meta result: %d\n", fuzzy[i].result_posix,
                   fuzzy[i].result_meta);

            if (fuzzy[i].result_posix == 0 && fuzzy[i].result_meta == 0) {
                for (int32 m = 0; m < MAX_MATCHES; m += 1) {
                    regmatch_t p_m = fuzzy[i].pmatch_posix[m];
                    regmatch_t m_m = fuzzy[i].pmatch_meta[m];

                    if (p_m.rm_so != m_m.rm_so || p_m.rm_eo != m_m.rm_eo) {
                        error2("   Group %d: posix[%d, %d], "
                               "meta[%d, %d]\n",
                               m, (int32)p_m.rm_so, (int32)p_m.rm_eo,
                               (int32)m_m.rm_so, (int32)m_m.rm_eo);
                    }
                }
            }
        }
    }

    {
        char name_posix[256];
        char name_meta[256];

        if (max_str_size < 2048) {
            double t_posix = timediff(t0_posix, t1_posix);
            double t_meta = timediff(t0_meta, t1_meta);

            if (t_posix < t_meta) {
                error2("\nPerformance regression at max_str_size=%d\n",
                       max_str_size);
            }
        }
        SNPRINTF(name_posix, YELLOW("posix [max_str_size=%d]"), max_str_size);
        SNPRINTF(name_meta, GREEN("meta [max_str_size=%d]"), max_str_size);

        PRINT_TIMINGS(fuzzy_len, t0_posix, t1_posix, name_posix);
        PRINT_TIMINGS(fuzzy_len, t0_meta, t1_meta, name_meta);
    }

#if FUZZY_PRECOMPILE_POSIX
    for (int32 i = 0; i < tests_len; i += 1) {
        regfree(&posix_regexes[i]);
    }
    free2(posix_regexes, tests_len*SIZEOF(regex_t));
#endif

    for (int32 i = 0; i < ntests; i += 1) {
        int32 idx = i*tests_len;

        free2(fuzzy[idx].input, fuzzy[idx].input_len + 1);
    }
    free2(fuzzy, SIZEOF(*fuzzy)*fuzzy_len);
    if (failed) {
        exit(EXIT_FAILURE);
    }
    return;
}

static void
run_file_fuzzy_tests(MetaRegex **tests, int32 tests_len) {
    DIR *dir = opendir("inputs");
    struct dirent *entry = NULL;
    bool failed = false;

    if (dir == NULL) {
        error("Error opening inputs directory: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

#if FUZZY_PRECOMPILE_POSIX
    regex_t *posix_regexes = malloc2(tests_len*SIZEOF(regex_t));
    for (int32 i = 0; i < tests_len; i += 1) {
        char *pattern_str = tests[i]->string;
        if (regcomp(&posix_regexes[i], pattern_str, REG_EXTENDED) != 0) {
            error("Pre-compilation failed for: %s\n", pattern_str);
            exit(EXIT_FAILURE);
        }
    }
#endif

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0
            || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[512];
        snprintf2(path, SIZEOF(path), "inputs/%s", entry->d_name);

        FILE *file = fopen(path, "rb");
        if (file == NULL) {
            continue;
        }

        fseek(file, 0, SEEK_END);
        int64 file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *input = malloc2(file_size + 1);
        if (fread64(input, 1, file_size, file) != file_size) {
            free2(input, file_size + 1);
            fclose(file);
            continue;
        }
        input[file_size] = '\0';
        fclose(file);
        int32 input_len = (int32)file_size;

        int32 *results_posix = malloc2(tests_len*SIZEOF(int32));
        int32 *results_meta = malloc2(tests_len*SIZEOF(int32));

        int64 pm_sz = tests_len*MAX_MATCHES * SIZEOF(regmatch_t);
        regmatch_t *pm_posix = malloc2(pm_sz);
        regmatch_t *pm_meta = malloc2(pm_sz);

        struct timespec t0_posix;
        struct timespec t1_posix;
        struct timespec t0_meta;
        struct timespec t1_meta;

        clock_gettime(CLOCK_MONOTONIC_RAW, &t0_posix);
        for (int32 j = 0; j < tests_len; j += 1) {
            regmatch_t *curr_pm = &pm_posix[j*MAX_MATCHES];
            for (int32 m = 0; m < MAX_MATCHES; m += 1) {
                curr_pm[m].rm_so = -1;
                curr_pm[m].rm_eo = -1;
            }
#if FUZZY_PRECOMPILE_POSIX
            results_posix[j]
                = regexec(&posix_regexes[j], input, MAX_MATCHES, curr_pm, 0);
#else
            regex_t compiled;
            char *pattern_str = tests[j]->string;
            if (regcomp(&compiled, pattern_str, REG_EXTENDED) == 0) {
                results_posix[j]
                    = regexec(&compiled, input, MAX_MATCHES, curr_pm, 0);
                regfree(&compiled);
            } else {
                results_posix[j] = REG_NOMATCH;
            }
#endif
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1_posix);

        clock_gettime(CLOCK_MONOTONIC_RAW, &t0_meta);
        for (int32 j = 0; j < tests_len; j += 1) {
            regmatch_t *curr_pm = &pm_meta[j*MAX_MATCHES];
            for (int32 m = 0; m < MAX_MATCHES; m += 1) {
                curr_pm[m].rm_so = -1;
                curr_pm[m].rm_eo = -1;
            }
            uint8 *meta_input = (uint8 *)input;
            MetaRegex *meta_pattern = tests[j];
            results_meta[j] = meta_regex_match(meta_pattern, meta_input,
                                               input_len, MAX_MATCHES, curr_pm);
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1_meta);

        for (int32 j = 0; j < tests_len; j += 1) {
            int32 mismatch = 0;
            regmatch_t *curr_posix = &pm_posix[j*MAX_MATCHES];
            regmatch_t *curr_meta = &pm_meta[j*MAX_MATCHES];

            if (results_posix[j] != results_meta[j]) {
                mismatch = 1;
            } else if (results_posix[j] == 0) {
                for (int32 m = 0; m < MAX_MATCHES; m += 1) {
                    if (curr_posix[m].rm_so != curr_meta[m].rm_so
                        || curr_posix[m].rm_eo != curr_meta[m].rm_eo) {
                        mismatch = 1;
                        break;
                    }
                }
            }

            if (mismatch) {
                failed = true;
                char *regex_str = tests[j]->string;
                error2("File Error: mismatch in file " GREEN(
                           "\"%s\"") " against regex " BLUE("\"%s\"") "\n",
                       entry->d_name, regex_str);
                error2("posix result: %d, meta result: %d\n", results_posix[j],
                       results_meta[j]);

                if (results_posix[j] == 0 && results_meta[j] == 0) {
                    for (int32 m = 0; m < MAX_MATCHES; m += 1) {
                        regmatch_t p_m = curr_posix[m];
                        regmatch_t m_m = curr_meta[m];

                        if (p_m.rm_so != m_m.rm_so || p_m.rm_eo != m_m.rm_eo) {
                            error2("   Group %d: posix[%d, %d], "
                                   "meta[%d, %d]\n",
                                   m, (int32)p_m.rm_so, (int32)p_m.rm_eo,
                                   (int32)m_m.rm_so, (int32)m_m.rm_eo);
                        }
                    }
                }
            }
        }

        char name_posix[256];
        char name_meta[256];

        SNPRINTF(name_posix, YELLOW("posix [%s]"), entry->d_name);
        SNPRINTF(name_meta, GREEN("meta [%s]"), entry->d_name);

        PRINT_TIMINGS(tests_len, t0_posix, t1_posix, name_posix);
        PRINT_TIMINGS(tests_len, t0_meta, t1_meta, name_meta);

        free2(results_posix, tests_len*SIZEOF(int32));
        free2(results_meta, tests_len*SIZEOF(int32));
        free2(pm_posix, pm_sz);
        free2(pm_meta, pm_sz);
        free2(input, file_size + 1);
    }

#if FUZZY_PRECOMPILE_POSIX
    for (int32 i = 0; i < tests_len; i += 1) {
        regfree(&posix_regexes[i]);
    }
    free2(posix_regexes, tests_len*SIZEOF(regex_t));
#endif

    closedir(dir);
    if (failed) {
        exit(EXIT_FAILURE);
    }
    return;
}
