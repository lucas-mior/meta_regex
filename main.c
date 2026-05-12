#include <assert.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.c"
#include "meta_regex.h"

typedef struct RegexTest {
    char *string;
    char *regex;
    MetaRegex meta_regex;
} RegexTest;

static RegexTest regex_tests[] = {
    {"abc5def",      META_REGEX("[0-9]")},
    {"hello world",  META_REGEX("[0-9]")},
    {"2hello world", META_REGEX("[0-9]")},
};

int
main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    for (int32 i = 0; i < LENGTH(regex_tests); i += 1) {
        regex_t compiled_regex;
        char *string = regex_tests[i].string;
        char *regex = regex_tests[i].regex;
        MetaRegex meta_regex = regex_tests[i].meta_regex;
        int compile_status;
        int posix_match_status;
        int meta_match_status = REG_NOMATCH;

        compile_status = regcomp(&compiled_regex, regex, REG_EXTENDED);
        
        if (compile_status != 0) {
            char error_message[256];
            regerror(compile_status, &compiled_regex, error_message, sizeof(error_message));
            error("Regex compilation failed: %s\n", error_message);
            exit(EXIT_FAILURE);
        }
        printf("Testing string: '%s'\n", string);
        posix_match_status = regexec(&compiled_regex, string, 0, NULL, 0);

        {
            if (meta_regex.type == META_REGEX_DIGIT) {
                for (int32 j = 0; string[j] != '\0'; j += 1) {
                    if (string[j] >= '0') {
                        if (string[j] <= '9') {
                            meta_match_status = 0;
                            break;
                        }
                    }
                }
            }
            assert(posix_match_status == meta_match_status);
        }

        regfree(&compiled_regex);
    }

    printf("\nEverything works!\n");
    exit(EXIT_SUCCESS);
}
