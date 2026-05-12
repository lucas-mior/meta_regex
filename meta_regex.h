#if !defined(META_REGEX_H)
#define META_REGEX_H

#include "cbase/util.c"

enum MetaRegexType {
    META_REGEX_DIGIT
};

typedef struct MetaRegex {
    enum MetaRegexType type;
} MetaRegex;

#define META_REGEX(...) __VA_ARGS__

#endif /* META_REGEX_H */
