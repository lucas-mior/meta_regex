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
    META_OP_BOUNDED
};

typedef struct MetaOp {
    enum MetaOpType type;
    int32 value;
    int32 min;
    int32 max;
    unsigned int mask[8];
    int32 high_codepoints[16];
} MetaOp;

typedef struct MetaRegex {
    char *string;
    MetaOp ops[64];
    int32 has_start_anchor;
    int32 has_end_anchor;
} MetaRegex;

#define R(...) { .string = __VA_ARGS__ }

#endif /* META_REGEX_H */
