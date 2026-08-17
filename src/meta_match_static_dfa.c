#if !defined(META_MATCH_STATIC_DFA)
#define META_MATCH_STATIC_DFA

#include "cbase.h"

#include <regex.h>
#include "meta_regex.h"
#include "meta_util.c"

// clang-format off
static const MatcherFeatures match_features_static_dfa = {
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

#if 0
static int32
#else
static inline __attribute__((always_inline)) int32
#endif
match_static_dfa(MetaRegex *regex, uint8 *input, int32 input_len, int32 offset,
                 MetaRegexMatch *pmatch, int32 pmatch_len) {
    StaticDfa *dfa = regex->static_dfa;
    StaticDfaState *states = dfa->states;
    StaticDfaState *current_state_ptr;
    int32 start_idx = dfa->start_state_nw;
    int32 last_accept;

    (void)input_len;

    /*
        If the generator proved that word context is irrelevant, it emits the
        same start state for both previous-byte contexts. In that common case,
        avoid reading input[offset - 1] and avoid the word_table lookup.
    */
    if (dfa->start_state_w != start_idx && offset > 0) {
        uint8 prev_b = input[offset - 1];
        if (word_table[prev_b]) {
            start_idx = dfa->start_state_w;
        }
    }

    current_state_ptr = &states[start_idx];
    last_accept = -1;

    for (int32 i = offset;; i += 1) {
        uint8 b = input[i];
        int32 *next_table;
        int32 next_state_idx;

        if (current_state_ptr->is_accepting[b]) {
            last_accept = i;
        }

        if (b == '\0') {
            break;
        }

        next_table = current_state_ptr->next;
        if ((next_state_idx = next_table[b]) == 0) {
            break;
        }

        current_state_ptr = &states[next_state_idx];
    }

    if (last_accept >= 0) {
        if (!regex->has_end_anchor || input[last_accept] == '\0') {
            if (pmatch && pmatch_len > 0) {
                pmatch[0].rm_so = offset;
                pmatch[0].rm_eo = last_accept;
            }
            return 0;
        }
    }

    return -1;
}

#endif /* META_MATCH_STATIC_DFA */
