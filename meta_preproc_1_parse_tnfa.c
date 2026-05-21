#include "meta.h"
#include "meta_preproc.h"
#include "primitives.h"
/* Tagged NFA construction helpers. */

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
    if (state >= preproc_config.max_tnfa_states) {
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
        || to >= preproc_config.max_tnfa_states) {
        return false;
    }
    if (tnfa->num_transitions >= preproc_config.max_tnfa_transitions) {
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

static int32
tnfa_fixed_find_group_end(ParsedOp *ops, int32 ops_count, int32 start) {
    return tnfa_find_group_end(ops, ops_count, start);
}

static int32 tnfa_fixed_length_range(ParsedOp *ops, int32 ops_count,
                                     int32 start, int32 end);

static int32
tnfa_fixed_length_branch(ParsedOp *ops, int32 ops_count, int32 start,
                         int32 end) {
    int32 total = 0;

    for (int32 i = start; i < end;) {
        enum MetaOpType type = ops[i].type;
        int32 atom_len = 0;
        int32 next = i + 1;

        if (type == META_OP_LITERAL || type == META_OP_CLASS
            || type == META_OP_ANY) {
            atom_len = 1;
        } else if (type == META_OP_GROUP_START) {
            int32 group_end = tnfa_fixed_find_group_end(ops, ops_count, i);
            if (group_end >= end) {
                return -1;
            }
            atom_len
                = tnfa_fixed_length_range(ops, ops_count, i + 1, group_end);
            if (atom_len < 0) {
                return -1;
            }
            next = group_end + 1;
        } else if (type == META_OP_GROUP_END || type == META_OP_WORD_START
                   || type == META_OP_WORD_END || type == META_OP_WORD_BOUNDARY
                   || type == META_OP_NON_WORD_BOUNDARY) {
            atom_len = 0;
        } else if (type == META_OP_ALTERNATION) {
            return -1;
        } else {
            return -1;
        }

        if (next < end) {
            enum MetaOpType q = ops[next].type;
            if (q == META_OP_STAR || q == META_OP_PLUS
                || q == META_OP_OPTIONAL) {
                return -1;
            }
            if (q == META_OP_BOUNDED) {
                if (ops[next].min != ops[next].max) {
                    return -1;
                }
                atom_len *= ops[next].min;
                next += 1;
            }
        }

        total += atom_len;
        i = next;
    }

    return total;
}

static int32
tnfa_fixed_length_range(ParsedOp *ops, int32 ops_count, int32 start,
                        int32 end) {
    int32 branch_starts[PREPROC_MAX_BRANCHES];
    int32 branch_ends[PREPROC_MAX_BRANCHES];
    int32 branch_count = tnfa_collect_branches(
        ops, start, end, branch_starts, branch_ends, PREPROC_MAX_BRANCHES);
    int32 fixed_len = -2;

    if (branch_count <= 0) {
        return -1;
    }

    for (int32 b = 0; b < branch_count; b += 1) {
        int32 len = tnfa_fixed_length_branch(ops, ops_count, branch_starts[b],
                                             branch_ends[b]);
        if (len < 0) {
            return -1;
        }
        if (fixed_len == -2) {
            fixed_len = len;
        } else if (fixed_len != len) {
            return -1;
        }
    }

    return fixed_len >= 0 ? fixed_len : -1;
}

static void
tnfa_mark_fixed_tags(ParsedTnfa *tnfa, ParsedOp *ops, int32 ops_count) {
    for (int32 i = 0; i < ops_count; i += 1) {
        if (ops[i].type != META_OP_GROUP_START) {
            continue;
        }

        int32 group = ops[i].value;
        int32 end = tnfa_find_group_end(ops, ops_count, i);
        int32 len;
        int32 start_tag;
        int32 end_tag;

        if (end >= ops_count || group <= 0) {
            continue;
        }

        len = tnfa_fixed_length_range(ops, ops_count, i + 1, end);
        if (len < 0) {
            continue;
        }

        start_tag = tnfa_group_start_tag(group);
        end_tag = tnfa_group_end_tag(group);
        if (start_tag <= 0 || start_tag > tnfa->num_tags || end_tag <= 0
            || end_tag > tnfa->num_tags) {
            continue;
        }

        tnfa->tags[start_tag - 1].fixed_base_tag = end_tag;
        tnfa->tags[start_tag - 1].fixed_offset = -len;
    }
    return;
}

static bool
build_tnfa_from_ops(ParsedTnfa *tnfa, ParsedOp *ops, int32 ops_count,
                    int32 group_count) {
    int32 branch_starts[PREPROC_MAX_BRANCHES];
    int32 branch_ends[PREPROC_MAX_BRANCHES];
    int32 top_branch_count = 0;

    memset64(tnfa, 0, SIZEOF(*tnfa));

    if (ops_count + 1 > preproc_config.max_tnfa_states) {
        return false;
    }
    if (group_count*2 > preproc_config.max_tnfa_tags) {
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
        start_tag->fixed_base_tag = 0;
        start_tag->fixed_offset = 0;
        end_tag->fixed_base_tag = 0;
        end_tag->fixed_offset = 0;
    }

    tnfa_mark_fixed_tags(tnfa, ops, ops_count);

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
            if (tnfa_tag_is_fixed(tnfa, tag)) {
                tag = META_TNFA_TAG_NONE;
            }
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
