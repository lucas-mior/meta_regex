#!/bin/sh -e

set -e
alias trace_on='set -x'
alias trace_off='{ set +x; } 2>/dev/null'

dir="$(readlink -f "$(dirname "$0")")"
cbase="cbase"

mkdir -p bin

target="${1:-build}"

CFLAGS="$CFLAGS -std=c11"
CFLAGS="$CFLAGS -Wextra -Wall"
# CFLAGS="$CFLAGS -Werror"
CFLAGS="$CFLAGS -Wno-unused-macros -Wno-unused-function"
CFLAGS="$CFLAGS -Wno-unknown-pragmas"
CFLAGS="$CFLAGS -Wfatal-errors"
CFLAGS="$CFLAGS -Wno-gnu-union-cast"
CFLAGS="$CFLAGS -Wno-missing-field-initializers"

CPPFLAGS="$CPPFLAGS -D_DEFAULT_SOURCE"
CPPFLAGS="$CPPFLAGS -I "$dir/$cbase" -I "$dir""
LDFLAGS="$LDFLAGS -lmagic -lm"

OS=$(uname -a)

if echo "$OS" | grep -q "Linux"; then
    if echo "$OS" | grep -q "GNU"; then
        GNUSOURCE="-D_GNU_SOURCE"
    fi
fi

CC=${CC:-cc}

case "$target" in
debug)
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=1 $GNUSOURCE"
    CFLAGS="$CFLAGS -g3"
    ;;
build)
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=1 $GNUSOURCE"
    CFLAGS="$CFLAGS -g -O2 -flto"
    ;;
callgrind)
    CFLAGS="$CFLAGS -Wno-unused-variable"
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=1 $GNUSOURCE"
    CPPFLAGS="$CPPFLAGS -DBENCHMARK=1"
    CFLAGS="$CFLAGS -g3 -O2 -flto"
    ;;
esac

if [ "$CC" = "clang" ]; then
    CFLAGS="$CFLAGS -Weverything"
    CFLAGS="$CFLAGS -Wno-unsafe-buffer-usage"
    CFLAGS="$CFLAGS -Wno-format-nonliteral"
    CFLAGS="$CFLAGS -Wno-format-pedantic"
    CFLAGS="$CFLAGS -Wno-pre-c11-compat"
    CFLAGS="$CFLAGS -Wno-disabled-macro-expansion"
    CFLAGS="$CFLAGS -Wno-c++-keyword"
    CFLAGS="$CFLAGS -Wno-covered-switch-default"
    CFLAGS="$CFLAGS -Wno-implicit-void-ptr-cast"
    CFLAGS="$CFLAGS -Wno-cast-qual"
    CFLAGS="$CFLAGS -Wno-constant-logical-operand"
    CFLAGS="$CFLAGS -Wno-cast-function-type-strict"
    CFLAGS="$CFLAGS -Wno-float-equal"
    CFLAGS="$CFLAGS -Wno-padded"
    CFLAGS="$CFLAGS -Wno-declaration-after-statement"
    CFLAGS="$CFLAGS -Wno-c++-compat"
fi

trace_on
ctags --kinds-C=+l+d \
    cbase/*.c cbase/*.h ./*.h ./*.c posix/*.c posix/*.h \
    2> /dev/null || true
vtags.sed tags > .tags.vim       2> /dev/null || true
trace_off

printf "\nBuilding preprocessor...\n"
trace_on
$CC $CPPFLAGS -O2 -flto $CFLAGS meta_preproc.c -o bin/meta_preproc $LDFLAGS
trace_off

printf "\nPreprocessing main.c...\n"
trace_on
./bin/meta_preproc main.c             > gen/main2.c
./bin/meta_preproc meta_tests_array.h > gen/meta_tests_array2.h
trace_off

printf "\nBuilding target program...\n"
trace_on
$CC $CPPFLAGS $CFLAGS gen/main2.c -o bin/regex_test $LDFLAGS
trace_off

case "$target" in
build)
    ./bin/regex_test
    ;;
debug)
    gdb ./bin/regex_test -ex 'break exit' -ex 'run'
    ;;
callgrind)
    valgrind --tool=callgrind bin/regex_test
    ;;
esac
