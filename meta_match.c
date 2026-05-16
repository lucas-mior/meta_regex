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

#if !defined(ALGO_LAZY_DFA)
#define ALGO_LAZY_DFA 0
#endif

#if !defined(ALGO_STATIC_DFA)
#define ALGO_STATIC_DFA 0
#endif

#if !defined(ALGO_STATIC_DFA)
#define ALGO_BTNFA_ALWAYS 1
#endif

#if ALGO_LAZY_DFA && ALGO_STATIC_DFA
#error "Cannot define both ALGO_LAZY_DFA and ALGO_STATIC_DFA"
#endif

enum MatchAlgorithm {
    MATCH_ALGO_BTNFA,
    MATCH_ALGO_LAZY_DFA,
    MATCH_ALGO_STATIC_DFA,
};

static int32 try_match_btnfa(MetaRegex *regex, uchar *string,
                             int32 offset, int64 nmatch, regmatch_t *pmatch);
static int32 try_match_dfa(MetaRegex *regex, uchar *string,
                           int32 offset, int64 nmatch, regmatch_t *pmatch);

static int32
meta_regex_match(MetaRegex *regex, uchar *string, int64 nmatch,
                 regmatch_t pmatch[]) {
    enum MatchAlgorithm algorithm;
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

        if (has_unsupported) {
            error("TODO: Word boundaries are not supported in DFA yet.\n");
        }

#if ALGO_LAZY_DFA
        algorithm = MATCH_ALGO_LAZY_DFA;
#elif ALGO_STATIC_DFA
        algorithm = MATCH_ALGO_STATIC_DFA;
#endif
    }
#endif

    /* ASSERT_EQUAL((uint)algorithm, (uint)MATCH_ALGO_BTNFA); */

    if (algorithm == MATCH_ALGO_BTNFA) {
        if (regex->has_start_anchor) {
            result = try_match_btnfa(regex, string, 0, nmatch, pmatch);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uchar b;
            int32 bit_match;

            b = (uchar)string[j];
            bit_match = (regex->fastmap[b >> 3] & (1 << (b & 7)));

            if (bit_match || regex->can_be_null) {
                result = try_match_btnfa(regex, string, j, nmatch, pmatch);
                if (result == 0) {
                    return 0;
                }
            }

            if (b == '\0') {
                break;
            }
        }
    } else if (algorithm == MATCH_ALGO_LAZY_DFA) {
        if (regex->has_start_anchor) {
            result = try_match_lazy_dfa(regex, string, 0, nmatch, pmatch);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uchar b;
            int32 bit_match;

            b = (uchar)string[j];
            bit_match = (regex->fastmap[b >> 3] & (1 << (b & 7)));

            if (bit_match || regex->can_be_null) {
                result = try_match_lazy_dfa(regex, string, j, nmatch, pmatch);
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
            result = try_match_dfa(regex, string, 0, nmatch, pmatch);
            if (result == 0) {
                return 0;
            }
            return REG_NOMATCH;
        }

        for (int32 j = 0;; j += 1) {
            uchar b;
            int32 bit_match;

            b = (uchar)string[j];
            bit_match = (regex->fastmap[b >> 3] & (1 << (b & 7)));

            if (bit_match || regex->can_be_null) {
                result = try_match_dfa(regex, string, j, nmatch, pmatch);
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
