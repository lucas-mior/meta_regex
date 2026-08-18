#define CBASE_IMPLEMENT
#include "cbase.h"

#include <regex.h>

#include "meta_regex.h"
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
    if (extract && !matchers[matcher]->extracts) {
        return 0;
    }
    if ((regex->used_ops & ~matchers[matcher]->supports) != 0) {
        return 0;
    }
    return 1;
}

static void
clear_pmatch(MetaRegexMatch *pmatch, int32 pmatch_len) {
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
pmatch_mismatch(MetaRegexMatch *reference, MetaRegexMatch *actual, int32 len) {
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
                      int32 actual_result, MetaRegexMatch *reference,
                      MetaRegexMatch *actual, int32 pmatch_len, bool extract,
                      bool compare_submatches) {
    char *matcher_name = MATCHER_str(matcher);

    error2("%s mismatch in %s with matcher %s\n", kind, case_name,
           matcher_name);
    MATCHER_str_free(matcher_name);
    error2("input " RED("\"%s\"") " against regex " BLUE("\"%s\"") "\n", input,
           regex);
    error2("reference result: %d, meta result: %d\n", reference_result,
           actual_result);

    if (reference_result == 0 && actual_result == 0 && extract) {
        int32 compare_len = 1;
        int32 group;

        if (compare_submatches) {
            compare_len = pmatch_len;
        }

        group = pmatch_mismatch(reference, actual, compare_len);
        if (group >= 0) {
            error2("capture group %d: reference[%d, %d], meta[%d, %d]\n", group,
                   (int32)reference[group].rm_so, (int32)reference[group].rm_eo,
                   (int32)actual[group].rm_so, (int32)actual[group].rm_eo);
        }
    }
    return;
}

static int32
result_mismatch(int32 reference_result, int32 actual_result,
                MetaRegexMatch *reference, MetaRegexMatch *actual,
                int32 pmatch_len, bool extract, bool compare_submatches) {
    if (reference_result != actual_result) {
        return 1;
    }
    if (reference_result == 0 && extract) {
        int32 compare_len = 1;

        if (compare_submatches) {
            compare_len = pmatch_len;
        }

        return pmatch_mismatch(reference, actual, compare_len) >= 0;
    }
    return 0;
}

static bool
libc_submatches_are_portable(MetaRegex *regex) {
    enum MetaOpType repeat_ops = (enum MetaOpType)(
        META_OP_STAR
        |META_OP_PLUS
        |META_OP_OPTIONAL
        |META_OP_BOUNDED
        |META_OP_SPLIT
        |META_OP_JUMP);

    if (regex->re_nsub <= 0) {
        return true;
    }
    if ((regex->used_ops & META_OP_ALTERNATION) == 0) {
        return true;
    }
    if ((regex->used_ops & repeat_ops) == 0) {
        return true;
    }

    return false;
}

static int32
run_libc_one(regex_t *compiled, char *input, MetaRegexMatch *pmatch,
             int32 pmatch_len, bool extract) {
    regmatch_t libc_pmatch[MAX_MATCHES];
    regmatch_t *pmatch_ptr = NULL;
    int64 nmatch = 0;
    int32 result;

    if (extract) {
        if (pmatch_len > MAX_MATCHES) {
            error("Too many matches requested.\n");
            exit(EXIT_FAILURE);
        }

        clear_pmatch(pmatch, pmatch_len);
        for (int32 i = 0; i < pmatch_len; i += 1) {
            libc_pmatch[i].rm_so = -1;
            libc_pmatch[i].rm_eo = -1;
        }

        pmatch_ptr = libc_pmatch;
        nmatch = pmatch_len;
    }

    result = regexec(compiled, input, (size_t)nmatch, pmatch_ptr, 0);
    if (result == REG_NOMATCH) {
        result = META_REG_NOMATCH;
    }
    if (result == 0 && extract) {
        for (int32 i = 0; i < pmatch_len; i += 1) {
            pmatch[i].rm_so = (int32)libc_pmatch[i].rm_so;
            pmatch[i].rm_eo = (int32)libc_pmatch[i].rm_eo;
        }
    }

    return result;
}

static int32
run_meta_one(MetaRegex *regex, char *input, int32 input_len,
             enum Matcher matcher, MetaRegexMatch *pmatch, int32 pmatch_len,
             bool extract) {
    MetaRegexMatch *pmatch_ptr = NULL;
    int32 nmatch = 0;

    if (extract) {
        pmatch_ptr = pmatch;
        nmatch = pmatch_len;
        clear_pmatch(pmatch, pmatch_len);
    }

    return meta_regex_match_with_algorithm(regex, (uint8 *)input, input_len,
                                           pmatch_ptr, nmatch, matcher);
}

static int32
run_fallback_reference_one(MetaRegex *regex, char *input, int32 input_len,
                           MetaRegexMatch *pmatch, int32 pmatch_len,
                           bool extract) {
    return run_meta_one(regex, input, input_len, MATCHER_BTNFA, pmatch,
                        pmatch_len, extract);
}

static bool
matcher_can_use_reference(bool libc_reference, enum Matcher matcher) {
    if (!libc_reference && matcher == MATCHER_BTNFA) {
        return false;
    }
    return true;
}

static void run_known_pairs(RegexTest *tests, int32 count, char *description,
                            bool extract);
static bool run_fuzzy_tests(MetaRegex **patterns, int32 tests_len, char *,
                            int32 max_str_size, int32 ntests, bool extract);
static bool run_file_fuzzy_tests(MetaRegex **tests, int32 tests_len,
                                 bool extract);
static bool run_extensive_regex_tests(MetaRegex **tests, int32 tests_len,
                                      char *tests_name);
static MetaRegex **random_regex_sample(MetaRegex **tests, int32 tests_len,
                                       int32 *sample_len);

#define RUN_KNOWN_PAIRS(ARRAY) \
    run_known_pairs(ARRAY, LENGTH(ARRAY), #ARRAY, true); \
    run_known_pairs(ARRAY, LENGTH(ARRAY), #ARRAY, false)

int32
main(void) {
    MetaRegex **regexes_extensive_sample;
    int32 regexes_extensive_sample_len;
    bool extensive_failed;

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

    regexes_extensive_sample = random_regex_sample(
        regexes_extensive, LENGTH(regexes_extensive),
        &regexes_extensive_sample_len);

    extensive_failed = run_extensive_regex_tests(
        regexes_extensive_sample, regexes_extensive_sample_len,
        "random half of regexes_extensive");
    free2(regexes_extensive_sample,
          LENGTH(regexes_extensive)*SIZEOF(*regexes_extensive_sample));

    if (extensive_failed) {
        printf(RED(
            "\nSample failed; running full regexes_extensive array ...\n"));
        srand((uint32)42);
        extensive_failed = run_extensive_regex_tests(
            regexes_extensive, LENGTH(regexes_extensive), "regexes_extensive");
    }

    printf("Exiting from %s...\n", __FILE__);

    if (extensive_failed) {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

#undef RUN_KNOWN_PAIRS

static void
run_known_pairs(RegexTest *tests, int32 count, char *description,
                bool extract) {
    RegexTest *reference = xmemdup(tests, count*SIZEOF(*reference));
    bool *libc_reference = malloc2(count*SIZEOF(*libc_reference));
    bool *reference_submatches = malloc2(
        count*SIZEOF(*reference_submatches));
    bool failed = false;

    printf("\n----- Running %s (%s) -----\n", description,
           extract ? "extracting" : "non-extracting");

    for (int32 i = 0; i < count; i += 1) {
        regex_t compiled_regex;
        char *regex = reference[i].meta_regex->string;
        int32 compiled = regcomp(&compiled_regex, regex, REG_EXTENDED);

        reference[i].input_len = strlen32(reference[i].input);

        if (compiled == 0) {
            libc_reference[i] = true;
            reference_submatches[i] = libc_submatches_are_portable(
                reference[i].meta_regex);
            reference[i].result = run_libc_one(
                &compiled_regex, reference[i].input, reference[i].pmatch,
                LENGTH(reference[i].pmatch), extract);
            regfree(&compiled_regex);
        } else {
            libc_reference[i] = false;
            reference_submatches[i] = true;
            reference[i].result = run_fallback_reference_one(
                reference[i].meta_regex, reference[i].input,
                reference[i].input_len, reference[i].pmatch,
                LENGTH(reference[i].pmatch), extract);
        }
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
                actual[i].result = META_REG_NOMATCH;
                continue;
            }

            actual[i].result = run_meta_one(
                meta_regex, actual[i].input, actual[i].input_len, matcher,
                actual[i].pmatch, LENGTH(actual[i].pmatch), extract);
        }
        for (int32 i = 0; i < count; i += 1) {
            if (!matcher_can_use_reference(libc_reference[i], matcher)) {
                continue;
            }
            if (!matcher_supports_regex(actual[i].meta_regex, matcher,
                                        extract)) {
                continue;
            }
            if (result_mismatch(reference[i].result, actual[i].result,
                                reference[i].pmatch, actual[i].pmatch,
                                LENGTH(actual[i].pmatch), extract,
                                reference_submatches[i])) {
                failed = true;
                report_match_mismatch(
                    "Known-pair", description, matcher, tests[i].input,
                    tests[i].meta_regex->string, reference[i].result,
                    actual[i].result, reference[i].pmatch, actual[i].pmatch,
                    LENGTH(actual[i].pmatch), extract,
                    reference_submatches[i]);
            }
        }

        free2(actual, count*SIZEOF(*actual));
    }

    free2(reference_submatches, count*SIZEOF(*reference_submatches));
    free2(libc_reference, count*SIZEOF(*libc_reference));
    free2(reference, count*SIZEOF(*reference));

    if (failed) {
        exit(EXIT_FAILURE);
    }
    return;
}

static bool
run_fuzzy_tests(MetaRegex **tests, int32 tests_len, char *tests_name,
                int32 max_str_size, int32 ninputs, bool extract) {
    int32 fuzzy_len = ninputs*tests_len;
    FuzzyTest *fuzzy = malloc2(SIZEOF(*fuzzy)*fuzzy_len);
    bool failed = false;
#if FUZZY_PRECOMPILE_LIBC
    regex_t *libc_regexes = malloc2(tests_len*SIZEOF(*libc_regexes));
    bool *libc_reference = malloc2(tests_len*SIZEOF(*libc_reference));
    bool *reference_submatches = malloc2(
        tests_len*SIZEOF(*reference_submatches));
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

        libc_reference[i]
            = regcomp(&libc_regexes[i], pattern_str, REG_EXTENDED) == 0;
        reference_submatches[i] = true;
        if (libc_reference[i]) {
            reference_submatches[i] = libc_submatches_are_portable(tests[i]);
        }
    }
#endif

    for (int32 i = 0; i < fuzzy_len; i += 1) {
        int32 idx = fuzzy[i].regex_idx;
#if FUZZY_PRECOMPILE_LIBC
        if (libc_reference[idx]) {
            fuzzy[i].result_libc = run_libc_one(
                &libc_regexes[idx], fuzzy[i].input, fuzzy[i].pmatch_libc,
                LENGTH(fuzzy[i].pmatch_libc), extract);
        } else {
            fuzzy[i].result_libc = run_fallback_reference_one(
                tests[idx], fuzzy[i].input, fuzzy[i].input_len,
                fuzzy[i].pmatch_libc, LENGTH(fuzzy[i].pmatch_libc), extract);
        }
#else
        regex_t compiled;
        char *pattern_str = tests[idx]->string;

        if (regcomp(&compiled, pattern_str, REG_EXTENDED) == 0) {
            fuzzy[i].result_libc
                = run_libc_one(&compiled, fuzzy[i].input,
                               fuzzy[i].pmatch_libc,
                               LENGTH(fuzzy[i].pmatch_libc), extract);
            regfree(&compiled);
        } else {
            fuzzy[i].result_libc = run_fallback_reference_one(
                tests[idx], fuzzy[i].input, fuzzy[i].input_len,
                fuzzy[i].pmatch_libc, LENGTH(fuzzy[i].pmatch_libc), extract);
        }
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
                fuzzy[i].result_meta = META_REG_NOMATCH;
                continue;
            }

            fuzzy[i].result_meta = run_meta_one(
                meta_pattern, fuzzy[i].input, fuzzy[i].input_len, matcher,
                fuzzy[i].pmatch_meta, LENGTH(fuzzy[i].pmatch_meta), extract);
        }
        for (int32 i = 0; i < fuzzy_len; i += 1) {
            MetaRegex *meta_pattern = tests[fuzzy[i].regex_idx];
            bool compare_submatches = true;
#if FUZZY_PRECOMPILE_LIBC
            if (!matcher_can_use_reference(libc_reference[fuzzy[i].regex_idx],
                                           matcher)) {
                continue;
            }
            compare_submatches = reference_submatches[fuzzy[i].regex_idx];
#endif
            if (!matcher_supports_regex(meta_pattern, matcher, extract)) {
                continue;
            }

            if (result_mismatch(fuzzy[i].result_libc, fuzzy[i].result_meta,
                                fuzzy[i].pmatch_libc, fuzzy[i].pmatch_meta,
                                LENGTH(fuzzy[i].pmatch_meta), extract,
                                compare_submatches)) {
                failed = true;
                report_match_mismatch(
                    "Fuzzy", tests_name, matcher, fuzzy[i].input,
                    meta_pattern->string, fuzzy[i].result_libc,
                    fuzzy[i].result_meta, fuzzy[i].pmatch_libc,
                    fuzzy[i].pmatch_meta, LENGTH(fuzzy[i].pmatch_meta),
                    extract, compare_submatches);
            }
        }
    }

#if FUZZY_PRECOMPILE_LIBC
    for (int32 i = 0; i < tests_len; i += 1) {
        if (libc_reference[i]) {
            regfree(&libc_regexes[i]);
        }
    }
    free2(reference_submatches, tests_len*SIZEOF(*reference_submatches));
    free2(libc_reference, tests_len*SIZEOF(*libc_reference));
    free2(libc_regexes, tests_len*SIZEOF(*libc_regexes));
#endif

    for (int32 i = 0; i < ninputs; i += 1) {
        int32 idx = i*tests_len;
        free2(fuzzy[idx].input, fuzzy[idx].input_len + 1);
    }
    free2(fuzzy, SIZEOF(*fuzzy)*fuzzy_len);

    return failed;
}

static bool
run_file_fuzzy_tests(MetaRegex **tests, int32 tests_len, bool extract) {
    DIR *dir = opendir("inputs");
    struct dirent *entry = NULL;
    bool failed = false;
    RegexTest dummy_test;
#if FUZZY_PRECOMPILE_LIBC
    regex_t *libc_regexes = malloc2(tests_len*SIZEOF(*libc_regexes));
    bool *libc_reference = malloc2(tests_len*SIZEOF(*libc_reference));
    bool *reference_submatches = malloc2(
        tests_len*SIZEOF(*reference_submatches));
#endif

    if (dir == NULL) {
        error("Error opening inputs directory: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

#if FUZZY_PRECOMPILE_LIBC
    for (int32 i = 0; i < tests_len; i += 1) {
        char *pattern_str = tests[i]->string;

        libc_reference[i]
            = regcomp(&libc_regexes[i], pattern_str, REG_EXTENDED) == 0;
        reference_submatches[i] = true;
        if (libc_reference[i]) {
            reference_submatches[i] = libc_submatches_are_portable(tests[i]);
        }
    }
#endif

    while ((entry = readdir(dir)) != NULL) {
        char path[512];
        char case_name[768];
        char size_pretty[32];
        uint8 *input;
        char *input_chars;
        int32 input_len;
        int32 *results_libc;
        int64 pm_sz;
        MetaRegexMatch *pm_libc;

        if (strequal(entry->d_name, ".") || strequal(entry->d_name, "..")) {
            continue;
        }

        SNPRINTF(path, "inputs/%s", entry->d_name);

        if (!read_entire_file(path, &input_chars, &input_len)) {
            continue;
        }

        input = (uint8 *)input_chars;

        bytes_pretty(size_pretty, input_len);
        SNPRINTF(case_name, "%s[%s]", entry->d_name, size_pretty);

        results_libc = malloc2(tests_len*SIZEOF(*results_libc));
        pm_sz = tests_len*LENGTH(dummy_test.pmatch) * SIZEOF(MetaRegexMatch);
        pm_libc = malloc2(pm_sz);

        for (int32 j = 0; j < tests_len; j += 1) {
            MetaRegexMatch *curr_pm = NULL;

            if (extract) {
                curr_pm = &pm_libc[j*LENGTH(dummy_test.pmatch)];
                clear_pmatch(curr_pm, LENGTH(dummy_test.pmatch));
            }
#if FUZZY_PRECOMPILE_LIBC
            if (libc_reference[j]) {
                results_libc[j]
                    = run_libc_one(&libc_regexes[j], (char *)input, curr_pm,
                                   LENGTH(dummy_test.pmatch), extract);
            } else {
                results_libc[j] = run_fallback_reference_one(
                    tests[j], (char *)input, input_len, curr_pm,
                    LENGTH(dummy_test.pmatch), extract);
            }
#else
            regex_t compiled;
            char *pattern_str = tests[j]->string;
            if (regcomp(&compiled, pattern_str, REG_EXTENDED) == 0) {
                results_libc[j]
                    = run_libc_one(&compiled, (char *)input, curr_pm,
                                   LENGTH(dummy_test.pmatch), extract);
                regfree(&compiled);
            } else {
                results_libc[j] = run_fallback_reference_one(
                    tests[j], (char *)input, input_len, curr_pm,
                    LENGTH(dummy_test.pmatch), extract);
            }
#endif
        }
        for (int32 mi = 0; mi < LENGTH(all_matchers); mi += 1) {
            enum Matcher matcher = all_matchers[mi];
            int32 *results_meta = malloc2(tests_len*SIZEOF(*results_meta));
            MetaRegexMatch *pm_meta = malloc2(pm_sz);
            if (!matcher_compile_enabled(matcher)) {
                free2(results_meta, tests_len*SIZEOF(*results_meta));
                free2(pm_meta, pm_sz);
                continue;
            }

            for (int32 j = 0; j < tests_len; j += 1) {
                MetaRegex *meta_pattern = tests[j];
                MetaRegexMatch *curr_m_pm = NULL;

                if (!matcher_supports_regex(meta_pattern, matcher, extract)) {
                    results_meta[j] = META_REG_NOMATCH;
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
                MetaRegexMatch *curr_libc = NULL;
                MetaRegexMatch *curr_meta = NULL;
                bool compare_submatches = true;

#if FUZZY_PRECOMPILE_LIBC
                if (!matcher_can_use_reference(libc_reference[j], matcher)) {
                    continue;
                }
                compare_submatches = reference_submatches[j];
#endif
                if (!matcher_supports_regex(meta_pattern, matcher, extract)) {
                    continue;
                }

                if (extract) {
                    curr_libc = &pm_libc[j*LENGTH(dummy_test.pmatch)];
                    curr_meta = &pm_meta[j*LENGTH(dummy_test.pmatch)];
                }

                if (result_mismatch(results_libc[j], results_meta[j], curr_libc,
                                    curr_meta, LENGTH(dummy_test.pmatch),
                                    extract, compare_submatches)) {
                    failed = true;
                    report_match_mismatch("File", case_name, matcher,
                                          (char *)input, meta_pattern->string,
                                          results_libc[j], results_meta[j],
                                          curr_libc, curr_meta,
                                          LENGTH(dummy_test.pmatch), extract,
                                          compare_submatches);
                }
            }

            free2(results_meta, tests_len*SIZEOF(*results_meta));
            free2(pm_meta, pm_sz);
        }

        free2(results_libc, tests_len*SIZEOF(*results_libc));
        free2(pm_libc, pm_sz);
        free2(input, input_len + 1);
    }

#if FUZZY_PRECOMPILE_LIBC
    for (int32 i = 0; i < tests_len; i += 1) {
        if (libc_reference[i]) {
            regfree(&libc_regexes[i]);
        }
    }
    free2(reference_submatches, tests_len*SIZEOF(*reference_submatches));
    free2(libc_reference, tests_len*SIZEOF(*libc_reference));
    free2(libc_regexes, tests_len*SIZEOF(*libc_regexes));
#endif

    closedir(dir);
    return failed;
}

static bool
run_extensive_regex_tests(MetaRegex **tests, int32 tests_len,
                          char *tests_name) {
    bool failed = false;

    printf(RED("\nTests with random inputs against %s ...\n"), tests_name);
    for (int32 max_input_len = 1; max_input_len <= 1024; max_input_len *= 2) {
        if (run_fuzzy_tests(tests, tests_len, tests_name, max_input_len, 100,
                            true)) {
            failed = true;
        }
        if (run_fuzzy_tests(tests, tests_len, tests_name, max_input_len, 100,
                            false)) {
            failed = true;
        }
    }

    printf(RED("\nTests from inputs/ against %s ...\n"), tests_name);
    if (run_file_fuzzy_tests(tests, tests_len, true)) {
        failed = true;
    }
    if (run_file_fuzzy_tests(tests, tests_len, false)) {
        failed = true;
    }

    return failed;
}

static MetaRegex **
random_regex_sample(MetaRegex **tests, int32 tests_len, int32 *sample_len) {
    MetaRegex **sample;
    rand_int_seed((uint64)time(NULL));

    *sample_len = tests_len / 2;
    if (*sample_len < 1) {
        *sample_len = tests_len;
    }

    sample = xmemdup(tests, tests_len*SIZEOF(*sample));

    for (int32 i = 0; i < tests_len; i += 1) {
        int32 j = i + rand_int() % (tests_len - i);
        MetaRegex *tmp = sample[i];

        sample[i] = sample[j];
        sample[j] = tmp;
    }

    return sample;
}
