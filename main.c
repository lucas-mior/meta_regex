#include <assert.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.c"
#include "meta_regex.h"
#include "meta_regex_match.c"

typedef struct RegexTest {
    char *string;
    MetaRegex meta_regex;
    int result;
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
};

int
main(int argc, char **argv) {
    struct timespec t0_posix;
    struct timespec t1_posix;
    struct timespec t0_meta;
    struct timespec t1_meta;
    RegexTest *tests_posix = xmemdup(regex_tests, SIZEOF(regex_tests));
    RegexTest *tests_meta = xmemdup(regex_tests, SIZEOF(regex_tests));
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

        match = regexec(&compiled_regex, string, 0, NULL, 0);
        printf(RED("%15s")" against "BLUE("%10s")": %d\n", string, regex, match);
        tests_posix[i].result = match;

        regfree(&compiled_regex);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_posix);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t0_meta);
    for (int32 i = 0; i < LENGTH(regex_tests); i += 1) {
        char *string = tests_meta[i].string;
        /* char *regex = tests_meta[i].meta_regex.string; */
        MetaRegex meta_regex = tests_meta[i].meta_regex;
        int match;

        match = meta_regex_match(meta_regex, string);
        tests_meta[i].result = match;
        /* printf(RED("%15s")" against "BLUE("%10s")": %d\n", string, regex, match); */
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1_meta);

    for (int32 i = 0; i < LENGTH(regex_tests); i += 1) {
        int result_posix = tests_posix[i].result;
        int result_meta = tests_meta[i].result;
        char *regex = regex_tests[i].meta_regex.string;
        char *string = regex_tests[i].string;

        if (result_posix != result_meta) {
            error("Error: regex "RED("\"%s\"")" against "BLUE("\"%s\"")"'\n",
                  regex, string);
            error("posix: %d\n", tests_posix[i].result);
            error("meta: %d\n", tests_meta[i].result);
        }
    }

    PRINT_TIMINGS(LENGTH(regex_tests), t0_posix, t1_posix, "posix tests");
    PRINT_TIMINGS(LENGTH(regex_tests), t0_meta, t1_meta, "meta tests");
    exit(EXIT_SUCCESS);
}
