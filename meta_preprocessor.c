#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbase/util.c"
#include "meta.h"

#define PREPROC_OP_BUFFER_SIZE 16384
#define PREPROC_MAX_STRING_LEN 256
#define PREPROC_MAX_GROUP_STACK 32
#define PREPROC_MAX_TEMP_OPS 1024
#define PREPROC_MAX_CLASS_NAME 32
#define PREPROC_MAX_NFA_STATES 2048
#define PREPROC_MAX_NFA_ITEMS 1024
#define PREPROC_MAX_BRANCHES 128
#define PREPROC_NFA_BITSET_WORDS 64

#define BITS_PER_UINT8 8
#define BITS_PER_UINT32 32

enum NfaStateType {
    NFA_STATE_ACCEPT = 0,
    NFA_STATE_LITERAL = 1,
    NFA_STATE_CLASS = 2,
    NFA_STATE_ANY = 3,
    NFA_STATE_SPLIT = 4,
    NFA_STATE_EMPTY = 5
};

typedef struct ParsedOp {
    enum MetaOpType type;
    int32 value;
    int32 min;
    int32 max;
    uint32 mask[META_CHAR_BITMASK_WORDS];
} ParsedOp;

typedef struct NfaState {
    int32 type;
    int32 c;
    uint32 mask[META_CHAR_BITMASK_WORDS];
    int32 next1;
    int32 next2;
} NfaState;

typedef struct NfaItem {
    ParsedOp base_op;
    int32 quant;
    int32 min;
    int32 max;
} NfaItem;

typedef struct DfaSet {
    uint32 bits[PREPROC_NFA_BITSET_WORDS];
} DfaSet;

static void
set_fastmap_bit(uchar *fastmap, int32 c) {
    if (c >= 0 && c < META_ALPHABET_SIZE) {
        fastmap[c / BITS_PER_UINT8] |= (1 << (c % BITS_PER_UINT8));
    }
    return;
}

static int32
get_branch_weight(ParsedOp *ops, int32 count) {
    int32 weight = 0;

    for (int32 i = 0; i < count; i += 1) {
        if (ops[i].type == META_OP_LITERAL || ops[i].type == META_OP_CLASS
            || ops[i].type == META_OP_ANY || ops[i].type == META_OP_BACKREF) {
            weight += 1;
        }
    }
    return weight;
}

static void
sort_alternations(ParsedOp *ops, int32 count) {
    int32 i = 0;
    int32 branch_starts[PREPROC_MAX_BRANCHES];
    int32 branch_ends[PREPROC_MAX_BRANCHES];
    int32 branch_keys[PREPROC_MAX_BRANCHES];
    int32 num_branches = 0;
    int32 current_start = 0;
    int32 depth = 0;

    while (i < count) {
        if (ops[i].type == META_OP_GROUP_START) {
            int32 depth_inner = 0;
            int32 end = i + 1;

            while (end < count) {
                if (ops[end].type == META_OP_GROUP_START) {
                    depth_inner += 1;
                } else if (ops[end].type == META_OP_GROUP_END) {
                    if (depth_inner == 0) {
                        break;
                    }
                    depth_inner -= 1;
                }
                end += 1;
            }
            if (end < count) {
                sort_alternations(ops + i + 1, end - (i + 1));
            }
            i = end + 1;
        } else {
            i += 1;
        }
    }

    for (int32 j = 0; j < count; j += 1) {
        if (ops[j].type == META_OP_GROUP_START) {
            depth += 1;
        } else if (ops[j].type == META_OP_GROUP_END) {
            depth -= 1;
        } else if (ops[j].type == META_OP_ALTERNATION && depth == 0) {
            if (num_branches >= PREPROC_MAX_BRANCHES - 1) {
                return;
            }
            branch_starts[num_branches] = current_start;
            branch_ends[num_branches] = j;

            int32 key = 98;
            for (int32 k = current_start; k < j; k += 1) {
                if (ops[k].type == META_OP_LITERAL) {
                    key = ops[k].value;
                    break;
                }
            }
            branch_keys[num_branches] = key;
            num_branches += 1;
            current_start = j + 1;
        }
    }

    branch_starts[num_branches] = current_start;
    branch_ends[num_branches] = count;

    int32 final_key = 98;
    for (int32 k = current_start; k < count; k += 1) {
        if (ops[k].type == META_OP_LITERAL) {
            final_key = ops[k].value;
            break;
        }
    }
    branch_keys[num_branches] = final_key;
    num_branches += 1;

    if (num_branches > 1) {
        ParsedOp temp_buf[PREPROC_MAX_TEMP_OPS];
        int32 sorted_indices[PREPROC_MAX_BRANCHES];
        int32 write_idx = 0;

        for (int32 b = 0; b < num_branches; b += 1) {
            sorted_indices[b] = b;
        }

        for (int32 b1 = 1; b1 < num_branches; b1 += 1) {
            int32 idx_item = sorted_indices[b1];
            int32 key_item = branch_keys[idx_item];
            int32 b2 = b1 - 1;

            while (b2 >= 0 && branch_keys[sorted_indices[b2]] > key_item) {
                sorted_indices[b2 + 1] = sorted_indices[b2];
                b2 -= 1;
            }
            sorted_indices[b2 + 1] = idx_item;
        }

        for (int32 b = 0; b < num_branches; b += 1) {
            int32 idx = sorted_indices[b];
            int32 b_start = branch_starts[idx];
            int32 b_end = branch_ends[idx];

            for (int32 j = b_start; j < b_end; j += 1) {
                temp_buf[write_idx] = ops[j];
                write_idx += 1;
            }
            if (b < num_branches - 1) {
                temp_buf[write_idx].type = META_OP_ALTERNATION;
                write_idx += 1;
            }
        }
        for (int32 j = 0; j < count; j += 1) {
            ops[j] = temp_buf[j];
        }
    }
    return;
}

static int32
compute_first_set(ParsedOp *ops, int32 pc, int32 temp_ops_count, uchar *fastmap,
                  uint8 *visited) {
    enum MetaOpType type = 0;
    int32 is_null = 0;

    if (pc >= temp_ops_count) {
        return 1;
    }
    if (pc < 0) {
        return 0;
    }
    if (visited[pc]) {
        return 0;
    }
    visited[pc] = 1;

    type = ops[pc].type;

    if (type == META_OP_LITERAL) {
        set_fastmap_bit(fastmap, ops[pc].value);
        if (pc + 1 < temp_ops_count) {
            if (ops[pc + 1].type == META_OP_STAR
                || ops[pc + 1].type == META_OP_OPTIONAL
                || (ops[pc + 1].type == META_OP_BOUNDED
                    && ops[pc + 1].min == 0)) {
                return compute_first_set(ops, pc + 2, temp_ops_count, fastmap,
                                         visited);
            }
        }
        return 0;
    } else if (type == META_OP_CLASS) {
        for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
            if ((ops[pc].mask[c / BITS_PER_UINT32]
                 & (1u << (c % BITS_PER_UINT32)))
                != 0) {
                set_fastmap_bit(fastmap, c);
            }
        }
        if (pc + 1 < temp_ops_count) {
            if (ops[pc + 1].type == META_OP_STAR
                || ops[pc + 1].type == META_OP_OPTIONAL
                || (ops[pc + 1].type == META_OP_BOUNDED
                    && ops[pc + 1].min == 0)) {
                return compute_first_set(ops, pc + 2, temp_ops_count, fastmap,
                                         visited);
            }
        }
        return 0;
    } else if (type == META_OP_ANY) {
        for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
            set_fastmap_bit(fastmap, c);
        }
        if (pc + 1 < temp_ops_count) {
            if (ops[pc + 1].type == META_OP_STAR
                || ops[pc + 1].type == META_OP_OPTIONAL
                || (ops[pc + 1].type == META_OP_BOUNDED
                    && ops[pc + 1].min == 0)) {
                return compute_first_set(ops, pc + 2, temp_ops_count, fastmap,
                                         visited);
            }
        }
        return 0;
    } else if (type == META_OP_SPLIT) {
        int32 p1 = compute_first_set(ops, pc + ops[pc].value, temp_ops_count, fastmap,
                               visited);
        int32 p2 = compute_first_set(ops, pc + ops[pc].min, temp_ops_count, fastmap,
                               visited);
        return (p1 || p2);
    } else if (type == META_OP_JUMP) {
        return compute_first_set(ops, pc + ops[pc].value, temp_ops_count,
                                 fastmap, visited);
    } else if (type == META_OP_ALTERNATION) {
        int32 depth = 0;
        int32 target = pc;

        while (target < temp_ops_count) {
            if (ops[target].type == META_OP_GROUP_START) {
                depth += 1;
            } else if (ops[target].type == META_OP_GROUP_END) {
                if (depth == 0) {
                    break;
                }
                depth -= 1;
            }
            target += 1;
        }
        return compute_first_set(ops, target, temp_ops_count, fastmap, visited);
    } else if (type == META_OP_GROUP_START) {
        int32 depth = 0;
        int32 scan = pc + 1;

        is_null
            = compute_first_set(ops, pc + 1, temp_ops_count, fastmap, visited);
        while (scan < temp_ops_count) {
            if (ops[scan].type == META_OP_GROUP_START) {
                depth += 1;
            } else if (ops[scan].type == META_OP_GROUP_END) {
                if (depth == 0) {
                    break;
                }
                depth -= 1;
            } else if (ops[scan].type == META_OP_ALTERNATION && depth == 0) {
                if (compute_first_set(ops, scan + 1, temp_ops_count, fastmap,
                                      visited)) {
                    is_null = 1;
                }
            }
            scan += 1;
        }
        return is_null;
    } else if (type == META_OP_GROUP_END || type == META_OP_WORD_START
               || type == META_OP_WORD_END || type == META_OP_WORD_BOUNDARY
               || type == META_OP_NON_WORD_BOUNDARY
               || type == META_OP_BACKREF) {
        return compute_first_set(ops, pc + 1, temp_ops_count, fastmap, visited);
    } else if (type == META_OP_STAR || type == META_OP_PLUS
               || type == META_OP_OPTIONAL || type == META_OP_BOUNDED) {
        return compute_first_set(ops, pc + 1, temp_ops_count, fastmap, visited);
    }

    return 0;
}

int32
main(int32 argc, char **argv) {
    FILE *input_file = NULL;
    int64 file_size = 0;
    char *buffer = NULL;
    char *cursor = NULL;
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
        char *found_macro = NULL;
        char *quote_start = NULL;
        char *quote_end = NULL;
        char *paren_end = NULL;
        char raw_string[PREPROC_MAX_STRING_LEN] = {0};
        char regex_string[PREPROC_MAX_STRING_LEN] = {0};
        char op_buffer[PREPROC_OP_BUFFER_SIZE] = {0};
        char *op_ptr = op_buffer;
        int32 space = SIZEOF(op_buffer);
        int32 prefix_length = 0;
        int32 has_start = 0;
        int32 has_end = 0;
        int32 regex_index = 0;
        int32 original_string_length = 0;
        int32 group_counter = 0;
        int32 group_stack[PREPROC_MAX_GROUP_STACK] = {0};
        int32 group_stack_ptr = 0;
        int32 has_alternation = 0;
        int32 has_backref = 0;
        ParsedOp temp_ops[PREPROC_MAX_TEMP_OPS] = {0};
        int32 temp_ops_count = 0;
        uchar fastmap[META_FASTMAP_SIZE] = {0};
        int32 can_be_null = 0;

        {
            char *scan = cursor;
            int32 in_line_comment = 0;
            int32 in_block_comment = 0;
            int32 in_string = 0;
            int32 in_char = 0;

            while (*scan != '\0') {
                if (in_line_comment) {
                    if (*scan == '\n') {
                        in_line_comment = 0;
                    }
                    scan += 1;
                } else if (in_block_comment) {
                    if (scan[0] == '*' && scan[1] == '/') {
                        in_block_comment = 0;
                        scan += 2;
                    } else {
                        scan += 1;
                    }
                } else if (in_string) {
                    if (scan[0] == '\\' && scan[1] != '\0') {
                        scan += 2;
                    } else if (scan[0] == '"') {
                        in_string = 0;
                        scan += 1;
                    } else {
                        scan += 1;
                    }
                } else if (in_char) {
                    if (scan[0] == '\\' && scan[1] != '\0') {
                        scan += 2;
                    } else if (scan[0] == '\'') {
                        in_char = 0;
                        scan += 1;
                    } else {
                        scan += 1;
                    }
                } else {
                    if (scan[0] == '/' && scan[1] == '/') {
                        in_line_comment = 1;
                        scan += 2;
                    } else if (scan[0] == '/' && scan[1] == '*') {
                        in_block_comment = 1;
                        scan += 2;
                    } else if (scan[0] == '"') {
                        in_string = 1;
                        scan += 1;
                    } else if (scan[0] == '\'') {
                        in_char = 1;
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
        }

        if (found_macro == NULL) {
            break;
        }

        prefix_length = (int32)(found_macro - cursor);
        printf("%.*s", prefix_length, cursor);

        if (found_macro[2] == 'N' && found_macro[3] == 'U'
            && found_macro[4] == 'L' && found_macro[5] == 'L'
            && found_macro[6] == ')') {
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
            has_start = 1;
            regex_index += 1;
        }

        while (regex_string[regex_index] != '\0') {
            int32 cp = (uchar)regex_string[regex_index];

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
                int32 is_group = (temp_ops_count > 0
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
                int32 is_group = (temp_ops_count > 0
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
                int32 is_group = (temp_ops_count > 0
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
                int32 has_m = 0;
                int32 valid = 0;
                while (regex_string[temp_idx] >= '0'
                       && regex_string[temp_idx] <= '9') {
                    m = m*10 + (regex_string[temp_idx] - '0');
                    has_m = 1;
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
                    valid = 1;
                    temp_idx += 1;
                }

                if (valid) {
                    int32 is_group = (temp_ops_count > 0
                                      && temp_ops[temp_ops_count - 1].type
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
                    } else if (temp_ops_count > 0) {
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
                    } else {
                        valid = 0;
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
                has_alternation = 1;
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
                int32 is_negated = 0;
                uint32 mask[META_CHAR_BITMASK_WORDS] = {0};
                int32 first_char = 1;
                regex_index += 1;
                if (regex_string[regex_index] == '^') {
                    is_negated = 1;
                    regex_index += 1;
                }
                while (regex_string[regex_index] != '\0') {
                    if (!first_char && regex_string[regex_index] == ']') {
                        break;
                    }
                    if (regex_string[regex_index] == '['
                        && regex_string[regex_index + 1] == ':') {
                        int32 colon_idx = regex_index + 2;
                        int32 found_end = 0;
                        while (regex_string[colon_idx] != '\0') {
                            if (regex_string[colon_idx] == ':'
                                && regex_string[colon_idx + 1] == ']') {
                                found_end = 1;
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
                                for (int32 c = 0; c < META_ALPHABET_SIZE;
                                     c += 1) {
                                    int32 match = 0;
                                    if (strcmp(class_name, "alnum") == 0) {
                                        match = ((c >= 'a' && c <= 'z')
                                                 || (c >= 'A' && c <= 'Z')
                                                 || (c >= '0' && c <= '9'));
                                    } else if (strcmp(class_name, "alpha")
                                               == 0) {
                                        match = ((c >= 'a' && c <= 'z')
                                                 || (c >= 'A' && c <= 'Z'));
                                    } else if (strcmp(class_name, "digit")
                                               == 0) {
                                        match = (c >= '0' && c <= '9');
                                    } else if (strcmp(class_name, "space")
                                               == 0) {
                                        match = (c == ' ' || c == '\t'
                                                 || c == '\n' || c == '\r'
                                                 || c == '\v' || c == '\f');
                                    } else if (strcmp(class_name, "lower")
                                               == 0) {
                                        match = (c >= 'a' && c <= 'z');
                                    } else if (strcmp(class_name, "upper")
                                               == 0) {
                                        match = (c >= 'A' && c <= 'Z');
                                    } else if (strcmp(class_name, "punct")
                                               == 0) {
                                        match = ((c >= 33 && c <= 47)
                                                 || (c >= 58 && c <= 64)
                                                 || (c >= 91 && c <= 96)
                                                 || (c >= 123 && c <= 126));
                                    } else if (strcmp(class_name, "xdigit")
                                               == 0) {
                                        match = ((c >= '0' && c <= '9')
                                                 || (c >= 'a' && c <= 'f')
                                                 || (c >= 'A' && c <= 'F'));
                                    } else if (strcmp(class_name, "print")
                                               == 0) {
                                        match = (c >= 32 && c <= 126);
                                    } else if (strcmp(class_name, "graph")
                                               == 0) {
                                        match = (c >= 33 && c <= 126);
                                    } else if (strcmp(class_name, "blank")
                                               == 0) {
                                        match = (c == ' ' || c == '\t');
                                    } else if (strcmp(class_name, "cntrl")
                                               == 0) {
                                        match = ((c >= 0 && c <= 31)
                                                 || (c == 127));
                                    }
                                    if (match) {
                                        mask[c / BITS_PER_UINT32]
                                            |= (1u << (c % BITS_PER_UINT32));
                                    }
                                }
                            }
                            regex_index = colon_idx + 2;
                            first_char = 0;
                            continue;
                        }
                    }
                    int32 c1 = (uchar)regex_string[regex_index];
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
                        c2 = (uchar)regex_string[regex_index + 1];
                        if (c2 >= 128) {
                            fprintf(stderr,
                                    "Error: Non-ASCII character inside "
                                    "bracket expression is not supported.\n");
                            exit(EXIT_FAILURE);
                        }
                        regex_index += 2;
                    }
                    for (int32 c = c1; c <= c2; c += 1) {
                        mask[c / BITS_PER_UINT32]
                            |= (1u << (c % BITS_PER_UINT32));
                    }
                    first_char = 0;
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
                    int32 c_cp = (uchar)regex_string[regex_index];
                    if (c_cp == 's' || c_cp == 'S') {
                        temp_ops[temp_ops_count].type = META_OP_CLASS;
                        for (int32 i = 0; i < META_CHAR_BITMASK_WORDS; i += 1) {
                            temp_ops[temp_ops_count].mask[i] = 0;
                        }
                        temp_ops[temp_ops_count].mask[' ' / BITS_PER_UINT32]
                            |= (1u << (' ' % BITS_PER_UINT32));
                        temp_ops[temp_ops_count].mask['\t' / BITS_PER_UINT32]
                            |= (1u << ('\t' % BITS_PER_UINT32));
                        temp_ops[temp_ops_count].mask['\n' / BITS_PER_UINT32]
                            |= (1u << ('\n' % BITS_PER_UINT32));
                        temp_ops[temp_ops_count].mask['\r' / BITS_PER_UINT32]
                            |= (1u << ('\r' % BITS_PER_UINT32));
                        temp_ops[temp_ops_count].mask['\f' / BITS_PER_UINT32]
                            |= (1u << ('\f' % BITS_PER_UINT32));
                        temp_ops[temp_ops_count].mask['\v' / BITS_PER_UINT32]
                            |= (1u << ('\v' % BITS_PER_UINT32));

                        if (c_cp == 'S') {
                            for (int32 i = 0; i < META_CHAR_BITMASK_WORDS;
                                 i += 1) {
                                temp_ops[temp_ops_count].mask[i]
                                    = ~temp_ops[temp_ops_count].mask[i];
                            }
                        }
                        temp_ops_count += 1;
                    } else if (c_cp >= '1' && c_cp <= '9') {
                        has_backref = 1;
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
                            can_be_null = 1;
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

        paren_end = strchr(quote_end, ')');
        original_string_length = (int32)(quote_end - quote_start) + 1;
        printf("&(MetaRegex){ .string = %.*s, .ops = { %s }, "
               ".has_start_anchor = %d, .has_end_anchor = %d, "
               ".has_alternation = %d, .re_nsub = %d, .has_backref = %d, "
               ".can_be_null = %d, .fastmap = {",
               original_string_length, quote_start, op_buffer, has_start,
               has_end, has_alternation, group_counter, has_backref,
               can_be_null);

        for (int32 i = 0; i < META_FASTMAP_SIZE; i += 1) {
            printf("0x%02x%s", fastmap[i],
                   (i == META_FASTMAP_SIZE - 1 ? "" : ", "));
        }
        printf("}");

        {
            int32 unsupported = 0;
            if (group_counter > 0) {
                unsupported = 1;
            }
            if (has_backref > 0) {
                unsupported = 1;
            }

            for (int32 i = 0; i < temp_ops_count; i += 1) {
                if (temp_ops[i].type == META_OP_WORD_BOUNDARY) {
                    unsupported = 1;
                    break;
                }
                if (temp_ops[i].type == META_OP_WORD_START) {
                    unsupported = 1;
                    break;
                }
                if (temp_ops[i].type == META_OP_WORD_END) {
                    unsupported = 1;
                    break;
                }
                if (temp_ops[i].type == META_OP_NON_WORD_BOUNDARY) {
                    unsupported = 1;
                    break;
                }
                if (temp_ops[i].type == META_OP_BACKREF) {
                    unsupported = 1;
                    break;
                }
            }

            if (unsupported) {
                fprintf(stderr,
                        "Warning: Unsupported regex feature in %.*s, "
                        "falling back to NFA.\n",
                        original_string_length, quote_start);
                printf(", .dfa = NULL }");
            } else {
                NfaState nfa[PREPROC_MAX_NFA_STATES];
                int32 nfa_count = 0;
                NfaItem items[PREPROC_MAX_NFA_ITEMS];
                int32 item_count = 0;
                int32 nfa_failed = 0;
                int32 nfa_accept = 0;
                int32 branch_starts[PREPROC_MAX_BRANCHES];
                int32 branch_count = 0;
                int32 b_start = -1;
                int32 prev_dangling = -1;
                int32 nfa_start_state = -1;
                int32 dfa_transitions[META_MAX_DFA_STATES][META_ALPHABET_SIZE];
                int32 dfa_accept[META_MAX_DFA_STATES];
                DfaSet dfa_sets[META_MAX_DFA_STATES];
                int32 dfa_count = 1;
                int32 start_dfa = 0;

                for (int32 i = 0; i < temp_ops_count; i += 1) {
                    if (temp_ops[i].type == META_OP_ALTERNATION) {
                        items[item_count].base_op.type = META_OP_ALTERNATION;
                        items[item_count].quant = 0;
                        item_count += 1;
                    } else if (temp_ops[i].type == META_OP_LITERAL
                               || temp_ops[i].type == META_OP_CLASS
                               || temp_ops[i].type == META_OP_ANY) {
                        items[item_count].base_op = temp_ops[i];
                        items[item_count].quant = 0;
                        if (i + 1 < temp_ops_count) {
                            enum MetaOpType qt = temp_ops[i + 1].type;
                            if (qt == META_OP_STAR) {
                                items[item_count].quant = 1;
                                i += 1;
                            } else if (qt == META_OP_PLUS) {
                                items[item_count].quant = 2;
                                i += 1;
                            } else if (qt == META_OP_OPTIONAL) {
                                items[item_count].quant = 3;
                                i += 1;
                            } else if (qt == META_OP_BOUNDED) {
                                items[item_count].quant = 4;
                                items[item_count].min = temp_ops[i + 1].min;
                                items[item_count].max = temp_ops[i + 1].max;
                                i += 1;
                            }
                        }
                        item_count += 1;
                    }
                }

                nfa_accept = nfa_count;
                nfa_count += 1;
                nfa[nfa_accept].type = NFA_STATE_ACCEPT;
                nfa[nfa_accept].next1 = -1;
                nfa[nfa_accept].next2 = -1;

                for (int32 i = 0; i <= item_count; i += 1) {
                    if (nfa_failed) {
                        break;
                    }
                    if (i == item_count
                        || items[i].base_op.type == META_OP_ALTERNATION) {
                        if (b_start == -1) {
                            b_start = nfa_count;
                            nfa_count += 1;
                            nfa[b_start].type = NFA_STATE_EMPTY;
                            nfa[b_start].next1 = -1;
                            nfa[b_start].next2 = -1;
                        }

                        if (prev_dangling != -1) {
                            if (nfa[prev_dangling].type == NFA_STATE_SPLIT) {
                                nfa[prev_dangling].next2 = nfa_accept;
                            } else {
                                nfa[prev_dangling].next1 = nfa_accept;
                            }
                        } else {
                            nfa[b_start].next1 = nfa_accept;
                        }

                        if (branch_count < PREPROC_MAX_BRANCHES) {
                            branch_starts[branch_count] = b_start;
                            branch_count += 1;
                        } else {
                            nfa_failed = 1;
                        }

                        b_start = -1;
                        prev_dangling = -1;
                        continue;
                    }

                    int32 s_base = nfa_count;
                    nfa_count += 1;
                    if (nfa_count > PREPROC_MAX_NFA_STATES) {
                        nfa_failed = 1;
                        break;
                    }

                    if (items[i].base_op.type == META_OP_ANY) {
                        nfa[s_base].type = NFA_STATE_ANY;
                    } else if (items[i].base_op.type == META_OP_CLASS) {
                        nfa[s_base].type = NFA_STATE_CLASS;
                    } else {
                        nfa[s_base].type = NFA_STATE_LITERAL;
                    }

                    nfa[s_base].c = items[i].base_op.value;
                    nfa[s_base].next1 = -1;
                    nfa[s_base].next2 = -1;
                    for (int32 k = 0; k < META_CHAR_BITMASK_WORDS; k += 1) {
                        nfa[s_base].mask[k] = items[i].base_op.mask[k];
                    }

                    int32 i_start = -1;
                    int32 i_out = -1;

                    if (items[i].quant == 0) {
                        i_start = s_base;
                        i_out = s_base;
                    } else if (items[i].quant == 1) {
                        int32 s_split = nfa_count;
                        nfa_count += 1;
                        if (nfa_count > PREPROC_MAX_NFA_STATES) {
                            nfa_failed = 1;
                            break;
                        }
                        nfa[s_split].type = NFA_STATE_SPLIT;
                        nfa[s_split].next1 = s_base;
                        nfa[s_split].next2 = -1;
                        nfa[s_base].next1 = s_split;
                        i_start = s_split;
                        i_out = s_split;
                    } else if (items[i].quant == 2) {
                        int32 s_split = nfa_count;
                        nfa_count += 1;
                        if (nfa_count > PREPROC_MAX_NFA_STATES) {
                            nfa_failed = 1;
                            break;
                        }
                        nfa[s_split].type = NFA_STATE_SPLIT;
                        nfa[s_split].next1 = s_base;
                        nfa[s_split].next2 = -1;
                        nfa[s_base].next1 = s_split;
                        i_start = s_base;
                        i_out = s_split;
                    } else if (items[i].quant == 3) {
                        int32 s_split = nfa_count;
                        nfa_count += 1;
                        int32 s_merge = nfa_count;
                        nfa_count += 1;
                        if (nfa_count > PREPROC_MAX_NFA_STATES) {
                            nfa_failed = 1;
                            break;
                        }
                        nfa[s_split].type = NFA_STATE_SPLIT;
                        nfa[s_merge].type = NFA_STATE_EMPTY;
                        nfa[s_split].next1 = s_base;
                        nfa[s_split].next2 = s_merge;
                        nfa[s_base].next1 = s_merge;
                        nfa[s_merge].next1 = -1;
                        nfa[s_merge].next2 = -1;
                        i_start = s_split;
                        i_out = s_merge;
                    } else if (items[i].quant == 4) {
                        int32 f_start = -1;
                        int32 l_out = -1;
                        for (int32 k = 0; k < items[i].min; k += 1) {
                            int32 copy = nfa_count;
                            nfa_count += 1;
                            if (nfa_count > PREPROC_MAX_NFA_STATES) {
                                nfa_failed = 1;
                                break;
                            }
                            nfa[copy] = nfa[s_base];
                            nfa[copy].next1 = -1;
                            nfa[copy].next2 = -1;
                            if (f_start == -1) {
                                f_start = copy;
                            }
                            if (l_out != -1) {
                                if (nfa[l_out].type == NFA_STATE_SPLIT) {
                                    nfa[l_out].next2 = copy;
                                } else {
                                    nfa[l_out].next1 = copy;
                                }
                            }
                            l_out = copy;
                        }
                        if (items[i].max == -1) {
                            int32 s_star = nfa_count;
                            nfa_count += 1;
                            int32 copy = nfa_count;
                            nfa_count += 1;
                            if (nfa_count > PREPROC_MAX_NFA_STATES) {
                                nfa_failed = 1;
                                break;
                            }
                            nfa[copy] = nfa[s_base];
                            nfa[copy].next1 = -1;
                            nfa[copy].next2 = -1;
                            nfa[s_star].type = NFA_STATE_SPLIT;
                            nfa[s_star].next1 = copy;
                            nfa[s_star].next2 = -1;
                            nfa[copy].next1 = s_star;
                            if (f_start == -1) {
                                f_start = s_star;
                            }
                            if (l_out != -1) {
                                if (nfa[l_out].type == NFA_STATE_SPLIT) {
                                    nfa[l_out].next2 = s_star;
                                } else {
                                    nfa[l_out].next1 = s_star;
                                }
                            }
                            l_out = s_star;
                        } else {
                            for (int32 k = items[i].min; k < items[i].max;
                                 k += 1) {
                                int32 s_split = nfa_count;
                                nfa_count += 1;
                                int32 copy = nfa_count;
                                nfa_count += 1;
                                int32 s_merge = nfa_count;
                                nfa_count += 1;
                                if (nfa_count > PREPROC_MAX_NFA_STATES) {
                                    nfa_failed = 1;
                                    break;
                                }
                                nfa[copy] = nfa[s_base];
                                nfa[copy].next1 = -1;
                                nfa[copy].next2 = -1;
                                nfa[s_split].type = NFA_STATE_SPLIT;
                                nfa[s_merge].type = NFA_STATE_EMPTY;
                                nfa[s_merge].next1 = -1;
                                nfa[s_merge].next2 = -1;
                                nfa[s_split].next1 = copy;
                                nfa[s_split].next2 = s_merge;
                                nfa[copy].next1 = s_merge;
                                if (f_start == -1) {
                                    f_start = s_split;
                                }
                                if (l_out != -1) {
                                    if (nfa[l_out].type == NFA_STATE_SPLIT) {
                                        nfa[l_out].next2 = s_split;
                                    } else {
                                        nfa[l_out].next1 = s_split;
                                    }
                                }
                                l_out = s_merge;
                            }
                        }
                        if (f_start == -1) {
                            f_start = nfa_count;
                            nfa_count += 1;
                            if (nfa_count > PREPROC_MAX_NFA_STATES) {
                                nfa_failed = 1;
                                break;
                            }
                            nfa[f_start].type = NFA_STATE_EMPTY;
                            nfa[f_start].next1 = -1;
                            nfa[f_start].next2 = -1;
                            l_out = f_start;
                        }
                        i_start = f_start;
                        i_out = l_out;
                    }

                    if (b_start == -1) {
                        b_start = i_start;
                    }
                    if (prev_dangling != -1) {
                        if (nfa[prev_dangling].type == NFA_STATE_SPLIT) {
                            nfa[prev_dangling].next2 = i_start;
                        } else {
                            nfa[prev_dangling].next1 = i_start;
                        }
                    }
                    prev_dangling = i_out;
                }

                if (!nfa_failed && branch_count > 0) {
                    nfa_start_state = branch_starts[0];
                    for (int32 i = 1; i < branch_count; i += 1) {
                        int32 s = nfa_count;
                        nfa_count += 1;
                        if (nfa_count > PREPROC_MAX_NFA_STATES) {
                            nfa_failed = 1;
                            break;
                        }
                        nfa[s].type = NFA_STATE_SPLIT;
                        nfa[s].next1 = branch_starts[i];
                        nfa[s].next2 = nfa_start_state;
                        nfa_start_state = s;
                    }
                } else {
                    nfa_failed = 1;
                }

                if (!nfa_failed) {
                    dfa_accept[0] = 0;
                    for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                        dfa_transitions[0][c] = 0;
                    }

                    DfaSet start_set = {0};
                    for (int32 i = 0; i < PREPROC_NFA_BITSET_WORDS; i += 1) {
                        start_set.bits[i] = 0;
                    }
                    start_set.bits[nfa_start_state / BITS_PER_UINT32]
                        |= (1u << (nfa_start_state % BITS_PER_UINT32));

                    int32 changed = 1;
                    while (changed) {
                        changed = 0;
                        for (int32 i = 0; i < nfa_count; i += 1) {
                            if ((start_set.bits[i / BITS_PER_UINT32]
                                 & (1u << (i % BITS_PER_UINT32)))) {
                                if (nfa[i].type == NFA_STATE_SPLIT
                                    || nfa[i].type == NFA_STATE_EMPTY) {
                                    if (nfa[i].next1 != -1
                                        && !(start_set.bits[nfa[i].next1
                                                             / BITS_PER_UINT32]
                                             & (1u << (nfa[i].next1
                                                       % BITS_PER_UINT32)))) {
                                        start_set.bits[nfa[i].next1
                                                       / BITS_PER_UINT32]
                                            |= (1u << (nfa[i].next1
                                                       % BITS_PER_UINT32));
                                        changed = 1;
                                    }
                                    if (nfa[i].type == NFA_STATE_SPLIT
                                        && nfa[i].next2 != -1
                                        && !(start_set.bits[nfa[i].next2
                                                             / BITS_PER_UINT32]
                                             & (1u << (nfa[i].next2
                                                       % BITS_PER_UINT32)))) {
                                        start_set.bits[nfa[i].next2
                                                       / BITS_PER_UINT32]
                                            |= (1u << (nfa[i].next2
                                                       % BITS_PER_UINT32));
                                        changed = 1;
                                    }
                                }
                            }
                        }
                    }

                    int32 match_id = -1;
                    for (int32 i = 1; i < dfa_count; i += 1) {
                        int32 match = 1;
                        for (int32 k = 0; k < PREPROC_NFA_BITSET_WORDS;
                             k += 1) {
                            if (dfa_sets[i].bits[k] != start_set.bits[k]) {
                                match = 0;
                                break;
                            }
                        }
                        if (match) {
                            match_id = i;
                            break;
                        }
                    }
                    if (match_id != -1) {
                        start_dfa = match_id;
                    } else if (dfa_count < META_MAX_DFA_STATES) {
                        dfa_sets[dfa_count] = start_set;
                        dfa_accept[dfa_count]
                            = (start_set.bits[nfa_accept / BITS_PER_UINT32]
                               & (1u << (nfa_accept % BITS_PER_UINT32)))
                              != 0;
                        for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                            dfa_transitions[dfa_count][c] = 0;
                        }
                        start_dfa = dfa_count;
                        dfa_count += 1;
                    } else {
                        nfa_failed = 1;
                    }

                    if (!nfa_failed) {
                        for (int32 d = 1; d < dfa_count; d += 1) {
                            if (nfa_failed) {
                                break;
                            }
                            for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                                DfaSet next_set = {0};
                                for (int32 i = 0; i < PREPROC_NFA_BITSET_WORDS;
                                     i += 1) {
                                    next_set.bits[i] = 0;
                                }
                                int32 has_next = 0;
                                for (int32 i = 0; i < nfa_count; i += 1) {
                                    if ((dfa_sets[d].bits[i / BITS_PER_UINT32]
                                         & (1u << (i % BITS_PER_UINT32)))) {
                                        if (nfa[i].type == NFA_STATE_LITERAL
                                            && nfa[i].c == c) {
                                            next_set.bits[nfa[i].next1
                                                          / BITS_PER_UINT32]
                                                |= (1u << (nfa[i].next1
                                                           % BITS_PER_UINT32));
                                            has_next = 1;
                                        } else if (
                                            nfa[i].type == NFA_STATE_CLASS
                                            && (nfa[i].mask[c / BITS_PER_UINT32]
                                                & (1u
                                                   << (c % BITS_PER_UINT32)))) {
                                            next_set.bits[nfa[i].next1
                                                          / BITS_PER_UINT32]
                                                |= (1u << (nfa[i].next1
                                                           % BITS_PER_UINT32));
                                            has_next = 1;
                                        } else if (nfa[i].type
                                                   == NFA_STATE_ANY) {
                                            next_set.bits[nfa[i].next1
                                                          / BITS_PER_UINT32]
                                                |= (1u << (nfa[i].next1
                                                           % BITS_PER_UINT32));
                                            has_next = 1;
                                        }
                                    }
                                }
                                if (has_next) {
                                    changed = 1;
                                    while (changed) {
                                        changed = 0;
                                        for (int32 i = 0; i < nfa_count;
                                             i += 1) {
                                            if ((next_set
                                                     .bits[i / BITS_PER_UINT32]
                                                 & (1u
                                                    << (i
                                                        % BITS_PER_UINT32)))) {
                                                if (nfa[i].type
                                                        == NFA_STATE_SPLIT
                                                    || nfa[i].type
                                                           == NFA_STATE_EMPTY) {
                                                    if (nfa[i].next1 != -1
                                                        && !(
                                                            next_set.bits
                                                                [nfa[i].next1
                                                                 / BITS_PER_UINT32]
                                                            & (1u
                                                               << (nfa[i].next1
                                                                   % BITS_PER_UINT32)))) {
                                                        next_set.bits
                                                            [nfa[i].next1
                                                             / BITS_PER_UINT32]
                                                            |= (1u
                                                                << (nfa[i].next1
                                                                    % BITS_PER_UINT32));
                                                        changed = 1;
                                                    }
                                                    if (nfa[i].type
                                                            == NFA_STATE_SPLIT
                                                        && nfa[i].next2 != -1
                                                        && !(
                                                            next_set.bits
                                                                [nfa[i].next2
                                                                 / BITS_PER_UINT32]
                                                            & (1u
                                                               << (nfa[i].next2
                                                                   % BITS_PER_UINT32)))) {
                                                        next_set.bits
                                                            [nfa[i].next2
                                                             / BITS_PER_UINT32]
                                                            |= (1u
                                                                << (nfa[i].next2
                                                                    % BITS_PER_UINT32));
                                                        changed = 1;
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    int32 match_id2 = -1;
                                    for (int32 i = 1; i < dfa_count; i += 1) {
                                        int32 match = 1;
                                        for (int32 k = 0;
                                             k < PREPROC_NFA_BITSET_WORDS;
                                             k += 1) {
                                            if (dfa_sets[i].bits[k]
                                                != next_set.bits[k]) {
                                                match = 0;
                                                break;
                                            }
                                        }
                                        if (match) {
                                            match_id2 = i;
                                            break;
                                        }
                                    }

                                    if (match_id2 != -1) {
                                        dfa_transitions[d][c] = match_id2;
                                    } else if (dfa_count
                                               < META_MAX_DFA_STATES) {
                                        dfa_sets[dfa_count] = next_set;
                                        dfa_accept[dfa_count]
                                            = (next_set.bits[nfa_accept
                                                             / BITS_PER_UINT32]
                                               & (1u << (nfa_accept
                                                         % BITS_PER_UINT32)))
                                              != 0;
                                        for (int32 k = 0;
                                             k < META_ALPHABET_SIZE; k += 1) {
                                            dfa_transitions[dfa_count][k] = 0;
                                        }
                                        dfa_transitions[d][c] = dfa_count;
                                        dfa_count += 1;
                                    } else {
                                        nfa_failed = 1;
                                    }
                                }
                            }
                        }
                    }
                }

                if (nfa_failed) {
                    fprintf(stderr,
                            "Warning: DFA conversion failed for %.*s, "
                            "falling back to NFA.\n",
                            original_string_length, quote_start);
                    printf(", .dfa = NULL } }");
                } else {
                    printf(", .dfa = &(Dfa){ .num_states = %d, "
                           ".start_state = %d, .states = {\n",
                           dfa_count, start_dfa);
                    for (int32 i = 0; i < dfa_count; i += 1) {
                        int32 has_transitions = 0;

                        printf("{ .is_accepting = %d, .next = {",
                               dfa_accept[i]);
                        for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                            if (dfa_transitions[i][c] != 0) {
                                has_transitions = 1;
                                break;
                            }
                        }
                        if (has_transitions) {
                            for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                                if (dfa_transitions[i][c] != 0) {
                                    printf("[%d]=%d,", c,
                                           dfa_transitions[i][c]);
                                }
                            }
                        } else {
                            printf("0");
                        }
                        printf("} },\n");
                    }
                    printf("} } }");
                }
            }
        }

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
