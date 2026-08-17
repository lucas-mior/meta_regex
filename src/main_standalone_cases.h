#if !defined(MAIN_STANDALONE_CASES_H)
#define MAIN_STANDALONE_CASES_H

static StandaloneTest standalone_tests[] = {
    {
        "class match",
        "abc5def",
        R("[0-9]", META_RE_NOSUB),
        0,
        1,
        {{3, 4}},
    },
    {
        "class mismatch",
        "hello world",
        R("[0-9]", META_RE_NOSUB),
        META_REG_NOMATCH,
        0,
        {{0}},
    },
    {
        "start anchor match",
        "2hello world",
        R("^[0-9]"),
        0,
        1,
        {{0, 1}},
    },
    {
        "start anchor mismatch",
        "hello 2",
        R("^[0-9]"),
        META_REG_NOMATCH,
        0,
        {{0}},
    },
    {
        "end anchor match",
        "test end5",
        R("[0-9]$"),
        0,
        1,
        {{8, 9}},
    },
    {
        "bounded wildcard match",
        "aXXXb",
        R("a.+b"),
        0,
        1,
        {{0, 5}},
    },
    {
        "two captures",
        "foo bar",
        R("(foo) (bar)"),
        0,
        3,
        {{0, 7}, {0, 3}, {4, 7}},
    },
    {
        "adjacent captures",
        "a1b2",
        R("([a-z])([0-9])"),
        0,
        3,
        {{0, 2}, {0, 1}, {1, 2}},
    },
    {
        "back reference match",
        "hello hello",
        R("([a-z]+) \\1"),
        0,
        2,
        {{0, 11}, {0, 5}},
    },
    {
        "back reference mismatch",
        "hello world",
        R("([a-z]+) \\1"),
        META_REG_NOMATCH,
        0,
        {{0}},
    },
};

#endif /* MAIN_STANDALONE_CASES_H */
