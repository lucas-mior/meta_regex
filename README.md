# meta_regex
Compile time regexes for C.

## Which regular expressions does it support?
A subset of posix extended.
See `meta_tests_array.h` to check which patterns are being tested.

### Not supported
- collating elements like `[[=a=]]` and `[[.a.]]` are not
- locale settings: This tool always assume text is UTF-8 encoded

## Getting started
```sh
git clone https://github.com/lucas-mior/meta_regex
cd meta_regex

# This builds and runs `main.c`
./build.sh
```

## Files
- `meta_preproc.c`: Program that reads your C code and transforms
   `R("regex[0-9]")` into the equivalent MetaRegex struct.
- `meta_regex.h` common stuff used at compile time and runtime.
- `meta_regex_match.c` runtime regex matcher
- `main.c` example of usage, with tests against the posix regex lib.
