# meta_regex
Compile time regex for C.

## Current status
The plan is to completely support posix extended regular expressions.
Currently only a subset of it is supported.

## Getting started
```sh
git clone https://github.com/lucas-mior/meta_regex
cd meta_regex

./build.sh
```

## Files
- `meta_preproc.c`: Program that reads your C code and transforms
   `META_REGEX("regex[0-9]"` into the equivalent MetaRegex struct.
- `meta_regex.h` common stuff used at compile time and runtime.
- `meta_regex_match.c` runtime regex matcher
- `main.c` example of usage, with tests against the posix regex lib.
