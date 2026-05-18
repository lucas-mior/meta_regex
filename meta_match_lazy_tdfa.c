#if !defined(META_LAZY_TDFA_C)
#define META_LAZY_TDFA_C

#include "primitives.h"
#include "meta.h"
#include <regex.h>

#define META_MAX_TAGS 32 /* Supports up to 16 groups (rm_so and rm_eo) */

typedef struct LazyTdfaKey {
    uint32 bits[META_PC_WORDS];
    int32 prev_is_word;
} LazyTdfaKey;

#define HASH_KEY_TYPE LazyTdfaKey
#define HASH_KEY_FIXED_LEN 0
#define HASH_VALUE_TYPE int32
#define HASH_VALUE_FORMATTER "%d"
#define HASH_TYPE tmap
#define HASH_DUPLICATE_KEYS 0
#include "hash.c"

typedef struct TdfaCommand {
    int32 set_tag[META_MAX_TAGS];
} TdfaCommand;

typedef struct LazyTdfaState {
    int32 next[META_ALPHABET_SIZE];
    TdfaCommand commands[META_ALPHABET_SIZE];
    int32 accepts_before[META_ALPHABET_SIZE];
    TdfaCommand accepts_before_cmds[META_ALPHABET_SIZE];
    int32 accepts_on_eof;
    TdfaCommand eof_cmd;
    LazyTdfaKey key;
} LazyTdfaState;

typedef struct LazyTdfa {
    struct Hash_tmap *state_tmap;
    int32 num_states;
    LazyTdfaState states[META_MAX_LAZY_DFA_STATES];
} LazyTdfa;

static int32
is_word_char_tdfa(int32 c) {
    if (c >= 'a' && c <= 'z') {
        return 1;
    }
    if (c >= 'A' && c <= 'Z') {
        return 1;
    }
    if (c >= '0' && c <= '9') {
        return 1;
    }
    if (c == '_') {
        return 1;
    }
    return 0;
}

static void tdfa_add_epsilon_closure(MetaOp *ops, int32 pc, NfaStateSet *set,
                                     int32 *is_accepting, int32 prev_is_word,
                                     int32 curr_is_word, uint32 *pc_masks);
static void tdfa_compute_core_transitions(MetaOp *ops,
                                          NfaStateSet *current_closed_set,
                                          int32 c, NfaStateSet *next_core_set);

static int32
try_match_lazy_tdfa(MetaRegex *regex, uchar *input, int32 input_len,
                    int32 offset, int64 nmatch, regmatch_t *pmatch) {
    LazyTdfa *ldfa;
    int32 current_state_id;
    int32 last_accept;
    int32 prev_is_word;
    int32 tags[META_MAX_TAGS];
    int32 best_tags[META_MAX_TAGS];
    (void)input_len;

    ldfa = (LazyTdfa *)regex->lazy_dfa;
    if (ldfa == NULL) {
        ldfa = malloc2(SIZEOF(LazyTdfa));
        ldfa->state_tmap = hash_create_tmap(META_MAX_LAZY_DFA_STATES, "tdfa");
        ldfa->num_states = 1;
        regex->lazy_dfa = ldfa;
    }

    if (offset > 0) {
        prev_is_word = is_word_char_tdfa((uchar)input[offset - 1]);
    } else {
        prev_is_word = 0;
    }

    for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
        tags[t] = -1;
        best_tags[t] = -1;
    }

    {
        LazyTdfaKey start_key;
        int32 depth;

        for (int32 i = 0; i < META_PC_WORDS; i += 1) {
            start_key.bits[i] = 0;
        }
        start_key.prev_is_word = prev_is_word;
        start_key.bits[0] = 1;

        depth = 0;
        for (int32 i = 0; regex->ops[i].type != META_OP_END; i += 1) {
            if (regex->ops[i].type == META_OP_GROUP_START) {
                depth += 1;
            } else if (regex->ops[i].type == META_OP_GROUP_END) {
                depth -= 1;
            } else if (regex->ops[i].type == META_OP_ALTERNATION
                       && depth == 0) {
                start_key.bits[(i + 1) / 32] |= (1u << ((i + 1) % 32));
            }
        }

        if (!hash_lookup_tmap(ldfa->state_tmap, &start_key, SIZEOF(start_key),
                              &current_state_id)) {
            current_state_id = ldfa->num_states;
            if (current_state_id < META_MAX_LAZY_DFA_STATES) {
                ldfa->num_states += 1;
                ldfa->states[current_state_id].key = start_key;
                ldfa->states[current_state_id].accepts_on_eof = -1;
                for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                    ldfa->states[current_state_id].eof_cmd.set_tag[t] = 0;
                }
                for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                    ldfa->states[current_state_id].next[c] = 0;
                    ldfa->states[current_state_id].accepts_before[c] = 0;
                    for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                        ldfa->states[current_state_id].accepts_before_cmds[c].set_tag[t] = 0;
                        ldfa->states[current_state_id].commands[c].set_tag[t]
                            = 0;
                    }
                }
                ASSERT(hash_insert_tmap(ldfa->state_tmap, &start_key,
                                        SIZEOF(start_key), current_state_id));
            } else {
                return -1;
            }
        }
    }

    last_accept = -1;

    for (int32 i = offset;; i += 1) {
        uchar b;

        b = (uchar)input[i];
        if (b == '\0') {
            if (current_state_id > 0
                && current_state_id < META_MAX_LAZY_DFA_STATES) {
                LazyTdfaState *state;
                state = &ldfa->states[current_state_id];

                if (state->accepts_on_eof == -1) {
                    NfaStateSet closed_set;
                    int32 is_acc;
                    uint32 pc_masks[META_MAX_OPS];

                    for (int32 k = 0; k < META_PC_WORDS; k += 1) {
                        closed_set.bits[k] = 0;
                    }
                    for (int32 k = 0; k < META_MAX_OPS; k += 1) {
                        pc_masks[k] = 0;
                    }
                    is_acc = 0;

                    for (int32 k = 0; k < META_MAX_OPS; k += 1) {
                        if ((state->key.bits[k / 32] & (1u << (k % 32))) != 0) {
                            tdfa_add_epsilon_closure(
                                regex->ops, k, &closed_set, &is_acc,
                                state->key.prev_is_word, 0, pc_masks);
                        }
                    }
                    state->accepts_on_eof = is_acc;
                    
                    for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                        state->eof_cmd.set_tag[t] = 0;
                    }
                    if (is_acc) {
                        for (int32 k = 0; k < META_MAX_OPS; k += 1) {
                            if ((closed_set.bits[k / 32] & (1u << (k % 32))) != 0) {
                                if (regex->ops[k].type == META_OP_END) {
                                    uint32 final_mask;
                                    final_mask = pc_masks[k];
                                    for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                                        if ((final_mask & (1u << t)) != 0) {
                                            state->eof_cmd.set_tag[t] = 1;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                if (state->accepts_on_eof) {
                    last_accept = i;
                    for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                        if (state->eof_cmd.set_tag[t]) {
                            tags[t] = i;
                        }
                    }
                    for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                        best_tags[t] = tags[t];
                    }
                }
            }
            break;
        }

        if (current_state_id > 0
            && current_state_id < META_MAX_LAZY_DFA_STATES) {
            LazyTdfaState *state;
            state = &ldfa->states[current_state_id];

            if (state->next[b] == 0) {
                int32 curr_is_word;
                NfaStateSet closed_set;
                int32 is_acc;
                NfaStateSet next_core;
                int32 set_is_empty;
                uint32 pc_masks[META_MAX_OPS];

                curr_is_word = is_word_char_tdfa(b);
                for (int32 k = 0; k < META_PC_WORDS; k += 1) {
                    closed_set.bits[k] = 0;
                }
                for (int32 k = 0; k < META_MAX_OPS; k += 1) {
                    pc_masks[k] = 0;
                }
                is_acc = 0;

                for (int32 k = 0; k < META_MAX_OPS; k += 1) {
                    if ((state->key.bits[k / 32] & (1u << (k % 32))) != 0) {
                        tdfa_add_epsilon_closure(
                            regex->ops, k, &closed_set, &is_acc,
                            state->key.prev_is_word, curr_is_word, pc_masks);
                    }
                }
                state->accepts_before[b] = is_acc;

                for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                    state->accepts_before_cmds[b].set_tag[t] = 0;
                }
                if (is_acc) {
                    for (int32 k = 0; k < META_MAX_OPS; k += 1) {
                        if ((closed_set.bits[k / 32] & (1u << (k % 32))) != 0) {
                            if (regex->ops[k].type == META_OP_END) {
                                uint32 final_mask;
                                final_mask = pc_masks[k];
                                for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                                    if ((final_mask & (1u << t)) != 0) {
                                        state->accepts_before_cmds[b].set_tag[t] = 1;
                                    }
                                }
                            }
                        }
                    }
                }

                tdfa_compute_core_transitions(regex->ops, &closed_set, b,
                                              &next_core);

                for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                    state->commands[b].set_tag[t] = 0;
                }
                
                for (int32 k = 0; k < META_MAX_OPS; k += 1) {
                    if ((closed_set.bits[k / 32] & (1u << (k % 32))) != 0) {
                        MetaOp *op;
                        int32 match;
                        op = &regex->ops[k];
                        match = 0;

                        if (op->type == META_OP_LITERAL) {
                            if (b == op->value) {
                                match = 1;
                            }
                        } else if (op->type == META_OP_CLASS) {
                            if (b >= 0 && b < META_ALPHABET_SIZE) {
                                if ((op->mask[b / 32] & (1u << (b % 32))) != 0) {
                                    match = 1;
                                }
                            }
                        } else if (op->type == META_OP_ANY) {
                            if (b != '\0') {
                                match = 1;
                            }
                        }

                        if (match) {
                            uint32 trans_mask;
                            trans_mask = pc_masks[k];
                            for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                                if ((trans_mask & (1u << t)) != 0) {
                                    state->commands[b].set_tag[t] = 1;
                                }
                            }
                        }
                    }
                }

                set_is_empty = 1;
                for (int32 k = 0; k < META_PC_WORDS; k += 1) {
                    if (next_core.bits[k] != 0) {
                        set_is_empty = 0;
                        break;
                    }
                }

                if (set_is_empty) {
                    state->next[b] = -1;
                } else {
                    LazyTdfaKey next_key;
                    int32 next_id;

                    for (int32 k = 0; k < META_PC_WORDS; k += 1) {
                        next_key.bits[k] = next_core.bits[k];
                    }
                    next_key.prev_is_word = curr_is_word;

                    if (!hash_lookup_tmap(ldfa->state_tmap, &next_key,
                                          SIZEOF(next_key), &next_id)) {
                        next_id = ldfa->num_states;
                        if (next_id < META_MAX_LAZY_DFA_STATES) {
                            ldfa->num_states += 1;
                            ldfa->states[next_id].key = next_key;
                            ldfa->states[next_id].accepts_on_eof = -1;
                            for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                                ldfa->states[next_id].eof_cmd.set_tag[t] = 0;
                            }
                            for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                                ldfa->states[next_id].next[c] = 0;
                                ldfa->states[next_id].accepts_before[c] = 0;
                                for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                                    ldfa->states[next_id].accepts_before_cmds[c].set_tag[t] = 0;
                                }
                            }
                            ASSERT(hash_insert_tmap(ldfa->state_tmap, &next_key,
                                                    SIZEOF(next_key), next_id));
                        } else {
                            next_id = -1;
                        }
                    }
                    state->next[b] = next_id;
                }
            }

            for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                if (state->commands[b].set_tag[t]) {
                    tags[t] = i;
                }
            }

            if (state->accepts_before[b]) {
                last_accept = i;
                for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                    best_tags[t] = tags[t];
                    if (state->accepts_before_cmds[b].set_tag[t]) {
                        best_tags[t] = i;
                    }
                }
            }
            current_state_id = state->next[b];

            if (current_state_id == -1) {
                break;
            }
        } else {
            break;
        }
    }

    if (last_accept >= 0) {
        if (!regex->has_end_anchor || input[last_accept] == '\0') {
            if (pmatch != NULL && nmatch > 0) {
                pmatch[0].rm_so = offset;
                pmatch[0].rm_eo = last_accept;

                for (int64 k = 1; k < nmatch && k <= regex->re_nsub; k += 1) {
                    int32 start_tag_idx;
                    int32 end_tag_idx;
                    int32 so;
                    int32 eo;

                    start_tag_idx = (k*2) - 2;
                    end_tag_idx = (k*2) - 1;

                    if (start_tag_idx < META_MAX_TAGS
                        && end_tag_idx < META_MAX_TAGS) {
                        so = best_tags[start_tag_idx];
                        eo = best_tags[end_tag_idx];

                        if (so == -1 || eo == -1) {
                            so = -1;
                            eo = -1;
                        }
                        pmatch[k].rm_so = so;
                        pmatch[k].rm_eo = eo;
                    }
                }
            }
            return 0;
        }
    }

    return -1;
}

static void
tdfa_add_epsilon_closure(MetaOp *ops, int32 pc, NfaStateSet *set,
                         int32 *is_accepting, int32 prev_is_word,
                         int32 curr_is_word, uint32 *pc_masks) {
    int32 stack[META_MAX_OPS];
    uint32 mask_stack[META_MAX_OPS];
    int32 stack_ptr;

    stack_ptr = 0;
    stack[stack_ptr] = pc;
    mask_stack[stack_ptr] = 0;
    stack_ptr += 1;

    while (stack_ptr > 0) {
        int32 current_pc;
        uint32 current_mask;
        MetaOp *op;

        stack_ptr -= 1;
        current_pc = stack[stack_ptr];
        current_mask = mask_stack[stack_ptr];

        if ((set->bits[current_pc / 32] & (1u << (current_pc % 32))) != 0) {
            continue;
        }

        set->bits[current_pc / 32] |= (1u << (current_pc % 32));
        pc_masks[current_pc] = current_mask;
        op = &ops[current_pc];

        if (op->type == META_OP_END) {
            *is_accepting = 1;
        } else if (op->type == META_OP_SPLIT) {
            stack[stack_ptr] = current_pc + op->value;
            mask_stack[stack_ptr] = current_mask;
            stack_ptr += 1;
            stack[stack_ptr] = current_pc + op->min;
            mask_stack[stack_ptr] = current_mask;
            stack_ptr += 1;
        } else if (op->type == META_OP_JUMP) {
            stack[stack_ptr] = current_pc + op->value;
            mask_stack[stack_ptr] = current_mask;
            stack_ptr += 1;
        } else if (op->type == META_OP_WORD_BOUNDARY) {
            if (prev_is_word != curr_is_word) {
                stack[stack_ptr] = current_pc + 1;
                mask_stack[stack_ptr] = current_mask;
                stack_ptr += 1;
            }
        } else if (op->type == META_OP_NON_WORD_BOUNDARY) {
            if (prev_is_word == curr_is_word) {
                stack[stack_ptr] = current_pc + 1;
                mask_stack[stack_ptr] = current_mask;
                stack_ptr += 1;
            }
        } else if (op->type == META_OP_WORD_START) {
            if (!prev_is_word && curr_is_word) {
                stack[stack_ptr] = current_pc + 1;
                mask_stack[stack_ptr] = current_mask;
                stack_ptr += 1;
            }
        } else if (op->type == META_OP_WORD_END) {
            if (prev_is_word && !curr_is_word) {
                stack[stack_ptr] = current_pc + 1;
                mask_stack[stack_ptr] = current_mask;
                stack_ptr += 1;
            }
        } else if (op->type == META_OP_GROUP_START) {
            int32 tag_idx;
            int32 depth;
            uint32 next_mask;

            next_mask = current_mask;
            tag_idx = (op->value*2) - 2;
            if (tag_idx >= 0 && tag_idx < META_MAX_TAGS) {
                next_mask |= (1u << tag_idx);
            }

            stack[stack_ptr] = current_pc + 1;
            mask_stack[stack_ptr] = next_mask;
            stack_ptr += 1;
            depth = 0;
            for (int32 i = current_pc + 1; ops[i].type != META_OP_END; i += 1) {
                if (ops[i].type == META_OP_GROUP_START) {
                    depth += 1;
                } else if (ops[i].type == META_OP_GROUP_END) {
                    if (depth == 0) {
                        break;
                    }
                    depth -= 1;
                } else if (ops[i].type == META_OP_ALTERNATION && depth == 0) {
                    stack[stack_ptr] = i + 1;
                    mask_stack[stack_ptr] = next_mask;
                    stack_ptr += 1;
                }
            }
        } else if (op->type == META_OP_GROUP_END) {
            int32 tag_idx;
            uint32 next_mask;

            next_mask = current_mask;
            tag_idx = (op->value*2) - 1;
            if (tag_idx >= 0 && tag_idx < META_MAX_TAGS) {
                next_mask |= (1u << tag_idx);
            }

            stack[stack_ptr] = current_pc + 1;
            mask_stack[stack_ptr] = next_mask;
            stack_ptr += 1;
        } else if (op->type == META_OP_ALTERNATION) {
            int32 depth;
            int32 i;

            depth = 0;
            i = current_pc + 1;
            while (ops[i].type != META_OP_END) {
                if (ops[i].type == META_OP_GROUP_START) {
                    depth += 1;
                } else if (ops[i].type == META_OP_GROUP_END) {
                    if (depth == 0) {
                        break;
                    }
                    depth -= 1;
                }
                i += 1;
            }
            stack[stack_ptr] = i;
            mask_stack[stack_ptr] = current_mask;
            stack_ptr += 1;
        }

        if (op->type == META_OP_LITERAL || op->type == META_OP_CLASS
            || op->type == META_OP_ANY) {
            MetaOp *next_op;

            next_op = &ops[current_pc + 1];
            if (next_op->type == META_OP_STAR
                || next_op->type == META_OP_OPTIONAL) {
                stack[stack_ptr] = current_pc + 2;
                mask_stack[stack_ptr] = current_mask;
                stack_ptr += 1;
            }
        }
    }
    return;
}

static void
tdfa_compute_core_transitions(MetaOp *ops, NfaStateSet *current_closed_set,
                              int32 c, NfaStateSet *next_core_set) {
    for (int32 i = 0; i < META_PC_WORDS; i += 1) {
        next_core_set->bits[i] = 0;
    }

    for (int32 i = 0; i < META_MAX_OPS; i += 1) {
        if ((current_closed_set->bits[i / 32] & (1u << (i % 32))) != 0) {
            MetaOp *op;
            int32 match;

            op = &ops[i];
            match = 0;

            if (op->type == META_OP_LITERAL) {
                if (c == op->value) {
                    match = 1;
                }
            } else if (op->type == META_OP_CLASS) {
                if (c >= 0 && c < META_ALPHABET_SIZE) {
                    if ((op->mask[c / 32] & (1u << (c % 32))) != 0) {
                        match = 1;
                    }
                }
            } else if (op->type == META_OP_ANY) {
                if (c != '\0') {
                    match = 1;
                }
            }

            if (match) {
                MetaOp *next_op;

                next_op = &ops[i + 1];
                if (next_op->type == META_OP_STAR
                    || next_op->type == META_OP_PLUS) {
                    next_core_set->bits[i / 32] |= (1u << (i % 32));
                    next_core_set->bits[(i + 2) / 32] |= (1u << ((i + 2) % 32));
                } else if (next_op->type == META_OP_OPTIONAL) {
                    next_core_set->bits[(i + 2) / 32] |= (1u << ((i + 2) % 32));
                } else {
                    next_core_set->bits[(i + 1) / 32] |= (1u << ((i + 1) % 32));
                }
            }
        }
    }
    return;
}

#endif /* META_LAZY_TDFA_C */
