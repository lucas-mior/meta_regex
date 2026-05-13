#!/bin/sh -e

set -e
alias trace_on='set -x'
alias trace_off='{ set +x; } 2>/dev/null'

dir="$(readlink -f "$(dirname "$0")")"
cbase="cbase"

mkdir -p bin

CFLAGS="$CFLAGS -std=c11"
CFLAGS="$CFLAGS -Wextra -Wall"
CFLAGS="$CFLAGS -Werror"
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

CFLAGS="$CFLAGS -g $GNUSOURCE -DDEBUGGING=1"

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
fi

trace_on
ctags --kinds-C=+l+d cbase/*.c cbase/*.h ./*.h ./*.c 2> /dev/null || true
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
$CC $CPPFLAGS -O2 -flto $CFLAGS gen/main2.c -o bin/regex_test $LDFLAGS
trace_off

printf "\nRunning Tests:\n"
./bin/regex_test
    # 2>&1 | sed -E 's/ +/ /' | column -s '' -t
    # 2>&1 | sed -E 's/\[[0-9;]*[mK]//g; s/: [01]$//' | xsel -b
