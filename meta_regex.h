#include "cbase/util.c"

enum MetaRegexType {
    META_REGEX_DIGIT
};

typedef struct MetaRegex {
    enum MetaRegexType type;
} MetaRegex;

#define META_REGEX(...) MetaRegex regex_meta = {0};
