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
        for literal in literals:
            out.write(f"    {{ R({literal}) }},\n")
        out.write("};\n\n")
        buckets.append((array_name, length_class, max_len))

    if buckets:
        out.write("static GeneratedBenchRegexBucket generated_bench_regex_buckets[] = {\n")
        for array_name, length_class, max_len in buckets:
            out.write("    {\n")
            out.write(f"        .array_name = \"{array_name}\",\n")
            out.write(f"        .input_path = \"0data/{array_name}/small.txt\",\n")
            out.write("        .regex_bucket = {\n")
            out.write(f"            .name = \"{array_name}\",\n")
            out.write(f"            .length_class = {length_class},\n")
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
