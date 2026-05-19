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
#include "meta_match_tnfa.c"
#include "meta_match_tdfa.c"

#define ENUM_PREFIX_ MATCHER_
#define ENUM_NAME Matcher
#define ENUM_BITFLAGS 1
#define ENUM_FIELDS \
    X(BTNFA) \
    X(TNFA) \
    X(LAZY_DFA) \
    X(STATIC_DFA) \
    X(TDFA)
#include "xenums.c"

#define HEURISTIC_DFA_MIN_INPUT_LEN 1

static MatcherFeatures matchers[] = {
    [MATCHER_BTNFA] = match_features_btnfa,
    [MATCHER_TNFA] = match_features_tnfa,
    [MATCHER_TDFA] = match_features_tdfa,
    [MATCHER_LAZY_DFA] = match_features_lazy_dfa,
    [MATCHER_STATIC_DFA] = match_features_static_dfa,
};

static int32
meta_regex_match_with_algorithm(MetaRegex *regex, uint8 *input, int32 input_len,
                                regmatch_t *pmatch, int32 pmatch_len,
                                enum Matcher matcher) {
    int32 result;

    switch (matcher) {
    case MATCHER_BTNFA: {
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
    case MATCHER_TDFA: {
        if (regex->has_start_anchor) {
            result = match_tdfa(regex, input, input_len, 0, pmatch, pmatch_len);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uint8 b = input[j];
            int32 bit_match = (regex->fastmap[b >> 3] & (1 << (b % 8)));

            if (bit_match || regex->can_be_null) {
                result = match_tdfa(regex, input, input_len, j, pmatch,
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
    case MATCHER_TNFA: {
        if (regex->has_start_anchor) {
            result = match_tnfa(regex, input, input_len, 0, pmatch, pmatch_len);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uint8 b = input[j];
            int32 bit_match = (regex->fastmap[b >> 3] & (1 << (b % 8)));

            if (bit_match || regex->can_be_null) {
                result = match_tnfa(regex, input, input_len, j, pmatch,
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
    case MATCHER_LAZY_DFA: {
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
    case MATCHER_STATIC_DFA: {
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
    case MATCHER_LAST:
    case MATCHER_NONE:
    default: {
        error("Undefined matching matcher.\n");
        exit(EXIT_FAILURE);
    }
    }

    return REG_NOMATCH;
}

static enum Matcher
meta_choose_matcher(MetaRegex *regex, int32 input_len, bool needs_extraction,
                    enum Matcher matchers_enabled) {
    enum Matcher matcher = MATCHER_BTNFA;

    if ((matchers_enabled & MATCHER_TDFA) && regex->tdfa) {
        if ((regex->used_ops & ~matchers[MATCHER_TDFA].supports) == 0) {
            matcher = MATCHER_TDFA;
        }
    }

    if (matcher == MATCHER_BTNFA && needs_extraction
        && (matchers_enabled & MATCHER_TNFA) && regex->tnfa) {
        if ((regex->used_ops & ~matchers[MATCHER_TNFA].supports) == 0) {
            matcher = MATCHER_TNFA;
        }
    }

    if (!needs_extraction && input_len >= HEURISTIC_DFA_MIN_INPUT_LEN) {
        if ((matchers_enabled & MATCHER_STATIC_DFA) && regex->static_dfa) {
            if ((regex->used_ops & ~matchers[MATCHER_STATIC_DFA].supports)
                == 0) {
                matcher = MATCHER_STATIC_DFA;
            }
        }

        if (matcher == MATCHER_BTNFA && (matchers_enabled & MATCHER_LAZY_DFA)) {
            if ((regex->used_ops & ~matchers[MATCHER_LAZY_DFA].supports) == 0) {
                matcher = MATCHER_LAZY_DFA;
            }
        }
    }

    return matcher;
}

static int32
meta_regex_match(MetaRegex *regex, uint8 *input, int32 input_len,
                 regmatch_t *pmatch, int32 pmatch_len,
                 enum Matcher matchers_enabled) {
    enum Matcher matcher;
    int32 needs_extraction;

    if (regex == NULL) {
        return REG_NOMATCH;
    }

    if (pmatch) {
        for (int32 k = 0; k < pmatch_len; k += 1) {
            pmatch[k].rm_so = -1;
            pmatch[k].rm_eo = -1;
        }
    }

    needs_extraction = (regex->re_nsub > 0 && pmatch_len > 1);
    matcher = meta_choose_matcher(regex, input_len, needs_extraction,
                                  matchers_enabled);

    return meta_regex_match_with_algorithm(regex, input, input_len, pmatch,
                                           pmatch_len, matcher);
}

#endif /* META_MATCH_C */
