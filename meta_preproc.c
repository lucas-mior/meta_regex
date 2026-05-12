#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbase/util.c"
#include "meta_regex.h"

int
main(int argc, char **argv) {
    FILE *input_file;
    char buffer[4096];
    char *macro_start = "META_REGEX(";
    char *target_regex = "\"[0-9]\");";

    if (argc < 2) {
        fprintf(stderr, "Usage: preprocessor <file.c>\n");
        exit(EXIT_FAILURE);
    }

    input_file = fopen(argv[1], "r");
    if (input_file == NULL) {
        fprintf(stderr, "Error opening file.\n");
        exit(EXIT_FAILURE);
    }

    while (fgets(buffer, sizeof(buffer), input_file) != NULL) {
        char *found_macro;
        char *identifier_start;
        char *comma;
        char *regex_start;

        found_macro = strstr(buffer, macro_start);
        if (found_macro == NULL) {
            printf("%s", buffer);
            continue;
        }

        identifier_start = found_macro + strlen(macro_start);
        comma = strchr(identifier_start, ',');
        if (comma == NULL) {
            printf("%s", buffer);
            continue;
        }

        regex_start = strstr(comma, target_regex);
        if (regex_start == NULL) {
            printf("%s", buffer);
            continue;
        }

        {
            int32 identifier_length;
            char identifier[256] = {0};
            int32 prefix_length;
            char *suffix;

            identifier_length = comma - identifier_start;
            strncpy(identifier, identifier_start, identifier_length);

            prefix_length = found_macro - buffer;
            suffix = regex_start + strlen(target_regex);

            printf("%.*sMetaRegex %s = { .type = META_REGEX_DIGIT };%s",
                   prefix_length, buffer, identifier, suffix);
        }
    }

    fclose(input_file);
    exit(EXIT_SUCCESS);
}
