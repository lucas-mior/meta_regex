#if !defined(META_REGEX_H)
#define META_REGEX_H

#include "cbase/util.c"

#define META_ALPHABET_SIZE 256
#define META_MAX_DFA_STATES 256
#define META_MAX_OPS 512
#define META_FASTMAP_SIZE 32
#define META_CHAR_BITMASK_WORDS 8
#define META_MAX_LAZY_DFA_STATES 2048
#define META_MAX_NFA_STATES 1024
#define META_PC_WORDS (META_MAX_OPS / 32)

enum MetaOpType {
    META_OP_END,
    META_OP_LITERAL,
    META_OP_ANY,
    META_OP_CLASS,
    META_OP_GROUP_START,
    META_OP_GROUP_END,
    META_OP_STAR,
    META_OP_PLUS,
    META_OP_OPTIONAL,
    META_OP_ALTERNATION,
    META_OP_BOUNDED,
    META_OP_SPLIT,
    META_OP_JUMP,
    META_OP_WORD_START,
    META_OP_WORD_END,
    META_OP_WORD_BOUNDARY,
    META_OP_NON_WORD_BOUNDARY,
    META_OP_BACKREF
};

enum MetaNfaStateType {
    META_NFA_MATCH,
    META_NFA_LITERAL,
    META_NFA_CLASS,
    META_NFA_ANY,
    META_NFA_SPLIT,
    META_NFA_EMPTY,
    META_NFA_SAVE,
    META_NFA_WORD_BOUNDARY,
    META_NFA_NON_WORD_BOUNDARY,
    META_NFA_WORD_START,
    META_NFA_WORD_END
};

typedef struct MetaOp {
    enum MetaOpType type;
    int32 value;
    int32 min;
    int32 max;
    uint32 mask[META_CHAR_BITMASK_WORDS];
} MetaOp;

typedef struct MetaNfaState {
    int32 type;
    int32 value;
    uint32 mask[META_CHAR_BITMASK_WORDS];
    int32 next1;
    int32 next2;
} MetaNfaState;

typedef struct DfaState {
    int32 is_accepting;
    int32 next[META_ALPHABET_SIZE];
} DfaState;

typedef struct Dfa {
    int32 num_states;
    int32 start_state;
    DfaState states[META_MAX_DFA_STATES];
} Dfa;

typedef struct NfaStateSet {
    uint32 bits[META_PC_WORDS];
} NfaStateSet;

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
    int32 num_tags;
    int32 tdfa_nfa_start;
    int32 tdfa_nfa_count;
    MetaNfaState *tdfa_nfa;
    Dfa *dfa;
    void *lazy_dfa;
} MetaRegex;

#define R(...) (&(MetaRegex){ .string = __VA_ARGS__ })

#endif /* META_REGEX_H */
