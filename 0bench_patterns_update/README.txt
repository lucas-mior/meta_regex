Updated benchmark integration
=============================

Files:
- build.sh: generates gen/main_bench_patterns.h from patterns/*.txt for bench/all/callgrind, then runs meta_preproc to create gen/main_bench_patterns2.h.
- main_bench.c: includes gen/main_bench_patterns2.h and runs each generated regex array against data/${array_name}/small.txt.

Behavior:
- Existing benchmark buckets in main_bench_regexes.h are left in place and still run first.
- Each patterns/*.txt file becomes one BenchRegexCase array whose name is the sanitized txt basename.
- Each quoted pattern line becomes one R("<pattern>") entry. Lines of the form name = "<pattern>" are also accepted.
- README*.txt files under patterns/ are ignored.
- If data/${array_name}/small.txt cannot be opened/read, main_bench exits with an error.
