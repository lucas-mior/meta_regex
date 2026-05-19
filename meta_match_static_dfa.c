#if !defined(META_MATCH_STATIC_DFA)
#define META_MATCH_STATIC_DFA

#include <regex.h>
#include "primitives.h"
#include "meta.h"

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

static int32
match_static_dfa(MetaRegex *regex, uint8 *input, int32 input_len, int32 offset,
                 regmatch_t *pmatch, int32 pmatch_len) {
    StaticDfaState *states = regex->static_dfa->states;
    int32 start_idx = regex->static_dfa->start_state_nw;

    if (offset > 0) {
        uint8 prev_b = input[offset - 1];
        if ((prev_b >= 'a' && prev_b <= 'z') || (prev_b >= 'A' && prev_b <= 'Z')
            || (prev_b >= '0' && prev_b <= '9') || prev_b == '_') {
            start_idx = regex->static_dfa->start_state_w;
        }
    }

    StaticDfaState *current_state_ptr = &states[start_idx];
    int32 last_accept = -1;

    (void)input_len;

    for (int32 i = offset;; i += 1) {
        uint8 b = input[i];

        if (current_state_ptr->is_accepting[b]) {
            last_accept = i;
        }

        if (b == '\0') {
            break;
        }

        int32 *next_table = current_state_ptr->next;
        int32 next_state_idx = next_table[b];

        if (next_state_idx == 0) {
            break;
        }

        current_state_ptr = &states[next_state_idx];
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

#endif /* META_MATCH_STATIC_DFA */
