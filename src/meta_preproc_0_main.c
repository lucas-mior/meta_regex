#define CBASE_IMPLEMENT
#include "cbase.h"

#include "meta_regex.h"
#include "meta_preproc.h"
#include "meta_preproc_1_parse_regex.c"
#include "meta_preproc_1_parse_tnfa.c"
#include "meta_preproc_1_parse_tdfa.c"
#include "meta_preproc_1_parse_source.c"
#include "meta_preproc_2_gen.c"

static int32
preproc_parse_int32(char *name, char *value, int32 min_value, int32 max_value) {
    char *end = NULL;
    int64 parsed;

    if (value == NULL) {
        error("Missing value for %s. Expected %s=N.\n", name, name);
        exit(EXIT_FAILURE);
    }

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed < min_value
        || parsed > max_value) {
        error("Invalid value for %s: %s. Expected an integer in [%d, %d].\n",
              name, value, min_value, max_value);
        exit(EXIT_FAILURE);
    }
    return (int32)parsed;
}

static bool
preproc_parse_bool(char *name, char *value) {
    int32 value_len;

    if (value == NULL) {
        error("Missing value for %s. Expected %s=true or %s=false.\n", name,
              name, name);
        exit(EXIT_FAILURE);
    }

    value_len = strlen32(value);
    if (STREQUAL(value, value_len, "true")) {
        return true;
    }
    if (STREQUAL(value, value_len, "on")) {
        return true;
    }
    if (STREQUAL(value, value_len, "yes")) {
        return true;
    }
    if (STREQUAL(value, value_len, "false")) {
        return false;
    }
    if (STREQUAL(value, value_len, "off")) {
        return false;
    }
    if (STREQUAL(value, value_len, "no")) {
        return false;
    }

    error("Invalid value for %s: %s. Expected true or false.\n", name, value);
    exit(EXIT_FAILURE);
}

// clang-format off
static void
usage(void) {
    error2("Usage: %s [option=value ...] <file.c>\n", program);
    error2("Options:\n");
    error2("  help=true\n");
    error2("  emit_static_dfa=true|false (default = true)\n");
    error2("  emit_tnfa=true|false (default = true)\n");
    error2("  emit_tdfa=true|false (default = true)\n");
    error2("  emit_tdfa_transition_index=true|false (default = true)\n");
    error2("  max_static_dfa_states=N (default = %d)\n",
           META_MAX_STATIC_DFA_STATES);
    error2("  max_tnfa_tags=N (default = %d)\n",
           PREPROC_MAX_TNFA_TAGS);
    error2("  max_tnfa_states=N (default = %d)\n",
           PREPROC_MAX_TNFA_STATES);
    error2("  max_tnfa_transitions=N (default = %d)\n",
           PREPROC_MAX_TNFA_TRANSITIONS);
    error2("  max_tdfa_states=N (default = %d)\n",
           PREPROC_MAX_TDFA_STATES);
    error2("  max_tdfa_transitions=N (default = %d)\n",
           PREPROC_MAX_TDFA_TRANSITIONS);
    error2("  max_tdfa_registers=N (default = %d)\n",
           PREPROC_MAX_TDFA_REGISTERS);
    error2("  max_tdfa_regops=N (default = %d)\n",
           PREPROC_MAX_TDFA_REGOPS);
    error2("  max_tdfa_transition_index_entries=N (default = %d)\n",
           PREPROC_MAX_TDFA_TRANS_INDEX_ENTRIES);
    return;
}
// clang-format on

#define APPLY_BOOL_OPTION(name) \
    do { \
        if (name != NULL) { \
            preproc_config.name = preproc_parse_bool(#name, name); \
        } \
    } while (0)

#define APPLY_INT_OPTION(name, min_value, max_value) \
    do { \
        if (name != NULL) { \
            preproc_config.name \
                = preproc_parse_int32(#name, name, min_value, max_value); \
        } \
    } while (0)

int32
main(int32 argc, char **argv) {
    int32 file_size = 0;
    char *buffer = NULL;
    char *filename = NULL;

    char *help = NULL;
    char *emit_static_dfa = NULL;
    char *emit_tnfa = NULL;
    char *emit_tdfa = NULL;
    char *emit_tdfa_transition_index = NULL;
    char *max_static_dfa_states = NULL;
    char *max_tnfa_tags = NULL;
    char *max_tnfa_states = NULL;
    char *max_tnfa_transitions = NULL;
    char *max_tdfa_states = NULL;
    char *max_tdfa_transitions = NULL;
    char *max_tdfa_registers = NULL;
    char *max_tdfa_regops = NULL;
    char *max_tdfa_transition_index_entries = NULL;

    program = argv[0];

    if (argc < 2) {
        usage();
        exit(EXIT_FAILURE);
    }

    for (int32 i = 1; i < argc; i += 1) {
        int32 arg_len = strlen32(argv[i]);

        PARSE_OPTION(argv[i], help)
        PARSE_OPTION(argv[i], emit_static_dfa)
        PARSE_OPTION(argv[i], emit_tnfa)
        PARSE_OPTION(argv[i], emit_tdfa)
        PARSE_OPTION(argv[i], emit_tdfa_transition_index)
        PARSE_OPTION(argv[i], max_static_dfa_states)
        PARSE_OPTION(argv[i], max_tnfa_tags)
        PARSE_OPTION(argv[i], max_tnfa_states)
        PARSE_OPTION(argv[i], max_tnfa_transitions)
        PARSE_OPTION(argv[i], max_tdfa_states)
        PARSE_OPTION(argv[i], max_tdfa_transitions)
        PARSE_OPTION(argv[i], max_tdfa_registers)
        PARSE_OPTION(argv[i], max_tdfa_regops)
        PARSE_OPTION(argv[i], max_tdfa_transition_index_entries)

        if (memchr64(argv[i], '=', arg_len) != NULL) {
            error("Unknown preprocessor option: %s. Options must use exact "
                  "name=value form.\n",
                  argv[i]);
            usage();
            exit(EXIT_FAILURE);
        }

        filename = argv[i];
    }

    if (help) {
        usage();
        exit(EXIT_SUCCESS);
    }

    APPLY_BOOL_OPTION(emit_static_dfa);
    APPLY_BOOL_OPTION(emit_tnfa);
    APPLY_BOOL_OPTION(emit_tdfa);
    APPLY_BOOL_OPTION(emit_tdfa_transition_index);

    APPLY_INT_OPTION(max_static_dfa_states, 0, META_MAX_STATIC_DFA_STATES);
    APPLY_INT_OPTION(max_tnfa_tags, 0, PREPROC_MAX_TNFA_TAGS);
    APPLY_INT_OPTION(max_tnfa_states, 0, PREPROC_MAX_TNFA_STATES);
    APPLY_INT_OPTION(max_tnfa_transitions, 0, PREPROC_MAX_TNFA_TRANSITIONS);
    APPLY_INT_OPTION(max_tdfa_states, 0, PREPROC_MAX_TDFA_STATES);
    APPLY_INT_OPTION(max_tdfa_transitions, 0, PREPROC_MAX_TDFA_TRANSITIONS);
    APPLY_INT_OPTION(max_tdfa_registers, 0, PREPROC_MAX_TDFA_REGISTERS);
    APPLY_INT_OPTION(max_tdfa_regops, 0, PREPROC_MAX_TDFA_REGOPS);
    APPLY_INT_OPTION(max_tdfa_transition_index_entries, 0,
                     PREPROC_MAX_TDFA_TRANS_INDEX_ENTRIES);

    if (filename == NULL) {
        usage();
        exit(EXIT_FAILURE);
    }

    if ((file_size = read_entire_file(filename, &buffer)) < 0) {
        exit(EXIT_FAILURE);
    }

    {
        RegexList parsed_list = parse_source_code(buffer, file_size);
        generate_source_code(buffer, file_size, &parsed_list, stdout);
        free2(parsed_list.items,
              parsed_list.capacity*SIZEOF(*parsed_list.items));
    }

    free2(buffer, file_size + 1);

    exit(EXIT_SUCCESS);
}

#undef APPLY_BOOL_OPTION
#undef APPLY_INT_OPTION
