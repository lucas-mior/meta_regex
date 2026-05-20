#!/bin/sh -e

# shellcheck disable=SC2086

set -e
alias trace_on='set -x'
alias trace_off='{ set +x; } 2>/dev/null'

dir="$(readlink -f "$(dirname "$0")")"
cbase="cbase"

mkdir -p bin gen

target="${1:-test}"

CFLAGS="$CFLAGS -std=c11"
CFLAGS="$CFLAGS -Wextra -Wall"
CFLAGS="$CFLAGS -Werror"
CFLAGS="$CFLAGS -Wno-unused-macros"
CFLAGS="$CFLAGS -Wno-unused-function"
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
preprocessor|test|bench|all)
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
fi

generate_bench_pattern_header() {
    out="$1"
    pattern_dir="$2"

    python3 process_patterns.py "$out" "$pattern_dir"
}

trace_on
ctags --kinds-C=+l+d \
    cbase/*.c cbase/*.h ./*.h ./*.c posix/*.c posix/*.h \
    2> /dev/null || true
vtags.sed tags > .tags.vim       2> /dev/null || true
trace_off

printf "\nBuilding preprocessor...\n"
trace_on
$CC $CPPFLAGS -O2 -flto $CFLAGS meta_preproc_0_main.c -o bin/meta_preproc $LDFLAGS
trace_off

if [ "$target" = "preprocessor" ]; then
    exit 0
fi

trace_on
./bin/meta_preproc main_tests_array.h   > gen/main_tests_array2.h
./bin/meta_preproc main_bench_regexes.h > gen/main_bench_regexes2.h
trace_off

case "$target" in
bench|all|callgrind)
    trace_on
    generate_bench_pattern_header gen/main_bench_patterns.h "$dir/0patterns"
    ./bin/meta_preproc gen/main_bench_patterns.h > gen/main_bench_patterns2.h
    trace_off
    ;;
esac

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
esac
