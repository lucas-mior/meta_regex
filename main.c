#include <assert.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

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
    {"f",            META_REGEX("[[=e=]]")},
    {"a",            META_REGEX("[[=a=][.b.]]")},
    {"b",            META_REGEX("[[=a=][.b.]]")},
    {"c",            META_REGEX("[[=a=][.b.]]")},
};

int
main(int argc, char **argv) {
    struct timespec t0_posix;
    struct timespec t1_posix;
    struct timespec t0_meta;
    struct timespec t1_meta;
    RegexTest *tests_posix = xmemdup(regex_tests, SIZEOF(regex_tests));
    RegexTest *tests_meta = xmemdup(regex_tests, SIZEOF(regex_tests));
    setlocale(LC_ALL, "");
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

        printf(RED("%15s")" against "BLUE("%18s")": %d\n",
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
    exit(EXIT_SUCCESS);
}
