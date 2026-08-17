#define CBASE_IMPLEMENT
#include "cbase.h"

#include "meta_regex.h"
#include "meta_match.c"

#define STANDALONE_MAX_MATCHES 4

typedef struct StandaloneTest {
    char *name;
    char *input;
    MetaRegex *meta_regex;
    int32 expected_result;
    int32 expected_match_len;
    MetaRegexMatch expected_match[STANDALONE_MAX_MATCHES];
} StandaloneTest;

#include "gen/main_standalone_cases2.h"

int32
main(void) {
    enum Matcher enabled_matchers = (enum Matcher)(
        MATCHER_BTNFA
        |MATCHER_TNFA
        |MATCHER_TDFA
        |MATCHER_LAZY_DFA
        |MATCHER_STATIC_DFA
    );
    bool failed = false;

    for (int32 i = 0; i < LENGTH(standalone_tests); i += 1) {
        StandaloneTest *test = &standalone_tests[i];
        MetaRegexMatch pmatch[STANDALONE_MAX_MATCHES];
        int32 input_len = strlen32(test->input);
        int32 result;

        for (int32 j = 0; j < LENGTH(pmatch); j += 1) {
            pmatch[j].rm_so = -1;
            pmatch[j].rm_eo = -1;
        }

        result = meta_regex_match(test->meta_regex, (uint8 *)test->input,
                                  input_len, pmatch, LENGTH(pmatch),
                                  enabled_matchers);

        if (result != test->expected_result) {
            failed = true;
            error("Standalone test failed: %s: result %d, expected %d\n",
                  test->name, result, test->expected_result);
            continue;
        }

        if (result != 0) {
            continue;
        }

        for (int32 j = 0; j < test->expected_match_len; j += 1) {
            if (pmatch[j].rm_so != test->expected_match[j].rm_so
                || pmatch[j].rm_eo != test->expected_match[j].rm_eo) {
                failed = true;
                error("Standalone test failed: %s: match %d [%d, %d], "
                      "expected [%d, %d]\n",
                      test->name, j, pmatch[j].rm_so, pmatch[j].rm_eo,
                      test->expected_match[j].rm_so,
                      test->expected_match[j].rm_eo);
            }
        }
    }

    if (failed) {
        exit(EXIT_FAILURE);
    }

    printf("Standalone tests passed: %d\n", (int32)LENGTH(standalone_tests));
    exit(EXIT_SUCCESS);
}
