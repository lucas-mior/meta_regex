#!/bin/sh

# shellcheck disable=SC2035 

clang-format -i *.c *.h \
    --style=file:/home/lucas/.config/clangd/clang-format.yaml

for f in *.c *.h; do 
    sed -E -i -f style.sed "$f"
done
