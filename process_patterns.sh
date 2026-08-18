#!/bin/sh -e

out_path=$1
pattern_dir=$2

if [ -z "$out_path" ] || [ -z "$pattern_dir" ]; then
    printf '%s\n' 'usage: process_patterns.sh <out-path> <pattern-dir>' >&2
    exit 1
fi

set -- "$pattern_dir"/*.txt
if [ ! -f "$1" ]; then
    set -- /dev/null
fi

awk '
function c_ident(name) {
    gsub(/[^0-9A-Za-z_]/, "_", name)
    if ((name == "") || (name !~ /^[A-Za-z_]/)) {
        name = "_" name
    }
    return name
}

function c_escape_raw(string) {
    gsub(/\\/, "\\\\", string)
    gsub(/"/, "\\\"", string)
    return "\"" string "\""
}

function trim(string) {
    sub(/^[[:space:]]*/, "", string)
    sub(/[[:space:]]*$/, "", string)
    return string
}

function extract_c_literal(line) {
    line = trim(line)
    if ((line == "") || (line ~ /^#/)) {
        return ""
    }

    if (match(line, /"(\\.|[^"\\])*"/)) {
        return substr(line, RSTART, RLENGTH)
    }

    return c_escape_raw(line)
}

function literal_len(c_literal) {
    return length(c_literal) - 2
}

function start_file(path, base) {
    current_file_started = 1
    skip_file = 0
    literal_count = 0
    max_len = 0

    base = path
    sub(/^.*\//, "", base)
    sub(/\.txt$/, "", base)

    if (tolower(base) ~ /^readme/) {
        skip_file = 1
        return
    }

    array_name = c_ident(base)
}

function finish_file(i) {
    if (!current_file_started || skip_file || (literal_count == 0)) {
        return
    }

    if (max_len <= 8) {
        length_class = "BENCH_LEN_1_8"
    } else if (max_len <= 16) {
        length_class = "BENCH_LEN_9_16"
    } else if (max_len <= 32) {
        length_class = "BENCH_LEN_17_32"
    } else if (max_len <= 64) {
        length_class = "BENCH_LEN_33_64"
    } else {
        length_class = "BENCH_LEN_LAST"
    }

    print "static BenchRegexCase " array_name "[] = {"
    for (i = 1; i <= literal_count; i += 1) {
        print "    { R(" literals[i] ") },"
    }
    print "};"
    print ""

    bucket_count += 1
    bucket_array_name[bucket_count] = array_name
    bucket_length_class[bucket_count] = length_class
    bucket_max_len[bucket_count] = max_len
}

BEGIN {
    print "#if !defined(META_BENCH_PATTERNS_H)"
    print "#define META_BENCH_PATTERNS_H"
    print ""
    print "#include \"cbase.h\""
    print "#include \"meta.h\""
    print ""
    print "typedef struct GeneratedBenchRegexBucket {"
    print "    char *array_name;"
    print "    char *input_path;"
    print "    BenchRegexBucket regex_bucket;"
    print "} GeneratedBenchRegexBucket;"
    print ""
}

FNR == 1 {
    finish_file()
    start_file(FILENAME)
}

{
    if (!skip_file) {
        literal = extract_c_literal($0)
        if (literal != "") {
            literal_count += 1
            literals[literal_count] = literal
            current_len = literal_len(literal)
            if (current_len > max_len) {
                max_len = current_len
            }
        }
    }
}

END {
    finish_file()

    if (bucket_count > 0) {
        print "static GeneratedBenchRegexBucket generated_bench_regex_buckets[] = {"
        for (i = 1; i <= bucket_count; i += 1) {
            array_name = bucket_array_name[i]
            print "    {"
            print "        .array_name = \"" array_name "\","
            print "        .input_path = \"0data/" array_name "/small.txt\","
            print "        .regex_bucket = {"
            print "            .name = \"" array_name "\","
            print "            .length_class = " bucket_length_class[i] ","
            print "            .max_regex_len = " bucket_max_len[i] ","
            print "            .cases = " array_name ","
            print "            .count = LENGTH(" array_name "),"
            print "        },"
            print "    },"
        }
        print "};"
        print "#define GENERATED_BENCH_REGEX_BUCKET_COUNT " \
              "LENGTH(generated_bench_regex_buckets)"
        print ""
    } else {
        print "static GeneratedBenchRegexBucket generated_bench_regex_buckets[1];"
        print "#define GENERATED_BENCH_REGEX_BUCKET_COUNT 0"
        print ""
    }

    print "#endif /* META_BENCH_PATTERNS_H */"
}
' "$@" > "$out_path"
