#if !defined(META_UTIL_C)
#define META_UTIL_C

static int32
is_word_char(uchar c) {
    int32 match;

    match = 0;
    if (c >= 'a' && c <= 'z') {
        match = 1;
    } else if (c >= 'A' && c <= 'Z') {
        match = 1;
    } else if (c >= '0' && c <= '9') {
        match = 1;
    } else if (c == '_') {
        match = 1;
    }

    return match;
}

static int32
is_word_char2(int32 c) {
    if (c >= 'a' && c <= 'z') {
        return 1;
    }
    if (c >= 'A' && c <= 'Z') {
        return 1;
    }
    if (c >= '0' && c <= '9') {
        return 1;
    }
    if (c == '_') {
        return 1;
    }
    return 0;
}

#endif
