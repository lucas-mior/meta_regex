#if !defined(UTF8_H)
#define UTF8_H

#include <stdlib.h>
#include "primitives.h"

static int32
utf8_decode(char *s, int32 *consumed) {
    uchar *u = (uchar *)s;
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

static void
utf8_random_string(char *buffer, int32 max_bytes) {
    int32 current_byte = 0;
    while (current_byte < max_bytes - 4) {
        int32 choice = rand() % 100;
        if (choice < 25) {
            buffer[current_byte] = (char)(32 + (rand() % 95));
            current_byte += 1;
        } else if (choice < 50) {
            buffer[current_byte] = (char)(0xC2 + (rand() % 30));
            buffer[current_byte + 1] = (char)(0x80 | (rand() % 64));
            current_byte += 2;
        } else if (choice < 75) {
            char b1 = (char)(0xE0 | (rand() % 16));
            char b2 = (char)(0x80 | (rand() % 64));
            char b3 = (char)(0x80 | (rand() % 64));
            if (b1 == (char)0xE0 && b2 < (char)0xA0) {
                b2 |= (char)0xA0;
            }
            if (b1 == (char)0xED && b2 > (char)0x9F) {
                b2 &= (char)0x9F;
            }
            buffer[current_byte] = b1;
            buffer[current_byte + 1] = b2;
            buffer[current_byte + 2] = b3;
            current_byte += 3;
        } else {
            char b1 = (char)(0xF0 | (rand() % 5));
            char b2 = (char)(0x80 | (rand() % 64));
            char b3 = (char)(0x80 | (rand() % 64));
            char b4 = (char)(0x80 | (rand() % 64));
            if (b1 == (char)0xF0 && b2 < (char)0x90) {
                b2 |= (char)0x90;
            }
            if (b1 == (char)0xF4 && b2 > (char)0x8F) {
                b2 &= (char)0x8F;
            }
            buffer[current_byte] = b1;
            buffer[current_byte + 1] = b2;
            buffer[current_byte + 2] = b3;
            buffer[current_byte + 3] = b4;
            current_byte += 4;
        }
    }
    buffer[current_byte] = '\0';
}

static void
ascii_random_string(char *buffer, int32 max_bytes) {
    int32 current_byte = 0;
    while (current_byte < max_bytes - 4) {
        buffer[current_byte] = (char)(32 + (rand() % 95));
        current_byte += 1;
    }
    buffer[current_byte] = '\0';
}

#endif
