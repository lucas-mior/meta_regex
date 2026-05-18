#if !defined(META_MATCH_C)
#define META_MATCH_C

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

#define USE_DFA_THRESHOLD 128

static MatcherFeatures matchers[] = {
    [MATCH_ALGO_BTNFA] = match_btnfa_features,
    [MATCH_ALGO_LAZY_DFA] = match_lazy_dfa_features,
    [MATCH_ALGO_STATIC_DFA] = match_static_dfa_features,
};

static int32
meta_regex_match(MetaRegex *regex, uint8 *input, int32 input_len,
                 regmatch_t *pmatch, int32 pmatch_len,
                 enum MatchAlgorithm enabled) {
    enum MatchAlgorithm algorithm = MATCH_ALGO_BTNFA;
    int32 result;
    int32 needs_extraction;

    if (regex == NULL) {
        return REG_NOMATCH;
    }

    if (pmatch != NULL) {
        for (int32 k = 0; k < pmatch_len; k += 1) {
            pmatch[k].rm_so = -1;
            pmatch[k].rm_eo = -1;
        }
    }

    needs_extraction = (regex->re_nsub > 0 && pmatch_len > 1);

    if (input_len >= USE_DFA_THRESHOLD) {
        if ((enabled & MATCH_ALGO_STATIC_DFA) && regex->static_dfa) {
            if (!needs_extraction || matchers[MATCH_ALGO_STATIC_DFA].extracts) {
                if ((regex->used_ops
                     & ~matchers[MATCH_ALGO_STATIC_DFA].supports)
                    == 0) {
                    algorithm = MATCH_ALGO_STATIC_DFA;
                }
            }
        }

        if (algorithm == MATCH_ALGO_BTNFA && (enabled & MATCH_ALGO_LAZY_DFA)) {
            if (!needs_extraction || matchers[MATCH_ALGO_LAZY_DFA].extracts) {
                if ((regex->used_ops & ~matchers[MATCH_ALGO_LAZY_DFA].supports)
                    == 0) {
                    algorithm = MATCH_ALGO_LAZY_DFA;
                }
            }
        }
    }

    switch (algorithm) {
    case MATCH_ALGO_BTNFA: {
        if (regex->has_start_anchor) {
            result
                = match_btnfa(regex, input, input_len, 0, pmatch, pmatch_len);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uint8 b = input[j];
            int32 bit_match = (regex->fastmap[b >> 3] & (1 << (b % 8)));

            if (bit_match || regex->can_be_null) {
                result = match_btnfa(regex, input, input_len, j, pmatch,
                                     pmatch_len);
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
            result = match_lazy_dfa(regex, input, input_len, 0, pmatch,
                                    pmatch_len);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uint8 b = input[j];
            int32 bit_match = (regex->fastmap[b >> 3] & (1 << (b % 8)));

            if (bit_match || regex->can_be_null) {
                result = match_lazy_dfa(regex, input, input_len, j, pmatch,
                                        pmatch_len);
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
            result = match_static_dfa(regex, input, input_len, 0, pmatch,
                                      pmatch_len);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uint8 b = input[j];
            int32 bit_match = (regex->fastmap[b >> 3] & (1 << (b % 8)));

            if (bit_match || regex->can_be_null) {
                result = match_static_dfa(regex, input, input_len, j, pmatch,
                                          pmatch_len);
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

#endif /* META_MATCH_C */
