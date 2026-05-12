#include <assert.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.c"
#include "meta_regex.h"

int
main(int argc, char **argv) {
    char *regex_string = "[0-9]";
    regex_t compiled_regex;
    int compile_status;
    char *test_strings[] = {
        "abc5def",
        "hello world"
    };
    int32 num_tests = 2;
    META_REGEX(regex_meta, "[0-9]");

    (void)argc;
    (void)argv;

    compile_status = regcomp(&compiled_regex, regex_string, REG_EXTENDED);

    if (compile_status != 0) {
        char error_message[256];
        regerror(compile_status, &compiled_regex, error_message, sizeof(error_message));
        error("Regex compilation failed: %s\n", error_message);
        exit(EXIT_FAILURE);
    }

    for (int32 t = 0; t < num_tests; t += 1) {
        char *input = test_strings[t];
        int match_status;
        
        printf("\nTesting string: '%s'\n", input);
        match_status = regexec(&compiled_regex, input, 0, NULL, 0);

        {
            int meta_match_status = REG_NOMATCH;

            if (regex_meta.type == META_REGEX_DIGIT) {
                for (int32 i = 0; input[i] != '\0'; i += 1) {
                    if (input[i] >= '0') {
                        if (input[i] <= '9') {
                            meta_match_status = 0;
                            break;
                        }
                    }
                }
            }
            assert(match_status == meta_match_status);
        }

        if (match_status == 0) {
            printf("Match found in target.\n");
        } else if (match_status == REG_NOMATCH) {
            printf("No match found in target.\n");
        } else {
            error("Regex execution error.\n");
            exit(EXIT_FAILURE);
        }
    }

    regfree(&compiled_regex);
    printf("\nEverything works!\n");
    exit(EXIT_SUCCESS);
}
