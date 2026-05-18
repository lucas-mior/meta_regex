#if !defined(META_REGEX_H)
#define META_REGEX_H

#include "cbase/util.c"

#define META_ALPHABET_SIZE 256
#define META_MAX_DFA_STATES 256
#define META_MAX_OPS 512
#define META_FASTMAP_SIZE 32
#define META_CHAR_BITMASK_WORDS 8
#define META_MAX_LAZY_DFA_STATES 2048
#define META_PC_WORDS (META_MAX_OPS / 32)

#define ENUM_PREFIX_ META_OP_
#define ENUM_NAME MetaOpType
#define ENUM_BITFLAGS 1
#define ENUM_FIELDS \
    X(END) \
    X(LITERAL) \
    X(ANY) \
    X(CLASS) \
    X(GROUP_START) \
    X(GROUP_END) \
    X(STAR) \
    X(PLUS) \
    X(OPTIONAL) \
    X(ALTERNATION) \
    X(BOUNDED) \
    X(SPLIT) \
    X(JUMP) \
    X(WORD_START) \
    X(WORD_END) \
    X(WORD_BOUNDARY) \
    X(NON_WORD_BOUNDARY) \
    X(BACKREF)
#include "xenums.c"

typedef struct MetaOp {
    enum MetaOpType type;
    int32 value;
    int32 min;
    int32 max;
    uint32 mask[META_CHAR_BITMASK_WORDS];
} MetaOp;

typedef struct StaticDfaState {
    int32 is_accepting;
    int32 next[META_ALPHABET_SIZE];
} StaticDfaState;

typedef struct StaticDfa {
    int32 num_states;
    int32 start_state;
    StaticDfaState *states;
} StaticDfa;

typedef struct NfaStateSet {
    uint32 bits[META_PC_WORDS];
} NfaStateSet;

typedef struct LazyDfa LazyDfa;

typedef struct MetaRegex {
    char *string;
    MetaOp ops[META_MAX_OPS];
    int32 has_start_anchor;
    int32 has_end_anchor;
    int32 has_alternation;
    int32 re_nsub;
    int32 has_backref;
    int32 can_be_null;
    uint8 fastmap[META_FASTMAP_SIZE];
    StaticDfa *static_dfa;
    LazyDfa *lazy_dfa;
} MetaRegex;

#define R(...) (&(MetaRegex){ .string = __VA_ARGS__ })

#endif /* META_REGEX_H */
