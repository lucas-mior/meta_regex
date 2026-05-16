#if !defined(META_LAZY_DFA_C)
#define META_LAZY_DFA_C

#include "primitives.h"
#include "meta.h"
#include <regex.h>

#define HASH_KEY_TYPE NfaStateSet
#define HASH_KEY_FIXED_LEN 0
#define HASH_VALUE_TYPE int32
#define HASH_VALUE_FORMATTER "%d"
#define HASH_TYPE map
#define HASH_DUPLICATE_KEYS 0
#include "hash.c"

typedef struct LazyDfa {
    struct Hash_map *state_map;
    int32 num_states;
    LazyDfaState states[META_MAX_LAZY_DFA_STATES];
} LazyDfa;

static void
add_epsilon_closure(MetaOp *ops, int32 pc, NfaStateSet *set,
                    int32 *is_accepting);
static void
compute_next_state(MetaOp *ops, NfaStateSet *current_set, int32 c,
                   NfaStateSet *next_set, int32 *is_accepting);

static int32
try_match_lazy_dfa(MetaRegex *regex, uchar *string, int32 offset, int64 nmatch,
                   regmatch_t *pmatch) {
    LazyDfa *ldfa;
    int32 current_state_id;
    int32 last_accept;

    ldfa = (LazyDfa *)regex->lazy_dfa;
    if (ldfa == NULL) {
        ldfa = malloc2(SIZEOF(LazyDfa));
        ldfa->state_map = hash_create_map(META_MAX_LAZY_DFA_STATES, "dfa");
        ldfa->num_states = 1;
        regex->lazy_dfa = ldfa;
    }

    {
        NfaStateSet start_set;
        int32 is_accepting;
        int32 depth;

        is_accepting = 0;
        for (int32 i = 0; i < META_PC_WORDS; i += 1) {
            start_set.bits[i] = 0;
        }
        add_epsilon_closure(regex->ops, 0, &start_set, &is_accepting);

        depth = 0;
        for (int32 i = 0; regex->ops[i].type != META_OP_END; i += 1) {
            if (regex->ops[i].type == META_OP_GROUP_START) {
                depth += 1;
            } else if (regex->ops[i].type == META_OP_GROUP_END) {
                depth -= 1;
            } else if (regex->ops[i].type == META_OP_ALTERNATION && depth == 0) {
                add_epsilon_closure(regex->ops, i + 1, &start_set, &is_accepting);
            }
        }

        if (!hash_lookup_map(ldfa->state_map,
                             &start_set, SIZEOF(start_set),
                             &current_state_id)) {
            current_state_id = ldfa->num_states;
            if (current_state_id < META_MAX_LAZY_DFA_STATES) {
                ldfa->num_states += 1;
                ldfa->states[current_state_id].is_accepting = is_accepting;
                for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                    ldfa->states[current_state_id].next[c] = 0;
                }
                for (int32 k = 0; k < META_PC_WORDS; k += 1) {
                    ldfa->states[current_state_id].set.bits[k] = start_set.bits[k];
                }
                ASSERT(hash_insert_map(ldfa->state_map,
                                       &start_set, SIZEOF(start_set),
                                       current_state_id));
            }
        }
    }

    last_accept = -1;

    for (int32 i = offset;; i += 1) {
        uchar b;
        int32 next_state_idx;

        if (current_state_id > 0 && current_state_id < META_MAX_LAZY_DFA_STATES) {
            if (ldfa->states[current_state_id].is_accepting) {
                last_accept = i;
            }
        }

        b = (uchar)string[i];
        if (b == '\0') {
            break;
        }

        if (current_state_id > 0 && current_state_id < META_MAX_LAZY_DFA_STATES) {
            next_state_idx = ldfa->states[current_state_id].next[b];
        } else {
            next_state_idx = 0;
        }

        if (next_state_idx == 0) {
            NfaStateSet next_set;
            int32 next_is_accepting;
            int32 set_is_empty;

            set_is_empty = 1;
            if (current_state_id > 0 && current_state_id < META_MAX_LAZY_DFA_STATES) {
                compute_next_state(regex->ops,
                                   &ldfa->states[current_state_id].set, b,
                                   &next_set, &next_is_accepting);
            } else {
                break;
            }

            for (int32 k = 0; k < META_PC_WORDS; k += 1) {
                if (next_set.bits[k] != 0) {
                    set_is_empty = 0;
                    break;
                }
            }

            if (set_is_empty) {
                break;
            }

            if (!hash_lookup_map(ldfa->state_map,
                                 &next_set, SIZEOF(next_set),
                                 &next_state_idx)) {
                next_state_idx = ldfa->num_states;
                if (next_state_idx < META_MAX_LAZY_DFA_STATES) {
                    ldfa->num_states += 1;
                    ldfa->states[next_state_idx].is_accepting = next_is_accepting;
                    for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                        ldfa->states[next_state_idx].next[c] = 0;
                    }
                    for (int32 k = 0; k < META_PC_WORDS; k += 1) {
                        ldfa->states[next_state_idx].set.bits[k] = next_set.bits[k];
                    }
                    ASSERT(hash_insert_map(ldfa->state_map,
                                           &next_set, SIZEOF(next_set),
                                           next_state_idx));
                }
            }

            if (current_state_id > 0 && current_state_id < META_MAX_LAZY_DFA_STATES) {
                ldfa->states[current_state_id].next[b] = next_state_idx;
            }
        }

        current_state_id = next_state_idx;
    }

    if (last_accept >= 0) {
        if (!regex->has_end_anchor || string[last_accept] == '\0') {
            if (pmatch != NULL && nmatch > 0) {
                pmatch[0].rm_so = offset;
                pmatch[0].rm_eo = last_accept;
            }
            return 0;
        }
    }

    return -1;
}

static void
add_epsilon_closure(MetaOp *ops, int32 pc, NfaStateSet *set,
                    int32 *is_accepting) {
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
        } else if (op->type == META_OP_GROUP_START) {
            int32 depth;

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
        } else if (op->type == META_OP_GROUP_END) {
            stack[stack_ptr] = current_pc + 1;
            stack_ptr += 1;
        }

        if (op->type == META_OP_LITERAL || op->type == META_OP_CLASS || op->type == META_OP_ANY) {
            MetaOp *next_op;

            next_op = &ops[current_pc + 1];
            if (next_op->type == META_OP_STAR || next_op->type == META_OP_OPTIONAL) {
                stack[stack_ptr] = current_pc + 2;
                stack_ptr += 1;
            }
        }
    }
    return;
}

static void
compute_next_state(MetaOp *ops, NfaStateSet *current_set, int32 c,
                   NfaStateSet *next_set, int32 *is_accepting) {
    for (int32 i = 0; i < META_PC_WORDS; i += 1) {
        next_set->bits[i] = 0;
    }
    *is_accepting = 0;

    for (int32 i = 0; i < META_MAX_OPS; i += 1) {
        if ((current_set->bits[i / 32] & (1u << (i % 32))) != 0) {
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
                if (next_op->type == META_OP_STAR || next_op->type == META_OP_PLUS) {
                    add_epsilon_closure(ops, i, next_set, is_accepting);
                    add_epsilon_closure(ops, i + 2, next_set, is_accepting);
                } else if (next_op->type == META_OP_OPTIONAL) {
                    add_epsilon_closure(ops, i + 2, next_set, is_accepting);
                } else {
                    add_epsilon_closure(ops, i + 1, next_set, is_accepting);
                }
            }
        }
    }
    return;
}

#endif /* META_LAZY_DFA_C */
