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
bench|build|callgrind|check|cross|debug|fast_feedback|preprocessor|standalone|test)
    ;;
*)
    common_build_unknown_mode
    ;;
esac

CC=$(common_get_compiler "$mode")

common_build_print_invocation "$script"

CPPFLAGS="$CPPFLAGS -I. -Isrc -Icbase"

CFLAGS="$CFLAGS -std=c11"
CFLAGS="$CFLAGS -Wfatal-errors"
CFLAGS="$CFLAGS -Wextra -Wall"
CFLAGS="$CFLAGS -Werror=all -Werror=extra"
# CFLAGS="$CFLAGS -Werror"  # Only uncomment occasionally, keep this line
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

LDFLAGS="$LDFLAGS -lm"

case "$mode" in
debug)
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=1"
    CFLAGS="$CFLAGS -g3"
    ;;
build|preprocessor|standalone|test|bench|check)
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=0"
    CFLAGS="$CFLAGS -g -O2 -flto"
    ;;
cross)
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=0"
    CFLAGS="$CFLAGS -g -O2"
    ;;
fast_feedback)
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=0"
    ;;
callgrind)
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=0"
    CPPFLAGS="$CPPFLAGS -DBENCHMARK=1"
    CFLAGS="$CFLAGS -g3 -O2 -flto"
    ;;
bench|build|callgrind|check|cross|debug|fast_feedback|preprocessor|standalone|test)
    ;;
*)
    common_build_unknown_mode
    ;;
esac

# trace_on
# common_build_tags cbase . posix
# trace_off

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

if common_outdated "gen/main_tests_array2.h" \
    src/main_tests_array.h bin/meta_preproc; then
    ./bin/meta_preproc src/main_tests_array.h > gen/main_tests_array2.h
fi

if common_outdated "gen/main_standalone_cases2.h" \
    src/main_standalone_cases.h bin/meta_preproc; then
    ./bin/meta_preproc src/main_standalone_cases.h \
        > gen/main_standalone_cases2.h
fi

if common_outdated "gen/main_bench_regexes2.h" \
    src/main_bench_regexes.h bin/meta_preproc; then
    ./bin/meta_preproc src/main_bench_regexes.h > gen/main_bench_regexes2.h
fi

if common_outdated "gen/main_bench_patterns.h" \
    process_patterns.sh "$dir/0patterns"/*; then
    ./process_patterns.sh gen/main_bench_patterns.h "$dir/0patterns"
fi

if common_outdated "gen/main_bench_patterns2.h" \
    gen/main_bench_patterns.h bin/meta_preproc; then
    ./bin/meta_preproc gen/main_bench_patterns.h \
        > gen/main_bench_patterns2.h
fi

test_exe=bin/meta_test
bench_exe=bin/meta_bench
standalone_exe=bin/meta_standalone
cross_use_standalone=0

if [ "$mode" = "cross" ]; then
    common_build_cross_all
    cross="$target"

    CFLAGS="$CFLAGS -Wno-padded"
    CFLAGS="$CFLAGS -target $cross"

    case "$cross" in
    *windows*)
        test_exe=bin/meta_test.exe
        bench_exe=bin/meta_bench.exe
        standalone_exe=bin/meta_standalone.exe
        cross_use_standalone=1
        ;;
    *)
        ;;
    esac
fi

case "$mode" in
build)
    trace_on
    $CC $CPPFLAGS $CFLAGS src/main_test.c -o "$test_exe" $LDFLAGS
    $CC $CPPFLAGS $CFLAGS src/main_bench.c -o "$bench_exe" $LDFLAGS
    trace_off
    ;;
fast_feedback)
    trace_on
    $CC $CPPFLAGS $CFLAGS src/main_test.c -o "$test_exe" $LDFLAGS
    trace_off
    ;;
standalone)
    trace_on
    $CC $CPPFLAGS $CFLAGS src/main_standalone.c \
        -o "$standalone_exe" $LDFLAGS
    "$standalone_exe"
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
    $CC $CPPFLAGS $CFLAGS src/main_bench.c -o "$bench_exe" $LDFLAGS
    bin/meta_bench --max-input-len 1024
    trace_off
    ;;
debug)
    trace_on
    $CC $CPPFLAGS $CFLAGS src/main_test.c -o "$test_exe" $LDFLAGS
    trace_off
    ;;
callgrind)
    trace_on
    $CC $CPPFLAGS $CFLAGS src/main_bench.c -o bin/meta_bench $LDFLAGS
    valgrind --tool=callgrind bin/meta_bench
    trace_off
    ;;
cross)
    trace_on
    if [ "$cross_use_standalone" = 1 ]; then
        $CC $CPPFLAGS $CFLAGS src/main_standalone.c \
            -o "$standalone_exe" $LDFLAGS
    else
        $CC $CPPFLAGS $CFLAGS src/main_test.c -o "$test_exe" $LDFLAGS
        $CC $CPPFLAGS $CFLAGS src/main_bench.c -o "$bench_exe" $LDFLAGS
    fi
    trace_off
    ;;
check)
    common_build_run_analyzers debug
    ;;
esac
