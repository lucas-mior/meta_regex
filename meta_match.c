#if !defined(META_REGEX_MATCH_C)
#define META_REGEX_MATCH_C

#include <regex.h>
#include <string.h>

#include "meta.h"
#include "util.c"
#include "meta_util.c"
#include "meta_match_lazy_dfa.c"
#include "meta_match_btnfa.c"
#include "meta_match_static_dfa.c"

#define ENUM_PREFIX_ MATCH_ALGO_
#define ENUM_NAME MatchAlgorithm
#define ENUM_BITFLAGS 1
#define ENUM_FIELDS \
    X(BTNFA) \
    X(LAZY_DFA) \
    X(STATIC_DFA)
#include "xenums.c"

#if !defined(ENABLE_LAZY_DFA)
#define ENABLE_LAZY_DFA 1
#endif
#if !defined(ENABLE_STATIC_DFA)
#define ENABLE_STATIC_DFA 1
#endif

#define USE_DFA_THRESHOLD 1

static int32
meta_regex_match(MetaRegex *regex, uint8 *input, int32 input_len, int64 nmatch,
                 regmatch_t pmatch[]) {
    enum MatchAlgorithm enabled = MATCH_ALGO_BTNFA;
    enum MatchAlgorithm algorithm = MATCH_ALGO_BTNFA;
    int32 result = 0;

    if (regex == NULL) {
        return REG_NOMATCH;
    }

    if (pmatch != NULL) {
        for (int64 k = 0; k < nmatch; k += 1) {
            pmatch[k].rm_so = -1;
            pmatch[k].rm_eo = -1;
        }
    }

    if (ENABLE_LAZY_DFA) {
        enabled |= MATCH_ALGO_LAZY_DFA;
    }
    if (ENABLE_STATIC_DFA) {
        enabled |= MATCH_ALGO_STATIC_DFA;
    }

    if (!regex->has_backref && input_len >= USE_DFA_THRESHOLD
        && !(regex->re_nsub > 0 && nmatch > 1)) {
        if ((enabled & MATCH_ALGO_STATIC_DFA)
            && regex->dfa != NULL) {
            int32 has_unsupported = 0;
            for (int32 i = 0; regex->ops[i].type != META_OP_END; i += 1) {
                if (regex->ops[i].type == META_OP_WORD_BOUNDARY
                    || regex->ops[i].type == META_OP_WORD_START
                    || regex->ops[i].type == META_OP_WORD_END
                    || regex->ops[i].type == META_OP_NON_WORD_BOUNDARY) {
                    has_unsupported = 1;
                    break;
                }
            }
            if (!has_unsupported) {
                algorithm = MATCH_ALGO_STATIC_DFA;
            }
        }

        if (algorithm == MATCH_ALGO_BTNFA
            && (enabled & MATCH_ALGO_LAZY_DFA)) {
            algorithm = MATCH_ALGO_LAZY_DFA;
        }
    }

    switch (algorithm) {
    case MATCH_ALGO_BTNFA: {
        if (regex->has_start_anchor) {
            result
                = try_match_btnfa(regex, input, input_len, 0, nmatch, pmatch);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uint8 b = input[j];
            int32 bit_match = (regex->fastmap[b >> 3] & (1 << (b % 8)));

            if (bit_match || regex->can_be_null) {
                result = try_match_btnfa(regex, input, input_len, j, nmatch,
                                         pmatch);
                if (result == 0) {
                    return 0;
                }
            }

            if (b == '\0') {
                break;
            }
        }
        break;
    }
    case MATCH_ALGO_LAZY_DFA: {
        if (regex->has_start_anchor) {
            result = try_match_lazy_dfa(regex, input, input_len, 0, nmatch,
                                        pmatch);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uint8 b = input[j];
            int32 bit_match = (regex->fastmap[b >> 3] & (1 << (b % 8)));

            if (bit_match || regex->can_be_null) {
                result = try_match_lazy_dfa(regex, input, input_len, j, nmatch,
                                            pmatch);
                if (result == 0) {
                    return 0;
                }
            }

            if (b == '\0') {
                break;
            }
        }
        break;
    }
    case MATCH_ALGO_STATIC_DFA: {
        if (regex->has_start_anchor) {
            result = try_match_static_dfa(regex, input, input_len, 0, nmatch,
                                          pmatch);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uint8 b = input[j];
            int32 bit_match = (regex->fastmap[b >> 3] & (1 << (b % 8)));

            if (bit_match || regex->can_be_null) {
                result = try_match_static_dfa(regex, input, input_len, j,
                                              nmatch, pmatch);
                if (result == 0) {
                    return 0;
                }
            }

            if (b == '\0') {
                break;
            }
        }
        break;
    }
    case MATCH_ALGO_LAST:
    case MATCH_ALGO_NONE:
    default: {
        error("Undefined matching algorithm.\n");
        exit(EXIT_FAILURE);
    }
    }

    return REG_NOMATCH;
}

#endif /* META_REGEX_MATCH_C */
