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

static void run_known_pairs(RegexTest *tests, int32 count, char *description,
                            bool extract);
static void run_fuzzy_tests(MetaRegex **patterns, int32 tests_len,
                            int32 max_str_size, int32 ntests, bool extract);
static void run_meta_only(RegexTest *tests, int32 count, char *description,
                          bool extract);
static void run_file_fuzzy_tests(MetaRegex **tests, int32 tests_len,
                                 bool extract);

#define RUN_KNOWN_PAIRS(ARRAY) \
    run_known_pairs(ARRAY, LENGTH(ARRAY), #ARRAY, true); \
    run_known_pairs(ARRAY, LENGTH(ARRAY), #ARRAY, false)
#define RUN_FUZZY_TESTS(ARRAY, MAX_STR_SIZE, NTESTS) \
    run_fuzzy_tests(ARRAY, LENGTH(ARRAY), MAX_STR_SIZE, NTESTS, true); \
    run_fuzzy_tests(ARRAY, LENGTH(ARRAY), MAX_STR_SIZE, NTESTS, false)

#define FUZZY_PRECOMPILE_LIBC 1

static FILE *csv;

#if !defined(ENABLE_LAZY_DFA)
#define ENABLE_LAZY_DFA 1
#endif
#if !defined(ENABLE_STATIC_DFA)
#define ENABLE_STATIC_DFA 1
#endif
#if !defined(ENABLE_TNFA)
#define ENABLE_TNFA 0
#endif
#if !defined(ENABLE_TDFA)
#define ENABLE_TDFA 1
#endif

static enum Matcher
matcher_enabled(enum Matcher enabled) {
    if (ENABLE_LAZY_DFA) {
        enabled |= MATCHER_LAZY_DFA;
    }
    if (ENABLE_STATIC_DFA) {
        enabled |= MATCHER_STATIC_DFA;
    }
    if (ENABLE_TNFA) {
        enabled |= MATCHER_TNFA;
    }
    if (ENABLE_TDFA) {
        enabled |= MATCHER_TDFA;
    }
    return enabled;
}

int32
main(void) {
    setlocale(LC_ALL, "C");
    srand((uint32)42);
    enum Matcher enabled = MATCHER_BTNFA;
    char csv_file[1024];

    enabled = matcher_enabled(enabled);
    SNPRINTF(csv_file, "benchmarks/timings-%lld-%s.csv", (llong)time(NULL),
             MATCHER_str(enabled));

    if ((csv = fopen(csv_file, "w")) == NULL) {
        error("Error opening %s for writing: %s.\n", csv_file, strerror(errno));
        fatal(EXIT_FAILURE);
    }
    fprintf(csv, "suite,case,count,libc_time,meta_time\n");

    printf(RED("\nTests with known (input, regex) pairs ...\n"));
    RUN_KNOWN_PAIRS(ascii_no_group_no_backref);
    RUN_KNOWN_PAIRS(ascii_with_group_no_backref);
    RUN_KNOWN_PAIRS(ascii_with_group_and_backref);
    RUN_KNOWN_PAIRS(utf8_against_ascii);
    RUN_KNOWN_PAIRS(utf8_against_utf8);
    RUN_KNOWN_PAIRS(ascii_catastrophic_no_group_no_backref);
    RUN_KNOWN_PAIRS(ascii_catastrophic_with_group_no_backref);
    RUN_KNOWN_PAIRS(ascii_catastrophic_with_group_and_backref);

    run_meta_only(utf8_against_utf8, LENGTH(utf8_against_utf8), "utf8", true);
    run_meta_only(utf8_against_utf8, LENGTH(utf8_against_utf8), "utf8", false);

    printf(RED(
        "\nTests with random inputs against extensive regex array ...\n") "\n");
    for (int32 max_input_len = 1; max_input_len <= 4096; max_input_len *= 2) {
        RUN_FUZZY_TESTS(regexes_extensive, max_input_len, 200);
    }

    printf(RED("\nTests from inputs/ against extensive regex array ...") "\n");
    run_file_fuzzy_tests(regexes_extensive, LENGTH(regexes_extensive), true);
    run_file_fuzzy_tests(regexes_extensive, LENGTH(regexes_extensive), false);

    if (fclose(csv)) {
        error("Error closing %s: %s.\n", csv_file, strerror(errno));
        exit(EXIT_FAILURE);
    }

#if 1
    switch (fork()) {
    case -1:
        error("Error forking: %s.\n", strerror(errno));
        exit(EXIT_FAILURE);
    case 0: {
        char *python[] = {
            "python",
            "benchmarks/plot.py",
            csv_file,
            NULL,
        };

        execvp(python[0], python);

        {
            char cmd[256];
            STRING_FROM_ARRAY(cmd, " ", python, LENGTH(python));
            error("Error executing\n%s\n%s\n", cmd, strerror(errno));
            _exit(EXIT_FAILURE);
        }
    }
    default:
        wait(NULL);
        break;
    }
#endif

    exit(EXIT_SUCCESS);
}

#undef RUN_FUZZY_TESTS
#undef RUN_KNOWN_PAIRS

static void
run_known_pairs(RegexTest *tests, int32 count, char *description,
                bool extract) {
    struct timespec t0_libc;
    struct timespec t1_libc;
    struct timespec t0_meta;
    struct timespec t1_meta;
    RegexTest *tests_libc = xmemdup(tests, count*SIZEOF(*tests_libc));
    RegexTest *tests_meta = xmemdup(tests, count*SIZEOF(*tests_meta));
    bool failed = false;
    enum Matcher enabled = MATCHER_BTNFA;

    enabled = matcher_enabled(enabled);

    printf("\n----- Running %s (%s) (LIBC vs Meta) -----\n", description,
           extract ? "extracting" : "non-extracting");

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_libc);
    for (int32 i = 0; i < count; i += 1) {
        regex_t compiled_regex;
        char *input = tests_libc[i].input;
        char *regex = tests_libc[i].meta_regex->string;
        int32 compiled = regcomp(&compiled_regex, regex, REG_EXTENDED);
        regmatch_t *pmatch_ptr = NULL;
        int32 pmatch_len = 0;

        if (compiled != 0) {
            char error_message[256];
            regerror(compiled, &compiled_regex, error_message,
                     SIZEOF(error_message));
            error("Regex compilation failed for" BLUE("\"%s\"") ": %s\n", regex,
                  error_message);
            exit(EXIT_FAILURE);
        }

        if (extract) {
            pmatch_ptr = tests_libc[i].pmatch;
            pmatch_len = LENGTH(tests_libc[i].pmatch);
        }

        tests_libc[i].result = regexec(&compiled_regex, input,
                                       (size_t)pmatch_len, pmatch_ptr, 0);
        regfree(&compiled_regex);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_libc);

    for (int32 i = 0; i < count; i += 1) {
        tests_meta[i].input_len = strlen32((char *)tests_meta[i].input);
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_meta);
    for (int32 i = 0; i < count; i += 1) {
        uint8 *input = (uint8 *)tests_meta[i].input;
        int32 input_len = tests_meta[i].input_len;
        MetaRegex *meta_regex = tests_meta[i].meta_regex;
        int32 m_pmatch_len = 0;
        regmatch_t *m_pmatch_ptr = NULL;

        if (extract) {
            m_pmatch_len = LENGTH(tests_meta[i].pmatch);
            m_pmatch_ptr = tests_meta[i].pmatch;
        }

        tests_meta[i].result = meta_regex_match(
            meta_regex, input, input_len, m_pmatch_ptr, m_pmatch_len, enabled);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_meta);

    for (int32 i = 0; i < count; i += 1) {
        RegexTest tp = tests_libc[i];
        RegexTest tm = tests_meta[i];
        char *regex = tests[i].meta_regex->string;
        char *input = tests[i].input;

        if (tp.result != tm.result) {
            error2("Error: result mismatch for input " RED(
                       "\"%s\"") " against regex " BLUE("\"%s\"") "\n",
                   input, regex);
            error2("libc: %d, meta: %d\n", tp.result, tm.result);
            failed = true;
        } else if (tp.result == 0 && extract) {
            for (int32 m = 0; m < LENGTH(tp.pmatch); m += 1) {
                regmatch_t p_m = tp.pmatch[m];
                regmatch_t m_m = tm.pmatch[m];

                if (p_m.rm_so != m_m.rm_so || p_m.rm_eo != m_m.rm_eo) {
                    error2("Mismatch in capture group %d:\ninput " RED(
                               "%s") " against regex " BLUE("%s") "\n",
                           m, input, regex);
                    error2("libc: rm_so=%d, rm_eo=%d\n", p_m.rm_so, p_m.rm_eo);
                    error2("meta:  rm_so=%d, rm_eo=%d\n", m_m.rm_so, m_m.rm_eo);
                    failed = true;
                }
            }
        }
    }

    if (failed) {
        exit(EXIT_FAILURE);
    }

    {
        double t_libc = timediff(t0_libc, t1_libc);
        double t_meta = timediff(t0_meta, t1_meta);
        fprintf(csv, "%s,%s,%d,%f,%f\n",
                extract ? "known_pairs_extract" : "known_pairs_no_extract",
                description, count, t_libc, t_meta);
    }

    free2(tests_libc, count*SIZEOF(*tests_libc));
    free2(tests_meta, count*SIZEOF(*tests_meta));
    return;
}

static void
run_meta_only(RegexTest *tests, int32 count, char *description, bool extract) {
    struct timespec t0;
    struct timespec t1;
    printf("\n----- Running %s (%s) (Meta Only) -----\n", description,
           extract ? "extracting" : "non-extracting");
    bool failed = false;
    enum Matcher enabled = MATCHER_BTNFA;

    enabled = matcher_enabled(enabled);

    for (int32 i = 0; i < count; i += 1) {
        tests[i].input_len = strlen32((char *)tests[i].input);
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    for (int32 i = 0; i < count; i += 1) {
        char *input = tests[i].input;
        int32 input_len = tests[i].input_len;
        MetaRegex *meta_regex = tests[i].meta_regex;
        int32 pmatch_len = 0;
        regmatch_t *pmatch_ptr = NULL;
        if (extract) {
            pmatch_len = LENGTH(tests[i].pmatch);
            pmatch_ptr = tests[i].pmatch;
        }

        int32 result = meta_regex_match(meta_regex, (uint8 *)input, input_len,
                                        pmatch_ptr, pmatch_len, enabled);
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

    double t_meta = timediff(t0, t1);
    fprintf(csv, "%s,%s,%d,0.0,%f\n",
            extract ? "meta_only_extract" : "meta_only_no_extract", description,
            count, t_meta);

    return;
}

static void
run_fuzzy_tests(MetaRegex **tests, int32 tests_len, int32 max_str_size,
                int32 ntests, bool extract) {
    int32 fuzzy_len = ntests*tests_len;
    FuzzyTest *fuzzy = malloc2(SIZEOF(*fuzzy)*fuzzy_len);
    struct timespec t0_libc;
    struct timespec t1_libc;
    struct timespec t0_meta;
    struct timespec t1_meta;
    bool failed = false;
    enum Matcher enabled = MATCHER_BTNFA;
#if FUZZY_PRECOMPILE_LIBC
    regex_t *libc_regexes = malloc2(tests_len*SIZEOF(*libc_regexes));
#endif

    enabled = matcher_enabled(enabled);

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

#if FUZZY_PRECOMPILE_LIBC
    for (int32 i = 0; i < tests_len; i += 1) {
        char *pattern_str = tests[i]->string;

        if (regcomp(&libc_regexes[i], pattern_str, REG_EXTENDED) != 0) {
            error("Pre-compilation failed for " BLUE("\"%s\"") "\n",
                  pattern_str);
            exit(EXIT_FAILURE);
        }
    }
#endif

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_libc);
    for (int32 i = 0; i < fuzzy_len; i += 1) {
        regmatch_t *pmatch_ptr = NULL;
        int32 pmatch_len = 0;
        if (extract) {
            pmatch_ptr = fuzzy[i].pmatch_libc;
            pmatch_len = LENGTH(fuzzy[i].pmatch_libc);
        }
#if FUZZY_PRECOMPILE_LIBC
        int32 idx = fuzzy[i].regex_idx;
        fuzzy[i].result_libc = regexec(&libc_regexes[idx], fuzzy[i].input,
                                       (size_t)pmatch_len, pmatch_ptr, 0);
#else
        regex_t compiled;
        char *pattern_str = tests[fuzzy[i].regex_idx]->string;

        if (regcomp(&compiled, pattern_str, REG_EXTENDED)) {
            error("Pre-compilation failed for " BLUE("\"%s\"") "\n",
                  pattern_str);
            exit(EXIT_FAILURE);
        }
        fuzzy[i].result_libc
            = regexec(&compiled, fuzzy[i].input, pmatch_len, pmatch_ptr, 0);
        regfree(&compiled);
#endif
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_libc);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_meta);
    for (int32 i = 0; i < fuzzy_len; i += 1) {
        uint8 *input = (uint8 *)fuzzy[i].input;
        int32 input_len = fuzzy[i].input_len;
        MetaRegex *meta_pattern = tests[fuzzy[i].regex_idx];
        int32 m_pmatch_len = 0;
        regmatch_t *m_pmatch_ptr = NULL;

        if (extract) {
            m_pmatch_ptr = fuzzy[i].pmatch_meta;
            m_pmatch_len = LENGTH(fuzzy[i].pmatch_meta);
        }

        fuzzy[i].result_meta
            = meta_regex_match(meta_pattern, input, input_len, m_pmatch_ptr,
                               m_pmatch_len, enabled);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_meta);

    for (int32 i = 0; i < fuzzy_len; i += 1) {
        char *input = fuzzy[i].input;
        char *regex = tests[fuzzy[i].regex_idx]->string;
        int32 mismatch = 0;

        if (fuzzy[i].result_libc != fuzzy[i].result_meta) {
            mismatch = 1;
        } else if (fuzzy[i].result_libc == 0 && extract) {
            for (int32 m = 0; m < LENGTH(fuzzy[i].pmatch_libc); m += 1) {
                if (fuzzy[i].pmatch_libc[m].rm_so
                        != fuzzy[i].pmatch_meta[m].rm_so
                    || fuzzy[i].pmatch_libc[m].rm_eo
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
            error2("libc result: %d, meta result: %d\n", fuzzy[i].result_libc,
                   fuzzy[i].result_meta);

            if (fuzzy[i].result_libc == 0 && fuzzy[i].result_meta == 0
                && extract) {
                for (int32 m = 0; m < LENGTH(fuzzy[i].pmatch_libc); m += 1) {
                    regmatch_t p_m = fuzzy[i].pmatch_libc[m];
                    regmatch_t m_m = fuzzy[i].pmatch_meta[m];

                    if (p_m.rm_so != m_m.rm_so || p_m.rm_eo != m_m.rm_eo) {
                        error2("   Group %d: libc[%d, %d], "
                               "meta[%d, %d]\n",
                               m, (int32)p_m.rm_so, (int32)p_m.rm_eo,
                               (int32)m_m.rm_so, (int32)m_m.rm_eo);
                    }
                }
            }
        }
    }

    double t_libc = timediff(t0_libc, t1_libc);
    double t_meta = timediff(t0_meta, t1_meta);
    if (max_str_size < 2048) {
        if (t_libc < t_meta) {
            error2("\nPerformance regression at max_str_size=%d\n",
                   max_str_size);
        }
    }

    fprintf(csv, "%s,%d,%d,%f,%f\n",
            extract ? "fuzzy_extract" : "fuzzy_no_extract", max_str_size,
            fuzzy_len, t_libc, t_meta);

#if FUZZY_PRECOMPILE_LIBC
    for (int32 i = 0; i < tests_len; i += 1) {
        regfree(&libc_regexes[i]);
    }
    free2(libc_regexes, tests_len*SIZEOF(*libc_regexes));
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
run_file_fuzzy_tests(MetaRegex **tests, int32 tests_len, bool extract) {
    DIR *dir = opendir("inputs");
    struct dirent *entry = NULL;
    bool failed = false;
    RegexTest dummy_test;
    enum Matcher enabled = MATCHER_BTNFA;

    if (dir == NULL) {
        error("Error opening inputs directory: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    enabled = matcher_enabled(enabled);

#if FUZZY_PRECOMPILE_LIBC
    regex_t *libc_regexes = malloc2(tests_len*SIZEOF(*libc_regexes));
    for (int32 i = 0; i < tests_len; i += 1) {
        char *pattern_str = tests[i]->string;
        if (regcomp(&libc_regexes[i], pattern_str, REG_EXTENDED) != 0) {
            error("Pre-compilation failed for: %s\n", pattern_str);
            exit(EXIT_FAILURE);
        }
    }
#endif

    while ((entry = readdir(dir)) != NULL) {
        char path[512];
        char case_name[768];
        char size_pretty[32];
        FILE *file;
        int64 file_size;
        uint8 *input;
        int32 input_len;

        if (strcmp(entry->d_name, ".") == 0
            || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        SNPRINTF(path, "inputs/%s", entry->d_name);

        if ((file = fopen(path, "rb")) == NULL) {
            error("Error opening file %s: %s.\n", path, strerror(errno));
            fatal(EXIT_FAILURE);
        }

        fseek(file, 0, SEEK_END);
        file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        bytes_pretty(size_pretty, file_size);
        SNPRINTF(case_name, "%s[%s]", entry->d_name, size_pretty);

        input = malloc2(file_size + 1);
        if (fread64(input, 1, file_size, file) != file_size) {
            error("Error reading %lld bytes from file %s: %s.\n",
                  (llong)file_size, path, strerror(errno));
            fatal(EXIT_FAILURE);
        }
        input[file_size] = '\0';
        fclose(file);
        input_len = (int32)file_size;

        int32 *results_libc = malloc2(tests_len*SIZEOF(*results_libc));
        int32 *results_meta = malloc2(tests_len*SIZEOF(*results_meta));

        int64 pm_sz
            = tests_len*LENGTH(dummy_test.pmatch) * SIZEOF(regmatch_t);
        regmatch_t *pm_libc = malloc2(pm_sz);
        regmatch_t *pm_meta = malloc2(pm_sz);

        struct timespec t0_libc;
        struct timespec t1_libc;
        struct timespec t0_meta;
        struct timespec t1_meta;

        clock_gettime(CLOCK_MONOTONIC_RAW, &t0_libc);
        for (int32 j = 0; j < tests_len; j += 1) {
            int32 pmatch_len = 0;
            regmatch_t *curr_pm = NULL;

            if (extract) {
                pmatch_len = LENGTH(dummy_test.pmatch);
                curr_pm = &pm_libc[j*LENGTH(dummy_test.pmatch)];

                for (int32 m = 0; m < LENGTH(dummy_test.pmatch); m += 1) {
                    curr_pm[m].rm_so = -1;
                    curr_pm[m].rm_eo = -1;
                }
            }
#if FUZZY_PRECOMPILE_LIBC
            results_libc[j] = regexec(&libc_regexes[j], (char *)input,
                                      (size_t)pmatch_len, curr_pm, 0);
#else
            regex_t compiled;
            char *pattern_str = tests[j]->string;
            if (regcomp(&compiled, pattern_str, REG_EXTENDED) == 0) {
                results_libc[j]
                    = regexec(&compiled, (char *)input, pmatch_len, curr_pm, 0);
                regfree(&compiled);
            } else {
                results_libc[j] = REG_NOMATCH;
            }
#endif
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1_libc);

        clock_gettime(CLOCK_MONOTONIC_RAW, &t0_meta);
        for (int32 j = 0; j < tests_len; j += 1) {
            MetaRegex *meta_pattern = tests[j];
            int32 m_pmatch_len = 0;
            regmatch_t *curr_m_pm = NULL;
            
            if (extract) {
                m_pmatch_len = LENGTH(dummy_test.pmatch);
                curr_m_pm = &pm_meta[j*LENGTH(dummy_test.pmatch)];
                for (int32 m = 0; m < LENGTH(dummy_test.pmatch); m += 1) {
                    curr_m_pm[m].rm_so = -1;
                    curr_m_pm[m].rm_eo = -1;
                }
            }

            results_meta[j]
                = meta_regex_match(meta_pattern, input, input_len,
                                   curr_m_pm, m_pmatch_len, enabled);
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1_meta);

        for (int32 j = 0; j < tests_len; j += 1) {
            int32 mismatch = 0;
            regmatch_t *curr_libc = NULL;
            regmatch_t *curr_meta = NULL;

            if (extract) {
                curr_libc = &pm_libc[j*LENGTH(dummy_test.pmatch)];
                curr_meta = &pm_meta[j*LENGTH(dummy_test.pmatch)];
            }

            if (results_libc[j] != results_meta[j]) {
                mismatch = 1;
            } else if (results_libc[j] == 0 && extract) {
                for (int32 m = 0; m < LENGTH(dummy_test.pmatch); m += 1) {
                    if (curr_libc[m].rm_so != curr_meta[m].rm_so
                        || curr_libc[m].rm_eo != curr_meta[m].rm_eo) {
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
                error2("libc result: %d, meta result: %d\n", results_libc[j],
                       results_meta[j]);

                if (results_libc[j] == 0 && results_meta[j] == 0 && extract) {
                    for (int32 m = 0; m < LENGTH(dummy_test.pmatch); m += 1) {
                        regmatch_t p_m = curr_libc[m];
                        regmatch_t m_m = curr_meta[m];

                        if (p_m.rm_so != m_m.rm_so || p_m.rm_eo != m_m.rm_eo) {
                            error2("   Group %d: libc[%d, %d], "
                                   "meta[%d, %d]\n",
                                   m, (int32)p_m.rm_so, (int32)p_m.rm_eo,
                                   (int32)m_m.rm_so, (int32)m_m.rm_eo);
                        }
                    }
                }
            }
        }

        double t_libc = timediff(t0_libc, t1_libc);
        double t_meta = timediff(t0_meta, t1_meta);
        fprintf(csv, "%s,%s,%d,%f,%f\n",
                extract ? "file_fuzzy_extract" : "file_fuzzy_no_extract",
                case_name, tests_len, t_libc, t_meta);

        free2(results_libc, tests_len*SIZEOF(*results_libc));
        free2(results_meta, tests_len*SIZEOF(*results_meta));
        free2(pm_libc, pm_sz);
        free2(pm_meta, pm_sz);
        free2(input, file_size + 1);
    }

#if FUZZY_PRECOMPILE_LIBC
    for (int32 i = 0; i < tests_len; i += 1) {
        regfree(&libc_regexes[i]);
    }
    free2(libc_regexes, tests_len*SIZEOF(*libc_regexes));
#endif

    closedir(dir);
    if (failed) {
        exit(EXIT_FAILURE);
    }
    return;
}
