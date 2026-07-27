#!/bin/sh -e

# shellcheck disable=SC2086

set -e
alias trace_on='set -x'
alias trace_off='{ set +x; } 2>/dev/null'

dir="$(readlink -f "$(dirname "$0")")"
cd "$dir" || exit
cbase="cbase"

mkdir -p bin gen

target="${1:-test}"

CFLAGS="$CFLAGS -std=c11"
CFLAGS="$CFLAGS -Wextra -Wall"
# CFLAGS="$CFLAGS -Werror"
CFLAGS="$CFLAGS -Wno-unused-macros"
CFLAGS="$CFLAGS -Wno-unknown-pragmas"
CFLAGS="$CFLAGS -Wfatal-errors"
CFLAGS="$CFLAGS -Wno-gnu-union-cast"
CFLAGS="$CFLAGS -Wno-missing-field-initializers"
CFLAGS="$CFLAGS -Wno-unused-variable"
CFLAGS="$CFLAGS -Wno-type-limits"

CPPFLAGS="$CPPFLAGS -D_DEFAULT_SOURCE"
CPPFLAGS="$CPPFLAGS -I${dir}/${cbase} -I $dir"
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
preprocessor|test|bench|all|check)
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=0 $GNUSOURCE"
    CFLAGS="$CFLAGS -g -O2 -flto"
    ;;
callgrind)
    CFLAGS="$CFLAGS -Wno-unused-variable"
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=0 $GNUSOURCE"
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
    CFLAGS="$CFLAGS -Wno-c++-compat"
    CFLAGS="$CFLAGS -Wno-covered-switch-default"
    CFLAGS="$CFLAGS -Wno-implicit-void-ptr-cast"
    CFLAGS="$CFLAGS -Wno-cast-qual"
    CFLAGS="$CFLAGS -Wno-constant-logical-operand"
    CFLAGS="$CFLAGS -Wno-cast-function-type-strict"
    CFLAGS="$CFLAGS -Wno-float-equal"
    CFLAGS="$CFLAGS -Wno-padded"
    CFLAGS="$CFLAGS -Wno-declaration-after-statement"
    CFLAGS="$CFLAGS -Wno-c23-extensions"
    CFLAGS="$CFLAGS -Wno-assign-enum"
    CFLAGS="$CFLAGS -Wno-implicit-int-enum-cast"
fi

needs_rebuild() {
    target_file="$1"
    shift
    if [ ! -f "$target_file" ]; then
        return 0
    fi
    for src in "$@"; do
        if [ "$src" -nt "$target_file" ]; then
            return 0
        fi
    done
    return 1
}

trace_on
ctags --kinds-C=+l+d \
    cbase/*.c cbase/*.h ./*.h ./*.c posix/*.c posix/*.h \
    2> /dev/null || true
vtags.sed tags > .tags.vim     2> /dev/null || true
trace_off

printf "\nChecking preprocessor...\n"
if needs_rebuild "bin/meta_preproc" \
    meta.h meta_preproc.h meta_preproc*.c; then
    printf "Building preprocessor...\n"
    trace_on
    $CC $CPPFLAGS -O2 -flto $CFLAGS \
        meta_preproc_0_main.c -o bin/meta_preproc $LDFLAGS
    trace_off
else
    printf "Preprocessor is up to date.\n"
fi

if [ "$target" = "preprocessor" ]; then
    exit 0
fi

printf "\nChecking generated files...\n"
trace_on

if needs_rebuild "gen/main_tests_array2.h" \
    main_tests_array.h bin/meta_preproc; then
    ./bin/meta_preproc main_tests_array.h > gen/main_tests_array2.h
fi

if needs_rebuild "gen/main_bench_regexes2.h" \
    main_bench_regexes.h bin/meta_preproc; then
    ./bin/meta_preproc main_bench_regexes.h > gen/main_bench_regexes2.h
fi

if needs_rebuild "gen/main_bench_patterns.h" \
    process_patterns.py "$dir/0patterns"/*; then
    python3 process_patterns.py gen/main_bench_patterns.h "$dir/0patterns"
fi

if needs_rebuild "gen/main_bench_patterns2.h" \
    gen/main_bench_patterns.h bin/meta_preproc; then
    ./bin/meta_preproc gen/main_bench_patterns.h \
        > gen/main_bench_patterns2.h
fi

trace_off

case "$target" in
all)
    trace_on
    $CC $CPPFLAGS $CFLAGS main_test.c -o bin/meta_test $LDFLAGS
    bin/meta_test
    $CC $CPPFLAGS $CFLAGS main_bench.c -o bin/meta_bench $LDFLAGS
    bin/meta_bench
    trace_off
    ;;
test)
    trace_on
    $CC $CPPFLAGS $CFLAGS main_test.c -o bin/meta_test $LDFLAGS
    bin/meta_test
    trace_off
    ;;
bench)
    trace_on
    $CC $CPPFLAGS $CFLAGS main_bench.c -o bin/meta_bench $LDFLAGS
    bin/meta_bench --max-input-len 1024
    trace_off
    ;;
debug)
    gdb ./bin/regex_test -ex 'break exit' -ex 'run'
    ;;
callgrind)
    trace_on
    $CC $CPPFLAGS $CFLAGS main_bench.c -o bin/meta_bench $LDFLAGS
    valgrind --tool=callgrind bin/meta_bench
    trace_off
    ;;
check)
    CC=gcc CFLAGS="-fanalyzer -fdiagnostics-color=never" "$0" test
    CFLAGS="--analyze -Xanalyzer -analyzer-output=text"
    CFLAGS="$CFLAGS -Xanalyzer -analyzer-werror"
    CFLAGS="$CFLAGS -Xanalyzer -analyzer-opt-analyze-headers"
    CFLAGS="$CFLAGS -Wno-unused-command-line-argument"
    CFLAGS="$CFLAGS -fno-color-diagnostics"
    CC=clang CFLAGS="$CFLAGS" "$0" test
    exit
    ;;
esac
