# meta_regex
Compile time regexes for C.

## Which regular expressions does it support?
A subset of posix extended.
See `meta_tests_array.h` to check which patterns are being tested.

### Not supported
- collating elements like `[[=a=]]` and `[[.a.]]` will give an error
- locale settings: they are completely ignored.

## Getting started
```sh
git clone https://github.com/lucas-mior/meta_regex
cd meta_regex

# This builds and runs `main.c`
./build.sh
```

## Usage Example

The engine uses a preprocessor to transform regex strings into static opcode
arrays. You define regexes using the R() macro. The meta_preproc.c program
expands this into a pointer to an anonymous MetaRegex literal.

1. Write your C code
```C
#include <stdio.h>
#include <regex.h>
#include "meta_regex.h"
#include "meta_regex_match.c"

int32
main(void) {
    char *text;
    MetaRegex *re;
    regmatch_t matches[2];
    int32 result;

    text = "The price is 42 dollars";
    re = R("is ([0-9]+)");

    if (re == NULL) {
        return 1;
    }

    result = meta_regex_match(re, text, 2, matches);
    if (result != 0) {
        return 1;
    }

    printf("Match found!\n");
    {
        int32 start;
        int32 end;

        start = (int32)matches[1].rm_so;
        end = (int32)matches[1].rm_eo;
        printf("Captured value: %.*s\n", end - start, text + start);
    }

    return 0;
}
```

2. Run the Preprocessor
Your build pipeline must run meta_preproc first to generate the expanded source
code:
```sh
# 1. Compile the preprocessor
cc meta_preproc.c -o bin/meta_preproc

# 2. Generate C code with compiled regexes
./bin/meta_preproc your_code.c > gen/your_code_baked.c

# 3. Compile the final program
cc gen/your_code_baked.c -o your_program

./your_program
```

## Files
- `meta_preproc.c`: Program that reads your C code and transforms
   `R("regex[0-9]")` into the equivalent MetaRegex struct.
- `meta_regex.h` common stuff used at compile time and runtime.
- `meta_regex_match.c` runtime regex matcher
- `main.c` example of usage, with tests against the posix regex lib.
