#if !defined(META_REGEX_H)
#define META_REGEX_H

#include "cbase/util.c"

enum MetaOpType {
    META_OP_END,
    META_OP_LITERAL,
    META_OP_ANY,
    META_OP_DIGIT,
    META_OP_ALPHA_LOWER,
    META_OP_ALPHA_UPPER
};

typedef struct MetaOp {
    enum MetaOpType type;
    char value;
} MetaOp;

typedef struct MetaRegex {
    MetaOp ops[32];
    int32 has_start_anchor;
    int32 has_end_anchor;
} MetaRegex;

#define META_REGEX(...) __VA_ARGS__

#endif /* META_REGEX_H */
