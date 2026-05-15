#!/bin/sh

clang-format -i *.c *.h \
    --style=file:/home/lucas/.config/clangd/clang-format.yaml

for f in *.c *.h; do 
    sed -E '
    s/\<memset\>memset64/g;
    s/\<memcpy\>memcpy64/g;
    s/\<memmove\>memmove64/g;
    s/\<memmem\>memmem64/g;
    s/\<memchr\>memchr64/g;
    s/\<strlen\>strlen32/g;
    s/\<strncmp\>strncmp32/g;
    s/\<unsigned ([a-z]+)\>/u\1/g;
    ' "$f"
done

git commit -a -m "apply clang-format"
