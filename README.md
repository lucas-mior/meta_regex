# meta_regex
Compile time regexes for C.

## Disclaimer
This is an exploration: don't use it in production.

## In which situations could it be nice to use this "library"?
- When you don't want to pay the cost of compiling regexes at run time.
  * You definetely won't pay for parsing it
  * If your regex is simple, you also won't pay for generating a DFA at runtime,
    you will get a fast AOT static DFA.
- When you don't need back references in your regexes
  * The backtracking NFA works, but it is slow
- When you don't need capturing the groups in your regexes
  * The TDFA supports capturing groups without exponential cost,
    but there is an overhead that makes it difficult to perform as well as the
    non tagged DFA.
- When you know that input is not very long
  * The performance is not competitive when input is long
- If you like to pass the length of your input instead of only relying on the
  nul termination
- If you don't care about binary size
  * The ahead of time static DFA needs to store lots of metadata
- If you can make assumptions about your input data and your regexes
- If you want absolute control over the regex matcher
  * You can enable/disable which matchers you want
  * TODO: You can tweak the heuristic parameters used to select the matcher
- If you want to make it easier to not depend on libc
  * The regex pre processor and runtime matchers use only some simple libc
    functions from `<string.h>` that you can easily implement yourself (and
    main, of course). Libc regexes are only needed for testing against
    `<regex.h>` implementation.
- When you dont want to learn a lexer generator syntax or you need features that
  it does not support.
  * re2c is awesome, but it may be simpler to use a `R("regex")` macro and a
    `meta_match_regex()` function call than to have to learn how to use re2c.

## Which regular expressions does it support?
A subset of posix extended.
See `meta_tests_array.h` to check which patterns are being tested.

### Not supported
- collating elements like `[[=a=]]` and `[[.a.]]` will give an error
- locale settings: they are completely ignored.
- regexes with non ascii characters inside bracket expressions like `[[á]]`:
  will trigger an error during pre processing of the regex.

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

## Project structure
- `meta.h`: definitions used both at compile time and runtime
- `meta_preproc*`: pre processor that compiles the regexes
- `main*`: tests and benchmarks against posix
- `meta_match*`: runtime matchers

## Overview of the available matchers
- BTNFA
  * The only one that supports backreferences.
  * Vulnerable to the problem described [here](https://swtch.com/~rsc/regexp/regexp1.html).
  * In general, slower than the others (except Tagged NFA).
- Static DFA
  * Tends to bloat the binary.
  * Does not support group extraction.
  * The pre processor only creates it if it does not exceed specified limits.
    This is done for 2 reaons: first, to not generate huge binaries. Second,
    to improve cache locality. Lazy DFA is better if you need complicated
    regular expressions with many possible states. I still have not come up
    with a good heuristic for this. By default, the runtime matcher will always
    choose static DFA over the lazy DFA, if it is available. So ultimately the
    pre processor is defining which regular expressions are worth compiling or
    not.
- Lazy DFA
  * Does not support group extraction.
- Tagged DFA
  * The only one that supports group extraction besides BTNFA.
  * All the considerations made on the static DFA item about binary size and
    cache locality apply here as well.
- Tagged NFA
  * This is ~almost~ always slower than all the others, never use it.

## TODO
- Implement heuristics for the high level dispatcher
- Implement arguments for the pre processor
  * limits
  * heuristics
- Organize testing, benchmarking and plotting
- Implement
  [Basic Unicode Support](https://www.unicode.org/reports/tr18/#Basic_Unicode_Support)
