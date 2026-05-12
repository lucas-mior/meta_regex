#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbase/util.c"
#include "meta_regex.h"

int
main(int argc, char **argv) {
    FILE *input_file;
    int64 file_size;
    char *buffer;
    char *cursor;
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

    fseek(input_file, 0, SEEK_END);
    file_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);

    buffer = malloc2(file_size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Error allocating memory.\n");
        exit(EXIT_FAILURE);
    }

    if (fread64(buffer, 1, file_size, input_file) != file_size) {
        fprintf(stderr, "Error reading file.\n");
        exit(EXIT_FAILURE);
    }
    buffer[file_size] = '\0';
    fclose(input_file);

    cursor = buffer;

    while (true) {
        char *found_macro;
        char *quote_start;
        char *quote_end;
        char *paren_end;
        char regex_string[256] = {0};
        char operations_buffer[2048] = {0};
        int32 prefix_length;
        int32 has_start = 0;
        int32 has_end = 0;
        int32 regex_index = 0;
        int32 original_string_length;

        found_macro = strstr(cursor, macro_start);
        if (found_macro == NULL) {
            break;
        }

        prefix_length = (int32)(found_macro - cursor);
        printf("%.*s", prefix_length, cursor);

        quote_start = strchr(found_macro, '"');
        if (quote_start == NULL) {
            printf("%s", macro_start);
            cursor = found_macro + strlen(macro_start);
            continue;
        }

        quote_end = quote_start + 1;
        while (*quote_end != '\0') {
            if (*quote_end == '\\') {
                if (*(quote_end + 1) != '\0') {
                    quote_end += 2;
                    continue;
                }
            }
            if (*quote_end == '"') {
                break;
            }
            quote_end += 1;
        }

        strncpy32(regex_string, quote_start + 1, quote_end - quote_start - 1);

        if (regex_string[regex_index] == '^') {
            has_start = 1;
            regex_index += 1;
        }

        while (regex_string[regex_index] != '\0') {
            if (regex_string[regex_index] == '$') {
                if (regex_string[regex_index + 1] == '\0') {
                    has_end = 1;
                    break;
                }
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

        paren_end = strchr(quote_end, ')');
        original_string_length = (int32)(quote_end - quote_start) + 1;

        printf("{ .string = %.*s, .ops = { %s }, .has_start_anchor = %d, .has_end_anchor = %d }",
               original_string_length, quote_start, operations_buffer, has_start, has_end);

        if (paren_end != NULL) {
            cursor = paren_end + 1;
        } else {
            cursor = quote_end + 1;
        }
    }

    printf("%s", cursor);
    free2(buffer, file_size + 1);
    exit(EXIT_SUCCESS);
}
