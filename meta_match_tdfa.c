#if !defined(META_MATCH_TDFA)
#define META_MATCH_TDFA

typedef struct TdfaThread {
    int32 pc;
    int32 tags[64];
} TdfaThread;

static int32
meta_match_tdfa(MetaRegex *regex, char *input, int32 input_len, int32 *ovector,
                int32 ovecsize) {
    if (regex == NULL) {
        return 0;
    }
    if (input == NULL) {
        return 0;
    }

    if (regex->tdfa_nfa != NULL) {
        int32 matched = 0;
        int32 best_tags[64] = {0};
        int32 max_states = regex->tdfa_nfa_count;
        TdfaThread *eval_threads;
        TdfaThread *next_threads;
        TdfaThread *stack;
        int32 *thread_gen;
        int32 next_count = 0;
        int32 gen = 0;

        for (int32 i = 0; i < 64; i += 1) {
            best_tags[i] = -1;
        }

        if (max_states < 2048) {
            max_states = 2048;
        }

        eval_threads = malloc2(max_states * sizeof(TdfaThread));
        next_threads = malloc2(max_states * sizeof(TdfaThread));
        stack = malloc2(max_states * 4 * sizeof(TdfaThread));
        thread_gen = malloc2(max_states * sizeof(int32));

        for (int32 i = 0; i < max_states; i += 1) {
            thread_gen[i] = 0;
        }

        for (int32 idx = 0; idx <= input_len; idx += 1) {
            int32 eval_count = 0;
            int32 stack_ptr = 0;

            gen += 1;

            if (!regex->has_start_anchor) {
                stack[stack_ptr].pc = regex->tdfa_nfa_start;
                for (int32 k = 0; k < regex->num_tags && k < 64; k += 1) {
                    stack[stack_ptr].tags[k] = -1;
                }
                stack_ptr += 1;
            } else if (idx == 0) {
                stack[stack_ptr].pc = regex->tdfa_nfa_start;
                for (int32 k = 0; k < regex->num_tags && k < 64; k += 1) {
                    stack[stack_ptr].tags[k] = -1;
                }
                stack_ptr += 1;
            }

            for (int32 i = next_count - 1; i >= 0; i -= 1) {
                stack[stack_ptr].pc = next_threads[i].pc;
                for (int32 k = 0; k < regex->num_tags && k < 64; k += 1) {
                    stack[stack_ptr].tags[k] = next_threads[i].tags[k];
                }
                stack_ptr += 1;
            }
            next_count = 0;

            while (stack_ptr > 0) {
                int32 pc;
                int32 current_tags[64] = {0};
                MetaNfaState *s;

                stack_ptr -= 1;
                pc = stack[stack_ptr].pc;
                
                for (int32 k = 0; k < regex->num_tags && k < 64; k += 1) {
                    current_tags[k] = stack[stack_ptr].tags[k];
                }

                if (thread_gen[pc] == gen) {
                    continue;
                }
                thread_gen[pc] = gen;

                s = &regex->tdfa_nfa[pc];

                if (s->type == META_NFA_LITERAL || s->type == META_NFA_CLASS 
                    || s->type == META_NFA_ANY) {
                    eval_threads[eval_count].pc = pc;
                    for (int32 k = 0; k < regex->num_tags && k < 64; k += 1) {
                        eval_threads[eval_count].tags[k] = current_tags[k];
                    }
                    eval_count += 1;
                } else if (s->type == META_NFA_MATCH) {
                    matched = 1;
                    for (int32 k = 0; k < regex->num_tags && k < 64; k += 1) {
                        best_tags[k] = current_tags[k];
                    }
                    stack_ptr = 0;
                } else if (s->type == META_NFA_SPLIT) {
                    stack[stack_ptr].pc = s->next2;
                    for (int32 k = 0; k < regex->num_tags && k < 64; k += 1) {
                        stack[stack_ptr].tags[k] = current_tags[k];
                    }
                    stack_ptr += 1;

                    stack[stack_ptr].pc = s->next1;
                    for (int32 k = 0; k < regex->num_tags && k < 64; k += 1) {
                        stack[stack_ptr].tags[k] = current_tags[k];
                    }
                    stack_ptr += 1;
                } else if (s->type == META_NFA_SAVE) {
                    stack[stack_ptr].pc = s->next1;
                    for (int32 k = 0; k < regex->num_tags && k < 64; k += 1) {
                        stack[stack_ptr].tags[k] = current_tags[k];
                    }
                    if (s->value >= 0 && s->value < 64) {
                        stack[stack_ptr].tags[s->value] = idx;
                    }
                    stack_ptr += 1;
                } else if (s->type == META_NFA_EMPTY) {
                    stack[stack_ptr].pc = s->next1;
                    for (int32 k = 0; k < regex->num_tags && k < 64; k += 1) {
                        stack[stack_ptr].tags[k] = current_tags[k];
                    }
                    stack_ptr += 1;
                } else if (s->type == META_NFA_WORD_BOUNDARY 
                           || s->type == META_NFA_NON_WORD_BOUNDARY 
                           || s->type == META_NFA_WORD_START 
                           || s->type == META_NFA_WORD_END) {
                    int32 prev_c = -1;
                    int32 curr_c = -1;
                    int32 prev_is_word = 0;
                    int32 curr_is_word = 0;
                    int32 boundary_match = 0;

                    if (idx > 0) {
                        prev_c = input[idx - 1];
                    }
                    if (idx < input_len) {
                        curr_c = input[idx];
                    }

                    if ((prev_c >= 'a' && prev_c <= 'z') 
                        || (prev_c >= 'A' && prev_c <= 'Z') 
                        || (prev_c >= '0' && prev_c <= '9') 
                        || prev_c == '_') {
                        prev_is_word = 1;
                    }
                    
                    if ((curr_c >= 'a' && curr_c <= 'z') 
                        || (curr_c >= 'A' && curr_c <= 'Z') 
                        || (curr_c >= '0' && curr_c <= '9') 
                        || curr_c == '_') {
                        curr_is_word = 1;
                    }

                    if (s->type == META_NFA_WORD_BOUNDARY) {
                        if (prev_is_word != curr_is_word) {
                            boundary_match = 1;
                        }
                    } else if (s->type == META_NFA_NON_WORD_BOUNDARY) {
                        if (prev_is_word == curr_is_word) {
                            boundary_match = 1;
                        }
                    } else if (s->type == META_NFA_WORD_START) {
                        if (!prev_is_word) {
                            if (curr_is_word) {
                                boundary_match = 1;
                            }
                        }
                    } else if (s->type == META_NFA_WORD_END) {
                        if (prev_is_word) {
                            if (!curr_is_word) {
                                boundary_match = 1;
                            }
                        }
                    }

                    if (boundary_match) {
                        stack[stack_ptr].pc = s->next1;
                        for (int32 k = 0; k < regex->num_tags && k < 64; k += 1) {
                            stack[stack_ptr].tags[k] = current_tags[k];
                        }
                        stack_ptr += 1;
                    }
                }
            }

            if (matched) {
                if (eval_count == 0) {
                    if (next_count == 0) {
                        break;
                    }
                }
            }

            if (idx < input_len) {
                int32 c = (uchar)input[idx];
                
                for (int32 i = 0; i < eval_count; i += 1) {
                    int32 pc = eval_threads[i].pc;
                    MetaNfaState *s = &regex->tdfa_nfa[pc];
                    int32 char_match = 0;

                    if (s->type == META_NFA_LITERAL) {
                        if (s->value == c) {
                            char_match = 1;
                        }
                    } else if (s->type == META_NFA_CLASS) {
                        int32 mask_idx = c / 32;
                        int32 mask_bit = c % 32;
                        
                        if (s->mask[mask_idx] & (1u << mask_bit)) {
                            char_match = 1;
                        }
                    } else if (s->type == META_NFA_ANY) {
                        char_match = 1;
                    }

                    if (char_match) {
                        next_threads[next_count].pc = s->next1;
                        for (int32 k = 0; k < regex->num_tags && k < 64; k += 1) {
                            next_threads[next_count].tags[k] = eval_threads[i].tags[k];
                        }
                        next_count += 1;
                    }
                }
            }
        }

        free2(eval_threads, max_states * sizeof(TdfaThread));
        free2(next_threads, max_states * sizeof(TdfaThread));
        free2(stack, max_states * 4 * sizeof(TdfaThread));
        free2(thread_gen, max_states * sizeof(int32));

        if (matched) {
            if (ovector != NULL) {
                for (int32 i = 0; i < regex->num_tags && i < ovecsize; i += 1) {
                    ovector[i] = best_tags[i];
                }
            }
            return 1;
        }
        
        return 0;
    }
    
    // Add fallback routine block here if you still want standard 
    // engine or classic DFA operation when tdfa_nfa is null.
    return 0;
}

#endif /* META_MATCH_TDFA */
