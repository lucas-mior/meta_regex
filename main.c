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
#define MAX_MATCHES 4

typedef struct RegexTest {
    char *string;
    MetaRegex meta_regex;
    int32 result;
    regmatch_t pmatch[MAX_MATCHES];
} RegexTest;

typedef struct FuzzyTest {
    char *string;
    int32 string_size;
    int32 regex_idx;
    int32 result_posix;
    regmatch_t pmatch_posix[MAX_MATCHES];
    int32 result_meta;
    regmatch_t pmatch_meta[MAX_MATCHES];
} FuzzyTest;

static RegexTest regex_tests[] = {
    {"abc5def",      R("[0-9]")},
    {"hello world",  R("[0-9]")},
    {"2hello world", R("^[0-9]")},
    {"hello 2",      R("^[0-9]")},
    {"test end5",    R("[0-9]$")},
    {"test 5end",    R("[0-9]$")},
    {"a.c",          R("a.c")},
    {"abc",          R("a.c")},
    {"abc",          R("^abc$")},
    {"abcd",         R("^abc$")},
    {"HELLO",        R("[A-Z]")},
    {"hello",        R("[A-Z]")},
    {"abc XYZ 123",  R("[a-z]")},
    {"123 XYZ",      R("[a-z]")},
    {"foo (bar)",    R("\\([a-z]+\\)")},
    {"foo bar",      R("(foo) (bar)")},
    {"a1b2",         R("([a-z])([0-9])")},
    {"nested",       R("n(e(s)t)ed")},
    {"abbbc",        R("ab*c")},
    {"ac",           R("ab*c")},
    {"ac",           R("ab+c")},
    {"abc",          R("ab+c")},
    {"abc",          R("ab?c")},
    {"ac",           R("ab?c")},
    {"abbc",         R("ab?c")},
    {"a123b",        R("a[0-9]+b")},
    {"aXXXb",        R("a.+b")},
    {"ab",           R("a.*b")},
    {"a",            R("a|b")},
    {"b",            R("a|b")},
    {"c",            R("a|b")},
    {"foo",          R("foo|bar")},
    {"bar",          R("foo|bar")},
    {"baz",          R("foo|bar")},
    {"abc",          R("a(b|c|d)c")},
    {"acc",          R("a(b|c|d)c")},
    {"adc",          R("a(b|c|d)c")},
    {"aec",          R("a(b|c|d)c")},
    {"123 foo",      R("([0-9]+) (foo|bar)")},
    {"456 bar",      R("([0-9]+) (foo|bar)")},
    {"789 baz",      R("([0-9]+) (foo|bar)")},
    {"a",            R("[abc]")},
    {"é",            R("[é]")},
    {"d",            R("[abc]")},
    {"x",            R("[^abc]")},
    {"a",            R("[^abc]")},
    {"foo123bar",    R("[0-9]+")},
    {"A",            R("[a-zA-Z]")},
    {"-",            R("[-abc]")},
    {"-",            R("[abc-]")},
    {"]",            R("[]abc]")},
    {"a",            R("[]abc]")},
    {"]",            R("[^]abc]")},
    {"x",            R("[^]abc]")},
    {"a{3}b",        R("a\\{3\\}b")},
    {"aaab",         R("a{3}b")},
    {"aab",          R("a{3}b")},
    {"aaaab",        R("a{3}b")},
    {"aab",          R("a{2,4}b")},
    {"aaab",         R("a{2,4}b")},
    {"aaaab",        R("a{2,4}b")},
    {"aaaaab",       R("a{2,4}b")},
    {"ab",           R("a{2,4}b")},
    {"aab",          R("a{2,}b")},
    {"aaaaaaab",     R("a{2,}b")},
    {"ab",           R("a{2,}b")},
    {"123a",         R("[[:digit:]]+")},
    {"123",          R("^[[:digit:]]+$")},
    {"abc",          R("^[[:alpha:]]+$")},
    {"a1B",          R("^[[:alnum:]]+$")},
    {" \t\n",        R("^[[:space:]]+$")},
    {"a B",          R("^[[:lower:]][[:space:]][[:upper:]]$")},
    {"a1 B",         R("^[[:lower:][:digit:]]+[[:space:]][[:upper:]]$")},
    {"!@#$%&*()-+=", R("^[[:punct:]]+$")},
    {" ",            R("^[^[:alnum:][:punct:]]$")},
    {"e",            R("^é$")},
};

int
main(void) {
    struct timespec t0_posix;
    struct timespec t1_posix;
    struct timespec t0_meta;
    struct timespec t1_meta;
    struct timespec t0_fuzzy_posix;
    struct timespec t1_fuzzy_posix;
    struct timespec t0_fuzzy_meta;
    struct timespec t1_fuzzy_meta;
    RegexTest *tests_posix = xmemdup(regex_tests, SIZEOF(regex_tests));
    RegexTest *tests_meta = xmemdup(regex_tests, SIZEOF(regex_tests));
    setlocale(LC_ALL, "en_US.UTF-8");
    srand((uint)42);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_posix);
    for (int32 i = 0; i < LENGTH(regex_tests); i += 1) {
        regex_t compiled_regex;
        char *string = tests_posix[i].string;
        char *regex = tests_posix[i].meta_regex.string;
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
        MetaRegex meta_regex = tests_meta[i].meta_regex;

        tests_meta[i].result
            = meta_regex_match(meta_regex, string,
                               MAX_MATCHES, tests_meta[i].pmatch);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_meta);

    for (int32 i = 0; i < LENGTH(regex_tests); i += 1) {
        RegexTest test_posix = tests_posix[i];
        RegexTest test_meta = tests_meta[i];
        char *regex = regex_tests[i].meta_regex.string;
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
                if (test_posix.pmatch[m].rm_so != test_meta.pmatch[m].rm_so) {
                    error("Error: mismatch rm_so in %s (group %d)\n",
                          string, m);
                }
                if (test_posix.pmatch[m].rm_eo != test_meta.pmatch[m].rm_eo) {
                    error("Error: mismatch rm_eo in %s (group %d)\n",
                          string, m);
                }
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

        /* Batch POSIX */
        clock_gettime(CLOCK_MONOTONIC_RAW, &t0_fuzzy_posix);
        for (int32 i = 0; i < NFUZZY; i += 1) {
            regex_t compiled;
            int32 comp_error;
            char *pattern_str;

            pattern_str = regex_tests[fuzzy_cases[i].regex_idx].meta_regex.string;
            if ((comp_error = regcomp(&compiled, pattern_str, REG_EXTENDED))) {
                error("Error compiling %s: %s.\n", pattern_str, strerror(errno));
                fatal(EXIT_FAILURE);
            }
            fuzzy_cases[i].result_posix
                = regexec(&compiled, fuzzy_cases[i].string,
                          MAX_MATCHES, fuzzy_cases[i].pmatch_posix, 0);
            regfree(&compiled);
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1_fuzzy_posix);

        clock_gettime(CLOCK_MONOTONIC_RAW, &t0_fuzzy_meta);
        for (int32 i = 0; i < NFUZZY; i += 1) {
            MetaRegex meta_pattern;

            meta_pattern = regex_tests[fuzzy_cases[i].regex_idx].meta_regex;

            fuzzy_cases[i].result_meta
                = meta_regex_match(meta_pattern,
                                   fuzzy_cases[i].string,
                                   MAX_MATCHES,
                                   fuzzy_cases[i].pmatch_meta);
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1_fuzzy_meta);

        for (int32 i = 0; i < NFUZZY; i += 1) {
            if (fuzzy_cases[i].result_posix != fuzzy_cases[i].result_meta) {
                char *string = fuzzy_cases[i].string;
                char *regex = regex_tests[fuzzy_cases[i].regex_idx].meta_regex.string;
                int grep_match;
                int meta_match;
                int posix_match;
                char *tmp_inputs = "/tmp/inputs.txt";
                char *tmp_regex = "/tmp/regex.txt";
                FILE *tmp_in;
                FILE *tmp_re;
                int32 argc = 0;
                char *argv[16];

                if ((tmp_in = fopen(tmp_inputs, "w")) == NULL) {
                    error("Error opening %s: %s.\n",
                          tmp_inputs, strerror(errno));
                    fatal(EXIT_FAILURE);
                }
                fputs(string, tmp_in);
                fclose(tmp_in);

                if ((tmp_re = fopen(tmp_regex, "w")) == NULL) {
                    error("Error opening %s: %s.\n",
                          tmp_regex, strerror(errno));
                    fatal(EXIT_FAILURE);
                }
                fputs(regex, tmp_re);
                fclose(tmp_re);

                argv[argc++] = "grep";
                argv[argc++] = "-qE";
                argv[argc++] = "-f";
                argv[argc++] = tmp_regex;
                argv[argc++] = tmp_inputs;
                argv[argc++] = NULL;

                grep_match = (util_command(argc, argv) == 0);
                meta_match = (fuzzy_cases[i].result_meta == 0);
                posix_match = (fuzzy_cases[i].result_posix == 0);

                error("Mismatch at %d: /%s/ against \"%s\"\n", i, regex, string);
                error("\nPOSIX says: %s\nMeta says: %s\nGrep says: %s\n", 
                      posix_match ? "MATCH" : "NOMATCH",
                      meta_match  ? "MATCH" : "NOMATCH",
                      grep_match  ? "MATCH" : "NOMATCH");
                if (meta_match != grep_match) {
                    exit(EXIT_FAILURE);
                }
                break;
            }
        }

        PRINT_TIMINGS(NFUZZY, t0_fuzzy_posix, t1_fuzzy_posix, "fuzzy posix tests");
        PRINT_TIMINGS(NFUZZY, t0_fuzzy_meta, t1_fuzzy_meta, "fuzzy meta tests");

        for (int32 i = 0; i < NFUZZY; i += 1) {
            free2(fuzzy_cases[i].string, fuzzy_cases[i].string_size);
        }
        free2(fuzzy_cases, SIZEOF(FuzzyTest)*NFUZZY);
    }

    free2(tests_posix, SIZEOF(regex_tests));
    free2(tests_meta, SIZEOF(regex_tests));

    exit(EXIT_SUCCESS);
}
