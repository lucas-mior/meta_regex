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
        char *quote_start;

        found_macro = strstr(buffer, macro_start);
        if (found_macro == NULL) {
            printf("%s", buffer);
            continue;
        }

        quote_start = strchr(found_macro, '"');
        if (quote_start == NULL) {
            printf("%s", buffer);
            continue;
        }

        {
            char *quote_end;

            quote_end = strchr(quote_start + 1, '"');
            if (quote_end != NULL) {
                char regex_string[256] = {0};
                int32 prefix_length;
                char *paren_end;
                char *end_of_line;
                int32 has_start = 0;
                int32 has_end = 0;
                char operations_buffer[2048] = {0};
                int32 regex_index = 0;
                int32 original_string_length;

                strncpy(regex_string, quote_start + 1, quote_end - quote_start - 1);

                if (regex_string[regex_index] == '^') {
                    has_start = 1;
                    regex_index += 1;
                }

                while (regex_string[regex_index] != '\0') {
                    if ((regex_string[regex_index] == '$') && (regex_string[regex_index + 1] == '\0')) {
                        has_end = 1;
                        break;
                    }
                    if (regex_string[regex_index] == '.') {
                        sprintf(operations_buffer + strlen(operations_buffer), "{META_OP_ANY, 0}, ");
                        regex_index += 1;
                        continue;
                    }
                    if (regex_string[regex_index] == '[') {
                        if (strncmp(&regex_string[regex_index], "[0-9]", 5) == 0) {
                            sprintf(operations_buffer + strlen(operations_buffer), "{META_OP_DIGIT, 0}, ");
                            regex_index += 5;
                            continue;
                        }
                        if (strncmp(&regex_string[regex_index], "[a-z]", 5) == 0) {
                            sprintf(operations_buffer + strlen(operations_buffer), "{META_OP_ALPHA_LOWER, 0}, ");
                            regex_index += 5;
                            continue;
                        }
                        if (strncmp(&regex_string[regex_index], "[A-Z]", 5) == 0) {
                            sprintf(operations_buffer + strlen(operations_buffer), "{META_OP_ALPHA_UPPER, 0}, ");
                            regex_index += 5;
                            continue;
                        }
                        sprintf(operations_buffer + strlen(operations_buffer), "{META_OP_LITERAL, '%c'}, ", regex_string[regex_index]);
                        regex_index += 1;
                        continue;
                    }
                    if (regex_string[regex_index] == '\\') {
                        regex_index += 1;
                        if (regex_string[regex_index] != '\0') {
                            sprintf(operations_buffer + strlen(operations_buffer), "{META_OP_LITERAL, '%c'}, ", regex_string[regex_index]);
                            regex_index += 1;
                        }
                        continue;
                    }
                    sprintf(operations_buffer + strlen(operations_buffer), "{META_OP_LITERAL, '%c'}, ", regex_string[regex_index]);
                    regex_index += 1;
                }
                sprintf(operations_buffer + strlen(operations_buffer), "{META_OP_END, 0}");

                prefix_length = found_macro - buffer;
                paren_end = strchr(quote_end, ')');
                
                if (paren_end != NULL) {
                    end_of_line = paren_end + 1;
                } else {
                    end_of_line = quote_end + 1;
                }

                original_string_length = (int32)(quote_end - quote_start) + 1;

                printf("%.*s{ .string = %.*s, .ops = { %s }, .has_start_anchor = %d, .has_end_anchor = %d }%s",
                       prefix_length, buffer, original_string_length, quote_start, operations_buffer, has_start, has_end, end_of_line);
                continue;
            }
        }
        printf("%s", buffer);
    }

    fclose(input_file);
    exit(EXIT_SUCCESS);
}
