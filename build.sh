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
bench|build|callgrind|check|cross|debug|debug-fast|fast_feedback|preprocessor|standalone|test)
    ;;
*)
    common_build_unknown_mode
    ;;
esac

CC=$(common_get_compiler "$mode")

is_msvc=0
is_clang_cl=0
is_cl=0
msvc_compiler=clang-cl

case "$CC" in
clang-cl|*/clang-cl|clang-cl.exe|*/clang-cl.exe)
    is_msvc=1
    is_clang_cl=1
    msvc_compiler=clang-cl
    ;;
cl|*/cl|cl.exe|*/cl.exe)
    is_msvc=1
    is_cl=1
    msvc_compiler=cl
    ;;
esac

common_build_print_invocation "$script"

CPPFLAGS="$CPPFLAGS -I. -Isrc -Icbase"

CFLAGS="$CFLAGS -std=c11"
CFLAGS="$CFLAGS -Wfatal-errors"

LDFLAGS="$LDFLAGS -lm"

case "$mode" in
debug)
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=1"
    CFLAGS="$CFLAGS -g3"
    ;;
debug-fast)
    CPPFLAGS="$CPPFLAGS -DDEBUGGING=1"
    CFLAGS="$CFLAGS -g2 -O2 -flto"
    CFLAGS="$CFLAGS -fsanitize=undefined"
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
bench|build|callgrind|check|cross|debug|debug-fast|fast_feedback|preprocessor|standalone|test)
    ;;
*)
    common_build_unknown_mode
    ;;
esac

# trace_on
# common_build_tags cbase . posix
# trace_off

preproc_exe=bin/meta_preproc

if [ "$is_cl" -eq 1 ]; then
    preproc_exe=$preproc_exe.exe
fi

if [ "$is_msvc" -eq 1 ]; then
    if [ -z "$CLANG_CL_TARGET" ]; then
        case "$(uname -a)" in
        *Linux*|*Darwin*|*BSD*)
            if [ "$is_clang_cl" -eq 1 ]; then
                CLANG_CL_TARGET=$(cc -dumpmachine 2>/dev/null || true)
            fi
            ;;
        esac
    fi

    if [ "$is_clang_cl" -eq 1 ] && [ -n "$CLANG_CL_TARGET" ]; then
        CFLAGS="$CFLAGS --target=$CLANG_CL_TARGET"
    fi

    CPPFLAGS=$(common_gcc_flags_to_msvc "$msvc_compiler" $CPPFLAGS)
    CFLAGS=$(common_gcc_flags_to_msvc "$msvc_compiler" $CFLAGS)
    LDFLAGS=$(common_gcc_flags_to_msvc "$msvc_compiler" $LDFLAGS)
fi

meta_build_exe() {
    output=$1
    source=$2

    if [ "$is_cl" -eq 1 ]; then
        $CC $CPPFLAGS $CFLAGS /Fe$output "$source" $LDFLAGS
    else
        $CC $CPPFLAGS $CFLAGS "$source" -o "$output" $LDFLAGS
    fi
}

if common_outdated "$preproc_exe" \
    src/meta_regex.h src/meta_preproc.h src/meta_preproc*.c; then
    trace_on
    meta_build_exe "$preproc_exe" src/meta_preproc_0_main.c
    trace_off
else
    printf "Preprocessor is up to date.\n"
fi

if [ "$mode" = "preprocessor" ]; then
    exit 0
fi

printf "\nChecking generated files...\n"

if common_outdated "gen/main_tests_array2.h" \
    src/main_tests_array.h "$preproc_exe"; then
    ./$preproc_exe src/main_tests_array.h > gen/main_tests_array2.h
fi

if common_outdated "gen/main_standalone_cases2.h" \
    src/main_standalone_cases.h "$preproc_exe"; then
    ./$preproc_exe src/main_standalone_cases.h \
        > gen/main_standalone_cases2.h
fi

if common_outdated "gen/main_bench_regexes2.h" \
    src/main_bench_regexes.h "$preproc_exe"; then
    ./$preproc_exe src/main_bench_regexes.h > gen/main_bench_regexes2.h
fi

if common_outdated "gen/main_bench_patterns.h" \
    process_patterns.sh "$dir/0patterns"/*; then
    ./process_patterns.sh gen/main_bench_patterns.h "$dir/0patterns"
fi

if common_outdated "gen/main_bench_patterns2.h" \
    gen/main_bench_patterns.h "$preproc_exe"; then
    ./$preproc_exe gen/main_bench_patterns.h \
        > gen/main_bench_patterns2.h
fi

test_exe=bin/meta_test
bench_exe=bin/meta_bench
standalone_exe=bin/meta_standalone
cross_use_standalone=0

if [ "$is_cl" -eq 1 ]; then
    test_exe=$test_exe.exe
    bench_exe=$bench_exe.exe
    standalone_exe=$standalone_exe.exe
fi

if [ "$mode" = "cross" ]; then
    common_build_cross_all
    cross="$target"

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
    meta_build_exe "$test_exe" src/main_test.c
    meta_build_exe "$bench_exe" src/main_bench.c
    trace_off
    ;;
fast_feedback)
    trace_on
    meta_build_exe "$test_exe" src/main_test.c
    trace_off
    ;;
standalone)
    trace_on
    meta_build_exe "$standalone_exe" src/main_standalone.c
    "$standalone_exe"
    trace_off
    ;;
test)
    TEST_DEFINE_MODULE=0 \
    TEST_DEFINE_TESTING=0 \
    TEST_EXE_PATH=$test_exe \
    TEST_REQUIRE_TESTING_MARKER=0 \
    TEST_SKIP_MAIN=0 \
        common_test "" src/main_test.c
    ;;
bench)
    trace_on
    meta_build_exe "$bench_exe" src/main_bench.c
    "$bench_exe" --max-input-len 1024
    trace_off
    ;;
debug|debug-fast)
    trace_on
    meta_build_exe "$test_exe" src/main_test.c
    trace_off
    ;;
callgrind)
    trace_on
    meta_build_exe "$bench_exe" src/main_bench.c
    valgrind --tool=callgrind "$bench_exe"
    trace_off
    ;;
cross)
    trace_on
    if [ "$cross_use_standalone" = 1 ]; then
        meta_build_exe "$standalone_exe" src/main_standalone.c
    else
        meta_build_exe "$test_exe" src/main_test.c
        meta_build_exe "$bench_exe" src/main_bench.c
    fi
    trace_off
    ;;
check)
    common_build_run_analyzers debug
    ;;
esac
