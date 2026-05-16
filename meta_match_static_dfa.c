#if !defined(META_MATCH_STATIC_DFA)
#define META_MATCH_STATIC_DFA

static int32
try_match_static_dfa(MetaRegex *regex, uchar *input, int32 input_len,
                     int32 offset, int64 nmatch, regmatch_t pmatch[]) {
    DfaState *states;
    DfaState *current_state_ptr;
    int32 last_accept;

    (void)input_len;

    states = regex->dfa->states;
    current_state_ptr = &states[regex->dfa->start_state];
    last_accept = -1;

    for (int32 i = offset;; i += 1) {
        uchar b;
        int32 next_state_idx;
        int32 *next_table;

        b = (uchar)input[i];

        if (current_state_ptr->is_accepting) {
            last_accept = i;
        }

        if (b == '\0') {
            break;
        }

        next_table = current_state_ptr->next;
        next_state_idx = next_table[b];

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
