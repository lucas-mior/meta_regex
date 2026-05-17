#if !defined(META_MATCH_TDFA)
#define META_MATCH_TDFA

#include "primitives.h"
#include "meta.h"
#include <regex.h>
#include <string.h>

typedef struct LazyTdfaKey {
    int16 num_pcs;
    int16 prev_is_word;
    int16 pcs[META_MAX_OPS];
} LazyTdfaKey;

#define HASH_KEY_TYPE LazyTdfaKey
#define HASH_KEY_FIXED_LEN 0
#define HASH_VALUE_TYPE int32
#define HASH_VALUE_FORMATTER "%d"
#define HASH_TYPE tdfa_map
#define HASH_DUPLICATE_KEYS 0
#include "hash.c"

typedef struct LazyTdfaTransition {
    int16 src_idx[META_MAX_OPS];
    uint64 saved_tags_mask[META_MAX_OPS];
} LazyTdfaTransition;

typedef struct LazyTdfaState {
    int32 next[META_ALPHABET_SIZE];
    int16 transition_idx[META_ALPHABET_SIZE];
    int32 num_pcs;
    int16 pcs[META_MAX_OPS];
} LazyTdfaState;

#define META_MAX_LAZY_TDFA_STATES 512
#define META_MAX_LAZY_TDFA_TRANSITIONS 4096

typedef struct LazyTdfa {
    struct Hash_tdfa_map *state_tdfa_map;
    int32 num_states;
    int32 num_transitions;
    LazyTdfaState states[META_MAX_LAZY_TDFA_STATES];
    LazyTdfaTransition transitions_pool[META_MAX_LAZY_TDFA_TRANSITIONS];

    int32 current_tags[META_MAX_OPS][64];
    int32 next_tags[META_MAX_OPS][64];
    int32 init_tags[META_MAX_OPS][64];
} LazyTdfa;

typedef struct EpsilonStackItem {
    int32 pc;
    uint64 mask;
} EpsilonStackItem;

static void
run_tdfa_epsilon_closure(MetaOp *ops, int32 start_pc, int32 src_idx,
                         int32 curr_is_word, int32 prev_is_word,
                         int32 *dest_pcs, int32 *dest_src_idx,
                         uint64 *dest_tag_mask, int32 *num_dest_pcs,
                         int32 *visited) {
    EpsilonStackItem stack[META_MAX_OPS];
    int32 stack_ptr;

    stack_ptr = 0;
    stack[stack_ptr].pc = start_pc;
    stack[stack_ptr].mask = 0;
    stack_ptr += 1;

    while (stack_ptr > 0) {
        int32 curr_pc;
        uint64 curr_mask;
        MetaOp *cop;

        stack_ptr -= 1;
        curr_pc = stack[stack_ptr].pc;
        curr_mask = stack[stack_ptr].mask;

        if (visited[curr_pc]) {
            continue;
        }
        visited[curr_pc] = 1;

        cop = &ops[curr_pc];

        if (cop->type == META_OP_LITERAL || cop->type == META_OP_CLASS
            || cop->type == META_OP_ANY) {
            MetaOp *next_cop;

            dest_pcs[*num_dest_pcs] = curr_pc;
            dest_src_idx[*num_dest_pcs] = src_idx;
            dest_tag_mask[*num_dest_pcs] = curr_mask;
            *num_dest_pcs += 1;

            next_cop = &ops[curr_pc + 1];
            if (next_cop->type == META_OP_STAR
                || next_cop->type == META_OP_OPTIONAL) {
                stack[stack_ptr].pc = curr_pc + 2;
                stack[stack_ptr].mask = curr_mask;
                stack_ptr += 1;
            }
            continue;
        }

        if (cop->type == META_OP_END) {
            dest_pcs[*num_dest_pcs] = curr_pc;
            dest_src_idx[*num_dest_pcs] = src_idx;
            dest_tag_mask[*num_dest_pcs] = curr_mask;
            *num_dest_pcs += 1;
            continue;
        }

        if (cop->type == META_OP_SPLIT) {
            stack[stack_ptr].pc = curr_pc + cop->min;
            stack[stack_ptr].mask = curr_mask;
            stack_ptr += 1;
            stack[stack_ptr].pc = curr_pc + cop->value;
            stack[stack_ptr].mask = curr_mask;
            stack_ptr += 1;
        } else if (cop->type == META_OP_JUMP) {
            stack[stack_ptr].pc = curr_pc + cop->value;
            stack[stack_ptr].mask = curr_mask;
            stack_ptr += 1;
        } else if (cop->type == META_OP_WORD_BOUNDARY) {
            if (prev_is_word != curr_is_word) {
                stack[stack_ptr].pc = curr_pc + 1;
                stack[stack_ptr].mask = curr_mask;
                stack_ptr += 1;
            }
        } else if (cop->type == META_OP_NON_WORD_BOUNDARY) {
            if (prev_is_word == curr_is_word) {
                stack[stack_ptr].pc = curr_pc + 1;
                stack[stack_ptr].mask = curr_mask;
                stack_ptr += 1;
            }
        } else if (cop->type == META_OP_WORD_START) {
            if (!prev_is_word && curr_is_word) {
                stack[stack_ptr].pc = curr_pc + 1;
                stack[stack_ptr].mask = curr_mask;
                stack_ptr += 1;
            }
        } else if (cop->type == META_OP_WORD_END) {
            if (prev_is_word && !curr_is_word) {
                stack[stack_ptr].pc = curr_pc + 1;
                stack[stack_ptr].mask = curr_mask;
                stack_ptr += 1;
            }
        } else if (cop->type == META_OP_GROUP_START) {
            int32 alts[256];
            int32 num_alts;
            int32 depth;

            num_alts = 0;
            depth = 0;
            for (int32 k = curr_pc + 1; ops[k].type != META_OP_END; k += 1) {
                if (ops[k].type == META_OP_GROUP_START) {
                    depth += 1;
                } else if (ops[k].type == META_OP_GROUP_END) {
                    if (depth == 0) {
                        break;
                    }
                    depth -= 1;
                } else if (ops[k].type == META_OP_ALTERNATION && depth == 0) {
                    alts[num_alts] = k + 1;
                    num_alts += 1;
                }
            }

            for (int32 k = num_alts - 1; k >= 0; k -= 1) {
                stack[stack_ptr].pc = alts[k];
                stack[stack_ptr].mask = curr_mask | (1ULL << (cop->value*2));
                stack_ptr += 1;
            }
            stack[stack_ptr].pc = curr_pc + 1;
            stack[stack_ptr].mask = curr_mask | (1ULL << (cop->value*2));
            stack_ptr += 1;
        } else if (cop->type == META_OP_ALTERNATION) {
            int32 depth;
            int32 k;

            depth = 0;
            k = curr_pc + 1;
            while (ops[k].type != META_OP_END) {
                if (ops[k].type == META_OP_GROUP_START) {
                    depth += 1;
                } else if (ops[k].type == META_OP_GROUP_END) {
                    if (depth == 0) {
                        break;
                    }
                    depth -= 1;
                }
                k += 1;
            }
            stack[stack_ptr].pc = k;
            stack[stack_ptr].mask = curr_mask;
            stack_ptr += 1;
        } else if (cop->type == META_OP_GROUP_END) {
            stack[stack_ptr].pc = curr_pc + 1;
            stack[stack_ptr].mask = curr_mask | (1ULL << (cop->value*2 + 1));
            stack_ptr += 1;
        } else {
            stack[stack_ptr].pc = curr_pc + 1;
            stack[stack_ptr].mask = curr_mask;
            stack_ptr += 1;
        }
    }
    return;
}

static int32
try_match_lazy_tdfa(MetaRegex *regex, uchar *input, int32 input_len,
                    int32 offset, int64 nmatch, regmatch_t *pmatch) {
    LazyTdfa *ldfa;
    int32 current_state_id;
    int32 last_accept;
    int32 prev_is_word;
    int32 best_tags[64];
    (void)input_len;

    ldfa = (LazyTdfa *)regex->lazy_dfa;
    if (ldfa == NULL) {
        ldfa = malloc2(SIZEOF(LazyTdfa));
        ldfa->state_tdfa_map
            = hash_create_tdfa_map(META_MAX_LAZY_TDFA_STATES, "tdfa");
        ldfa->num_states = 1;
        ldfa->num_transitions = 1;
        regex->lazy_dfa = ldfa;
    }

    for (int32 k = 0; k < 64; k += 1) {
        best_tags[k] = -1;
    }
    last_accept = -1;

    if (offset > 0) {
        prev_is_word = is_word_char2((uchar)input[offset - 1]);
    } else {
        prev_is_word = 0;
    }

    {
        LazyTdfaKey start_key;
        int32 visited[META_MAX_OPS];
        int32 dest_pcs[META_MAX_OPS];
        int32 dest_src_idx[META_MAX_OPS];
        uint64 dest_tag_mask[META_MAX_OPS];
        int32 num_dest_pcs;

        memset64(&start_key, 0, SIZEOF(start_key));
        start_key.prev_is_word = prev_is_word;

        for (int32 i = 0; i < META_MAX_OPS; i += 1) {
            for (int32 k = 0; k < 64; k += 1) {
                ldfa->init_tags[i][k] = -1;
            }
            visited[i] = 0;
        }

        num_dest_pcs = 0;
        run_tdfa_epsilon_closure(regex->ops, 0, 0, prev_is_word, prev_is_word,
                                 dest_pcs, dest_src_idx, dest_tag_mask,
                                 &num_dest_pcs, visited);

        start_key.num_pcs = num_dest_pcs;
        for (int32 i = 0; i < num_dest_pcs; i += 1) {
            start_key.pcs[i] = dest_pcs[i];
            for (int32 t = 0; t < 64; t += 1) {
                if ((dest_tag_mask[i] & (1ULL << t)) != 0) {
                    ldfa->init_tags[i][t] = offset;
                }
            }
        }

        if (!hash_lookup_tdfa_map(ldfa->state_tdfa_map, &start_key,
                                  SIZEOF(start_key), &current_state_id)) {
            current_state_id = ldfa->num_states;
            if (current_state_id < META_MAX_LAZY_TDFA_STATES) {
                ldfa->num_states += 1;
                ldfa->states[current_state_id].num_pcs = start_key.num_pcs;
                for (int32 i = 0; i < start_key.num_pcs; i += 1) {
                    ldfa->states[current_state_id].pcs[i] = start_key.pcs[i];
                }
                for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                    ldfa->states[current_state_id].next[c] = 0;
                    ldfa->states[current_state_id].transition_idx[c] = 0;
                }
                ASSERT(hash_insert_tdfa_map(ldfa->state_tdfa_map, &start_key,
                                            SIZEOF(start_key),
                                            current_state_id));
            } else {
                return -1;
            }
        }
    }

    for (int32 i = 0; i < ldfa->states[current_state_id].num_pcs; i += 1) {
        for (int32 t = 0; t < 64; t += 1) {
            ldfa->current_tags[i][t] = ldfa->init_tags[i][t];
        }
        if (regex->ops[ldfa->states[current_state_id].pcs[i]].type
            == META_OP_END) {
            last_accept = offset;
            for (int32 t = 0; t < 64; t += 1) {
                best_tags[t] = ldfa->init_tags[i][t];
            }
            break;
        }
    }

    for (int32 i = offset;; i += 1) {
        uchar b;
        LazyTdfaState *state;
        int32 next_state;

        b = (uchar)input[i];
        if (b == '\0') {
            break;
        }

        if (current_state_id <= 0
            || current_state_id >= META_MAX_LAZY_TDFA_STATES) {
            break;
        }

        state = &ldfa->states[current_state_id];
        next_state = state->next[b];

        if (next_state == 0) {
            int32 curr_is_word;
            int32 visited[META_MAX_OPS];
            int32 dest_pcs[META_MAX_OPS];
            int32 dest_src_idx[META_MAX_OPS];
            uint64 dest_tag_mask[META_MAX_OPS];
            int32 num_dest_pcs;
            LazyTdfaKey next_key;

            curr_is_word = is_word_char2(b);
            num_dest_pcs = 0;
            for (int32 k = 0; k < META_MAX_OPS; k += 1) {
                visited[k] = 0;
            }

            for (int32 src_idx = 0; src_idx < state->num_pcs; src_idx += 1) {
                int32 start_pc;
                MetaOp *op;
                int32 match;

                start_pc = state->pcs[src_idx];
                op = &regex->ops[start_pc];
                match = 0;

                if (op->type == META_OP_LITERAL) {
                    if (b == op->value) {
                        match = 1;
                    }
                } else if (op->type == META_OP_CLASS) {
                    if ((op->mask[b / 32] & (1u << (b % 32))) != 0) {
                        match = 1;
                    }
                } else if (op->type == META_OP_ANY) {
                    match = 1;
                }

                if (match) {
                    MetaOp *next_op;
                    EpsilonStackItem stack[META_MAX_OPS];
                    int32 stack_ptr;

                    next_op = &regex->ops[start_pc + 1];
                    stack_ptr = 0;

                    if (next_op->type == META_OP_STAR
                        || next_op->type == META_OP_PLUS) {
                        stack[stack_ptr].pc = start_pc + 2;
                        stack[stack_ptr].mask = 0;
                        stack_ptr += 1;
                        stack[stack_ptr].pc = start_pc;
                        stack[stack_ptr].mask = 0;
                        stack_ptr += 1;
                    } else if (next_op->type == META_OP_OPTIONAL) {
                        stack[stack_ptr].pc = start_pc + 2;
                        stack[stack_ptr].mask = 0;
                        stack_ptr += 1;
                    } else {
                        stack[stack_ptr].pc = start_pc + 1;
                        stack[stack_ptr].mask = 0;
                        stack_ptr += 1;
                    }

                    while (stack_ptr > 0) {
                        int32 p_pc;
                        stack_ptr -= 1;
                        p_pc = stack[stack_ptr].pc;
                        run_tdfa_epsilon_closure(
                            regex->ops, p_pc, src_idx, curr_is_word,
                            curr_is_word, dest_pcs, dest_src_idx, dest_tag_mask,
                            &num_dest_pcs, visited);
                    }
                }
            }

            if (num_dest_pcs == 0) {
                state->next[b] = -1;
                next_state = -1;
            } else {
                memset64(&next_key, 0, SIZEOF(next_key));
                next_key.num_pcs = num_dest_pcs;
                next_key.prev_is_word = curr_is_word;
                for (int32 k = 0; k < num_dest_pcs; k += 1) {
                    next_key.pcs[k] = dest_pcs[k];
                }

                if (!hash_lookup_tdfa_map(ldfa->state_tdfa_map, &next_key,
                                          SIZEOF(next_key), &next_state)) {
                    next_state = ldfa->num_states;
                    if (next_state < META_MAX_LAZY_TDFA_STATES) {
                        ldfa->num_states += 1;
                        ldfa->states[next_state].num_pcs = next_key.num_pcs;
                        for (int32 k = 0; k < next_key.num_pcs; k += 1) {
                            ldfa->states[next_state].pcs[k] = next_key.pcs[k];
                        }
                        for (int32 c = 0; c < META_ALPHABET_SIZE; c += 1) {
                            ldfa->states[next_state].next[c] = 0;
                            ldfa->states[next_state].transition_idx[c] = 0;
                        }
                        ASSERT(hash_insert_tdfa_map(ldfa->state_tdfa_map,
                                                    &next_key, SIZEOF(next_key),
                                                    next_state));
                    } else {
                        next_state = -1;
                    }
                }

                if (next_state != -1) {
                    int32 t_idx;
                    t_idx = ldfa->num_transitions;
                    if (t_idx < META_MAX_LAZY_TDFA_TRANSITIONS) {
                        ldfa->num_transitions += 1;
                        for (int32 k = 0; k < num_dest_pcs; k += 1) {
                            ldfa->transitions_pool[t_idx].src_idx[k]
                                = dest_src_idx[k];
                            ldfa->transitions_pool[t_idx].saved_tags_mask[k]
                                = dest_tag_mask[k];
                        }
                        state->next[b] = next_state;
                        state->transition_idx[b] = t_idx;
                    } else {
                        next_state = -1;
                        state->next[b] = -1;
                    }
                }
            }
        }

        if (next_state != -1) {
            LazyTdfaTransition *trans;
            trans = &ldfa->transitions_pool[state->transition_idx[b]];

            for (int32 k = 0; k < ldfa->states[next_state].num_pcs; k += 1) {
                int32 src;
                uint64 mask;

                src = trans->src_idx[k];
                mask = trans->saved_tags_mask[k];

                for (int32 t = 0; t < 64; t += 1) {
                    ldfa->next_tags[k][t] = ldfa->current_tags[src][t];
                    if ((mask & (1ULL << t)) != 0) {
                        ldfa->next_tags[k][t] = i + 1;
                    }
                }

                if (regex->ops[ldfa->states[next_state].pcs[k]].type
                    == META_OP_END) {
                    last_accept = i + 1;
                    for (int32 t = 0; t < 64; t += 1) {
                        best_tags[t] = ldfa->next_tags[k][t];
                    }
                }
            }

            for (int32 k = 0; k < ldfa->states[next_state].num_pcs; k += 1) {
                for (int32 t = 0; t < 64; t += 1) {
                    ldfa->current_tags[k][t] = ldfa->next_tags[k][t];
                }
            }
        }

        current_state_id = next_state;
        if (current_state_id == -1) {
            break;
        }
    }

    if (last_accept >= 0) {
        if (!regex->has_end_anchor || input[last_accept] == '\0') {
            if (pmatch != NULL && nmatch > 0) {
                pmatch[0].rm_so = offset;
                pmatch[0].rm_eo = last_accept;
                for (int64 k = 1; k < nmatch; k += 1) {
                    pmatch[k].rm_so = best_tags[k*2];
                    pmatch[k].rm_eo = best_tags[k*2 + 1];
                }
            }
            return 0;
        }
    }

    return REG_NOMATCH;
}

#endif /* META_MATCH_TDFA */
