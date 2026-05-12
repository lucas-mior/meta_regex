#include <assert.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>

#include "cbase/util.c"

#include "meta_regex.h"
#define META_REGEX(...)

int
main(int argc, char **argv) {
    char *input;
    char *regex_string = "[0-9]";
    regex_t compiled_regex;
    int compile_status;
    int match_status;
    META_REGEX(regex_meta, "[0-9]");

    if (argc < 2) {
        error("usage: regex <string>");
    }
    input = argv[1];

    compile_status = regcomp(&compiled_regex, regex_string, REG_EXTENDED);

    if (compile_status != 0) {
        char error_message[256];
        regerror(compile_status, &compiled_regex, error_message, sizeof(error_message));
        error("Regex compilation failed: %s\n", error_message);
        exit(EXIT_FAILURE);
    }

    match_status = regexec(&compiled_regex, input, 0, NULL, 0);
    regfree(&compiled_regex);

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
        printf("Match found in target: '%s'\n", input);
        exit(EXIT_SUCCESS);
    }

    if (match_status == REG_NOMATCH) {
        printf("No match found in target: '%s'\n", input);
        exit(EXIT_FAILURE);
    }

    error("Regex execution error.\n");
    exit(EXIT_FAILURE);
}
