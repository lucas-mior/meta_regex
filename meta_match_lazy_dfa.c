#if !defined(META_MATCH_LAZY_DFA_C)
#define META_MATCH_LAZY_DFA_C

#include <regex.h>
#include "meta.h"
#include "meta_util.c"
#include "primitives.h"

// clang-format off
static const MatcherFeatures match_features_lazy_dfa = {
    .supports = (enum MetaOpType)(
        META_OP_END
        | META_OP_LITERAL
        | META_OP_ANY
        | META_OP_CLASS
        | META_OP_GROUP_START
        | META_OP_GROUP_END
        | META_OP_STAR
        | META_OP_PLUS
        | META_OP_OPTIONAL
        | META_OP_ALTERNATION
        | META_OP_BOUNDED
        | META_OP_SPLIT
        | META_OP_JUMP
        | META_OP_WORD_START
        | META_OP_WORD_END
        | META_OP_WORD_BOUNDARY
        | META_OP_NON_WORD_BOUNDARY
    ),
    .extracts = false,
};
// clang-format on

typedef struct LazyDfaKey {
    uint32 bits[META_PC_WORDS];
    int32 prev_is_word;
} LazyDfaKey;

#define HASH_KEY_TYPE LazyDfaKey
#define HASH_KEY_FIXED_LEN 1
#define HASH_VALUE_TYPE int32
#define HASH_VALUE_FORMATTER "%d"
#define HASH_TYPE map
#define HASH_DUPLICATE_KEYS 0
#include "hash.c"

typedef struct LazyDfaState {
    int32 next[META_ALPHABET_SIZE];
    int32 accepts_before[META_ALPHABET_SIZE];
    int32 accepts_on_eof;

    /*
        Cached epsilon closures for this state.  A closure depends on the
        previous-byte wordness stored in key.prev_is_word and on the current
        byte wordness, but not on the exact current byte.
    */
    int32 closure_ready[2];
    int32 closure_accepts[2];
    NfaStateSet closure[2];

    LazyDfaKey key;
} LazyDfaState;

typedef struct LazyDfa {
    struct Hash_map state_map;
    int32 num_states;
    int32 op_count;
    int32 pc_words;
    int32 uses_context;
    int32 start_state_nw;
    int32 start_state_w;
    LazyDfaKey start_key_base;
    LazyDfaState states[META_MAX_LAZY_DFA_STATES];
} LazyDfa;

static void compute_core_transitions(MetaOp *ops,
                                     NfaStateSet *current_closed_set,
                                     int32 pc_words, int32 c,
                                     NfaStateSet *next_core_set);
static void lazy_dfa_init_state(LazyDfaState *state, LazyDfaKey key);
static void lazy_dfa_ensure_closure(MetaRegex *regex, LazyDfa *ldfa,
                                    LazyDfaState *state, int32 curr_is_word);
static void lazy_dfa_compute_closure_from_key(MetaRegex *regex, LazyDfa *ldfa,
                                              LazyDfaKey *key,
                                              NfaStateSet *set,
                                              int32 *is_accepting,
                                              int32 curr_is_word);
static void lazy_dfa_push_pc(int32 *stack, int32 *stack_ptr, int32 pc,
                             int32 op_count);
static int32 lazy_dfa_get_or_add_state(LazyDfa *ldfa, LazyDfaKey *key);
static void lazy_dfa_init_start_states(MetaRegex *regex, LazyDfa *ldfa);
static int32 lazy_dfa_uses_context(MetaRegex *regex);
static int32 lazy_dfa_op_count(MetaRegex *regex);
static int32 lazy_dfa_pc_words(int32 op_count);
static int32 lazy_dfa_ctz32(uint32 word);
static int32 lazy_dfa_set_is_empty(NfaStateSet *set, int32 pc_words);

static int32
lazy_dfa_uses_context(MetaRegex *regex) {
    enum MetaOpType context_ops = (enum MetaOpType)(
        META_OP_WORD_START | META_OP_WORD_END | META_OP_WORD_BOUNDARY
        | META_OP_NON_WORD_BOUNDARY);

    return ((regex->used_ops & context_ops) != 0);
}

static int32
lazy_dfa_op_count(MetaRegex *regex) {
    int32 count = 0;

    while (count < META_MAX_OPS) {
        if (regex->ops[count].type == META_OP_END) {
            count += 1;
            break;
        }
        count += 1;
    }

    if (count <= 0 || count > META_MAX_OPS) {
        count = META_MAX_OPS;
    }
    return count;
}

static int32
lazy_dfa_pc_words(int32 op_count) {
    int32 words = (op_count + 31) / 32;

    if (words <= 0) {
        words = 1;
    }
    if (words > META_PC_WORDS) {
        words = META_PC_WORDS;
    }
    return words;
}

static int32
lazy_dfa_ctz32(uint32 word) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctz(word);
#else
    int32 bit = 0;
    while (((word >> bit) & 1u) == 0u) {
        bit += 1;
    }
    return bit;
#endif
}

static int32
lazy_dfa_set_is_empty(NfaStateSet *set, int32 pc_words) {
    for (int32 k = 0; k < pc_words; k += 1) {
        if (set->bits[k] != 0) {
            return 0;
        }
    }
    return 1;
}

static void
lazy_dfa_init_state(LazyDfaState *state, LazyDfaKey key) {
    state->key = key;
    state->accepts_on_eof = -1;
    state->closure_ready[0] = 0;
    state->closure_ready[1] = 0;
    state->closure_accepts[0] = 0;
    state->closure_accepts[1] = 0;

    for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
        state->next[c] = 0;
        state->accepts_before[c] = 0;
    }
    return;
}

static int32
lazy_dfa_get_or_add_state(LazyDfa *ldfa, LazyDfaKey *key) {
    int32 state_id;

    if (hash_lookup_map(&ldfa->state_map, key, &state_id)) {
        return state_id;
    }

    state_id = ldfa->num_states;
    if (state_id >= META_MAX_LAZY_DFA_STATES) {
        return -1;
    }

    ldfa->num_states += 1;
    lazy_dfa_init_state(&ldfa->states[state_id], *key);
    hash_insert_map(&ldfa->state_map, key, state_id);
    return state_id;
}

static void
lazy_dfa_init_start_states(MetaRegex *regex, LazyDfa *ldfa) {
    LazyDfaKey key;
    int32 depth = 0;

    for (int32 i = 0; i < META_PC_WORDS; i += 1) {
        key.bits[i] = 0;
    }
    key.prev_is_word = 0;
    key.bits[0] = 1;

    for (int32 i = 0; regex->ops[i].type != META_OP_END; i += 1) {
        if (regex->ops[i].type == META_OP_GROUP_START) {
            depth += 1;
        } else if (regex->ops[i].type == META_OP_GROUP_END) {
            depth -= 1;
        } else if (regex->ops[i].type == META_OP_ALTERNATION && depth == 0) {
            key.bits[(i + 1) / 32] |= (1u << ((i + 1) % 32));
        }
    }

    ldfa->start_key_base = key;

    key.prev_is_word = 0;
    ldfa->start_state_nw = lazy_dfa_get_or_add_state(ldfa, &key);

    if (ldfa->uses_context) {
        key.prev_is_word = 1;
        ldfa->start_state_w = lazy_dfa_get_or_add_state(ldfa, &key);
    } else {
        ldfa->start_state_w = ldfa->start_state_nw;
    }
    return;
}

static void
lazy_dfa_push_pc(int32 *stack, int32 *stack_ptr, int32 pc, int32 op_count) {
    if (pc >= 0 && pc < op_count && *stack_ptr < META_MAX_OPS) {
        stack[*stack_ptr] = pc;
        *stack_ptr += 1;
    }
    return;
}

static void
lazy_dfa_compute_closure_from_key(MetaRegex *regex, LazyDfa *ldfa,
                                  LazyDfaKey *key, NfaStateSet *set,
                                  int32 *is_accepting,
                                  int32 curr_is_word) {
    MetaOp *ops = regex->ops;
    int32 stack[META_MAX_OPS];
    int32 stack_ptr = 0;

    *is_accepting = 0;

    for (int32 k = 0; k < ldfa->pc_words; k += 1) {
        set->bits[k] = 0;
    }

    for (int32 w = 0; w < ldfa->pc_words; w += 1) {
        uint32 word = key->bits[w];

        while (word != 0) {
            int32 bit = lazy_dfa_ctz32(word);
            int32 pc = w*32 + bit;

            lazy_dfa_push_pc(stack, &stack_ptr, pc, ldfa->op_count);
            word &= word - 1;
        }
    }

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
            lazy_dfa_push_pc(stack, &stack_ptr, current_pc + op->value,
                             ldfa->op_count);
            lazy_dfa_push_pc(stack, &stack_ptr, current_pc + op->min,
                             ldfa->op_count);
        } else if (op->type == META_OP_JUMP) {
            lazy_dfa_push_pc(stack, &stack_ptr, current_pc + op->value,
                             ldfa->op_count);
        } else if (op->type == META_OP_WORD_BOUNDARY) {
            if (key->prev_is_word != curr_is_word) {
                lazy_dfa_push_pc(stack, &stack_ptr, current_pc + 1,
                                 ldfa->op_count);
            }
        } else if (op->type == META_OP_NON_WORD_BOUNDARY) {
            if (key->prev_is_word == curr_is_word) {
                lazy_dfa_push_pc(stack, &stack_ptr, current_pc + 1,
                                 ldfa->op_count);
            }
        } else if (op->type == META_OP_WORD_START) {
            if (!key->prev_is_word && curr_is_word) {
                lazy_dfa_push_pc(stack, &stack_ptr, current_pc + 1,
                                 ldfa->op_count);
            }
        } else if (op->type == META_OP_WORD_END) {
            if (key->prev_is_word && !curr_is_word) {
                lazy_dfa_push_pc(stack, &stack_ptr, current_pc + 1,
                                 ldfa->op_count);
            }
        } else if (op->type == META_OP_GROUP_START) {
            int32 depth = 0;

            lazy_dfa_push_pc(stack, &stack_ptr, current_pc + 1,
                             ldfa->op_count);
            for (int32 i = current_pc + 1; i < ldfa->op_count
                 && ops[i].type != META_OP_END; i += 1) {
                if (ops[i].type == META_OP_GROUP_START) {
                    depth += 1;
                } else if (ops[i].type == META_OP_GROUP_END) {
                    if (depth == 0) {
                        break;
                    }
                    depth -= 1;
                } else if (ops[i].type == META_OP_ALTERNATION && depth == 0) {
                    lazy_dfa_push_pc(stack, &stack_ptr, i + 1,
                                     ldfa->op_count);
                }
            }
        } else if (op->type == META_OP_ALTERNATION) {
            int32 depth = 0;
            int32 i = current_pc + 1;

            while (i < ldfa->op_count && ops[i].type != META_OP_END) {
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
            lazy_dfa_push_pc(stack, &stack_ptr, i, ldfa->op_count);
        } else if (op->type == META_OP_GROUP_END) {
            lazy_dfa_push_pc(stack, &stack_ptr, current_pc + 1,
                             ldfa->op_count);
        }

        if (op->type == META_OP_LITERAL || op->type == META_OP_CLASS
            || op->type == META_OP_ANY) {
            MetaOp *next_op = &ops[current_pc + 1];

            if (next_op->type == META_OP_STAR
                || next_op->type == META_OP_OPTIONAL) {
                lazy_dfa_push_pc(stack, &stack_ptr, current_pc + 2,
                                 ldfa->op_count);
            }
        }
    }
    return;
}

static void
lazy_dfa_ensure_closure(MetaRegex *regex, LazyDfa *ldfa, LazyDfaState *state,
                        int32 curr_is_word) {
    NfaStateSet *closed_set;
    int32 is_acc = 0;

    curr_is_word = ldfa->uses_context ? !!curr_is_word : 0;
    if (state->closure_ready[curr_is_word]) {
        return;
    }

    closed_set = &state->closure[curr_is_word];
    lazy_dfa_compute_closure_from_key(regex, ldfa, &state->key, closed_set,
                                      &is_acc, curr_is_word);

    state->closure_accepts[curr_is_word] = is_acc;
    state->closure_ready[curr_is_word] = 1;
    return;
}

static int32
match_lazy_dfa(MetaRegex *regex, uint8 *input, int32 input_len, int32 offset,
               regmatch_t *pmatch, int64 pmatch_len) {
    LazyDfa *ldfa = regex->lazy_dfa;
    int32 current_state_id;
    int32 last_accept = -1;
    int32 prev_is_word;
    (void)input_len;

    if (ldfa == NULL) {
        ldfa = malloc2(SIZEOF(*ldfa));
        ldfa->state_map = hash_create_map_value(256, "dfa");
        ldfa->num_states = 1;
        ldfa->op_count = lazy_dfa_op_count(regex);
        ldfa->pc_words = lazy_dfa_pc_words(ldfa->op_count);
        ldfa->uses_context = lazy_dfa_uses_context(regex);
        ldfa->start_state_nw = -1;
        ldfa->start_state_w = -1;
        lazy_dfa_init_start_states(regex, ldfa);
        regex->lazy_dfa = ldfa;
    }

    if (ldfa->uses_context && offset > 0) {
        prev_is_word = word_table[input[offset - 1]];
    } else {
        prev_is_word = 0;
    }

    current_state_id = prev_is_word ? ldfa->start_state_w : ldfa->start_state_nw;
    if (current_state_id < 0) {
        return -1;
    }

    for (int32 i = offset;; i += 1) {
        uint8 b = input[i];

        if (b == '\0') {
            if (current_state_id > 0
                && current_state_id < META_MAX_LAZY_DFA_STATES) {
                LazyDfaState *state = &ldfa->states[current_state_id];

                if (state->accepts_on_eof == -1) {
                    lazy_dfa_ensure_closure(regex, ldfa, state, 0);
                    state->accepts_on_eof = state->closure_accepts[0];
                }
                if (state->accepts_on_eof) {
                    last_accept = i;
                }
            }
            break;
        }

        if (current_state_id > 0
            && current_state_id < META_MAX_LAZY_DFA_STATES) {
            LazyDfaState *state = &ldfa->states[current_state_id];
            int32 next_id = state->next[b];

            if (next_id == 0) {
                int32 curr_is_word = ldfa->uses_context ? word_table[b] : 0;
                NfaStateSet next_core;

                lazy_dfa_ensure_closure(regex, ldfa, state, curr_is_word);
                state->accepts_before[b] = state->closure_accepts[curr_is_word];

                compute_core_transitions(regex->ops,
                                         &state->closure[curr_is_word],
                                         ldfa->pc_words, b, &next_core);

                if (lazy_dfa_set_is_empty(&next_core, ldfa->pc_words)) {
                    next_id = -1;
                } else {
                    LazyDfaKey next_key;

                    for (int32 k = 0; k < META_PC_WORDS; k += 1) {
                        next_key.bits[k] = 0;
                    }
                    for (int32 k = 0; k < ldfa->pc_words; k += 1) {
                        next_key.bits[k] = next_core.bits[k];
                    }
                    next_key.prev_is_word = ldfa->uses_context ? curr_is_word : 0;

                    next_id = lazy_dfa_get_or_add_state(ldfa, &next_key);
                }
                state->next[b] = next_id;
            }

            if (state->accepts_before[b]) {
                last_accept = i;
            }
            current_state_id = next_id;

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
            }
            return 0;
        }
    }

    return -1;
}

static void
compute_core_transitions(MetaOp *ops, NfaStateSet *current_closed_set,
                         int32 pc_words, int32 c, NfaStateSet *next_core_set) {
    for (int32 i = 0; i < pc_words; i += 1) {
        next_core_set->bits[i] = 0;
    }

    for (int32 w = 0; w < pc_words; w += 1) {
        uint32 word = current_closed_set->bits[w];

        while (word != 0) {
            int32 bit = lazy_dfa_ctz32(word);
            int32 i = w*32 + bit;
            MetaOp *op = &ops[i];
            int32 match = 0;

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
                MetaOp *next_op = &ops[i + 1];

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

            word &= word - 1;
        }
    }
    return;
}

#endif /* META_MATCH_LAZY_DFA_C */
