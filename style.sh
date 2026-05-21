#!/bin/sh

# shellcheck disable=SC2035 

clang-format -i *.c \
    --style=file:/home/lucas/.config/clangd/clang-format.yaml

for f in *.c *.h; do 
    sed -E -i '
    s/\<memset\>/memset64/g;
    s/\<memcpy\>/memcpy64/g;
    s/\<memmove\>/memmove64/g;
    s/\<memmem\>/memmem64/g;
    s/\<memchr\>/memchr64/g;
    s/\<strlen\>/strlen32/g;
    s/\<strncmp\>/strncmp32/g;
    s/\<strncpy\>/strncpy32/g;
    s/\<fread\>/fread64/g;
    s/\<fwrite\>/fwrite64/g;

    s/\<unsigned ([a-z]+)/u\1/g;

    s/\<uchar\>/uint8/g;
    s/\<ushort\>/uint16/g;
    s/\<short\>/int16/g;
    s/\<uint\>/uint32/g;
    s/\<int\>/int32/g;
    s/\<ulong\>/uint64/g;
    s/\<long\>/int64/g;

    s/([^(])\<size_t\>([^)])/\1int64\2/g;
    s/\<ssize_t\>/int64/g;
    s/\<ptrdiff_t\>/int64/g;

    s/(\S+) \* (\S+)/\1*\2/g;
    ' "$f"
done
