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
        char raw_string[256] = {0};
        char regex_string[256] = {0};
        char op_buffer[2048] = {0};
        char *op_ptr = op_buffer;
        int32 space = SIZEOF(op_buffer);
        int32 prefix_length;
        int32 has_start = 0;
        int32 has_end = 0;
        int32 regex_index = 0;
        int32 original_string_length;
        int32 group_counter = 1;
        int32 group_stack[32] = {0};
        int32 group_stack_ptr = 0;

        found_macro = strstr(cursor, macro_start);
        if (found_macro == NULL) {
            break;
        }

        prefix_length = (int32)(found_macro - cursor);
        printf("%.*s", prefix_length, cursor);

        quote_start = strchr(found_macro, '"');
        if (quote_start == NULL) {
            printf("%s", macro_start);
            cursor = found_macro + strlen32(macro_start);
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

        strncpy32(raw_string, quote_start + 1, quote_end - quote_start - 1);

        {
            int32 u_idx = 0;
            for (int32 i = 0; raw_string[i] != '\0'; i += 1) {
                if (raw_string[i] == '\\') {
                    if (raw_string[i + 1] != '\0') {
                        i += 1;
                        if (raw_string[i] == 'n') {
                            regex_string[u_idx] = '\n';
                        } else if (raw_string[i] == 't') {
                            regex_string[u_idx] = '\t';
                        } else if (raw_string[i] == 'r') {
                            regex_string[u_idx] = '\r';
                        } else if (raw_string[i] == '\\') {
                            regex_string[u_idx] = '\\';
                        } else if (raw_string[i] == '"') {
                            regex_string[u_idx] = '"';
                        } else {
                            regex_string[u_idx] = raw_string[i];
                        }
                        u_idx += 1;
                    }
                } else {
                    regex_string[u_idx] = raw_string[i];
                    u_idx += 1;
                }
            }
        }

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
            if (regex_string[regex_index] == '*') {
                int32 w = snprintf2(op_ptr, space, "{META_OP_STAR, 0}, ");
                op_ptr += w;
                space -= w;
                regex_index += 1;
                continue;
            }
            if (regex_string[regex_index] == '+') {
                int32 w = snprintf2(op_ptr, space, "{META_OP_PLUS, 0}, ");
                op_ptr += w;
                space -= w;
                regex_index += 1;
                continue;
            }
            if (regex_string[regex_index] == '?') {
                int32 w = snprintf2(op_ptr, space, "{META_OP_OPTIONAL, 0}, ");
                op_ptr += w;
                space -= w;
                regex_index += 1;
                continue;
            }
            if (regex_string[regex_index] == '|') {
                int32 w = snprintf2(op_ptr, space, "{META_OP_ALTERNATION, 0}, ");
                op_ptr += w;
                space -= w;
                regex_index += 1;
                continue;
            }
            if (regex_string[regex_index] == '(') {
                int32 w;

                group_stack[group_stack_ptr] = group_counter;
                group_stack_ptr += 1;
                w = snprintf2(op_ptr, space, "{META_OP_GROUP_START, %d}, ", group_counter);
                op_ptr += w;
                space -= w;
                group_counter += 1;
                regex_index += 1;
                continue;
            }
            if (regex_string[regex_index] == ')') {
                int32 w;
                int32 current_group;

                group_stack_ptr -= 1;
                current_group = group_stack[group_stack_ptr];
                w = snprintf2(op_ptr, space, "{META_OP_GROUP_END, %d}, ", current_group);
                op_ptr += w;
                space -= w;
                regex_index += 1;
                continue;
            }
            if (regex_string[regex_index] == '.') {
                int32 w = snprintf2(op_ptr, space, "{META_OP_ANY, 0}, ");
                op_ptr += w;
                space -= w;
                regex_index += 1;
                continue;
            }
            if (regex_string[regex_index] == '[') {
                if (strncmp(&regex_string[regex_index], "[0-9]", 5) == 0) {
                    int32 w = snprintf2(op_ptr, space, "{META_OP_DIGIT, 0}, ");
                    op_ptr += w;
                    space -= w;
                    regex_index += 5;
                    continue;
                }
                if (strncmp(&regex_string[regex_index], "[a-z]", 5) == 0) {
                    int32 w = snprintf2(op_ptr, space, "{META_OP_ALPHA_LOWER, 0}, ");
                    op_ptr += w;
                    space -= w;
                    regex_index += 5;
                    continue;
                }
                if (strncmp(&regex_string[regex_index], "[A-Z]", 5) == 0) {
                    int32 w = snprintf2(op_ptr, space, "{META_OP_ALPHA_UPPER, 0}, ");
                    op_ptr += w;
                    space -= w;
                    regex_index += 5;
                    continue;
                }
                {
                    int32 w = snprintf2(op_ptr, space, "{META_OP_LITERAL, %d}, ", regex_string[regex_index]);
                    op_ptr += w;
                    space -= w;
                    regex_index += 1;
                }
                continue;
            }
            if (regex_string[regex_index] == '\\') {
                regex_index += 1;
                if (regex_string[regex_index] != '\0') {
                    int32 w = snprintf2(op_ptr, space, "{META_OP_LITERAL, %d}, ", regex_string[regex_index]);
                    op_ptr += w;
                    space -= w;
                    regex_index += 1;
                }
                continue;
            }
            {
                int32 w = snprintf2(op_ptr, space, "{META_OP_LITERAL, %d}, ", regex_string[regex_index]);
                op_ptr += w;
                space -= w;
                regex_index += 1;
            }
        }
        
        {
            int32 w = snprintf2(op_ptr, space, "{META_OP_END, 0}");
            op_ptr += w;
            space -= w;
        }

        paren_end = strchr(quote_end, ')');
        original_string_length = (int32)(quote_end - quote_start) + 1;

        printf("{ .string = %.*s, .ops = { %s }, .has_start_anchor = %d, .has_end_anchor = %d }",
               original_string_length, quote_start, op_buffer, has_start, has_end);

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
