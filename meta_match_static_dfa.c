#if !defined(META_MATCH_STATIC_DFA)
#define META_MATCH_STATIC_DFA

static int32
try_match_static_dfa(MetaRegex *regex, uint8 *input, int32 input_len,
                     int32 offset, int64 nmatch, regmatch_t pmatch[]) {
    StaticDfaState *states = regex->static_dfa->states;
    StaticDfaState *current_state_ptr = &states[regex->static_dfa->start_state];
    int32 last_accept = -1;

    (void)input_len;

    for (int32 i = offset;; i += 1) {
        uint8 b = input[i];

        if (current_state_ptr->is_accepting) {
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
            if (pmatch != NULL && nmatch > 0) {
                pmatch[0].rm_so = offset;
                pmatch[0].rm_eo = last_accept;
            }
            return 0;
        }
    }

    return -1;
}

#endif /* META_MATCH_STATIC_DFA */
