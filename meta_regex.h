#if !defined(META_REGEX_H)
#define META_REGEX_H

#include "cbase/util.c"

enum MetaOpType {
    META_OP_END,
    META_OP_LITERAL,
    META_OP_ANY,
    META_OP_DIGIT,
    META_OP_ALPHA_LOWER,
    META_OP_ALPHA_UPPER,
    META_OP_GROUP_START,
    META_OP_GROUP_END
};

typedef struct MetaOp {
    enum MetaOpType type;
    int32 value;
} MetaOp;

typedef struct MetaRegex {
    char *string;
    MetaOp ops[64];
    int32 has_start_anchor;
    int32 has_end_anchor;
} MetaRegex;

#define META_REGEX(...) { .string = __VA_ARGS__ }

#endif /* META_REGEX_H */
