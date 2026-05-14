#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbase/util.c"
#include "meta_regex.h"

typedef struct ParsedOp {
    enum MetaOpType type;
    int32 value;
    int32 min;
    int32 max;
    unsigned int mask[8];
} ParsedOp;

int
main(int argc, char **argv) {
    FILE *input_file;
    int64 file_size;
    char *buffer;
    char *cursor;
    char *macro_start = "R(";

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
        char op_buffer[4096] = {0};
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
        int32 has_alternation = 0;
        ParsedOp temp_ops[1024] = {0};
        int32 temp_ops_count = 0;

        found_macro = strstr(cursor, macro_start);
        if (found_macro == NULL) {
            break;
        }

        prefix_length = (int32)(found_macro - cursor);
        printf("%.*s", prefix_length, cursor);

        if (found_macro[2] == 'N' && found_macro[3] == 'U' && 
            found_macro[4] == 'L' && found_macro[5] == 'L' && 
            found_macro[6] == ')') {
            printf("NULL");
            cursor = found_macro + 7;
            continue;
        }

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
                        switch (raw_string[i]) {
                        case 'n': regex_string[u_idx] = '\n'; break;
                        case 't': regex_string[u_idx] = '\t'; break;
                        case 'r': regex_string[u_idx] = '\r'; break;
                        case '\\': regex_string[u_idx] = '\\'; break;
                        case '"': regex_string[u_idx] = '"'; break;
                        default: regex_string[u_idx] = raw_string[i]; break;
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
            int32 cp = (unsigned char)regex_string[regex_index];

            switch (cp) {
            case '$': {
                if (regex_string[regex_index + 1] == '\0') {
                    has_end = 1;
                    regex_index += 1;
                    break;
                }
                temp_ops[temp_ops_count].type = META_OP_LITERAL;
                temp_ops[temp_ops_count].value = cp;
                temp_ops_count += 1;
                regex_index += 1;
                break;
            }
            case '*': {
                temp_ops[temp_ops_count].type = META_OP_STAR;
                temp_ops_count += 1;
                regex_index += 1;
                break;
            }
            case '+': {
                temp_ops[temp_ops_count].type = META_OP_PLUS;
                temp_ops_count += 1;
                regex_index += 1;
                break;
            }
            case '?': {
                temp_ops[temp_ops_count].type = META_OP_OPTIONAL;
                temp_ops_count += 1;
                regex_index += 1;
                break;
            }
            case '{': {
                int32 temp_idx = regex_index + 1;
                int32 m = 0;
                int32 n = -1;
                int32 has_m = 0;
                int32 valid = 0;
                while (regex_string[temp_idx] >= '0' && 
                       regex_string[temp_idx] <= '9') {
                    m = m * 10 + (regex_string[temp_idx] - '0');
                    has_m = 1;
                    temp_idx += 1;
                }
                if (regex_string[temp_idx] == ',') {
                    temp_idx += 1;
                    if (regex_string[temp_idx] >= '0' && 
                        regex_string[temp_idx] <= '9') {
                        n = 0;
                        while (regex_string[temp_idx] >= '0' && 
                               regex_string[temp_idx] <= '9') {
                            n = n * 10 + (regex_string[temp_idx] - '0');
                            temp_idx += 1;
                        }
                    }
                } else {
                    n = m;
                }
                if (regex_string[temp_idx] == '}' && has_m) {
                    valid = 1;
                    temp_idx += 1;
                }
                if (valid) {
                    int32 target_start = temp_ops_count - 1;
                    int32 is_group = 0;
                    if (temp_ops[target_start].type == META_OP_GROUP_END) {
                        int32 depth = 0;
                        is_group = 1;
                        for (int32 i = target_start; i >= 0; i -= 1) {
                            if (temp_ops[i].type == META_OP_GROUP_END) {
                                depth += 1;
                            } else if (temp_ops[i].type == META_OP_GROUP_START) {
                                depth -= 1;
                                if (depth == 0) {
                                    target_start = i;
                                    break;
                                }
                            }
                        }
                    }

                    if (is_group && m == n && m > 0) {
                        int32 target_len = temp_ops_count - target_start;
                        for (int32 k = 1; k < m; k += 1) {
                            for (int32 i = 0; i < target_len; i += 1) {
                                temp_ops[temp_ops_count] = temp_ops[target_start + i];
                                temp_ops_count += 1;
                            }
                        }
                    } else if (is_group && m == 0 && n == 0) {
                        temp_ops_count = target_start;
                    } else {
                        temp_ops[temp_ops_count].type = META_OP_BOUNDED;
                        temp_ops[temp_ops_count].min = m;
                        temp_ops[temp_ops_count].max = n;
                        temp_ops_count += 1;
                    }

                    regex_index = temp_idx;
                    break;
                }
                temp_ops[temp_ops_count].type = META_OP_LITERAL;
                temp_ops[temp_ops_count].value = cp;
                temp_ops_count += 1;
                regex_index += 1;
                break;
            }
            case '|': {
                temp_ops[temp_ops_count].type = META_OP_ALTERNATION;
                temp_ops_count += 1;
                has_alternation = 1;
                regex_index += 1;
                break;
            }
            case '(': {
                group_stack[group_stack_ptr] = group_counter;
                group_stack_ptr += 1;
                temp_ops[temp_ops_count].type = META_OP_GROUP_START;
                temp_ops[temp_ops_count].value = group_counter;
                temp_ops_count += 1;
                group_counter += 1;
                regex_index += 1;
                break;
            }
            case ')': {
                int32 current_group;
                group_stack_ptr -= 1;
                current_group = group_stack[group_stack_ptr];
                temp_ops[temp_ops_count].type = META_OP_GROUP_END;
                temp_ops[temp_ops_count].value = current_group;
                temp_ops_count += 1;
                regex_index += 1;
                break;
            }
            case '.': {
                temp_ops[temp_ops_count].type = META_OP_ANY;
                temp_ops_count += 1;
                regex_index += 1;
                break;
            }
            case '[': {
                int32 is_negated = 0;
                unsigned int mask[8] = {0};
                int32 first_char = 1;
                regex_index += 1;
                if (regex_string[regex_index] == '^') {
                    is_negated = 1;
                    regex_index += 1;
                }
                while (regex_string[regex_index] != '\0') {
                    if (!first_char && regex_string[regex_index] == ']') break;
                    if (regex_string[regex_index] == '[' && 
                        regex_string[regex_index + 1] == ':') {
                        int32 colon_idx = regex_index + 2;
                        int32 found_end = 0;
                        while (regex_string[colon_idx] != '\0') {
                            if (regex_string[colon_idx] == ':' && 
                                regex_string[colon_idx + 1] == ']') {
                                found_end = 1;
                                break;
                            }
                            colon_idx += 1;
                        }
                        if (found_end) {
                            char class_name[32] = {0};
                            int32 name_len = colon_idx - (regex_index + 2);
                            if (name_len < 32) {
                                strncpy32(class_name, 
                                          &regex_string[regex_index + 2], 
                                          name_len);
                                for (int32 c = 0; c < 256; c += 1) {
                                    int32 match = 0;
                                    if (strcmp(class_name, "alnum") == 0) {
                                        match = ((c >= 'a' && c <= 'z') || 
                                                 (c >= 'A' && c <= 'Z') || 
                                                 (c >= '0' && c <= '9'));
                                    } else if (strcmp(class_name, "alpha") == 0) {
                                        match = ((c >= 'a' && c <= 'z') || 
                                                 (c >= 'A' && c <= 'Z'));
                                    } else if (strcmp(class_name, "digit") == 0) {
                                        match = (c >= '0' && c <= '9');
                                    } else if (strcmp(class_name, "space") == 0) {
                                        match = (c == ' ' || c == '\t' || 
                                                 c == '\n' || c == '\r' || 
                                                 c == '\v' || c == '\f');
                                    } else if (strcmp(class_name, "lower") == 0) {
                                        match = (c >= 'a' && c <= 'z');
                                    } else if (strcmp(class_name, "upper") == 0) {
                                        match = (c >= 'A' && c <= 'Z');
                                    } else if (strcmp(class_name, "punct") == 0) {
                                        match = ((c >= 33 && c <= 47) || 
                                                 (c >= 58 && c <= 64) || 
                                                 (c >= 91 && c <= 96) || 
                                                 (c >= 123 && c <= 126));
                                    } else if (strcmp(class_name, "xdigit") == 0) {
                                        match = ((c >= '0' && c <= '9') || 
                                                 (c >= 'a' && c <= 'f') || 
                                                 (c >= 'A' && c <= 'F'));
                                    } else if (strcmp(class_name, "print") == 0) {
                                        match = (c >= 32 && c <= 126);
                                    } else if (strcmp(class_name, "graph") == 0) {
                                        match = (c >= 33 && c <= 126);
                                    } else if (strcmp(class_name, "blank") == 0) {
                                        match = (c == ' ' || c == '\t');
                                    } else if (strcmp(class_name, "cntrl") == 0) {
                                        match = ((c >= 0 && c <= 31) || 
                                                 (c == 127));
                                    }
                                    if (match) mask[c / 32] |= (1u << (c % 32));
                                }
                            }
                            regex_index = colon_idx + 2;
                            first_char = 0;
                            continue;
                        }
                    }
                    int32 c1 = (unsigned char)regex_string[regex_index];
                    int32 c2 = c1;
                    regex_index += 1;
                    if (regex_string[regex_index] == '-' && 
                        regex_string[regex_index + 1] != ']' && 
                        regex_string[regex_index + 1] != '\0') {
                        c2 = (unsigned char)regex_string[regex_index + 1];
                        regex_index += 2;
                    }
                    for (int32 c = c1; c <= c2; c += 1) {
                        mask[c / 32] |= (1u << (c % 32));
                    }
                    first_char = 0;
                }
                if (regex_string[regex_index] == ']') regex_index += 1;
                if (is_negated) {
                    for (int32 i = 0; i < 8; i += 1) {
                        mask[i] = ~mask[i];
                    }
                }
                temp_ops[temp_ops_count].type = META_OP_CLASS;
                for (int32 i = 0; i < 8; i += 1) {
                    temp_ops[temp_ops_count].mask[i] = mask[i];
                }
                temp_ops_count += 1;
                break;
            }
            case '\\': {
                regex_index += 1;
                if (regex_string[regex_index] != '\0') {
                    int32 c_cp = (unsigned char)regex_string[regex_index];
                    temp_ops[temp_ops_count].type = META_OP_LITERAL;
                    temp_ops[temp_ops_count].value = c_cp;
                    temp_ops_count += 1;
                    regex_index += 1;
                }
                break;
            }
            default: {
                temp_ops[temp_ops_count].type = META_OP_LITERAL;
                temp_ops[temp_ops_count].value = cp;
                temp_ops_count += 1;
                regex_index += 1;
                break;
            }
            }
        }

        for (int32 i = 0; i <= temp_ops_count; i += 1) {
            int32 w = 0;
            if (i == temp_ops_count) {
                w = snprintf2(op_ptr, space, "{META_OP_END, 0, 0, 0, {0}}\n");
            } else if (temp_ops[i].type == META_OP_LITERAL) {
                w = snprintf2(op_ptr, space, "{META_OP_LITERAL, %d, 0, 0, {0}},\n", temp_ops[i].value);
            } else if (temp_ops[i].type == META_OP_CLASS) {
                w = snprintf2(op_ptr, space, "{META_OP_CLASS, 0, 0, 0, {%u, %u, %u, %u, %u, %u, %u, %u}},\n", 
                              temp_ops[i].mask[0], temp_ops[i].mask[1], temp_ops[i].mask[2], temp_ops[i].mask[3], 
                              temp_ops[i].mask[4], temp_ops[i].mask[5], temp_ops[i].mask[6], temp_ops[i].mask[7]);
            } else if (temp_ops[i].type == META_OP_BOUNDED) {
                w = snprintf2(op_ptr, space, "{META_OP_BOUNDED, 0, %d, %d, {0}},\n", temp_ops[i].min, temp_ops[i].max);
            } else if (temp_ops[i].type == META_OP_GROUP_START) {
                w = snprintf2(op_ptr, space, "{META_OP_GROUP_START, %d, 0, 0, {0}},\n", temp_ops[i].value);
            } else if (temp_ops[i].type == META_OP_GROUP_END) {
                w = snprintf2(op_ptr, space, "{META_OP_GROUP_END, %d, 0, 0, {0}},\n", temp_ops[i].value);
            } else {
                char *type_str = "META_OP_UNKNOWN";
                if (temp_ops[i].type == META_OP_STAR) type_str = "META_OP_STAR";
                else if (temp_ops[i].type == META_OP_PLUS) type_str = "META_OP_PLUS";
                else if (temp_ops[i].type == META_OP_OPTIONAL) type_str = "META_OP_OPTIONAL";
                else if (temp_ops[i].type == META_OP_ALTERNATION) type_str = "META_OP_ALTERNATION";
                else if (temp_ops[i].type == META_OP_ANY) type_str = "META_OP_ANY";
                
                w = snprintf2(op_ptr, space, "{%s, 0, 0, 0, {0}},\n", type_str);
            }
            op_ptr += w;
            space -= w;
        }

        paren_end = strchr(quote_end, ')');
        original_string_length = (int32)(quote_end - quote_start) + 1;
        printf("&(MetaRegex){ .string = %.*s, .ops = { %s }, "
               ".has_start_anchor = %d, .has_end_anchor = %d, "
               ".has_alternation = %d }",
               original_string_length, quote_start, op_buffer, has_start, 
               has_end, has_alternation);
        cursor = (paren_end != NULL) ? paren_end + 1 : quote_end + 1;
    }
    printf("%s", cursor);
    free2(buffer, file_size + 1);
    exit(EXIT_SUCCESS);
}
