/* Regex parsing helpers: POSIX classes, alternation normalization,
 * fastmap/first-set helpers. */

static void
set_fastmap_bit(uint8 *fastmap, int32 c) {
    if (c >= 0 && c < META_ALPHABET_SIZE) {
        fastmap[c / 8] |= (1 << (c % 8));
    }
    return;
}

static void
populate_posix_class_mask(char *class_name, uint32 *mask) {
    for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
        bool match = false;
        if (strcmp(class_name, "alnum") == 0) {
            match = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                     || (c >= '0' && c <= '9'));
        } else if (strcmp(class_name, "alpha") == 0) {
            match = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
        } else if (strcmp(class_name, "digit") == 0) {
            match = (c >= '0' && c <= '9');
        } else if (strcmp(class_name, "space") == 0) {
            match = (c == ' ' || c == '\t' || c == '\n' || c == '\r'
                     || c == '\v' || c == '\f');
        } else if (strcmp(class_name, "lower") == 0) {
            match = (c >= 'a' && c <= 'z');
        } else if (strcmp(class_name, "upper") == 0) {
            match = (c >= 'A' && c <= 'Z');
        } else if (strcmp(class_name, "punct") == 0) {
            match = ((c >= 33 && c <= 47) || (c >= 58 && c <= 64)
                     || (c >= 91 && c <= 96) || (c >= 123 && c <= 126));
        } else if (strcmp(class_name, "xdigit") == 0) {
            match = ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')
                     || (c >= 'A' && c <= 'F'));
        } else if (strcmp(class_name, "print") == 0) {
            match = (c >= 32 && c <= 126);
        } else if (strcmp(class_name, "graph") == 0) {
            match = (c >= 33 && c <= 126);
        } else if (strcmp(class_name, "blank") == 0) {
            match = (c == ' ' || c == '\t');
        } else if (strcmp(class_name, "cntrl") == 0) {
            match = ((c >= 0 && c <= 31) || (c == 127));
        }
        if (match) {
            mask[c / 32] |= (1u << (c % 32));
        }
    }
    return;
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
compute_first_set(ParsedOp *ops, int32 pc, int32 temp_ops_count, uint8 *fastmap,
                  uint8 *visited) {
    enum MetaOpType type = 0;
    bool is_null = false;

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
            if ((ops[pc].mask[c / 32] & (1u << (c % 32))) != 0) {
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
        bool p1 = compute_first_set(ops, pc + ops[pc].value, temp_ops_count,
                                    fastmap, visited);
        bool p2 = compute_first_set(ops, pc + ops[pc].min, temp_ops_count,
                                    fastmap, visited);
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
                    is_null = true;
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
