#include "primitives.h"
#include "meta.h"
#include "meta_preproc.h"

/* Source scanner and R(...) extraction entry point. */


static char *
preproc_find_macro_end(char *macro_start) {
    char *p = macro_start;
    int32 depth = 0;
    int32 in_string = 0;
    int32 in_char = 0;
    int32 in_line_comment = 0;
    int32 in_block_comment = 0;

    while (*p != '\0') {
        if (in_line_comment) {
            if (*p == '\n') {
                in_line_comment = 0;
            }
            p += 1;
            continue;
        }
        if (in_block_comment) {
            if (p[0] == '*' && p[1] == '/') {
                in_block_comment = 0;
                p += 2;
            } else {
                p += 1;
            }
            continue;
        }
        if (in_string) {
            if (p[0] == '\\' && p[1] != '\0') {
                p += 2;
            } else if (p[0] == '"') {
                in_string = 0;
                p += 1;
            } else {
                p += 1;
            }
            continue;
        }
        if (in_char) {
            if (p[0] == '\\' && p[1] != '\0') {
                p += 2;
            } else if (p[0] == '\'') {
                in_char = 0;
                p += 1;
            } else {
                p += 1;
            }
            continue;
        }

        if (p[0] == '/' && p[1] == '/') {
            in_line_comment = 1;
            p += 2;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            in_block_comment = 1;
            p += 2;
            continue;
        }
        if (*p == '"') {
            in_string = 1;
            p += 1;
            continue;
        }
        if (*p == '\'') {
            in_char = 1;
            p += 1;
            continue;
        }

        if (*p == '(') {
            depth += 1;
        } else if (*p == ')') {
            depth -= 1;
            if (depth == 0) {
                return p;
            }
        }
        p += 1;
    }

    return NULL;
}

static void
preproc_copy_trimmed_slice(char *dst, int32 dst_size, char *start, char *end) {
    int32 len;

    if (dst_size <= 0) {
        return;
    }

    while (start < end && (*start == ' ' || *start == '\t' || *start == '\n'
                           || *start == '\r')) {
        start += 1;
    }
    while (end > start && (end[-1] == ' ' || end[-1] == '\t'
                           || end[-1] == '\n' || end[-1] == '\r')) {
        end -= 1;
    }

    len = (int32)(end - start);
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    if (len > 0) {
        memcpy64(dst, start, len);
    }
    dst[len] = '\0';
    return;
}

static enum MetaRegexFlags
preproc_parse_regex_flags(char *flags_expr) {
    return META_RE_parse(flags_expr);
}

static RegexList
parse_source_code(char *buffer, int64 source_len) {
    RegexList list = {0};
    char *cursor = buffer;
    char *macro_start = "R(";
    (void)source_len;

    while (true) {
        char *found_macro = NULL;
        char *quote_start = NULL;
        char *quote_end = NULL;
        char *paren_end = NULL;
        char *flags_start = NULL;
        char *flags_end = NULL;
        char raw_string[PREPROC_MAX_STRING_LEN] = {0};
        char regex_string[PREPROC_MAX_STRING_LEN] = {0};
        char op_buffer[PREPROC_OP_BUFFER_SIZE] = {0};
        char flags_buffer[PREPROC_MAX_FLAGS_EXPR] = "0";
        char *op_ptr = op_buffer;
        int32 space = SIZEOF(op_buffer);
        bool has_start = false;
        bool has_end = false;
        int32 regex_index = 0;
        int32 original_string_length = 0;
        int32 group_counter = 0;
        int32 group_stack[PREPROC_MAX_GROUP_STACK] = {0};
        int32 group_stack_ptr = 0;
        bool has_alternation = false;
        ParsedOp temp_ops[PREPROC_MAX_TEMP_OPS] = {0};
        int32 temp_ops_count = 0;
        uint8 fastmap[META_FASTMAP_SIZE] = {0};
        bool can_be_null = false;
        enum MetaOpType used_ops = 0;
        enum MetaRegexFlags flags = META_RE_NONE;
        bool extract_submatches = false;

        {
            char *scan = cursor;
            bool in_line_comment = false;
            bool in_block_comment = false;
            bool in_string = false;
            bool in_char = false;

            while (*scan != '\0') {
                if (in_line_comment) {
                    if (*scan == '\n') {
                        in_line_comment = false;
                    }
                    scan += 1;
                    continue;
                }
                if (in_block_comment) {
                    if (scan[0] == '*' && scan[1] == '/') {
                        in_block_comment = false;
                        scan += 2;
                    } else {
                        scan += 1;
                    }
                    continue;
                }
                if (in_string) {
                    if (scan[0] == '\\' && scan[1] != '\0') {
                        scan += 2;
                    } else if (scan[0] == '"') {
                        in_string = false;
                        scan += 1;
                    } else {
                        scan += 1;
                    }
                    continue;
                }
                if (in_char) {
                    if (scan[0] == '\\' && scan[1] != '\0') {
                        scan += 2;
                    } else if (scan[0] == '\'') {
                        in_char = false;
                        scan += 1;
                    } else {
                        scan += 1;
                    }
                    continue;
                }
                if (scan[0] == '/' && scan[1] == '/') {
                    in_line_comment = true;
                    scan += 2;
                } else if (scan[0] == '/' && scan[1] == '*') {
                    in_block_comment = true;
                    scan += 2;
                } else if (scan[0] == '"') {
                    in_string = true;
                    scan += 1;
                } else if (scan[0] == '\'') {
                    in_char = true;
                    scan += 1;
                } else if (scan[0] == macro_start[0]
                           && scan[1] == macro_start[1]) {
                    found_macro = scan;
                    break;
                } else {
                    scan += 1;
                }
            }
        }

        if (found_macro == NULL) {
            break;
        }

        // Push new representation struct into list
        if (list.capacity == 0) {
            list.capacity = 16;
            list.items = malloc2(list.capacity*SIZEOF(*list.items));
        } else if (list.count >= list.capacity) {
            int64 old_capacity;
            old_capacity = list.capacity;
            list.capacity *= 2;
            list.items = realloc2(list.items, old_capacity, list.capacity,
                                  SIZEOF(*list.items));
        }
        ExtractedRegex *regex = &list.items[list.count];
        memset64(regex, 0, SIZEOF(*regex));

        regex->source_start_offset = found_macro - buffer;

        if (found_macro[2] == 'N' && found_macro[3] == 'U'
            && found_macro[4] == 'L' && found_macro[5] == 'L'
            && found_macro[6] == ')') {
            regex->is_null_macro = true;
            regex->source_end_offset = (found_macro + 7) - buffer;
            cursor = found_macro + 7;
            list.count++;
            continue;
        }

        quote_start = strchr(found_macro, '"');
        if (quote_start == NULL) {
            error("Error parsing regex: Quotes not found.\n");
            exit(EXIT_FAILURE);
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
                        case 'n':
                            regex_string[u_idx] = '\n';
                            break;
                        case 't':
                            regex_string[u_idx] = '\t';
                            break;
                        case 'r':
                            regex_string[u_idx] = '\r';
                            break;
                        case '\\':
                            regex_string[u_idx] = '\\';
                            break;
                        case '"':
                            regex_string[u_idx] = '"';
                            break;
                        default:
                            regex_string[u_idx] = raw_string[i];
                            break;
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
            has_start = true;
            regex_index += 1;
        }

        while (regex_string[regex_index] != '\0') {
            int32 cp = (uint8)regex_string[regex_index];

            switch (cp) {
            case '$': {
                if (regex_string[regex_index + 1] == '\0') {
                    has_end = true;
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
                bool is_group = (temp_ops_count > 0
                                 && temp_ops[temp_ops_count - 1].type
                                        == META_OP_GROUP_END);
                if (is_group) {
                    int32 target_start = temp_ops_count - 1;
                    int32 depth = 0;
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
                    int32 group_len = temp_ops_count - target_start;
                    for (int32 i = temp_ops_count - 1; i >= target_start;
                         i -= 1) {
                        temp_ops[i + 1] = temp_ops[i];
                    }
                    temp_ops_count += 1;

                    temp_ops[target_start].type = META_OP_SPLIT;
                    temp_ops[target_start].value = 1;
                    temp_ops[target_start].min = group_len + 2;

                    temp_ops[temp_ops_count].type = META_OP_JUMP;
                    temp_ops[temp_ops_count].value = -(group_len + 1);
                    temp_ops_count += 1;
                    regex_index += 1;
                    break;
                }
                temp_ops[temp_ops_count].type = META_OP_STAR;
                temp_ops_count += 1;
                regex_index += 1;
                break;
            }
            case '+': {
                bool is_group = (temp_ops_count > 0
                                 && temp_ops[temp_ops_count - 1].type
                                        == META_OP_GROUP_END);
                if (is_group) {
                    int32 target_start = temp_ops_count - 1;
                    int32 depth = 0;
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
                    int32 group_len = temp_ops_count - target_start;
                    temp_ops[temp_ops_count].type = META_OP_SPLIT;
                    temp_ops[temp_ops_count].value = -group_len;
                    temp_ops[temp_ops_count].min = 1;
                    temp_ops_count += 1;
                    regex_index += 1;
                    break;
                }
                temp_ops[temp_ops_count].type = META_OP_PLUS;
                temp_ops_count += 1;
                regex_index += 1;
                break;
            }
            case '?': {
                bool is_group = (temp_ops_count > 0
                                 && temp_ops[temp_ops_count - 1].type
                                        == META_OP_GROUP_END);
                if (is_group) {
                    int32 target_start = temp_ops_count - 1;
                    int32 depth = 0;
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
                    int32 group_len = temp_ops_count - target_start;
                    for (int32 i = temp_ops_count - 1; i >= target_start;
                         i -= 1) {
                        temp_ops[i + 1] = temp_ops[i];
                    }
                    temp_ops_count += 1;

                    temp_ops[target_start].type = META_OP_SPLIT;
                    temp_ops[target_start].value = 1;
                    temp_ops[target_start].min = group_len + 1;
                    regex_index += 1;
                    break;
                }
                temp_ops[temp_ops_count].type = META_OP_OPTIONAL;
                temp_ops_count += 1;
                regex_index += 1;
                break;
            }
            case '{': {
                int32 temp_idx = regex_index + 1;
                int32 m = 0;
                int32 n = -1;
                bool has_m = false;
                bool valid = false;
                while (regex_string[temp_idx] >= '0'
                       && regex_string[temp_idx] <= '9') {
                    m = m*10 + (regex_string[temp_idx] - '0');
                    has_m = true;
                    temp_idx += 1;
                }
                if (regex_string[temp_idx] == ',') {
                    temp_idx += 1;
                    if (regex_string[temp_idx] >= '0'
                        && regex_string[temp_idx] <= '9') {
                        n = 0;
                        while (regex_string[temp_idx] >= '0'
                               && regex_string[temp_idx] <= '9') {
                            n = n*10 + (regex_string[temp_idx] - '0');
                            temp_idx += 1;
                        }
                    }
                } else {
                    n = m;
                }
                if (regex_string[temp_idx] == '}' && has_m) {
                    valid = true;
                    temp_idx += 1;
                }

                if (valid && temp_ops_count > 0) {
                    bool is_group = (temp_ops[temp_ops_count - 1].type
                                     == META_OP_GROUP_END);
                    if (is_group) {
                        int32 target_start = temp_ops_count - 1;
                        int32 depth = 0;
                        for (int32 i = target_start; i >= 0; i -= 1) {
                            if (temp_ops[i].type == META_OP_GROUP_END) {
                                depth += 1;
                            } else if (temp_ops[i].type
                                       == META_OP_GROUP_START) {
                                depth -= 1;
                                if (depth == 0) {
                                    target_start = i;
                                    break;
                                }
                            }
                        }
                        int32 group_len = temp_ops_count - target_start;
                        ParsedOp group_buf[PREPROC_MAX_STRING_LEN];

                        for (int32 i = 0; i < group_len; i += 1) {
                            group_buf[i] = temp_ops[target_start + i];
                        }
                        temp_ops_count = target_start;
                        for (int32 k = 0; k < m; k += 1) {
                            for (int32 i = 0; i < group_len; i += 1) {
                                temp_ops[temp_ops_count] = group_buf[i];
                                temp_ops_count += 1;
                            }
                        }
                        if (n == -1) {
                            temp_ops[temp_ops_count].type = META_OP_SPLIT;
                            temp_ops[temp_ops_count].value = 1;
                            temp_ops[temp_ops_count].min = group_len + 2;
                            temp_ops_count += 1;
                            for (int32 i = 0; i < group_len; i += 1) {
                                temp_ops[temp_ops_count] = group_buf[i];
                                temp_ops_count += 1;
                            }
                            temp_ops[temp_ops_count].type = META_OP_JUMP;
                            temp_ops[temp_ops_count].value = -(group_len + 1);
                            temp_ops_count += 1;
                        } else {
                            for (int32 k = m; k < n; k += 1) {
                                temp_ops[temp_ops_count].type = META_OP_SPLIT;
                                temp_ops[temp_ops_count].value = 1;
                                temp_ops[temp_ops_count].min = group_len + 1;
                                temp_ops_count += 1;
                                for (int32 i = 0; i < group_len; i += 1) {
                                    temp_ops[temp_ops_count] = group_buf[i];
                                    temp_ops_count += 1;
                                }
                            }
                        }
                        regex_index = temp_idx;
                        break;
                    } else {
                        int32 target_start = temp_ops_count - 1;
                        ParsedOp op_to_repeat = temp_ops[target_start];

                        if (temp_ops_count + m + (n == -1 ? 2 : (n - m)*2)
                            >= PREPROC_MAX_TEMP_OPS) {
                            fprintf(stderr, "Error: Quantifier unrolling "
                                            "exceeds max ops.\n");
                            exit(EXIT_FAILURE);
                        }
                        temp_ops_count = target_start;
                        for (int32 k = 0; k < m; k += 1) {
                            temp_ops[temp_ops_count] = op_to_repeat;
                            temp_ops_count += 1;
                        }
                        if (n == -1) {
                            temp_ops[temp_ops_count] = op_to_repeat;
                            temp_ops_count += 1;
                            temp_ops[temp_ops_count].type = META_OP_STAR;
                            temp_ops_count += 1;
                        } else {
                            for (int32 k = m; k < n; k += 1) {
                                temp_ops[temp_ops_count] = op_to_repeat;
                                temp_ops_count += 1;
                                temp_ops[temp_ops_count].type
                                    = META_OP_OPTIONAL;
                                temp_ops_count += 1;
                            }
                        }
                        regex_index = temp_idx;
                        break;
                    }
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
                has_alternation = true;
                regex_index += 1;
                break;
            }
            case '(': {
                group_counter += 1;
                group_stack[group_stack_ptr] = group_counter;
                group_stack_ptr += 1;
                temp_ops[temp_ops_count].type = META_OP_GROUP_START;
                temp_ops[temp_ops_count].value = group_counter;
                temp_ops_count += 1;
                regex_index += 1;
                break;
            }
            case ')': {
                int32 current_group = group_stack[group_stack_ptr - 1];
                group_stack_ptr -= 1;
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
                bool is_negated = false;
                uint32 mask[META_CHAR_BITMASK_WORDS] = {0};
                bool first_char = true;
                regex_index += 1;
                if (regex_string[regex_index] == '^') {
                    is_negated = true;
                    regex_index += 1;
                }
                while (regex_string[regex_index] != '\0') {
                    if (!first_char && regex_string[regex_index] == ']') {
                        break;
                    }
                    if (regex_string[regex_index] == '['
                        && regex_string[regex_index + 1] == ':') {
                        int32 colon_idx = regex_index + 2;
                        bool found_end = false;
                        while (regex_string[colon_idx] != '\0') {
                            if (regex_string[colon_idx] == ':'
                                && regex_string[colon_idx + 1] == ']') {
                                found_end = true;
                                break;
                            }
                            colon_idx += 1;
                        }
                        if (found_end) {
                            char class_name[PREPROC_MAX_CLASS_NAME] = {0};
                            int32 name_len = colon_idx - (regex_index + 2);
                            if (name_len < PREPROC_MAX_CLASS_NAME) {
                                strncpy32(class_name,
                                          &regex_string[regex_index + 2],
                                          name_len);
                                populate_posix_class_mask(class_name, mask);
                            }
                            regex_index = colon_idx + 2;
                            first_char = false;
                            continue;
                        }
                    }
                    int32 c1 = (uint8)regex_string[regex_index];
                    if (c1 >= 128) {
                        fprintf(stderr,
                                "Error: Non-ASCII character inside bracket "
                                "expression is not supported.\n");
                        exit(EXIT_FAILURE);
                    }
                    int32 c2 = c1;
                    regex_index += 1;
                    if (regex_string[regex_index] == '-'
                        && regex_string[regex_index + 1] != ']'
                        && regex_string[regex_index + 1] != '\0') {
                        c2 = (uint8)regex_string[regex_index + 1];
                        if (c2 >= 128) {
                            fprintf(stderr,
                                    "Error: Non-ASCII character inside bracket "
                                    "expression is not supported.\n");
                            exit(EXIT_FAILURE);
                        }
                        regex_index += 2;
                    }
                    for (int32 c = c1; c <= c2; c += 1) {
                        mask[c / 32] |= (1u << (c % 32));
                    }
                    first_char = false;
                }
                if (regex_string[regex_index] == ']') {
                    regex_index += 1;
                }
                if (is_negated) {
                    for (int32 i = 0; i < META_CHAR_BITMASK_WORDS; i += 1) {
                        mask[i] = ~mask[i];
                    }
                }
                temp_ops[temp_ops_count].type = META_OP_CLASS;
                for (int32 i = 0; i < META_CHAR_BITMASK_WORDS; i += 1) {
                    temp_ops[temp_ops_count].mask[i] = mask[i];
                }
                temp_ops_count += 1;
                break;
            }
            case '\\': {
                regex_index += 1;
                if (regex_string[regex_index] != '\0') {
                    int32 c_cp = (uint8)regex_string[regex_index];
                    if (c_cp == 's' || c_cp == 'S') {
                        temp_ops[temp_ops_count].type = META_OP_CLASS;
                        for (int32 i = 0; i < META_CHAR_BITMASK_WORDS; i += 1) {
                            temp_ops[temp_ops_count].mask[i] = 0;
                        }
                        temp_ops[temp_ops_count].mask[' ' / 32]
                            |= (1u << (' ' % 32));
                        temp_ops[temp_ops_count].mask['\t' / 32]
                            |= (1u << ('\t' % 32));
                        temp_ops[temp_ops_count].mask['\n' / 32]
                            |= (1u << ('\n' % 32));
                        temp_ops[temp_ops_count].mask['\r' / 32]
                            |= (1u << ('\r' % 32));
                        temp_ops[temp_ops_count].mask['\f' / 32]
                            |= (1u << ('\f' % 32));
                        temp_ops[temp_ops_count].mask['\v' / 32]
                            |= (1u << ('\v' % 32));

                        if (c_cp == 'S') {
                            for (int32 i = 0; i < META_CHAR_BITMASK_WORDS;
                                 i += 1) {
                                temp_ops[temp_ops_count].mask[i]
                                    = ~temp_ops[temp_ops_count].mask[i];
                            }
                        }
                        temp_ops_count += 1;
                    } else if (c_cp >= '1' && c_cp <= '9') {
                        temp_ops[temp_ops_count].type = META_OP_BACKREF;
                        temp_ops[temp_ops_count].value = c_cp - '0';
                        temp_ops_count += 1;
                    } else if (c_cp == '<') {
                        temp_ops[temp_ops_count].type = META_OP_WORD_START;
                        temp_ops_count += 1;
                    } else if (c_cp == '>') {
                        temp_ops[temp_ops_count].type = META_OP_WORD_END;
                        temp_ops_count += 1;
                    } else if (c_cp == 'b') {
                        temp_ops[temp_ops_count].type = META_OP_WORD_BOUNDARY;
                        temp_ops_count += 1;
                    } else if (c_cp == 'B') {
                        temp_ops[temp_ops_count].type
                            = META_OP_NON_WORD_BOUNDARY;
                        temp_ops_count += 1;
                    } else {
                        temp_ops[temp_ops_count].type = META_OP_LITERAL;
                        temp_ops[temp_ops_count].value = c_cp;
                        temp_ops_count += 1;
                    }
                }
                regex_index += 1;
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

        if (has_alternation) {
            sort_alternations(temp_ops, temp_ops_count);
        }

        for (int32 i = 0; i < temp_ops_count; i += 1) {
            used_ops |= (uint32)temp_ops[i].type;
        }
        used_ops |= (uint32)META_OP_END;

        {
            uint8 visited[PREPROC_MAX_TEMP_OPS];
            int32 depth = 0;
            int32 scan = 0;

            for (int32 i = 0; i < PREPROC_MAX_TEMP_OPS; i += 1) {
                visited[i] = 0;
            }

            can_be_null = compute_first_set(temp_ops, 0, temp_ops_count,
                                            fastmap, visited);

            if (has_alternation) {
                while (scan < temp_ops_count) {
                    if (temp_ops[scan].type == META_OP_GROUP_START) {
                        depth += 1;
                    } else if (temp_ops[scan].type == META_OP_GROUP_END) {
                        depth -= 1;
                    } else if (temp_ops[scan].type == META_OP_ALTERNATION
                               && depth == 0) {
                        for (int32 i = 0; i < PREPROC_MAX_TEMP_OPS; i += 1) {
                            visited[i] = 0;
                        }
                        if (compute_first_set(temp_ops, scan + 1,
                                              temp_ops_count, fastmap,
                                              visited)) {
                            can_be_null = true;
                        }
                    }
                    scan += 1;
                }
            }
        }

        for (int32 i = 0; i <= temp_ops_count; i += 1) {
            int32 w = 0;
            if (i == temp_ops_count) {
                w = snprintf2(op_ptr, space, "{META_OP_END, 0, 0, 0, {0}}\n");
            } else if (temp_ops[i].type == META_OP_LITERAL) {
                w = snprintf2(op_ptr, space,
                              "{META_OP_LITERAL, %d, 0, 0, {0}},\n",
                              temp_ops[i].value);
            } else if (temp_ops[i].type == META_OP_CLASS) {
                w = snprintf2(op_ptr, space,
                              "{META_OP_CLASS, 0, 0, 0, {%u, %u, %u, %u, %u, "
                              "%u, %u, %u}},\n",
                              temp_ops[i].mask[0], temp_ops[i].mask[1],
                              temp_ops[i].mask[2], temp_ops[i].mask[3],
                              temp_ops[i].mask[4], temp_ops[i].mask[5],
                              temp_ops[i].mask[6], temp_ops[i].mask[7]);
            } else if (temp_ops[i].type == META_OP_BOUNDED) {
                w = snprintf2(op_ptr, space,
                              "{META_OP_BOUNDED, 0, %d, %d, {0}},\n",
                              temp_ops[i].min, temp_ops[i].max);
            } else if (temp_ops[i].type == META_OP_GROUP_START) {
                w = snprintf2(op_ptr, space,
                              "{META_OP_GROUP_START, %d, 0, 0, {0}},\n",
                              temp_ops[i].value);
            } else if (temp_ops[i].type == META_OP_GROUP_END) {
                w = snprintf2(op_ptr, space,
                              "{META_OP_GROUP_END, %d, 0, 0, {0}},\n",
                              temp_ops[i].value);
            } else if (temp_ops[i].type == META_OP_SPLIT) {
                w = snprintf2(op_ptr, space,
                              "{META_OP_SPLIT, %d, %d, 0, {0}},\n",
                              temp_ops[i].value, temp_ops[i].min);
            } else if (temp_ops[i].type == META_OP_JUMP) {
                w = snprintf2(op_ptr, space, "{META_OP_JUMP, %d, 0, 0, {0}},\n",
                              temp_ops[i].value);
            } else if (temp_ops[i].type == META_OP_WORD_START) {
                w = snprintf2(op_ptr, space,
                              "{META_OP_WORD_START, 0, 0, 0, {0}},\n");
            } else if (temp_ops[i].type == META_OP_WORD_END) {
                w = snprintf2(op_ptr, space,
                              "{META_OP_WORD_END, 0, 0, 0, {0}},\n");
            } else if (temp_ops[i].type == META_OP_WORD_BOUNDARY) {
                w = snprintf2(op_ptr, space,
                              "{META_OP_WORD_BOUNDARY, 0, 0, 0, {0}},\n");
            } else if (temp_ops[i].type == META_OP_NON_WORD_BOUNDARY) {
                w = snprintf2(op_ptr, space,
                              "{META_OP_NON_WORD_BOUNDARY, 0, 0, 0, {0}},\n");
            } else if (temp_ops[i].type == META_OP_BACKREF) {
                w = snprintf2(op_ptr, space,
                              "{META_OP_BACKREF, %d, 0, 0, {0}},\n",
                              temp_ops[i].value);
            } else {
                char *type_str = "META_OP_UNKNOWN";
                if (temp_ops[i].type == META_OP_STAR) {
                    type_str = "META_OP_STAR";
                } else if (temp_ops[i].type == META_OP_PLUS) {
                    type_str = "META_OP_PLUS";
                } else if (temp_ops[i].type == META_OP_OPTIONAL) {
                    type_str = "META_OP_OPTIONAL";
                } else if (temp_ops[i].type == META_OP_ALTERNATION) {
                    type_str = "META_OP_ALTERNATION";
                } else if (temp_ops[i].type == META_OP_ANY) {
                    type_str = "META_OP_ANY";
                }
                w = snprintf2(op_ptr, space, "{%s, 0, 0, 0, {0}},\n", type_str);
            }
            op_ptr += w;
            space -= w;
        }

        paren_end = preproc_find_macro_end(found_macro);
        if (paren_end == NULL) {
            error("Error parsing regex: closing ')' not found.\n");
            exit(EXIT_FAILURE);
        }

        flags_start = quote_end + 1;
        while (flags_start < paren_end
               && (*flags_start == ' ' || *flags_start == '\t'
                   || *flags_start == '\n' || *flags_start == '\r')) {
            flags_start += 1;
        }
        if (flags_start < paren_end && *flags_start == ',') {
            flags_start += 1;
            flags_end = paren_end;
            preproc_copy_trimmed_slice(flags_buffer, SIZEOF(flags_buffer),
                                       flags_start, flags_end);
            if (flags_buffer[0] == '\0') {
                strcpy(flags_buffer, "0");
            }
        }

        flags = preproc_parse_regex_flags(flags_buffer);
        if ((flags & META_RE_EXTRACT)
            && (flags & META_RE_NOSUB)) {
            error("R() flags cannot request both extract and no-submatch mode: %s\n",
                  flags_buffer);
            exit(EXIT_FAILURE);
        }

        extract_submatches = preproc_config.default_extract_submatches;
        if (flags & META_RE_EXTRACT) {
            extract_submatches = true;
        }
        if (flags & META_RE_NOSUB) {
            extract_submatches = false;
        }

        original_string_length = (int32)(quote_end - quote_start) + 1;

        if (extract_submatches && (used_ops & META_OP_BACKREF) == 0
            && (preproc_config.emit_tnfa || preproc_config.emit_tdfa)) {
            regex->tnfa = malloc2(SIZEOF(*regex->tnfa));
            if (!build_tnfa_from_ops(regex->tnfa, temp_ops, temp_ops_count,
                                     group_counter)) {
                fprintf(stderr, "Warning: TNFA construction failed for %.*s.\n",
                        original_string_length, quote_start);
                regex->tnfa = NULL;
            }

            if (regex->tnfa != NULL && preproc_config.emit_tdfa) {
                regex->tdfa = malloc2(SIZEOF(*regex->tdfa));
                if (!build_tdfa_from_tnfa(regex->tdfa, regex->tnfa)) {
                    fprintf(stderr,
                            "Warning: TDFA construction failed for %.*s.\n",
                            original_string_length, quote_start);
                    regex->tdfa = NULL;
                }
            }
        }

        // Save data to the extracted AST/IR structure
        regex->quote_start_offset = quote_start - buffer;
        regex->original_string_length = original_string_length;
        regex->has_start = has_start;
        regex->has_end = has_end;
        regex->group_counter = group_counter;
        regex->can_be_null = can_be_null;
        regex->flags = flags;
        regex->extract_submatches = extract_submatches;
        strncpy32(regex->flags_buffer, flags_buffer, PREPROC_MAX_FLAGS_EXPR);
        regex->used_ops = used_ops;
        memcpy64(regex->fastmap, fastmap, META_FASTMAP_SIZE);
        memcpy64(regex->temp_ops, temp_ops,
                 temp_ops_count*SIZEOF(*regex->temp_ops));
        regex->temp_ops_count = temp_ops_count;
        strncpy32(regex->op_buffer, op_buffer, PREPROC_OP_BUFFER_SIZE);

        if (paren_end) {
            regex->source_end_offset = (paren_end + 1) - buffer;
            cursor = paren_end + 1;
        } else {
            regex->source_end_offset = (quote_end + 1) - buffer;
            cursor = quote_end + 1;
        }

        list.count++;
    }

    return list;
}
