#if !defined(META_REGEX_H)
#define META_REGEX_H

#include "cbase/util.c"

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

typedef struct MetaOp {
    enum MetaOpType type;
    int32 value;
    int32 min;
    int32 max;
    uint32 mask[8];
} MetaOp;

typedef struct DfaState {
    int32 is_accepting;
    int32 next[256];
} DfaState;

typedef struct Dfa {
    int32 num_states;
    int32 start_state;
    DfaState states[256];
} Dfa;

typedef struct MetaRegex {
    char *string;
    MetaOp ops[512];
    int32 has_start_anchor;
    int32 has_end_anchor;
    int32 has_alternation;
    int32 re_nsub;
    int32 has_backref;
    int32 can_be_null;
    uint8 fastmap[32];
    Dfa *dfa;
} MetaRegex;

#define R(...) (&(MetaRegex){ .string = __VA_ARGS__ })

#endif /* META_REGEX_H */
