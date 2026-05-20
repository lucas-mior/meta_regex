#include <assert.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
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

#define FUZZY_PRECOMPILE_LIBC 1

#if !defined(ENABLE_BTNFA)
#define ENABLE_BTNFA 1
#endif
#if !defined(ENABLE_LAZY_DFA)
#define ENABLE_LAZY_DFA 0
#endif
#if !defined(ENABLE_STATIC_DFA)
#define ENABLE_STATIC_DFA 0
#endif
#if !defined(ENABLE_TNFA)
#define ENABLE_TNFA 0
#endif
#if !defined(ENABLE_TDFA)
#define ENABLE_TDFA 1
#endif

static enum Matcher all_matchers[] = {
    MATCHER_BTNFA,    MATCHER_TNFA,       MATCHER_TDFA,
    MATCHER_LAZY_DFA, MATCHER_STATIC_DFA,
};

static int32
matcher_compile_enabled(enum Matcher matcher) {
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
matcher_storage_available(MetaRegex *regex, enum Matcher matcher) {
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
matcher_supports_regex(MetaRegex *regex, enum Matcher matcher, bool extract) {
    if (!matcher_compile_enabled(matcher)) {
        return 0;
    }
    if (!matcher_storage_available(regex, matcher)) {
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
clear_pmatch(regmatch_t *pmatch, int32 pmatch_len) {
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
pmatch_mismatch(regmatch_t *reference, regmatch_t *actual, int32 len) {
    for (int32 i = 0; i < len; i += 1) {
        if (reference[i].rm_so != actual[i].rm_so
            || reference[i].rm_eo != actual[i].rm_eo) {
            return i;
        }
    }
    return -1;
}

static void
report_match_mismatch(char *kind, char *case_name, enum Matcher matcher,
                      char *input, char *regex, int32 reference_result,
                      int32 actual_result, regmatch_t *reference,
                      regmatch_t *actual, int32 pmatch_len, bool extract) {
    error2("%s mismatch in %s with matcher %s\n", kind, case_name,
           MATCHER_str(matcher));
    error2("input " RED("\"%s\"") " against regex " BLUE("\"%s\"") "\n", input,
           regex);
    error2("libc result: %d, meta result: %d\n", reference_result,
           actual_result);

    if (reference_result == 0 && actual_result == 0 && extract) {
        int32 group = pmatch_mismatch(reference, actual, pmatch_len);
        if (group >= 0) {
            error2("capture group %d: libc[%d, %d], meta[%d, %d]\n", group,
                   (int32)reference[group].rm_so, (int32)reference[group].rm_eo,
                   (int32)actual[group].rm_so, (int32)actual[group].rm_eo);
        }
    }
    return;
}

static int32
result_mismatch(int32 reference_result, int32 actual_result,
                regmatch_t *reference, regmatch_t *actual, int32 pmatch_len,
                bool extract) {
    if (reference_result != actual_result) {
        return 1;
    }
    if (reference_result == 0 && extract) {
        return pmatch_mismatch(reference, actual, pmatch_len) >= 0;
    }
    return 0;
}

static int32
run_libc_one(regex_t *compiled, char *input, regmatch_t *pmatch,
             int32 pmatch_len, bool extract) {
    regmatch_t *pmatch_ptr = NULL;
    int64 nmatch = 0;

    if (extract) {
        pmatch_ptr = pmatch;
        nmatch = pmatch_len;
        clear_pmatch(pmatch, pmatch_len);
    }
    return regexec(compiled, input, (size_t)nmatch, pmatch_ptr, 0);
}

static int32
run_meta_one(MetaRegex *regex, char *input, int32 input_len,
             enum Matcher matcher, regmatch_t *pmatch, int32 pmatch_len,
             bool extract) {
    regmatch_t *pmatch_ptr = NULL;
    int32 nmatch = 0;

    if (extract) {
        pmatch_ptr = pmatch;
        nmatch = pmatch_len;
        clear_pmatch(pmatch, pmatch_len);
    }

    return meta_regex_match_with_algorithm(regex, (uint8 *)input, input_len,
                                           pmatch_ptr, nmatch, matcher);
}

static void run_known_pairs(RegexTest *tests, int32 count, char *description,
                            bool extract);
static void run_fuzzy_tests(MetaRegex **patterns, int32 tests_len, char *,
                            int32 max_str_size, int32 ntests, bool extract);
static void run_file_fuzzy_tests(MetaRegex **tests, int32 tests_len,
                                 bool extract);

#define RUN_KNOWN_PAIRS(ARRAY) \
    run_known_pairs(ARRAY, LENGTH(ARRAY), #ARRAY, true); \
    run_known_pairs(ARRAY, LENGTH(ARRAY), #ARRAY, false)
#define RUN_FUZZY_TESTS(ARRAY, MAX_STR_SIZE, NINPUTS) \
    run_fuzzy_tests(ARRAY, LENGTH(ARRAY), #ARRAY, MAX_STR_SIZE, NINPUTS, true); \
    run_fuzzy_tests(ARRAY, LENGTH(ARRAY), #ARRAY, MAX_STR_SIZE, NINPUTS, false)

int32
main(void) {
    setlocale(LC_ALL, "C");
    srand((uint32)42);

    printf(RED("\nTests with known (input, regex) pairs ...\n"));
    RUN_KNOWN_PAIRS(ascii_no_group_no_backref);
    RUN_KNOWN_PAIRS(ascii_with_group_no_backref);
    RUN_KNOWN_PAIRS(ascii_with_group_and_backref);
    RUN_KNOWN_PAIRS(utf8_against_ascii);
    RUN_KNOWN_PAIRS(utf8_against_utf8);
    RUN_KNOWN_PAIRS(ascii_catastrophic_no_group_no_backref);
    RUN_KNOWN_PAIRS(ascii_catastrophic_with_group_no_backref);
    RUN_KNOWN_PAIRS(ascii_catastrophic_with_group_and_backref);

    printf(RED(
        "\nTests with random inputs against extensive regex array ...\n") "\n");
    for (int32 max_input_len = 1; max_input_len <= 4096; max_input_len *= 2) {
        RUN_FUZZY_TESTS(regexes_extensive, max_input_len, 200);
    }

    printf(RED("\nTests from inputs/ against extensive regex array ...") "\n");
    run_file_fuzzy_tests(regexes_extensive, LENGTH(regexes_extensive), true);
    run_file_fuzzy_tests(regexes_extensive, LENGTH(regexes_extensive), false);

    exit(EXIT_SUCCESS);
}

#undef RUN_FUZZY_TESTS
#undef RUN_KNOWN_PAIRS

static void
run_known_pairs(RegexTest *tests, int32 count, char *description,
                bool extract) {
    RegexTest *reference = xmemdup(tests, count*SIZEOF(*reference));
    bool failed = false;

    printf("\n----- Running %s (%s) -----\n", description,
           extract ? "extracting" : "non-extracting");

    for (int32 i = 0; i < count; i += 1) {
        regex_t compiled_regex;
        char *regex = reference[i].meta_regex->string;
        int32 compiled = regcomp(&compiled_regex, regex, REG_EXTENDED);

        reference[i].input_len = strlen32(reference[i].input);

        if (compiled != 0) {
            char error_message[256];
            regerror(compiled, &compiled_regex, error_message,
                     SIZEOF(error_message));
            error("Regex compilation failed for " BLUE("\"%s\"") ": %s\n",
                  regex, error_message);
            exit(EXIT_FAILURE);
        }

        reference[i].result = run_libc_one(
            &compiled_regex, reference[i].input, reference[i].pmatch,
            LENGTH(reference[i].pmatch), extract);
        regfree(&compiled_regex);
    }
    for (int32 mi = 0; mi < LENGTH(all_matchers); mi += 1) {
        enum Matcher matcher = all_matchers[mi];
        RegexTest *actual;

        if (!matcher_compile_enabled(matcher)) {
            continue;
        }

        actual = xmemdup(tests, count*SIZEOF(*actual));
        for (int32 i = 0; i < count; i += 1) {
            MetaRegex *meta_regex = actual[i].meta_regex;
            actual[i].input_len = reference[i].input_len;

            if (!matcher_supports_regex(meta_regex, matcher, extract)) {
                actual[i].result = REG_NOMATCH;
                continue;
            }

            actual[i].result = run_meta_one(
                meta_regex, actual[i].input, actual[i].input_len, matcher,
                actual[i].pmatch, LENGTH(actual[i].pmatch), extract);
        }
        for (int32 i = 0; i < count; i += 1) {
            if (!matcher_supports_regex(actual[i].meta_regex, matcher,
                                        extract)) {
                continue;
            }
            if (result_mismatch(reference[i].result, actual[i].result,
                                reference[i].pmatch, actual[i].pmatch,
                                LENGTH(actual[i].pmatch), extract)) {
                failed = true;
                report_match_mismatch(
                    "Known-pair", description, matcher, tests[i].input,
                    tests[i].meta_regex->string, reference[i].result,
                    actual[i].result, reference[i].pmatch, actual[i].pmatch,
                    LENGTH(actual[i].pmatch), extract);
            }
        }

        free2(actual, count*SIZEOF(*actual));
    }

    free2(reference, count*SIZEOF(*reference));

    if (failed) {
        exit(EXIT_FAILURE);
    }
    return;
}

static void
run_fuzzy_tests(MetaRegex **tests, int32 tests_len, char *tests_name,
                int32 max_str_size, int32 ninputs, bool extract) {
    int32 fuzzy_len = ninputs*tests_len;
    FuzzyTest *fuzzy = malloc2(SIZEOF(*fuzzy)*fuzzy_len);
    bool failed = false;
    (void)tests_name;
#if FUZZY_PRECOMPILE_LIBC
    regex_t *libc_regexes = malloc2(tests_len*SIZEOF(*libc_regexes));
#endif

    for (int32 i = 0; i < ninputs; i += 1) {
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

    for (int32 i = 0; i < fuzzy_len; i += 1) {
        int32 idx = fuzzy[i].regex_idx;
#if FUZZY_PRECOMPILE_LIBC
        fuzzy[i].result_libc = run_libc_one(
            &libc_regexes[idx], fuzzy[i].input, fuzzy[i].pmatch_libc,
            LENGTH(fuzzy[i].pmatch_libc), extract);
#else
        regex_t compiled;
        char *pattern_str = tests[idx]->string;

        if (regcomp(&compiled, pattern_str, REG_EXTENDED)) {
            error("Pre-compilation failed for " BLUE("\"%s\"") "\n",
                  pattern_str);
            exit(EXIT_FAILURE);
        }
        fuzzy[i].result_libc
            = run_libc_one(&compiled, fuzzy[i].input, fuzzy[i].pmatch_libc,
                           LENGTH(fuzzy[i].pmatch_libc), extract);
        regfree(&compiled);
#endif
    }
    for (int32 mi = 0; mi < LENGTH(all_matchers); mi += 1) {
        enum Matcher matcher = all_matchers[mi];
        if (!matcher_compile_enabled(matcher)) {
            continue;
        }

        for (int32 i = 0; i < fuzzy_len; i += 1) {
            MetaRegex *meta_pattern = tests[fuzzy[i].regex_idx];

            if (!matcher_supports_regex(meta_pattern, matcher, extract)) {
                fuzzy[i].result_meta = REG_NOMATCH;
                continue;
            }

            fuzzy[i].result_meta = run_meta_one(
                meta_pattern, fuzzy[i].input, fuzzy[i].input_len, matcher,
                fuzzy[i].pmatch_meta, LENGTH(fuzzy[i].pmatch_meta), extract);
        }
        for (int32 i = 0; i < fuzzy_len; i += 1) {
            MetaRegex *meta_pattern = tests[fuzzy[i].regex_idx];
            if (!matcher_supports_regex(meta_pattern, matcher, extract)) {
                continue;
            }

            if (result_mismatch(fuzzy[i].result_libc, fuzzy[i].result_meta,
                                fuzzy[i].pmatch_libc, fuzzy[i].pmatch_meta,
                                LENGTH(fuzzy[i].pmatch_meta), extract)) {
                failed = true;
                report_match_mismatch(
                    "Fuzzy", "random", matcher, fuzzy[i].input,
                    meta_pattern->string, fuzzy[i].result_libc,
                    fuzzy[i].result_meta, fuzzy[i].pmatch_libc,
                    fuzzy[i].pmatch_meta, LENGTH(fuzzy[i].pmatch_meta),
                    extract);
            }
        }
    }

#if FUZZY_PRECOMPILE_LIBC
    for (int32 i = 0; i < tests_len; i += 1) {
        regfree(&libc_regexes[i]);
    }
    free2(libc_regexes, tests_len*SIZEOF(*libc_regexes));
#endif

    for (int32 i = 0; i < ninputs; i += 1) {
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
#if FUZZY_PRECOMPILE_LIBC
    regex_t *libc_regexes = malloc2(tests_len*SIZEOF(*libc_regexes));
#endif

    if (dir == NULL) {
        error("Error opening inputs directory: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

#if FUZZY_PRECOMPILE_LIBC
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
        int32 *results_libc;
        int64 pm_sz;
        regmatch_t *pm_libc;

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

        results_libc = malloc2(tests_len*SIZEOF(*results_libc));
        pm_sz = tests_len*LENGTH(dummy_test.pmatch) * SIZEOF(regmatch_t);
        pm_libc = malloc2(pm_sz);

        for (int32 j = 0; j < tests_len; j += 1) {
            regmatch_t *curr_pm = NULL;

            if (extract) {
                curr_pm = &pm_libc[j*LENGTH(dummy_test.pmatch)];
                clear_pmatch(curr_pm, LENGTH(dummy_test.pmatch));
            }
#if FUZZY_PRECOMPILE_LIBC
            results_libc[j]
                = run_libc_one(&libc_regexes[j], (char *)input, curr_pm,
                               LENGTH(dummy_test.pmatch), extract);
#else
            regex_t compiled;
            char *pattern_str = tests[j]->string;
            if (regcomp(&compiled, pattern_str, REG_EXTENDED) == 0) {
                results_libc[j]
                    = run_libc_one(&compiled, (char *)input, curr_pm,
                                   LENGTH(dummy_test.pmatch), extract);
                regfree(&compiled);
            } else {
                results_libc[j] = REG_NOMATCH;
            }
#endif
        }
        for (int32 mi = 0; mi < LENGTH(all_matchers); mi += 1) {
            enum Matcher matcher = all_matchers[mi];
            int32 *results_meta = malloc2(tests_len*SIZEOF(*results_meta));
            regmatch_t *pm_meta = malloc2(pm_sz);
            if (!matcher_compile_enabled(matcher)) {
                free2(results_meta, tests_len*SIZEOF(*results_meta));
                free2(pm_meta, pm_sz);
                continue;
            }

            for (int32 j = 0; j < tests_len; j += 1) {
                MetaRegex *meta_pattern = tests[j];
                regmatch_t *curr_m_pm = NULL;

                if (!matcher_supports_regex(meta_pattern, matcher, extract)) {
                    results_meta[j] = REG_NOMATCH;
                    continue;
                }

                if (extract) {
                    curr_m_pm = &pm_meta[j*LENGTH(dummy_test.pmatch)];
                    clear_pmatch(curr_m_pm, LENGTH(dummy_test.pmatch));
                }

                results_meta[j] = run_meta_one(
                    meta_pattern, (char *)input, input_len, matcher, curr_m_pm,
                    LENGTH(dummy_test.pmatch), extract);
            }
            for (int32 j = 0; j < tests_len; j += 1) {
                MetaRegex *meta_pattern = tests[j];
                regmatch_t *curr_libc = NULL;
                regmatch_t *curr_meta = NULL;

                if (!matcher_supports_regex(meta_pattern, matcher, extract)) {
                    continue;
                }

                if (extract) {
                    curr_libc = &pm_libc[j*LENGTH(dummy_test.pmatch)];
                    curr_meta = &pm_meta[j*LENGTH(dummy_test.pmatch)];
                }

                if (result_mismatch(results_libc[j], results_meta[j], curr_libc,
                                    curr_meta, LENGTH(dummy_test.pmatch),
                                    extract)) {
                    failed = true;
                    report_match_mismatch("File", case_name, matcher,
                                          (char *)input, meta_pattern->string,
                                          results_libc[j], results_meta[j],
                                          curr_libc, curr_meta,
                                          LENGTH(dummy_test.pmatch), extract);
                }
            }

            free2(results_meta, tests_len*SIZEOF(*results_meta));
            free2(pm_meta, pm_sz);
        }

        free2(results_libc, tests_len*SIZEOF(*results_libc));
        free2(pm_libc, pm_sz);
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
