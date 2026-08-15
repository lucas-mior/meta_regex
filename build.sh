#!/bin/sh -e

# shellcheck disable=SC2086

dir=$(dirname "$(readlink -f "$0")")
cd "$dir" || exit

# shellcheck source=./cbase/common.sh
. "./cbase/common.sh"

mkdir -p bin gen

script=$(basename "$0")
common_build_parse_args "$@"

case "$mode" in
all|bench|build|callgrind|check|debug|fast_feedback|preprocessor|test)
    ;;
*)
    common_build_unknown_mode
    ;;
esac

CC=$(common_get_compiler "$mode")

common_build_print_invocation "$script"

CPPFLAGS="$CPPFLAGS -Isrc -Icbase"

CFLAGS="$CFLAGS -std=c11"
CFLAGS="$CFLAGS -Wfatal-errors"
CFLAGS="$CFLAGS -Wextra -Wall"
CFLAGS="$CFLAGS -Werror=all -Werror=extra"
CFLAGS="$CFLAGS -Werror"  # Only uncomment occasionally, keep this line
CFLAGS="$CFLAGS -Wno-missing-field-initializers"
CFLAGS="$CFLAGS -Wno-unused-function"

if [ "$CC" = "clang" ] || [ "$CC" = "zig cc" ]; then
    CFLAGS="$CFLAGS -Weverything"
    CFLAGS="$CFLAGS -Wno-assign-enum"
    CFLAGS="$CFLAGS -Wno-c++-keyword"
    CFLAGS="$CFLAGS -Wno-cast-qual"
    CFLAGS="$CFLAGS -Wno-constant-logical-operand"
    CFLAGS="$CFLAGS -Wno-covered-switch-default"
    CFLAGS="$CFLAGS -Wno-declaration-after-statement"
    CFLAGS="$CFLAGS -Wno-disabled-macro-expansion"
    CFLAGS="$CFLAGS -Wno-float-equal"
    CFLAGS="$CFLAGS -Wno-format-nonliteral"
    CFLAGS="$CFLAGS -Wno-implicit-int-enum-cast"
    CFLAGS="$CFLAGS -Wno-implicit-void-ptr-cast"
    CFLAGS="$CFLAGS -Wno-missing-field-initializers"
    CFLAGS="$CFLAGS -Wno-padded"
    CFLAGS="$CFLAGS -Wno-pre-c11-compat"
    CFLAGS="$CFLAGS -Wno-type-limits"
    CFLAGS="$CFLAGS -Wno-unknown-pragmas"
    CFLAGS="$CFLAGS -Wno-unsafe-buffer-usage"
    CFLAGS="$CFLAGS -Wno-unused-macros"
    CFLAGS="$CFLAGS -Wno-unused-variable"
    CFLAGS="$CFLAGS -Wno-used-but-marked-unused"
fi

LDFLAGS="$LDFLAGS -lmagic -lm"

case "$mode" in
debug)
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=1"
    CFLAGS="$CFLAGS -g3"
    ;;
build|preprocessor|test|bench|all|check)
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=0"
    CFLAGS="$CFLAGS -g -O2 -flto"
    ;;
fast_feedback)
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=0"
    ;;
callgrind)
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=0"
    CPPFLAGS="$CPPFLAGS -DBENCHMARK=1"
    CFLAGS="$CFLAGS -g3 -O2 -flto"
    ;;
all|bench|build|callgrind|check|debug|fast_feedback|preprocessor|test)
    ;;
*)
    common_build_unknown_mode
    ;;
esac

trace_on
common_build_tags cbase . posix
trace_off

if common_outdated "bin/meta_preproc" \
    src/meta_regex.h src/meta_preproc.h src/meta_preproc*.c; then
    trace_on
    $CC $CPPFLAGS $CFLAGS -O2 -flto \
        src/meta_preproc_0_main.c -o bin/meta_preproc $LDFLAGS
    trace_off
else
    printf "Preprocessor is up to date.\n"
fi

if [ "$mode" = "preprocessor" ]; then
    exit 0
fi

printf "\nChecking generated files...\n"
trace_on

if common_outdated "gen/main_tests_array2.h" \
    main_tests_array.h bin/meta_preproc; then
    ./bin/meta_preproc main_tests_array.h > gen/main_tests_array2.h
fi

if common_outdated "gen/main_bench_regexes2.h" \
    main_bench_regexes.h bin/meta_preproc; then
    ./bin/meta_preproc main_bench_regexes.h > gen/main_bench_regexes2.h
fi

if common_outdated "gen/main_bench_patterns.h" \
    process_patterns.py "$dir/0patterns"/*; then
    python3 process_patterns.py gen/main_bench_patterns.h "$dir/0patterns"
fi

if common_outdated "gen/main_bench_patterns2.h" \
    gen/main_bench_patterns.h bin/meta_preproc; then
    ./bin/meta_preproc gen/main_bench_patterns.h \
        > gen/main_bench_patterns2.h
fi

trace_off

case "$mode" in
build|all)
    trace_on
    $CC $CPPFLAGS $CFLAGS src/main_test.c -o bin/meta_test $LDFLAGS
    $CC $CPPFLAGS $CFLAGS src/main_bench.c -o bin/meta_bench $LDFLAGS
    trace_off
    ;;
fast_feedback)
    trace_on
    $CC $CPPFLAGS $CFLAGS src/main_test.c -o bin/meta_test $LDFLAGS
    trace_off
    ;;
test)
    TEST_DEFINE_MODULE=0 \
    TEST_DEFINE_TESTING=0 \
    TEST_EXE_PATH=bin/meta_test \
    TEST_REQUIRE_TESTING_MARKER=0 \
    TEST_SKIP_MAIN=0 \
        common_test "" src/main_test.c
    ;;
bench)
    trace_on
    $CC $CPPFLAGS $CFLAGS src/main_bench.c -o bin/meta_bench $LDFLAGS
    bin/meta_bench --max-input-len 1024
    trace_off
    ;;
debug)
    trace_on
    $CC $CPPFLAGS $CFLAGS src/main_test.c -o bin/meta_test $LDFLAGS
    trace_off
    ;;
callgrind)
    trace_on
    $CC $CPPFLAGS $CFLAGS src/main_bench.c -o bin/meta_bench $LDFLAGS
    valgrind --tool=callgrind bin/meta_bench
    trace_off
    ;;
check)
    common_build_run_analyzers debug
    ;;
esac
