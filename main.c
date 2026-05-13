#include <assert.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <time.h>

#include "util.c"
#include "meta_regex.h"
#include "meta_regex_match.c"

#define MAX_MATCHES 4
#define FUZZY_ITERATIONS 1000

typedef struct RegexTest {
    char *string;
    MetaRegex meta_regex;
    int32 result;
    regmatch_t pmatch[MAX_MATCHES];
} RegexTest;

typedef struct FuzzyTest {
    char *string;
    int32 string_size;
    int32 pattern_index;
    int32 result_posix;
    regmatch_t pmatch_posix[MAX_MATCHES];
    int32 result_meta;
    regmatch_t pmatch_meta[MAX_MATCHES];
} FuzzyTest;

static RegexTest regex_tests[] = {
    {"abc5def",      META_REGEX("[0-9]")},
    {"hello world",  META_REGEX("[0-9]")},
    {"2hello world", META_REGEX("^[0-9]")},
    {"hello 2",      META_REGEX("^[0-9]")},
    {"test end5",    META_REGEX("[0-9]$")},
    {"test 5end",    META_REGEX("[0-9]$")},
    {"a.c",          META_REGEX("a.c")},
    {"abc",          META_REGEX("a.c")},
    {"abc",          META_REGEX("^abc$")},
    {"abcd",         META_REGEX("^abc$")},
    {"HELLO",        META_REGEX("[A-Z]")},
    {"hello",        META_REGEX("[A-Z]")},
    {"abc XYZ 123",  META_REGEX("[a-z]")},
    {"123 XYZ",      META_REGEX("[a-z]")},
    {"foo (bar)",    META_REGEX("\\([a-z]+\\)")},
    {"foo bar",      META_REGEX("(foo) (bar)")},
    {"a1b2",         META_REGEX("([a-z])([0-9])")},
    {"nested",       META_REGEX("n(e(s)t)ed")},
    {"abbbc",        META_REGEX("ab*c")},
    {"ac",           META_REGEX("ab*c")},
    {"ac",           META_REGEX("ab+c")},
    {"abc",          META_REGEX("ab+c")},
    {"abc",          META_REGEX("ab?c")},
    {"ac",           META_REGEX("ab?c")},
    {"abbc",         META_REGEX("ab?c")},
    {"a123b",        META_REGEX("a[0-9]+b")},
    {"aXXXb",        META_REGEX("a.+b")},
    {"ab",           META_REGEX("a.*b")},
    {"a",            META_REGEX("a|b")},
    {"b",            META_REGEX("a|b")},
    {"c",            META_REGEX("a|b")},
    {"foo",          META_REGEX("foo|bar")},
    {"bar",          META_REGEX("foo|bar")},
    {"baz",          META_REGEX("foo|bar")},
    {"abc",          META_REGEX("a(b|c|d)c")},
    {"acc",          META_REGEX("a(b|c|d)c")},
    {"adc",          META_REGEX("a(b|c|d)c")},
    {"aec",          META_REGEX("a(b|c|d)c")},
    {"123 foo",      META_REGEX("([0-9]+) (foo|bar)")},
    {"456 bar",      META_REGEX("([0-9]+) (foo|bar)")},
    {"789 baz",      META_REGEX("([0-9]+) (foo|bar)")},
    {"a",            META_REGEX("[abc]")},
    {"d",            META_REGEX("[abc]")},
    {"x",            META_REGEX("[^abc]")},
    {"a",            META_REGEX("[^abc]")},
    {"foo123bar",    META_REGEX("[0-9]+")},
    {"A",            META_REGEX("[a-zA-Z]")},
    {"-",            META_REGEX("[-abc]")},
    {"-",            META_REGEX("[abc-]")},
    {"]",            META_REGEX("[]abc]")},
    {"a",            META_REGEX("[]abc]")},
    {"]",            META_REGEX("[^]abc]")},
    {"x",            META_REGEX("[^]abc]")},
    {"a{3}b",        META_REGEX("a\\{3\\}b")},
    {"aaab",         META_REGEX("a{3}b")},
    {"aab",          META_REGEX("a{3}b")},
    {"aaaab",        META_REGEX("a{3}b")},
    {"aab",          META_REGEX("a{2,4}b")},
    {"aaab",         META_REGEX("a{2,4}b")},
    {"aaaab",        META_REGEX("a{2,4}b")},
    {"aaaaab",       META_REGEX("a{2,4}b")},
    {"ab",           META_REGEX("a{2,4}b")},
    {"aab",          META_REGEX("a{2,}b")},
    {"aaaaaaab",     META_REGEX("a{2,}b")},
    {"ab",           META_REGEX("a{2,}b")},
    {"123a",         META_REGEX("[[:digit:]]+")},
    {"123",          META_REGEX("^[[:digit:]]+$")},
    {"abc",          META_REGEX("^[[:alpha:]]+$")},
    {"a1B",          META_REGEX("^[[:alnum:]]+$")},
    {" \t\n",        META_REGEX("^[[:space:]]+$")},
    {"a B",          META_REGEX("^[[:lower:]][[:space:]][[:upper:]]$")},
    {"a1 B",         META_REGEX("^[[:lower:][:digit:]]+[[:space:]][[:upper:]]$")},
    {"!",            META_REGEX("^[[:punct:]]$")},
    {" ",            META_REGEX("^[^[:alnum:][:punct:]]$")},
    {"aàà",          META_REGEX("[[.a.]]")},
    {"b",            META_REGEX("[[.a.]]")},
    {"eéèêẽë",       META_REGEX("^[[=e=]]+$")},
    {"aáàâãä",       META_REGEX("^[[=e=]]+$")},
    {"aáàâãä",       META_REGEX("^[[=a=]]+$")},
    {"f",            META_REGEX("[[=e=]]")},
    {"a",            META_REGEX("[[=a=][.b.]]")},
    {"b",            META_REGEX("[[=a=][.b.]]")},
    {"c",            META_REGEX("[[=a=][.b.]]")},
};

static void
generate_random_utf8_string(char *buffer, int32 max_bytes) {
    int32 current_byte = 0;

    while (current_byte < max_bytes - 4) {
        int32 choice = rand() % 100;

        if (choice < 70) {
            /* Strictly printable ASCII: 32 (space) to 126 (~) */
            buffer[current_byte] = (char)(32 + (rand() % 95));
            current_byte += 1;
        } else if (choice < 85) {
            /* 2-byte sequence: 0xC2-0xDF followed by 0x80-0xBF */
            /* We start at 0xC2 to avoid overlong encodings (0xC0, 0xC1) */
            buffer[current_byte] = (char)(0xC2 + (rand() % 30));
            buffer[current_byte + 1] = (char)(0x80 | (rand() % 64));
            current_byte += 2;
        } else if (choice < 95) {
            /* 3-byte sequence: 0xE0-0xEF followed by two continuation bytes */
            char b1 = (char)(0xE0 | (rand() % 16));
            char b2 = (char)(0x80 | (rand() % 64));
            char b3 = (char)(0x80 | (rand() % 64));

            /* Prevent overlongs and UTF-16 surrogate halves (U+D800 - U+DFFF) */
            if (b1 == (char)0xE0 && b2 < (char)0xA0) {
                b2 |= (char)0xA0;
            }
            if (b1 == (char)0xED && b2 > (char)0x9F) {
                b2 &= (char)0x9F;
            }

            buffer[current_byte] = b1;
            buffer[current_byte + 1] = b2;
            buffer[current_byte + 2] = b3;
            current_byte += 3;
        } else {
            /* 4-byte sequence: 0xF0-0xF4 followed by three continuation bytes */
            char b1 = (char)(0xF0 | (rand() % 5));
            char b2 = (char)(0x80 | (rand() % 64));
            char b3 = (char)(0x80 | (rand() % 64));
            char b4 = (char)(0x80 | (rand() % 64));

            /* Prevent overlongs and out-of-range values (> U+10FFFF) */
            if (b1 == (char)0xF0 && b2 < (char)0x90) {
                b2 |= (char)0x90;
            }
            if (b1 == (char)0xF4 && b2 > (char)0x8F) {
                b2 &= (char)0x8F;
            }

            buffer[current_byte] = b1;
            buffer[current_byte + 1] = b2;
            buffer[current_byte + 2] = b3;
            buffer[current_byte + 3] = b4;
            current_byte += 4;
        }
    }

    buffer[current_byte] = '\0';
    return;
}

int
main(int argc, char **argv) {
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
    setlocale(LC_ALL, "");
    srand((uint)42);
    (void)argc;
    (void)argv;

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
        }
        if (test_posix.result == 0) {
            for (size_t m = 0; m < MAX_MATCHES; m += 1) {
                if (test_posix.pmatch[m].rm_so != test_meta.pmatch[m].rm_so) {
                    error("Error: mismatch rm_so in %s (group %zu)\n",
                          string, m);
                }
                if (test_posix.pmatch[m].rm_eo != test_meta.pmatch[m].rm_eo) {
                    error("Error: mismatch rm_eo in %s (group %zu)\n",
                          string, m);
                }
            }
        }
    }

    PRINT_TIMINGS(LENGTH(regex_tests), t0_posix, t1_posix, "posix tests");
    PRINT_TIMINGS(LENGTH(regex_tests), t0_meta, t1_meta, "meta tests");

    printf("\n--- Starting Fuzzy Testing (%d iterations) ---\n", FUZZY_ITERATIONS);
    {
        FuzzyTest *fuzzy_cases;
        int32 fuzzy_iterations = FUZZY_ITERATIONS;

        /* Phase 1: Create the fuzzy data and the pattern index to match */
        fuzzy_cases = malloc2(SIZEOF(FuzzyTest)*fuzzy_iterations);
        for (int32 i = 0; i < fuzzy_iterations; i += 1) {
            int32 length = 1 + (rand() % 4096);
            fuzzy_cases[i].string_size = length + 1;
            fuzzy_cases[i].string = malloc2(fuzzy_cases[i].string_size);
            generate_random_utf8_string(fuzzy_cases[i].string, length);
            fuzzy_cases[i].pattern_index = rand() % LENGTH(regex_tests);
        }

        /* Phase 2: Test the fuzzy data on posix regexes */
        clock_gettime(CLOCK_MONOTONIC_RAW, &t0_fuzzy_posix);
        for (int32 i = 0; i < fuzzy_iterations; i += 1) {
            regex_t compiled;
            int32 comp_error;
            char *pattern_str;

            pattern_str = regex_tests[fuzzy_cases[i].pattern_index].meta_regex.string;
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

        /* Phase 3: Test the same fuzzy data on meta regexes */
        clock_gettime(CLOCK_MONOTONIC_RAW, &t0_fuzzy_meta);
        for (int32 i = 0; i < fuzzy_iterations; i += 1) {
            MetaRegex meta_pattern;

            meta_pattern = regex_tests[fuzzy_cases[i].pattern_index].meta_regex;

            fuzzy_cases[i].result_meta
                = meta_regex_match(meta_pattern,
                                   fuzzy_cases[i].string,
                                   MAX_MATCHES,
                                   fuzzy_cases[i].pmatch_meta);
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1_fuzzy_meta);

        /* Phase 4: Compare results using the same format as static tests */
        FILE *file = fopen("problems.txt", "w");
        for (int32 i = 0; i < fuzzy_iterations; i += 1) {
            char *string = fuzzy_cases[i].string;
            char *regex;

            regex = regex_tests[fuzzy_cases[i].pattern_index].meta_regex.string;

            if (fuzzy_cases[i].result_posix != fuzzy_cases[i].result_meta) {
                error("Error: result mismatch for regex "
                      RED("\"%s\"")" against "BLUE("\"%s\"")"\n", regex, string);
                error("posix: %d, meta: %d\n",
                      fuzzy_cases[i].result_posix, fuzzy_cases[i].result_meta);
                fprintf(file, "%s against %s\n", regex, string);
            }
            
            /* if (fuzzy_cases[i].result_posix == 0) { */
            /*     for (size_t m = 0; m < MAX_MATCHES; m += 1) { */
            /*         if (fuzzy_cases[i].pmatch_posix[m].rm_so
             *             != fuzzy_cases[i].pmatch_meta[m].rm_so) { */
            /*             error("Error: mismatch rm_so in %s (group %zu)\n",
             *                   string, m); */
            /*         } */
            /*         if (fuzzy_cases[i].pmatch_posix[m].rm_eo
             *             != fuzzy_cases[i].pmatch_meta[m].rm_eo) { */
            /*             error("Error: mismatch rm_eo in %s (group %zu)\n",
             *                   string, m); */
            /*         } */
            /*     } */
            /* } */
        }
        fclose(file);

        PRINT_TIMINGS(fuzzy_iterations, t0_fuzzy_posix, t1_fuzzy_posix, "fuzzy posix tests");
        PRINT_TIMINGS(fuzzy_iterations, t0_fuzzy_meta, t1_fuzzy_meta, "fuzzy meta tests");

        /* Cleanup fuzzy data */
        for (int32 i = 0; i < fuzzy_iterations; i += 1) {
            free2(fuzzy_cases[i].string, fuzzy_cases[i].string_size);
        }
        free2(fuzzy_cases, SIZEOF(FuzzyTest)*fuzzy_iterations);
    }

    /* Cleanup static test copies */
    free2(tests_posix, SIZEOF(regex_tests));
    free2(tests_meta, SIZEOF(regex_tests));

    exit(EXIT_SUCCESS);
}
