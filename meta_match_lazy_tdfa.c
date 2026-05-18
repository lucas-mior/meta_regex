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
    int32 accepts_on_eof;
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
                                     int32 curr_is_word, TdfaCommand *cmd);
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
                for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                    ldfa->states[current_state_id].next[c] = 0;
                    ldfa->states[current_state_id].accepts_before[c] = 0;
                    for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
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
                    TdfaCommand dummy_cmd;

                    for (int32 k = 0; k < META_PC_WORDS; k += 1) {
                        closed_set.bits[k] = 0;
                    }
                    for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                        dummy_cmd.set_tag[t] = 0;
                    }
                    is_acc = 0;

                    for (int32 k = 0; k < META_MAX_OPS; k += 1) {
                        if ((state->key.bits[k / 32] & (1u << (k % 32))) != 0) {
                            tdfa_add_epsilon_closure(
                                regex->ops, k, &closed_set, &is_acc,
                                state->key.prev_is_word, 0, &dummy_cmd);
                        }
                    }
                    state->accepts_on_eof = is_acc;
                }
                if (state->accepts_on_eof) {
                    last_accept = i;
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
                TdfaCommand edge_cmd;

                curr_is_word = is_word_char_tdfa(b);
                for (int32 k = 0; k < META_PC_WORDS; k += 1) {
                    closed_set.bits[k] = 0;
                }
                for (int32 t = 0; t < META_MAX_TAGS; t += 1) {
                    edge_cmd.set_tag[t] = 0;
                }
                is_acc = 0;

                for (int32 k = 0; k < META_MAX_OPS; k += 1) {
                    if ((state->key.bits[k / 32] & (1u << (k % 32))) != 0) {
                        tdfa_add_epsilon_closure(
                            regex->ops, k, &closed_set, &is_acc,
                            state->key.prev_is_word, curr_is_word, &edge_cmd);
                    }
                }
                state->accepts_before[b] = is_acc;
                state->commands[b] = edge_cmd;

                tdfa_compute_core_transitions(regex->ops, &closed_set, b,
                                              &next_core);

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
                            for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                                ldfa->states[next_id].next[c] = 0;
                                ldfa->states[next_id].accepts_before[c] = 0;
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
                    end_tag_idx   = (k*2) - 1;

                    if (start_tag_idx < META_MAX_TAGS
                        && end_tag_idx < META_MAX_TAGS) {
                        so = best_tags[start_tag_idx];
                        eo = best_tags[end_tag_idx];
                        /*
                         * Sanitize: if only one endpoint was recorded the
                         * group was never fully matched (e.g. skipped via ?).
                         * Report both as -1 to match POSIX semantics.
                         */
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
                         int32 curr_is_word, TdfaCommand *cmd) {
    int32 stack[META_MAX_OPS];
    int32 stack_ptr;

    stack_ptr = 0;
    stack[stack_ptr] = pc;
    stack_ptr += 1;

    while (stack_ptr > 0) {
        int32 current_pc;
        MetaOp *op;

        stack_ptr -= 1;
        current_pc = stack[stack_ptr];

        if ((set->bits[current_pc / 32] & (1u << (current_pc % 32))) != 0) {
            continue;
        }

        set->bits[current_pc / 32] |= (1u << (current_pc % 32));
        op = &ops[current_pc];

        if (op->type == META_OP_END) {
            *is_accepting = 1;
        } else if (op->type == META_OP_SPLIT) {
            stack[stack_ptr] = current_pc + op->value;
            stack_ptr += 1;
            stack[stack_ptr] = current_pc + op->min;
            stack_ptr += 1;
        } else if (op->type == META_OP_JUMP) {
            stack[stack_ptr] = current_pc + op->value;
            stack_ptr += 1;
        } else if (op->type == META_OP_WORD_BOUNDARY) {
            if (prev_is_word != curr_is_word) {
                stack[stack_ptr] = current_pc + 1;
                stack_ptr += 1;
            }
        } else if (op->type == META_OP_NON_WORD_BOUNDARY) {
            if (prev_is_word == curr_is_word) {
                stack[stack_ptr] = current_pc + 1;
                stack_ptr += 1;
            }
        } else if (op->type == META_OP_WORD_START) {
            if (!prev_is_word && curr_is_word) {
                stack[stack_ptr] = current_pc + 1;
                stack_ptr += 1;
            }
        } else if (op->type == META_OP_WORD_END) {
            if (prev_is_word && !curr_is_word) {
                stack[stack_ptr] = current_pc + 1;
                stack_ptr += 1;
            }
        } else if (op->type == META_OP_GROUP_START) {
            int32 tag_idx;
            int32 depth;

            tag_idx = (op->value*2) - 2;
            if (tag_idx >= 0 && tag_idx < META_MAX_TAGS) {
                cmd->set_tag[tag_idx] = 1;
            }

            stack[stack_ptr] = current_pc + 1;
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
                    stack_ptr += 1;
                }
            }
        } else if (op->type == META_OP_GROUP_END) {
            int32 tag_idx;

            tag_idx = (op->value*2) - 1;
            if (tag_idx >= 0 && tag_idx < META_MAX_TAGS) {
                cmd->set_tag[tag_idx] = 1;
            }

            stack[stack_ptr] = current_pc + 1;
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
            stack_ptr += 1;
        }

        if (op->type == META_OP_LITERAL || op->type == META_OP_CLASS
            || op->type == META_OP_ANY) {
            MetaOp *next_op;

            next_op = &ops[current_pc + 1];
            if (next_op->type == META_OP_STAR
                || next_op->type == META_OP_OPTIONAL) {
                stack[stack_ptr] = current_pc + 2;
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
