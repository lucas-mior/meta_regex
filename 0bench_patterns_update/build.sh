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
PYTHON=${PYTHON:-python3}

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

    "$PYTHON" - "$out" "$pattern_dir" <<'PYGEN'
import glob
import os
import re
import sys

out_path = sys.argv[1]
pattern_dir = sys.argv[2]
files = sorted(glob.glob(os.path.join(pattern_dir, "*.txt")))


def c_ident(name):
    name = re.sub(r"[^0-9A-Za-z_]", "_", name)
    if not name or not re.match(r"[A-Za-z_]", name[0]):
        name = "_" + name
    return name


def c_escape_raw(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def extract_c_literal(line):
    line = line.strip()
    if not line or line.startswith("#"):
        return None

    # Expected forms:
    #     "pattern"
    #     name = "pattern"
    match = re.search(r'"(?:\\.|[^"\\])*"', line)
    if match:
        return match.group(0)

    return c_escape_raw(line)


def literal_len(c_literal):
    # Approximate length is enough for benchmark metadata. Avoid interpreting
    # escapes here so that \\x00 cannot become a real NUL while generating.
    return max(0, len(c_literal) - 2)

buckets = []
with open(out_path, "w", encoding="utf-8") as out:
    out.write("#if !defined(META_BENCH_PATTERNS_H)\n")
    out.write("#define META_BENCH_PATTERNS_H\n\n")
    out.write("#include \"meta.h\"\n\n")
    out.write("typedef struct GeneratedBenchRegexBucket {\n")
    out.write("    char *array_name;\n")
    out.write("    char *input_path;\n")
    out.write("    BenchRegexBucket regex_bucket;\n")
    out.write("} GeneratedBenchRegexBucket;\n\n")

    for path in files:
        base = os.path.splitext(os.path.basename(path))[0]
        if base.lower().startswith("readme"):
            continue

        array_name = c_ident(base)
        literals = []
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                literal = extract_c_literal(line)
                if literal is not None:
                    literals.append(literal)

        if not literals:
            continue

        max_len = max(literal_len(literal) for literal in literals)
        if max_len <= 8:
            length_class = "BENCH_LEN_1_8"
        elif max_len <= 16:
            length_class = "BENCH_LEN_9_16"
        elif max_len <= 32:
            length_class = "BENCH_LEN_17_32"
        elif max_len <= 64:
            length_class = "BENCH_LEN_33_64"
        else:
            length_class = "BENCH_LEN_LAST"

        out.write(f"static BenchRegexCase {array_name}[] = {{\n")
        for i, literal in enumerate(literals):
            out.write(
                f"    {{ \"{array_name}_{i}\", R({literal}), "
                f"{literal_len(literal)}, {length_class}, "
                "BENCH_FEATURE_NO_BACKREFS },\n"
            )
        out.write("};\n\n")
        buckets.append((array_name, length_class, max_len))

    if buckets:
        out.write("static GeneratedBenchRegexBucket generated_bench_regex_buckets[] = {\n")
        for array_name, length_class, max_len in buckets:
            out.write("    {\n")
            out.write(f"        .array_name = \"{array_name}\",\n")
            out.write(f"        .input_path = \"data/{array_name}/small.txt\",\n")
            out.write("        .regex_bucket = {\n")
            out.write(f"            .name = \"{array_name}\",\n")
            out.write(f"            .length_class = {length_class},\n")
            out.write("            .feature_class = BENCH_FEATURE_NO_BACKREFS,\n")
            out.write(f"            .max_regex_len = {max_len},\n")
            out.write(f"            .cases = {array_name},\n")
            out.write(f"            .count = LENGTH({array_name}),\n")
            out.write("        },\n")
            out.write("    },\n")
        out.write("};\n")
        out.write("#define GENERATED_BENCH_REGEX_BUCKET_COUNT "
                  "LENGTH(generated_bench_regex_buckets)\n\n")
    else:
        out.write("static GeneratedBenchRegexBucket generated_bench_regex_buckets[1];\n")
        out.write("#define GENERATED_BENCH_REGEX_BUCKET_COUNT 0\n\n")

    out.write("#endif /* META_BENCH_PATTERNS_H */\n")
PYGEN
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
    generate_bench_pattern_header gen/main_bench_patterns.h "$dir/patterns"
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
    bin/meta_bench
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
