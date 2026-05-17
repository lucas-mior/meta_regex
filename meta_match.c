#if !defined(META_REGEX_MATCH_C)
#define META_REGEX_MATCH_C

#include <regex.h>
#include <string.h>

#include "meta.h"
#include "util.c"
#include "meta_util.c"
#include "meta_match_lazy_dfa.c"
#include "meta_match_tdfa.c"
#include "meta_match_btnfa.c"
#include "meta_match_static_dfa.c"

#if !defined(ALGO_TDFA)
#define ALGO_TDFA 1
#endif
#if !defined(ALGO_LAZY_DFA)
#define ALGO_LAZY_DFA 0
#endif
#if !defined(ALGO_STATIC_DFA)
#define ALGO_STATIC_DFA 0
#endif

#if !ALGO_STATIC_DFA && !ALGO_LAZY_DFA && !ALGO_TDFA
#define ALGO_BTNFA_ALWAYS 1
#endif

#if (ALGO_LAZY_DFA && ALGO_STATIC_DFA) || (ALGO_LAZY_DFA && ALGO_TDFA) || (ALGO_STATIC_DFA && ALGO_TDFA)
#error "Cannot define more than one of ALGO_TDFA, ALGO_LAZY_DFA, and ALGO_STATIC_DFA"
#endif

#define ENUM_PREFIX_ MATCH_ALGO_
#define ENUM_NAME MatchAlgorithm
#define ENUM_BITFLAGS 0
#define ENUM_FIELDS \
    X(BTNFA) \
    X(TDFA) \
    X(LAZY_DFA) \
    X(STATIC_DFA)
#include "xenums.c"

#define USE_DFA_THRESHOLD 256

static int32
meta_regex_match(MetaRegex *regex, uchar *input, int32 input_len,
                 int64 nmatch, regmatch_t pmatch[]) {
    enum MatchAlgorithm algorithm = MATCH_ALGO_BTNFA;
    int32 result;

    if (regex == NULL) {
        return REG_NOMATCH;
    }

    if (pmatch != NULL) {
        for (int64 k = 0; k < nmatch; k += 1) {
            pmatch[k].rm_so = -1;
            pmatch[k].rm_eo = -1;
        }
    }

#if defined(ALGO_BTNFA_ALWAYS)
    algorithm = MATCH_ALGO_BTNFA;
#else
    if (regex->has_backref) {
        algorithm = MATCH_ALGO_BTNFA;
    } else {
#if ALGO_TDFA
        if (regex->tdfa_nfa == NULL) {
            algorithm = MATCH_ALGO_BTNFA;
        } else {
            algorithm = MATCH_ALGO_TDFA;
        }
#else
        int32 has_unsupported;

        has_unsupported = 0;
        for (int32 i = 0; regex->ops[i].type != META_OP_END; i += 1) {
            if (regex->ops[i].type == META_OP_WORD_BOUNDARY
                || regex->ops[i].type == META_OP_WORD_START
                || regex->ops[i].type == META_OP_WORD_END
                || regex->ops[i].type == META_OP_NON_WORD_BOUNDARY) {
                has_unsupported = 1;
                break;
            }
        }

        if (has_unsupported || (regex->re_nsub > 0 && nmatch > 1)
            || input_len < USE_DFA_THRESHOLD) {
            algorithm = MATCH_ALGO_BTNFA;
        } else {
#if ALGO_LAZY_DFA
            algorithm = MATCH_ALGO_LAZY_DFA;
#elif ALGO_STATIC_DFA
            if (regex->dfa == NULL) {
                algorithm = MATCH_ALGO_BTNFA;
            } else {
                algorithm = MATCH_ALGO_STATIC_DFA;
            }
#endif
        }
#endif
    }
#endif

    error("algorithm: %s\n", MATCH_ALGO_str(algorithm));

    if (algorithm == MATCH_ALGO_BTNFA) {
        if (regex->has_start_anchor) {
            result = try_match_btnfa(regex, input, input_len, 0, nmatch,
                                     pmatch);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uchar b;
            int32 bit_match;

            b = (uchar)input[j];
            bit_match = (regex->fastmap[b >> 3] & (1 << (b & 7)));

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
    } else if (algorithm == MATCH_ALGO_TDFA) {
        int32 ovector[64];
        int32 ovecsize;

        ovecsize = 64;
        for (int32 i = 0; i < ovecsize; i += 1) {
            ovector[i] = -1;
        }

        result = meta_match_tdfa(regex, (char *)input, input_len, ovector,
                                 ovecsize);

        if (result == 1) {
            if (pmatch != NULL) {
                for (int64 k = 0; k < nmatch; k += 1) {
                    if ((k * 2 + 1) < regex->num_tags && (k * 2 + 1) < ovecsize) {
                        pmatch[k].rm_so = ovector[k * 2];
                        pmatch[k].rm_eo = ovector[k * 2 + 1];
                    }
                }
            }
            return 0;
        }
    } else if (algorithm == MATCH_ALGO_LAZY_DFA) {
        if (regex->has_start_anchor) {
            result = try_match_lazy_dfa(regex, input, input_len, 0, nmatch,
                                        pmatch);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uchar b;
            int32 bit_match;

            b = (uchar)input[j];
            bit_match = (regex->fastmap[b >> 3] & (1 << (b & 7)));

            if (bit_match || regex->can_be_null) {
                result = try_match_lazy_dfa(regex, input, input_len, j,
                                            nmatch, pmatch);
                if (result == 0) {
                    return 0;
                }
            }

            if (b == '\0') {
                break;
            }
        }
    } else if (algorithm == MATCH_ALGO_STATIC_DFA) {
        if (regex->has_start_anchor) {
            result = try_match_static_dfa(regex, input, input_len, 0, nmatch,
                                          pmatch);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uchar b;
            int32 bit_match;

            b = (uchar)input[j];
            bit_match = (regex->fastmap[b >> 3] & (1 << (b & 7)));

            if (bit_match || regex->can_be_null) {
                result = try_match_static_dfa(regex, input, input_len, j, nmatch,
                                              pmatch);
                if (result == 0) {
                    return 0;
                }
            }

            if (b == '\0') {
                break;
            }
        }
    } else {
        error("Undefined matching algorithm.\n");
        exit(EXIT_FAILURE);
    }

    return REG_NOMATCH;
}

#endif /* META_REGEX_MATCH_C */
