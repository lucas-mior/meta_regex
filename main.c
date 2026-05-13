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

typedef struct RegexTest {
    char *string;
    MetaRegex meta_regex;
    int result;
    regmatch_t pmatch[MAX_MATCHES];
} RegexTest;

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
            buffer[current_byte] = (char)(32 + (rand() % 95));
            current_byte += 1;
        } else if (choice < 85) {
            buffer[current_byte] = (char)(0xC0 | ((rand() % 32)));
            buffer[current_byte + 1] = (char)(0x80 | (rand() % 64));
            current_byte += 2;
        } else if (choice < 95) {
            buffer[current_byte] = (char)(0xE0 | (rand() % 16));
            buffer[current_byte + 1] = (char)(0x80 | (rand() % 64));
            buffer[current_byte + 2] = (char)(0x80 | (rand() % 64));
            current_byte += 3;
        } else {
            buffer[current_byte] = (char)(0xF0 | (rand() % 8));
            buffer[current_byte + 1] = (char)(0x80 | (rand() % 64));
            buffer[current_byte + 2] = (char)(0x80 | (rand() % 64));
            buffer[current_byte + 3] = (char)(0x80 | (rand() % 64));
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
    RegexTest *tests_posix = xmemdup(regex_tests, SIZEOF(regex_tests));
    RegexTest *tests_meta = xmemdup(regex_tests, SIZEOF(regex_tests));
    setlocale(LC_ALL, "");
    srand((unsigned int)time(NULL));
    (void)argc;
    (void)argv;

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_posix);
    for (int32 i = 0; i < LENGTH(regex_tests); i += 1) {
        regex_t compiled_regex;
        char *string = tests_posix[i].string;
        char *regex = tests_posix[i].meta_regex.string;
        int compile_status;
        int match;

        compile_status = regcomp(&compiled_regex, regex, REG_EXTENDED);
        
        if (compile_status != 0) {
            char error_message[256];
            regerror(compile_status, &compiled_regex,
                     error_message, sizeof(error_message));
            error("Regex compilation failed: %s\n", error_message);
            exit(EXIT_FAILURE);
        }

        match = regexec(&compiled_regex, string, MAX_MATCHES, tests_posix[i].pmatch, 0);
        tests_posix[i].result = match;

        regfree(&compiled_regex);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_posix);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_meta);
    for (int32 i = 0; i < LENGTH(regex_tests); i += 1) {
        char *string = tests_meta[i].string;
        MetaRegex meta_regex = tests_meta[i].meta_regex;
        int match;

        match = meta_regex_match(meta_regex, string, MAX_MATCHES, tests_meta[i].pmatch);
        tests_meta[i].result = match;
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_meta);

    for (int32 i = 0; i < LENGTH(regex_tests); i += 1) {
        RegexTest test_posix = tests_posix[i];
        RegexTest test_meta = tests_meta[i];
        char *regex = regex_tests[i].meta_regex.string;
        char *string = regex_tests[i].string;

        printf(RED("%15s")"against"BLUE("%18s")": %d\n",
               string, regex, test_posix.result);

        if (test_posix.result != test_meta.result) {
            error("Error: regex "RED("\"%s\"")" against "BLUE("\"%s\"")"'\n",
                  regex, string);
            error("posix: %d\n", test_posix.result);
            error("meta: %d\n", test_meta.result);
        }

        if (test_posix.result == 0) {
            for (size_t m = 0; m < MAX_MATCHES; m += 1) {
                if (test_posix.pmatch[m].rm_so != test_meta.pmatch[m].rm_so) {
                    error("Error: mismatch rm_so in "
                          RED("\"%s\"")" against "BLUE("\"%s\"")
                          " (group %zu): posix %d, meta %d\n",
                          string, regex,
                          m, test_posix.pmatch[m].rm_so, test_meta.pmatch[m].rm_so);
                }
                if (test_posix.pmatch[m].rm_eo != test_meta.pmatch[m].rm_eo) {
                    error("Error: mismatch rm_eo in "
                          RED("\"%s\"")" against "BLUE("\"%s\"")
                          " (group %zu): posix %d, meta %d\n",
                          string, regex,
                          m, test_posix.pmatch[m].rm_eo, test_meta.pmatch[m].rm_eo);
                }
            }
        }
    }

    PRINT_TIMINGS(LENGTH(regex_tests), t0_posix, t1_posix, "posix tests");
    PRINT_TIMINGS(LENGTH(regex_tests), t0_meta, t1_meta, "meta tests");

    printf("\n--- Starting Fuzzy Testing (1000 iterations) ---\n");

    {
        int32 fuzzy_iterations = 1000;
        char *fuzzy_buffer = malloc2(4097);

        for (int32 i = 0; i < fuzzy_iterations; i += 1) {
            int32 length = 1 + (rand() % 4096);
            int32 pattern_index = rand() % LENGTH(regex_tests);
            char *pattern_string = regex_tests[pattern_index].meta_regex.string;
            MetaRegex meta_pattern = regex_tests[pattern_index].meta_regex;
            regex_t posix_pattern;
            regmatch_t posix_pmatch[MAX_MATCHES];
            regmatch_t meta_pmatch[MAX_MATCHES];
            int posix_res;
            int meta_res;

            generate_random_utf8_string(fuzzy_buffer, length);

            regcomp(&posix_pattern, pattern_string, REG_EXTENDED);
            
            posix_res = regexec(&posix_pattern, fuzzy_buffer, MAX_MATCHES, posix_pmatch, 0);
            meta_res = meta_regex_match(meta_pattern, fuzzy_buffer, MAX_MATCHES, meta_pmatch);

            if (posix_res != meta_res) {
                error("Fuzzy failure (result) at iteration %d\nPattern: %s\n", i, pattern_string);
            } else if (posix_res == 0) {
                for (int32 m = 0; m < MAX_MATCHES; m += 1) {
                    if (posix_pmatch[m].rm_so != meta_pmatch[m].rm_so || posix_pmatch[m].rm_eo != meta_pmatch[m].rm_eo) {
                        error("Fuzzy failure (match groups) at iteration %d\nPattern: %s\n", i, pattern_string);
                        break;
                    }
                }
            }

            regfree(&posix_pattern);
        }

        free2(fuzzy_buffer, 4097);
    }

    exit(EXIT_SUCCESS);
}
