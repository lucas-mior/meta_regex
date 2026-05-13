#if !defined(UTF8_H)
#define UTF8_H

#include "primitives.h"

static int32
utf8_decode(char *s, int32 *consumed) {
    unsigned char *u = (unsigned char *)s;
    int32 cp = 0;
    int32 len = 0;

    if (u[0] == '\0') {
        *consumed = 0;
        return 0;
    }

    if (u[0] < 0x80) {
        cp = u[0];
        len = 1;
    } else if ((u[0] & 0xE0) == 0xC0) {
        cp = u[0] & 0x1F;
        len = 2;
    } else if ((u[0] & 0xF0) == 0xE0) {
        cp = u[0] & 0x0F;
        len = 3;
    } else if ((u[0] & 0xF8) == 0xF0) {
        cp = u[0] & 0x07;
        len = 4;
    } else {
        cp = u[0];
        len = 1;
    }

    for (int32 i = 1; i < len; i += 1) {
        if ((u[i] & 0xC0) != 0x80) {
            *consumed = 1;
            return u[0];
        }
        cp = (cp << 6) | (u[i] & 0x3F);
    }

    *consumed = len;
    return cp;
}

#endif
