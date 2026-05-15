#!/bin/sh

clang-format -i *.c *.h \
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
    s/\<unsigned ([a-z]+)/u\1/g;
    s/\<uint\>/uint32/g;
    s/\<int\>/int32/g;
    s/\<ulong\>/uint64/g;
    s/\<long\>/int64/g;
    s/\<size_t\>/int64/g;
    s/\<ssize_t\>/int64/g;
    s/\<ptrdiff_t\>/int64/g;
    ' "$f"
done

# git commit -a -m "apply clang-format"
