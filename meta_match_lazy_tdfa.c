#if !defined(META_MATCH_LAZY_TDFA_C)
#define META_MATCH_LAZY_TDFA_C

#include "primitives.h"
#include "meta.h"
#include "meta_util.c"
#include <regex.h>

static const MatcherFeatures match_features_lazy_tdfa = {
    .supports = (enum MetaOpType)(
        META_OP_END | META_OP_LITERAL | META_OP_ANY | META_OP_CLASS
        | META_OP_GROUP_START | META_OP_GROUP_END | META_OP_STAR | META_OP_PLUS
        | META_OP_OPTIONAL | META_OP_ALTERNATION | META_OP_BOUNDED
        | META_OP_SPLIT | META_OP_JUMP | META_OP_WORD_START | META_OP_WORD_END
        | META_OP_WORD_BOUNDARY | META_OP_NON_WORD_BOUNDARY),
    .extracts = true,
};

typedef struct TdfaRegCmd {
    int32 dest;
    int32 src;
    int32 set_pos;
} TdfaRegCmd;

typedef struct LazyTdfaKey {
    uint32 bits[META_PC_WORDS];
    int32 prev_is_word;
    int32 tags[META_MAX_OPS][32];
} LazyTdfaKey;

#define HASH_KEY_TYPE LazyTdfaKey
#define HASH_KEY_FIXED_LEN 0
#define HASH_VALUE_TYPE int32
#define HASH_VALUE_FORMATTER "%d"
#define HASH_TYPE tmap
#define HASH_DUPLICATE_KEYS 0
#include "hash.c"

typedef struct LazyTdfaState {
    int32 next[META_ALPHABET_SIZE];
    int32 accepts_before[META_ALPHABET_SIZE];
    int32 accepts_on_eof;
    TdfaRegCmd cmds_next[META_ALPHABET_SIZE][16];
    int32 num_cmds_next[META_ALPHABET_SIZE];
    TdfaRegCmd cmds_eof[16];
    int32 num_cmds_eof;
    LazyTdfaKey key;
} LazyTdfaState;

typedef struct LazyTdfa {
    struct Hash_tmap *state_tmap;
    int32 num_states;
    int32 next_reg_id;
    LazyTdfaState states[META_MAX_LAZY_DFA_STATES];
} LazyTdfa;

static void add_tdfa_epsilon_closure(MetaOp *ops, int32 pc, NfaStateSet *set,
                                     int32 *is_accepting, int32 prev_is_word,
                                     int32 curr_is_word, LazyTdfaKey *key,
                                     int32 *next_reg_id);
static void compute_tdfa_core_transitions(
    MetaOp *ops, NfaStateSet *current_closed_set, int32 c,
    NfaStateSet *next_core_set, LazyTdfaKey *current_key, LazyTdfaKey *next_key,
    TdfaRegCmd *cmds, int32 *num_cmds, int32 *next_reg_id);

static int32
match_lazy_tdfa(MetaRegex *regex, uint8 *input, int32 input_len, int32 offset,
                regmatch_t *pmatch, int64 pmatch_len) {
    LazyTdfa *ldfa;
    int32 current_state_id;
    int32 last_accept;
    int32 prev_is_word;
    int32 active_regs[4096];
    int32 best_regs[32];

    ldfa = regex->lazy_tdfa;
    last_accept = -1;
    prev_is_word = (offset > 0) ? is_word_char(input[offset - 1]) : 0;
    (void)input_len;

    for (int32 i = 0; i < 4096; i += 1) {
        active_regs[i] = -1;
    }
    for (int32 i = 0; i < 32; i += 1) {
        best_regs[i] = -1;
    }

    if (ldfa == NULL) {
        ldfa = malloc2(SIZEOF(*ldfa));
        ldfa->state_tmap = hash_create_tmap(META_MAX_LAZY_DFA_STATES, "tdfa");
        ldfa->num_states = 1;
        ldfa->next_reg_id = 32;
        regex->lazy_tdfa = ldfa;
    }

    {
        LazyTdfaKey start_key;
        int32 depth;

        depth = 0;

        for (int32 i = 0; i < META_PC_WORDS; i += 1) {
            start_key.bits[i] = 0;
        }
        for (int32 i = 0; i < META_MAX_OPS; i += 1) {
            for (int32 j = 0; j < 32; j += 1) {
                start_key.tags[i][j] = j;
            }
        }

        start_key.prev_is_word = prev_is_word;
        start_key.bits[0] = 1;

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
                    ldfa->states[current_state_id].num_cmds_next[c] = 0;
                }
                ldfa->states[current_state_id].num_cmds_eof = 0;
                ASSERT(hash_insert_tmap(ldfa->state_tmap, &start_key,
                                        SIZEOF(start_key), current_state_id));
            } else {
                return -1;
            }
        }
    }

    for (int32 i = offset;; i += 1) {
        uint8 b;

        b = input[i];

        if (b == '\0') {
            if (current_state_id > 0
                && current_state_id < META_MAX_LAZY_DFA_STATES) {
                LazyTdfaState *state;

                state = &ldfa->states[current_state_id];

                if (state->accepts_on_eof == -1) {
                    NfaStateSet closed_set;
                    int32 is_acc;

                    is_acc = 0;

                    for (int32 k = 0; k < META_PC_WORDS; k += 1) {
                        closed_set.bits[k] = 0;
                    }

                    for (int32 k = 0; k < META_MAX_OPS; k += 1) {
                        if ((state->key.bits[k / 32] & (1u << (k % 32))) != 0) {
                            add_tdfa_epsilon_closure(
                                regex->ops, k, &closed_set, &is_acc,
                                state->key.prev_is_word, 0, &state->key,
                                &ldfa->next_reg_id);
                        }
                    }
                    state->accepts_on_eof = is_acc;
                }
                if (state->accepts_on_eof) {
                    last_accept = i;
                    for (int32 cmd_i = 0; cmd_i < state->num_cmds_eof;
                         cmd_i += 1) {
                        TdfaRegCmd cmd;

                        cmd = state->cmds_eof[cmd_i];
                        if (cmd.set_pos) {
                            active_regs[cmd.dest] = i;
                        } else {
                            active_regs[cmd.dest] = active_regs[cmd.src];
                        }
                    }
                    for (int32 reg_i = 0; reg_i < 32; reg_i += 1) {
                        best_regs[reg_i] = active_regs[reg_i];
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

                curr_is_word = is_word_char(b);
                is_acc = 0;
                set_is_empty = 1;

                for (int32 k = 0; k < META_PC_WORDS; k += 1) {
                    closed_set.bits[k] = 0;
                }

                for (int32 k = 0; k < META_MAX_OPS; k += 1) {
                    if ((state->key.bits[k / 32] & (1u << (k % 32))) != 0) {
                        add_tdfa_epsilon_closure(
                            regex->ops, k, &closed_set, &is_acc,
                            state->key.prev_is_word, curr_is_word, &state->key,
                            &ldfa->next_reg_id);
                    }
                }
                state->accepts_before[b] = is_acc;

                {
                    LazyTdfaKey next_key;

                    for (int32 k = 0; k < META_PC_WORDS; k += 1) {
                        next_key.bits[k] = 0;
                    }
                    for (int32 r1 = 0; r1 < META_MAX_OPS; r1 += 1) {
                        for (int32 r2 = 0; r2 < 32; r2 += 1) {
                            next_key.tags[r1][r2] = -1;
                        }
                    }

                    compute_tdfa_core_transitions(
                        regex->ops, &closed_set, b, &next_core, &state->key,
                        &next_key, state->cmds_next[b],
                        &state->num_cmds_next[b], &ldfa->next_reg_id);

                    for (int32 k = 0; k < META_PC_WORDS; k += 1) {
                        if (next_core.bits[k] != 0) {
                            set_is_empty = 0;
                            break;
                        }
                    }

                    if (set_is_empty) {
                        state->next[b] = -1;
                    } else {
                        int32 next_id;

                        next_id = 0;

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
                                for (int32 c = 0; c < META_ALPHABET_SIZE;
                                     c += 1) {
                                    ldfa->states[next_id].next[c] = 0;
                                    ldfa->states[next_id].accepts_before[c] = 0;
                                    ldfa->states[next_id].num_cmds_next[c] = 0;
                                }
                                ldfa->states[next_id].num_cmds_eof = 0;
                                ASSERT(hash_insert_tmap(
                                    ldfa->state_tmap, &next_key,
                                    SIZEOF(next_key), next_id));
                            } else {
                                next_id = -1;
                            }
                        }
                        state->next[b] = next_id;
                    }
                }
            }

            for (int32 cmd_i = 0; cmd_i < state->num_cmds_next[b]; cmd_i += 1) {
                TdfaRegCmd cmd;

                cmd = state->cmds_next[b][cmd_i];
                if (cmd.set_pos) {
                    active_regs[cmd.dest] = i;
                } else {
                    active_regs[cmd.dest] = active_regs[cmd.src];
                }
            }

            if (state->accepts_before[b]) {
                last_accept = i;
                for (int32 reg_i = 0; reg_i < 32; reg_i += 1) {
                    best_regs[reg_i] = active_regs[reg_i];
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
            if (pmatch != NULL && pmatch_len > 0) {
                pmatch[0].rm_so = offset;
                pmatch[0].rm_eo = last_accept;

                for (int32 k = 1; k < pmatch_len && k < 16; k += 1) {
                    pmatch[k].rm_so = best_regs[k*2];
                    pmatch[k].rm_eo = best_regs[k*2 + 1];
                }
            }
            return 0;
        }
    }

    return -1;
}

static void
add_tdfa_epsilon_closure(MetaOp *ops, int32 pc, NfaStateSet *set,
                         int32 *is_accepting, int32 prev_is_word,
                         int32 curr_is_word, LazyTdfaKey *key,
                         int32 *next_reg_id) {
    /* TODO: Implemented epsilon closure with tag tracking */
    return;
}

static void
compute_tdfa_core_transitions(MetaOp *ops, NfaStateSet *current_closed_set,
                              int32 c, NfaStateSet *next_core_set,
                              LazyTdfaKey *current_key, LazyTdfaKey *next_key,
                              TdfaRegCmd *cmds, int32 *num_cmds,
                              int32 *next_reg_id) {
    /* TODO: Implemented core transitions with tag updating and resolving */
    return;
}

#endif /* META_MATCH_LAZY_TDFA_C */
