#include "meta_preproc.h"

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

static int32
tnfa_group_start_tag(int32 group) {
    return group*2 - 1;
}

static int32
tnfa_group_end_tag(int32 group) {
    return group*2;
}

static int32
tnfa_new_state(ParsedTnfa *tnfa) {
    int32 state = tnfa->num_states;
    if (state >= PREPROC_MAX_TNFA_STATES) {
        return -1;
    }
    tnfa->states[state].first_transition = -1;
    tnfa->states[state].transition_count = 0;
    tnfa->num_states += 1;
    return state;
}

static bool
tnfa_add_transition(ParsedTnfa *tnfa, enum MetaTnfaTransitionKind kind,
                    int32 from, int32 to, int32 value, uint32 *mask,
                    int32 priority, int32 tag) {
    if (from < 0 || from >= tnfa->num_states || to < 0
        || to >= PREPROC_MAX_TNFA_STATES) {
        return false;
    }
    if (tnfa->num_transitions >= PREPROC_MAX_TNFA_TRANSITIONS) {
        return false;
    }

    MetaTnfaTransition *tr = &tnfa->transitions[tnfa->num_transitions];
    memset64(tr, 0, SIZEOF(*tr));
    tr->kind = kind;
    tr->from = from;
    tr->to = to;
    tr->value = value;
    tr->priority = priority;
    tr->tag = tag;
    if (mask) {
        memcpy64(tr->mask, mask, SIZEOF(tr->mask));
    }

    tnfa->states[from].transition_count += 1;
    tnfa->num_transitions += 1;
    return true;
}

static bool
tnfa_add_epsilon(ParsedTnfa *tnfa, int32 from, int32 to, int32 priority,
                 int32 tag) {
    return tnfa_add_transition(tnfa, META_TNFA_TRANS_EPSILON, from, to, 0, NULL,
                               priority, tag);
}

static int32
tnfa_find_group_end(ParsedOp *ops, int32 ops_count, int32 start) {
    int32 depth = 0;

    if (start < 0 || start >= ops_count
        || ops[start].type != META_OP_GROUP_START) {
        return ops_count;
    }

    for (int32 i = start + 1; i < ops_count; i += 1) {
        if (ops[i].type == META_OP_GROUP_START) {
            depth += 1;
        } else if (ops[i].type == META_OP_GROUP_END) {
            if (depth == 0) {
                return i;
            }
            depth -= 1;
        }
    }
    return ops_count;
}

static int32
tnfa_find_alt_target(ParsedOp *ops, int32 ops_count, int32 alt_pc) {
    int32 depth = 0;

    for (int32 i = alt_pc + 1; i < ops_count; i += 1) {
        if (ops[i].type == META_OP_GROUP_START) {
            depth += 1;
        } else if (ops[i].type == META_OP_GROUP_END) {
            if (depth == 0) {
                return i;
            }
            depth -= 1;
        }
    }
    return ops_count;
}

static int32
tnfa_collect_branches(ParsedOp *ops, int32 start, int32 end,
                      int32 *branch_starts, int32 *branch_ends,
                      int32 max_branches) {
    int32 depth = 0;
    int32 count = 0;
    int32 current_start = start;

    if (count >= max_branches) {
        return 0;
    }

    for (int32 i = start; i < end; i += 1) {
        if (ops[i].type == META_OP_GROUP_START) {
            depth += 1;
        } else if (ops[i].type == META_OP_GROUP_END) {
            depth -= 1;
        } else if (ops[i].type == META_OP_ALTERNATION && depth == 0) {
            if (count >= max_branches) {
                return 0;
            }
            branch_starts[count] = current_start;
            branch_ends[count] = i;
            count += 1;
            current_start = i + 1;
        }
    }

    if (count >= max_branches) {
        return 0;
    }
    branch_starts[count] = current_start;
    branch_ends[count] = end;
    count += 1;
    return count;
}

static int32
tnfa_collect_tags_in_range(ParsedOp *ops, int32 start, int32 end, int32 *tags,
                           int32 max_tags) {
    int32 count = 0;

    if (start < 0) {
        start = 0;
    }

    for (int32 i = start; i < end; i += 1) {
        if (ops[i].type == META_OP_GROUP_START
            || ops[i].type == META_OP_GROUP_END) {
            int32 tag = 0;
            if (ops[i].type == META_OP_GROUP_START) {
                tag = tnfa_group_start_tag(ops[i].value);
            } else {
                tag = tnfa_group_end_tag(ops[i].value);
            }
            if (count >= max_tags) {
                return -1;
            }
            tags[count] = tag;
            count += 1;
        }
    }
    return count;
}

static int32
tnfa_collect_tags_except_branch(ParsedOp *ops, int32 *branch_starts,
                                int32 *branch_ends, int32 branch_count,
                                int32 selected_branch, int32 *tags,
                                int32 max_tags) {
    int32 count = 0;

    for (int32 b = 0; b < branch_count; b += 1) {
        int32 n = 0;
        if (b == selected_branch) {
            continue;
        }
        n = tnfa_collect_tags_in_range(ops, branch_starts[b], branch_ends[b],
                                       tags + count, max_tags - count);
        if (n < 0) {
            return -1;
        }
        count += n;
    }
    return count;
}

static bool
tnfa_tag_occurs_in_range(ParsedOp *ops, int32 start, int32 end, int32 tag) {
    if (start < 0) {
        start = 0;
    }
    if (end < start) {
        end = start;
    }

    for (int32 i = start; i < end; i += 1) {
        if (ops[i].type == META_OP_GROUP_START
            && tnfa_group_start_tag(ops[i].value) == tag) {
            return true;
        }
        if (ops[i].type == META_OP_GROUP_END
            && tnfa_group_end_tag(ops[i].value) == tag) {
            return true;
        }
    }
    return false;
}

static int32
tnfa_filter_tags_without_prior_occurrence(ParsedOp *ops, int32 prior_start,
                                          int32 prior_end, int32 *tags,
                                          int32 tag_count) {
    int32 write = 0;

    for (int32 i = 0; i < tag_count; i += 1) {
        if (!tnfa_tag_occurs_in_range(ops, prior_start, prior_end, tags[i])) {
            tags[write] = tags[i];
            write += 1;
        }
    }
    return write;
}

static bool
tnfa_add_negative_tag_chain(ParsedTnfa *tnfa, int32 from, int32 to,
                            int32 priority, int32 *tags, int32 tag_count) {
    int32 current = from;

    if (tag_count == 0) {
        return tnfa_add_epsilon(tnfa, from, to, priority, META_TNFA_TAG_NONE);
    }

    for (int32 i = 0; i < tag_count; i += 1) {
        int32 next = to;
        if (i + 1 < tag_count) {
            next = tnfa_new_state(tnfa);
            if (next < 0) {
                return false;
            }
        }
        if (!tnfa_add_epsilon(tnfa, current, next, priority,
                              META_TNFA_NEG_TAG(tags[i]))) {
            return false;
        }
        current = next;
    }
    return true;
}

static bool
tnfa_add_tagged_choice(ParsedTnfa *tnfa, int32 from, int32 to, int32 priority,
                       int32 tag, int32 *negative_tags,
                       int32 negative_tag_count) {
    if (tag == META_TNFA_TAG_NONE) {
        return tnfa_add_negative_tag_chain(tnfa, from, to, priority,
                                           negative_tags, negative_tag_count);
    }

    if (negative_tag_count == 0) {
        return tnfa_add_epsilon(tnfa, from, to, priority, tag);
    }

    int32 after_tag = tnfa_new_state(tnfa);
    if (after_tag < 0) {
        return false;
    }
    if (!tnfa_add_epsilon(tnfa, from, after_tag, priority, tag)) {
        return false;
    }
    return tnfa_add_negative_tag_chain(tnfa, after_tag, to, 1, negative_tags,
                                       negative_tag_count);
}

static bool
tnfa_add_choice_branches(ParsedTnfa *tnfa, ParsedOp *ops, int32 *branch_starts,
                         int32 *branch_ends, int32 branch_count, int32 from,
                         int32 tag) {
    for (int32 b = 0; b < branch_count; b += 1) {
        int32 negative_tags[PREPROC_MAX_TNFA_TAGS];
        int32 negative_count = tnfa_collect_tags_except_branch(
            ops, branch_starts, branch_ends, branch_count, b, negative_tags,
            PREPROC_MAX_TNFA_TAGS);
        if (negative_count < 0) {
            return false;
        }
        if (!tnfa_add_tagged_choice(tnfa, from, branch_starts[b], b + 1, tag,
                                    negative_tags, negative_count)) {
            return false;
        }
    }
    return true;
}

static bool
tnfa_add_atom_transition(ParsedTnfa *tnfa, ParsedOp *ops, int32 ops_count,
                         int32 pc, enum MetaTnfaTransitionKind kind) {
    int32 next_pc = pc + 1;
    enum MetaOpType postfix = 0;

    if (next_pc < ops_count) {
        postfix = ops[next_pc].type;
    }

    if (postfix == META_OP_STAR) {
        if (!tnfa_add_transition(tnfa, kind, pc, next_pc, ops[pc].value,
                                 ops[pc].mask, 0, META_TNFA_TAG_NONE)) {
            return false;
        }
        if (!tnfa_add_epsilon(tnfa, pc, pc + 2, 2, META_TNFA_TAG_NONE)) {
            return false;
        }
        if (!tnfa_add_epsilon(tnfa, next_pc, pc, 1, META_TNFA_TAG_NONE)) {
            return false;
        }
        return tnfa_add_epsilon(tnfa, next_pc, pc + 2, 2, META_TNFA_TAG_NONE);
    }

    if (postfix == META_OP_PLUS) {
        if (!tnfa_add_transition(tnfa, kind, pc, next_pc, ops[pc].value,
                                 ops[pc].mask, 0, META_TNFA_TAG_NONE)) {
            return false;
        }
        if (!tnfa_add_epsilon(tnfa, next_pc, pc, 1, META_TNFA_TAG_NONE)) {
            return false;
        }
        return tnfa_add_epsilon(tnfa, next_pc, pc + 2, 2, META_TNFA_TAG_NONE);
    }

    if (postfix == META_OP_OPTIONAL) {
        if (!tnfa_add_transition(tnfa, kind, pc, next_pc, ops[pc].value,
                                 ops[pc].mask, 0, META_TNFA_TAG_NONE)) {
            return false;
        }
        if (!tnfa_add_epsilon(tnfa, pc, pc + 2, 2, META_TNFA_TAG_NONE)) {
            return false;
        }
        return tnfa_add_epsilon(tnfa, next_pc, pc + 2, 1, META_TNFA_TAG_NONE);
    }

    return tnfa_add_transition(tnfa, kind, pc, next_pc, ops[pc].value,
                               ops[pc].mask, 0, META_TNFA_TAG_NONE);
}

static bool
build_tnfa_from_ops(ParsedTnfa *tnfa, ParsedOp *ops, int32 ops_count,
                    int32 group_count) {
    int32 branch_starts[PREPROC_MAX_BRANCHES];
    int32 branch_ends[PREPROC_MAX_BRANCHES];
    int32 top_branch_count = 0;

    memset64(tnfa, 0, SIZEOF(*tnfa));

    if (ops_count + 1 > PREPROC_MAX_TNFA_STATES) {
        return false;
    }
    if (group_count*2 > PREPROC_MAX_TNFA_TAGS) {
        return false;
    }

    tnfa->num_states = ops_count + 1;
    tnfa->start_state = 0;
    tnfa->final_state = ops_count;

    for (int32 i = 0; i < tnfa->num_states; i += 1) {
        tnfa->states[i].first_transition = -1;
        tnfa->states[i].transition_count = 0;
    }

    tnfa->num_tags = group_count*2;
    for (int32 g = 1; g <= group_count; g += 1) {
        int32 start_id = tnfa_group_start_tag(g);
        int32 end_id = tnfa_group_end_tag(g);
        MetaTnfaTag *start_tag = &tnfa->tags[start_id - 1];
        MetaTnfaTag *end_tag = &tnfa->tags[end_id - 1];

        start_tag->id = start_id;
        start_tag->group = g;
        start_tag->role = META_TNFA_TAG_GROUP_START;
        start_tag->is_multivalued = 0;

        end_tag->id = end_id;
        end_tag->group = g;
        end_tag->role = META_TNFA_TAG_GROUP_END;
        end_tag->is_multivalued = 0;
    }

    top_branch_count = tnfa_collect_branches(ops, 0, ops_count, branch_starts,
                                             branch_ends, PREPROC_MAX_BRANCHES);
    if (top_branch_count == 0) {
        return false;
    }
    if (top_branch_count > 1) {
        int32 start = tnfa_new_state(tnfa);
        if (start < 0) {
            return false;
        }
        tnfa->start_state = start;
        if (!tnfa_add_choice_branches(tnfa, ops, branch_starts, branch_ends,
                                      top_branch_count, start,
                                      META_TNFA_TAG_NONE)) {
            return false;
        }
    }

    for (int32 pc = 0; pc < ops_count; pc += 1) {
        ParsedOp *op = &ops[pc];

        if (op->type == META_OP_LITERAL) {
            if (!tnfa_add_atom_transition(tnfa, ops, ops_count, pc,
                                          META_TNFA_TRANS_LITERAL)) {
                return false;
            }
        } else if (op->type == META_OP_CLASS) {
            if (!tnfa_add_atom_transition(tnfa, ops, ops_count, pc,
                                          META_TNFA_TRANS_CLASS)) {
                return false;
            }
        } else if (op->type == META_OP_ANY) {
            if (!tnfa_add_atom_transition(tnfa, ops, ops_count, pc,
                                          META_TNFA_TRANS_ANY)) {
                return false;
            }
        } else if (op->type == META_OP_GROUP_START) {
            int32 group_end = tnfa_find_group_end(ops, ops_count, pc);
            int32 tag = tnfa_group_start_tag(op->value);
            int32 branch_count = 0;

            branch_count
                = tnfa_collect_branches(ops, pc + 1, group_end, branch_starts,
                                        branch_ends, PREPROC_MAX_BRANCHES);
            if (branch_count == 0) {
                return false;
            }
            if (branch_count > 1) {
                if (!tnfa_add_choice_branches(tnfa, ops, branch_starts,
                                              branch_ends, branch_count, pc,
                                              tag)) {
                    return false;
                }
            } else if (!tnfa_add_epsilon(tnfa, pc, pc + 1, 1, tag)) {
                return false;
            }
        } else if (op->type == META_OP_GROUP_END) {
            if (!tnfa_add_epsilon(tnfa, pc, pc + 1, 1,
                                  tnfa_group_end_tag(op->value))) {
                return false;
            }
        } else if (op->type == META_OP_SPLIT) {
            int32 t1 = pc + op->value;
            int32 t2 = pc + op->min;
            if (t1 < 0 || t1 > ops_count || t2 < 0 || t2 > ops_count) {
                return false;
            }
            if (!tnfa_add_epsilon(tnfa, pc, t1, 1, META_TNFA_TAG_NONE)) {
                return false;
            }
            if (t1 > pc && t2 > t1) {
                int32 negative_tags[PREPROC_MAX_TNFA_TAGS];
                int32 negative_count = tnfa_collect_tags_in_range(
                    ops, t1, t2, negative_tags, PREPROC_MAX_TNFA_TAGS);
                if (negative_count < 0) {
                    return false;
                }

                /*
                    If the skipped range contains tags that already occurred
                    earlier in the op stream, this SPLIT is an optional copy of
                    a repeated capturing subexpression, produced by bounded
                    repetition unrolling. Skipping an extra repetition must not
                    erase the capture from the last successful repetition.

                    Tags with no prior occurrence still need negative tags: this
                    preserves ordinary optional/alternative semantics for
                    captures that have never participated on the current path.
                */
                negative_count = tnfa_filter_tags_without_prior_occurrence(
                    ops, 0, pc, negative_tags, negative_count);

                if (!tnfa_add_negative_tag_chain(tnfa, pc, t2, 2, negative_tags,
                                                 negative_count)) {
                    return false;
                }
            } else if (!tnfa_add_epsilon(tnfa, pc, t2, 2, META_TNFA_TAG_NONE)) {
                return false;
            }
        } else if (op->type == META_OP_JUMP) {
            int32 t1 = pc + op->value;
            if (t1 < 0 || t1 > ops_count) {
                return false;
            }

            if (t1 < pc && t1 < ops_count && ops[t1].type == META_OP_SPLIT) {
                int32 body = t1 + ops[t1].value;
                int32 exit = t1 + ops[t1].min;
                int32 loop_entry = tnfa_new_state(tnfa);

                if (body < 0 || body > ops_count || exit < 0 || exit > ops_count
                    || loop_entry < 0) {
                    return false;
                }

                /*
                    Backward jumps target the SPLIT inserted for a repeated
                    group. The original SPLIT's skip branch represents zero
                    repetitions and may need negative tags. Re-entering after
                    one successful repetition must use a separate state whose
                    exit branch does not clear the repeated group's tags.
                */
                if (!tnfa_add_epsilon(tnfa, pc, loop_entry, 1,
                                      META_TNFA_TAG_NONE)) {
                    return false;
                }
                if (!tnfa_add_epsilon(tnfa, loop_entry, body, 1,
                                      META_TNFA_TAG_NONE)) {
                    return false;
                }
                if (!tnfa_add_epsilon(tnfa, loop_entry, exit, 2,
                                      META_TNFA_TAG_NONE)) {
                    return false;
                }
            } else if (!tnfa_add_epsilon(tnfa, pc, t1, 1, META_TNFA_TAG_NONE)) {
                return false;
            }
        } else if (op->type == META_OP_ALTERNATION) {
            int32 target = tnfa_find_alt_target(ops, ops_count, pc);
            if (!tnfa_add_epsilon(tnfa, pc, target, 1, META_TNFA_TAG_NONE)) {
                return false;
            }
        } else if (op->type == META_OP_WORD_START) {
            if (!tnfa_add_transition(tnfa, META_TNFA_TRANS_WORD_START, pc,
                                     pc + 1, 0, NULL, 0, META_TNFA_TAG_NONE)) {
                return false;
            }
        } else if (op->type == META_OP_WORD_END) {
            if (!tnfa_add_transition(tnfa, META_TNFA_TRANS_WORD_END, pc, pc + 1,
                                     0, NULL, 0, META_TNFA_TAG_NONE)) {
                return false;
            }
        } else if (op->type == META_OP_WORD_BOUNDARY) {
            if (!tnfa_add_transition(tnfa, META_TNFA_TRANS_WORD_BOUNDARY, pc,
                                     pc + 1, 0, NULL, 0, META_TNFA_TAG_NONE)) {
                return false;
            }
        } else if (op->type == META_OP_NON_WORD_BOUNDARY) {
            if (!tnfa_add_transition(tnfa, META_TNFA_TRANS_NON_WORD_BOUNDARY,
                                     pc, pc + 1, 0, NULL, 0,
                                     META_TNFA_TAG_NONE)) {
                return false;
            }
        } else if (op->type == META_OP_STAR || op->type == META_OP_PLUS
                   || op->type == META_OP_OPTIONAL
                   || op->type == META_OP_BOUNDED) {
            /* Postfix operator states are wired by their preceding atom. */
        } else if (op->type == META_OP_BACKREF) {
            return false;
        }
    }

    return true;
}

typedef struct TdfaBuildConfig {
    int32 tnfa_state;
    int32 look[PREPROC_MAX_TNFA_TAGS + 1];
} TdfaBuildConfig;

typedef struct TdfaWorkConfig {
    int32 tnfa_state;
    int32 source_config;
    int32 h[PREPROC_MAX_TNFA_TAGS + 1];
    int32 look[PREPROC_MAX_TNFA_TAGS + 1];
} TdfaWorkConfig;

typedef struct TdfaBuildState {
    int32 prev_is_w;
    int32 curr_is_w;
    int32 config_count;
    TdfaBuildConfig *configs;

    /* Flattened [config][tag] register table. */
    int32 *regs;
} TdfaBuildState;

static int32
tdfa_is_word_char(int32 c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '_');
}

static void
tdfa_init_tag_vector(int32 *v, int32 tag_count) {
    for (int32 i = 0; i < tag_count; i += 1) {
        v[i] = -1;
    }
    return;
}

static void
tdfa_copy_tag_vector(int32 *dst, int32 *src, int32 tag_count) {
    for (int32 i = 0; i < tag_count; i += 1) {
        dst[i] = src[i];
    }
    return;
}

static int32
tdfa_state_reg(TdfaBuildState *state, int32 config, int32 tag_count,
               int32 tag) {
    return state->regs[config*tag_count + tag];
}

static bool
tdfa_add_regop(ParsedTdfa *tdfa, enum MetaTdfaRegOpKind kind, int32 dst,
               int32 src) {
    if (tdfa->num_ops >= PREPROC_MAX_TDFA_REGOPS) {
        return false;
    }

    MetaTdfaRegOp *op = &tdfa->ops[tdfa->num_ops];
    op->kind = kind;
    op->dst = dst;
    op->src = src;
    tdfa->num_ops += 1;
    return true;
}

static bool
tdfa_add_set_from_tag(ParsedTdfa *tdfa, int32 dst, int32 tag_value) {
    if (tag_value == 0) {
        return tdfa_add_regop(tdfa, META_TDFA_REGOP_SET_NIL, dst, 0);
    }
    return tdfa_add_regop(tdfa, META_TDFA_REGOP_SET_POS, dst, 0);
}

static bool
tdfa_tnfa_has_context_assertions(ParsedTnfa *tnfa) {
    for (int32 i = 0; i < tnfa->num_transitions; i += 1) {
        MetaTnfaTransition *tr = &tnfa->transitions[i];
        if (tr->kind == META_TNFA_TRANS_WORD_START
            || tr->kind == META_TNFA_TRANS_WORD_END
            || tr->kind == META_TNFA_TRANS_WORD_BOUNDARY
            || tr->kind == META_TNFA_TRANS_NON_WORD_BOUNDARY) {
            return true;
        }
    }
    return false;
}

static int32
tdfa_assertion_matches(enum MetaTnfaTransitionKind kind, int32 prev_is_w,
                       int32 curr_is_w) {
    if (kind == META_TNFA_TRANS_WORD_START) {
        return (!prev_is_w && curr_is_w);
    }
    if (kind == META_TNFA_TRANS_WORD_END) {
        return (prev_is_w && !curr_is_w);
    }
    if (kind == META_TNFA_TRANS_WORD_BOUNDARY) {
        return (prev_is_w != curr_is_w);
    }
    if (kind == META_TNFA_TRANS_NON_WORD_BOUNDARY) {
        return (prev_is_w == curr_is_w);
    }
    return 0;
}

static int32
tdfa_zero_width_transition_enabled(MetaTnfaTransition *tr, int32 prev_is_w,
                                   int32 curr_is_w) {
    if (tr->kind == META_TNFA_TRANS_EPSILON) {
        return 1;
    }
    return tdfa_assertion_matches(tr->kind, prev_is_w, curr_is_w);
}

static int32
tdfa_transition_is_zero_width(MetaTnfaTransition *tr) {
    return (tr->kind == META_TNFA_TRANS_EPSILON
            || tr->kind == META_TNFA_TRANS_WORD_START
            || tr->kind == META_TNFA_TRANS_WORD_END
            || tr->kind == META_TNFA_TRANS_WORD_BOUNDARY
            || tr->kind == META_TNFA_TRANS_NON_WORD_BOUNDARY);
}

static bool
tdfa_symbol_transition_matches(MetaTnfaTransition *tr, int32 c) {
    if (tr->kind == META_TNFA_TRANS_LITERAL) {
        return c == tr->value;
    }
    if (tr->kind == META_TNFA_TRANS_CLASS) {
        return ((tr->mask[c / 32] & (1u << (c % 32))) != 0);
    }
    if (tr->kind == META_TNFA_TRANS_ANY) {
        return c != '\0';
    }
    return false;
}

static int32
tdfa_collect_zero_width_edges(ParsedTnfa *tnfa, int32 state, int32 prev_is_w,
                              int32 curr_is_w, int32 *edge_indices) {
    int32 edge_count = 0;

    for (int32 i = 0; i < tnfa->num_transitions; i += 1) {
        MetaTnfaTransition *tr = &tnfa->transitions[i];
        if (tr->from != state) {
            continue;
        }
        if (!tdfa_transition_is_zero_width(tr)) {
            continue;
        }
        if (!tdfa_zero_width_transition_enabled(tr, prev_is_w, curr_is_w)) {
            continue;
        }
        edge_indices[edge_count] = i;
        edge_count += 1;
    }

    for (int32 i = 1; i < edge_count; i += 1) {
        int32 edge = edge_indices[i];
        int32 priority = tnfa->transitions[edge].priority;
        int32 j = i - 1;

        while (j >= 0
               && tnfa->transitions[edge_indices[j]].priority > priority) {
            edge_indices[j + 1] = edge_indices[j];
            j -= 1;
        }
        edge_indices[j + 1] = edge;
    }

    return edge_count;
}

static void
tdfa_apply_lookahead_tag(TdfaWorkConfig *cfg, int32 tag) {
    int32 id;

    if (tag == META_TNFA_TAG_NONE) {
        return;
    }

    id = META_TNFA_TAG_ID(tag);
    if (id <= 0 || id > PREPROC_MAX_TNFA_TAGS) {
        return;
    }

    cfg->look[id] = META_TNFA_TAG_IS_NEGATIVE(tag) ? 0 : 1;
    return;
}

static int32
tdfa_epsilon_closure(ParsedTnfa *tnfa, int32 tag_count,
                     TdfaWorkConfig *input_configs, int32 input_count,
                     TdfaWorkConfig *output_configs, TdfaWorkConfig *stack,
                     int32 *edge_indices, int32 work_capacity, int32 prev_is_w,
                     int32 curr_is_w) {
    uint8 closed_seen[PREPROC_MAX_TNFA_STATES];
    int32 stack_count = 0;
    int32 output_count = 0;

    for (int32 i = 0; i < tnfa->num_states; i += 1) {
        closed_seen[i] = 0;
    }

    for (int32 i = input_count - 1; i >= 0; i -= 1) {
        if (stack_count >= work_capacity) {
            return -1;
        }
        stack[stack_count] = input_configs[i];
        stack_count += 1;
    }

    while (stack_count > 0) {
        TdfaWorkConfig cfg;
        int32 edge_count;
        int32 selected_count = 0;

        stack_count -= 1;
        cfg = stack[stack_count];

        if (cfg.tnfa_state < 0 || cfg.tnfa_state >= tnfa->num_states) {
            continue;
        }
        if (closed_seen[cfg.tnfa_state]) {
            continue;
        }
        closed_seen[cfg.tnfa_state] = 1;

        if (output_count >= work_capacity) {
            return -1;
        }
        output_configs[output_count] = cfg;
        output_count += 1;

        edge_count = tdfa_collect_zero_width_edges(
            tnfa, cfg.tnfa_state, prev_is_w, curr_is_w, edge_indices);

        for (int32 i = 0; i < edge_count; i += 1) {
            MetaTnfaTransition *tr = &tnfa->transitions[edge_indices[i]];
            if (tr->to < 0 || tr->to >= tnfa->num_states
                || closed_seen[tr->to]) {
                continue;
            }
            edge_indices[selected_count] = edge_indices[i];
            selected_count += 1;
        }

        for (int32 i = selected_count - 1; i >= 0; i -= 1) {
            MetaTnfaTransition *tr = &tnfa->transitions[edge_indices[i]];
            TdfaWorkConfig next = cfg;

            next.tnfa_state = tr->to;
            if (tr->kind == META_TNFA_TRANS_EPSILON) {
                tdfa_apply_lookahead_tag(&next, tr->tag);
            }

            if (stack_count >= work_capacity) {
                return -1;
            }
            stack[stack_count] = next;
            stack_count += 1;
        }
    }

    (void)tag_count;
    return output_count;
}

static int32
tdfa_step_on_symbol(ParsedTnfa *tnfa, TdfaBuildState *source, int32 symbol,
                    int32 tag_count, TdfaWorkConfig *work,
                    int32 work_capacity) {
    int32 work_count = 0;

    for (int32 i = 0; i < source->config_count; i += 1) {
        TdfaBuildConfig *cfg = &source->configs[i];

        for (int32 j = 0; j < tnfa->num_transitions; j += 1) {
            MetaTnfaTransition *tr = &tnfa->transitions[j];
            TdfaWorkConfig *dst;

            if (tr->from != cfg->tnfa_state) {
                continue;
            }
            if (!tdfa_symbol_transition_matches(tr, symbol)) {
                continue;
            }
            if (tr->to < 0 || tr->to >= tnfa->num_states) {
                continue;
            }
            if (work_count >= work_capacity) {
                return -1;
            }

            dst = &work[work_count];
            dst->tnfa_state = tr->to;
            dst->source_config = i;
            tdfa_copy_tag_vector(dst->h, cfg->look, tag_count);
            tdfa_init_tag_vector(dst->look, tag_count);
            work_count += 1;
        }
    }

    return work_count;
}

static bool
tdfa_same_signature(TdfaBuildState *state, TdfaWorkConfig *configs,
                    int32 config_count, int32 tag_count, int32 prev_is_w,
                    int32 curr_is_w) {
    if (state->prev_is_w != prev_is_w || state->curr_is_w != curr_is_w) {
        return false;
    }
    if (state->config_count != config_count) {
        return false;
    }

    for (int32 i = 0; i < config_count; i += 1) {
        if (state->configs[i].tnfa_state != configs[i].tnfa_state) {
            return false;
        }
        for (int32 t = 1; t < tag_count; t += 1) {
            if (state->configs[i].look[t] != configs[i].look[t]) {
                return false;
            }
        }
    }

    return true;
}

static int32
tdfa_find_state(TdfaBuildState *states, ParsedTdfa *tdfa,
                TdfaWorkConfig *configs, int32 config_count, int32 tag_count,
                int32 prev_is_w, int32 curr_is_w) {
    for (int32 i = 0; i < tdfa->num_states; i += 1) {
        if (tdfa_same_signature(&states[i], configs, config_count, tag_count,
                                prev_is_w, curr_is_w)) {
            return i;
        }
    }
    return -1;
}

static bool
tdfa_add_final_ops(ParsedTdfa *tdfa, ParsedTnfa *tnfa,
                   TdfaBuildState *build_state, int32 state_id,
                   int32 tag_count) {
    MetaTdfaState *state = &tdfa->states[state_id];

    for (int32 i = 0; i < build_state->config_count; i += 1) {
        TdfaBuildConfig *cfg = &build_state->configs[i];

        if (cfg->tnfa_state != tnfa->final_state) {
            continue;
        }

        state->is_accepting = 1;
        state->first_final_op = tdfa->num_ops;

        for (int32 t = 1; t < tag_count; t += 1) {
            int32 dst = tdfa->final_register_base + t - 1;

            if (cfg->look[t] != -1) {
                if (!tdfa_add_set_from_tag(tdfa, dst, cfg->look[t])) {
                    return false;
                }
            } else {
                int32 src = tdfa_state_reg(build_state, i, tag_count, t);
                if (!tdfa_add_regop(tdfa, META_TDFA_REGOP_COPY, dst, src)) {
                    return false;
                }
            }
        }

        state->final_op_count = tdfa->num_ops - state->first_final_op;
        return true;
    }

    return true;
}

static int32
tdfa_add_state(TdfaBuildState *states, ParsedTdfa *tdfa, ParsedTnfa *tnfa,
               TdfaWorkConfig *configs, int32 config_count, int32 tag_count,
               int32 *next_register, int32 prev_is_w, int32 curr_is_w) {
    int32 state_id;
    TdfaBuildState *build_state;
    MetaTdfaState *state;

    if (tdfa->num_states >= PREPROC_MAX_TDFA_STATES) {
        return -1;
    }

    state_id = tdfa->num_states;
    tdfa->num_states += 1;

    state = &tdfa->states[state_id];
    state->is_accepting = 0;
    state->first_transition = -1;
    state->transition_count = 0;
    state->first_final_op = -1;
    state->final_op_count = 0;

    build_state = &states[state_id];
    build_state->prev_is_w = prev_is_w;
    build_state->curr_is_w = curr_is_w;
    build_state->config_count = config_count;
    build_state->configs = NULL;
    build_state->regs = NULL;

    if (config_count > 0) {
        build_state->configs
            = malloc2(SIZEOF(*build_state->configs)*config_count);
        build_state->regs
            = malloc2(SIZEOF(*build_state->regs)*config_count * tag_count);
        if (build_state->configs == NULL || build_state->regs == NULL) {
            return -1;
        }
    }

    for (int32 i = 0; i < config_count; i += 1) {
        build_state->configs[i].tnfa_state = configs[i].tnfa_state;
        tdfa_copy_tag_vector(build_state->configs[i].look, configs[i].look,
                             tag_count);

        for (int32 t = 0; t < tag_count; t += 1) {
            if (t == 0) {
                build_state->regs[i*tag_count + t] = 0;
            } else {
                if (*next_register > PREPROC_MAX_TDFA_REGISTERS) {
                    return -1;
                }
                build_state->regs[i*tag_count + t] = *next_register;
                *next_register += 1;
            }
        }
    }

    if (!tdfa_add_final_ops(tdfa, tnfa, build_state, state_id, tag_count)) {
        return -1;
    }

    return state_id;
}

static bool
tdfa_emit_transition_ops(ParsedTdfa *tdfa, TdfaBuildState *source,
                         TdfaBuildState *target, TdfaWorkConfig *closed,
                         int32 closed_count, int32 tag_count) {
    for (int32 i = 0; i < closed_count; i += 1) {
        int32 source_config = closed[i].source_config;

        if (source_config < 0 || source_config >= source->config_count) {
            return false;
        }

        for (int32 t = 1; t < tag_count; t += 1) {
            int32 dst = tdfa_state_reg(target, i, tag_count, t);

            if (closed[i].h[t] != -1 && closed[i].look[t] == -1) {
                if (!tdfa_add_set_from_tag(tdfa, dst, closed[i].h[t])) {
                    return false;
                }
            } else {
                int32 src = tdfa_state_reg(source, source_config, tag_count, t);
                if (src != dst) {
                    if (!tdfa_add_regop(tdfa, META_TDFA_REGOP_COPY, dst, src)) {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

static bool
tdfa_add_transition(ParsedTdfa *tdfa, int32 from, int32 to, int32 symbol,
                    int32 next_is_word, int32 first_op, int32 op_count) {
    if (tdfa->num_transitions >= PREPROC_MAX_TDFA_TRANSITIONS) {
        return false;
    }

    MetaTdfaTransition *tr = &tdfa->transitions[tdfa->num_transitions];
    tr->from = from;
    tr->to = to;
    tr->symbol = symbol;
    tr->next_is_word = next_is_word;
    tr->first_op = first_op;
    tr->op_count = op_count;
    tdfa->num_transitions += 1;
    return true;
}

static bool
build_tdfa_from_tnfa(ParsedTdfa *tdfa, ParsedTnfa *tnfa) {
    int32 tag_count;
    int32 next_register;
    int32 work_capacity;
    int32 uses_context;
    TdfaBuildState *build_states = NULL;
    TdfaWorkConfig *work = NULL;
    TdfaWorkConfig *closed = NULL;
    TdfaWorkConfig *stack = NULL;
    int32 *edge_indices = NULL;
    TdfaWorkConfig initial;

    if (tdfa == NULL || tnfa == NULL) {
        return false;
    }
    if (tnfa->num_tags < 0 || tnfa->num_tags > PREPROC_MAX_TNFA_TAGS
        || tnfa->num_states <= 0 || tnfa->num_states > PREPROC_MAX_TNFA_STATES
        || tnfa->num_transitions < 0) {
        return false;
    }

    memset64(tdfa, 0, SIZEOF(*tdfa));

    tdfa->num_tags = tnfa->num_tags;
    tdfa->start_state = 0;
    tdfa->start_state_nw_nw = -1;
    tdfa->start_state_nw_w = -1;
    tdfa->start_state_w_nw = -1;
    tdfa->start_state_w_w = -1;
    tdfa->final_register_base = 1;
    for (int32 i = 0; i < tnfa->num_tags; i += 1) {
        tdfa->tags[i] = tnfa->tags[i];
    }

    tag_count = tnfa->num_tags + 1;
    next_register = tnfa->num_tags + 1;
    work_capacity = tnfa->num_states + tnfa->num_transitions + 1;
    uses_context = tdfa_tnfa_has_context_assertions(tnfa);
    tdfa->uses_context = uses_context;
    if (work_capacity <= 0 || work_capacity > PREPROC_MAX_TDFA_WORK_CONFIGS) {
        return false;
    }

    build_states = malloc2(SIZEOF(*build_states)*PREPROC_MAX_TDFA_STATES);
    work = malloc2(SIZEOF(*work)*work_capacity);
    closed = malloc2(SIZEOF(*closed)*work_capacity);
    stack = malloc2(SIZEOF(*stack)*work_capacity);
    edge_indices
        = malloc2(SIZEOF(*edge_indices)
                  * (tnfa->num_transitions > 0 ? tnfa->num_transitions : 1));
    if (build_states == NULL || work == NULL || closed == NULL || stack == NULL
        || edge_indices == NULL) {
        return false;
    }
    memset64(build_states, 0, SIZEOF(*build_states)*PREPROC_MAX_TDFA_STATES);

    initial.tnfa_state = tnfa->start_state;
    initial.source_config = -1;
    tdfa_init_tag_vector(initial.h, tag_count);
    tdfa_init_tag_vector(initial.look, tag_count);

    if (uses_context) {
        for (int32 prev = 0; prev <= 1; prev += 1) {
            for (int32 curr = 0; curr <= 1; curr += 1) {
                int32 initial_count = tdfa_epsilon_closure(
                    tnfa, tag_count, &initial, 1, closed, stack, edge_indices,
                    work_capacity, prev, curr);
                int32 state_id;

                if (initial_count < 0) {
                    return false;
                }

                state_id
                    = tdfa_find_state(build_states, tdfa, closed, initial_count,
                                      tag_count, prev, curr);
                if (state_id < 0) {
                    state_id = tdfa_add_state(build_states, tdfa, tnfa, closed,
                                              initial_count, tag_count,
                                              &next_register, prev, curr);
                    if (state_id < 0) {
                        return false;
                    }
                }

                if (!prev && !curr) {
                    tdfa->start_state_nw_nw = state_id;
                } else if (!prev && curr) {
                    tdfa->start_state_nw_w = state_id;
                } else if (prev && !curr) {
                    tdfa->start_state_w_nw = state_id;
                } else {
                    tdfa->start_state_w_w = state_id;
                }
            }
        }
        tdfa->start_state = tdfa->start_state_nw_nw;
    } else {
        int32 initial_count
            = tdfa_epsilon_closure(tnfa, tag_count, &initial, 1, closed, stack,
                                   edge_indices, work_capacity, 0, 0);
        if (initial_count < 0) {
            return false;
        }
        if (tdfa_add_state(build_states, tdfa, tnfa, closed, initial_count,
                           tag_count, &next_register, 0, 0)
            != 0) {
            return false;
        }
        tdfa->start_state = 0;
        tdfa->start_state_nw_nw = 0;
        tdfa->start_state_nw_w = 0;
        tdfa->start_state_w_nw = 0;
        tdfa->start_state_w_w = 0;
    }

    for (int32 state_id = 0; state_id < tdfa->num_states; state_id += 1) {
        TdfaBuildState *source = &build_states[state_id];
        MetaTdfaState *state = &tdfa->states[state_id];

        state->first_transition = tdfa->num_transitions;

        for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
            int32 work_count;
            int32 next_min = 0;
            int32 next_max = uses_context ? 1 : 0;

            if (uses_context && tdfa_is_word_char(c) != source->curr_is_w) {
                continue;
            }

            work_count = tdfa_step_on_symbol(tnfa, source, c, tag_count, work,
                                             work_capacity);
            if (work_count < 0) {
                return false;
            }
            if (work_count == 0) {
                continue;
            }

            for (int32 next_curr = next_min; next_curr <= next_max;
                 next_curr += 1) {
                int32 prev = uses_context ? tdfa_is_word_char(c) : 0;
                int32 curr = uses_context ? next_curr : 0;
                int32 next_is_word = uses_context ? next_curr : -1;
                int32 closed_count;
                int32 target_id;
                int32 first_op;
                int32 op_count;

                closed_count = tdfa_epsilon_closure(
                    tnfa, tag_count, work, work_count, closed, stack,
                    edge_indices, work_capacity, prev, curr);
                if (closed_count < 0) {
                    return false;
                }

                target_id
                    = tdfa_find_state(build_states, tdfa, closed, closed_count,
                                      tag_count, prev, curr);
                if (target_id < 0) {
                    target_id = tdfa_add_state(build_states, tdfa, tnfa, closed,
                                               closed_count, tag_count,
                                               &next_register, prev, curr);
                    if (target_id < 0) {
                        return false;
                    }
                }

                first_op = tdfa->num_ops;
                if (!tdfa_emit_transition_ops(tdfa, source,
                                              &build_states[target_id], closed,
                                              closed_count, tag_count)) {
                    return false;
                }
                op_count = tdfa->num_ops - first_op;

                if (!tdfa_add_transition(tdfa, state_id, target_id, c,
                                         next_is_word, first_op, op_count)) {
                    return false;
                }
            }
        }

        state->transition_count
            = tdfa->num_transitions - state->first_transition;
        if (state->transition_count == 0) {
            state->first_transition = -1;
        }
    }

    tdfa->num_registers = next_register - 1;
    return true;
}

RegexList
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
        char raw_string[PREPROC_MAX_STRING_LEN] = {0};
        char regex_string[PREPROC_MAX_STRING_LEN] = {0};
        char op_buffer[PREPROC_OP_BUFFER_SIZE] = {0};
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

        paren_end = strchr(quote_end, ')');
        original_string_length = (int32)(quote_end - quote_start) + 1;

        if ((used_ops & META_OP_BACKREF) == 0) {
            regex->tnfa = malloc2(SIZEOF(*regex->tnfa));
            if (!build_tnfa_from_ops(regex->tnfa, temp_ops, temp_ops_count,
                                     group_counter)) {
                fprintf(stderr, "Warning: TNFA construction failed for %.*s.\n",
                        original_string_length, quote_start);
                regex->tnfa = NULL;
            }

            if (regex->tnfa != NULL) {
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
