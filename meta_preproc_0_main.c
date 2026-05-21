#include "meta_preproc.h"
#include <errno.h>
#include "meta_preproc_1_parse_regex.c"
#include "meta_preproc_1_parse_tnfa.c"
#include "meta_preproc_1_parse_tdfa.c"
#include "meta_preproc_1_parse_source.c"
#include "meta_preproc_2_gen.c"

static int32
preproc_parse_int32(char *name, char *value, int32 min_value, int32 max_value) {
    char *end = NULL;
    int64 parsed;

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

static int32
preproc_parse_bool(char *name, char *value) {
    if (strcmp(value, "1") == 0 || strcmp(value, "true") == 0
        || strcmp(value, "yes") == 0 || strcmp(value, "on") == 0) {
        return 1;
    }
    if (strcmp(value, "0") == 0 || strcmp(value, "false") == 0
        || strcmp(value, "no") == 0 || strcmp(value, "off") == 0) {
        return 0;
    }
    error("Invalid value for %s: %s. Expected 0/1, true/false, yes/no, or "
          "on/off.\n",
          name, value);
    exit(EXIT_FAILURE);
}

static int32
preproc_option_is(char *arg, char *name) {
    return strcmp(arg, name) == 0
           || (arg[0] == '-' && arg[1] == '-' && strcmp(arg + 2, name) == 0);
}

static bool
preproc_parse_config_option(char *arg) {
    char *value = NULL;

    if (preproc_option_is(arg, "help")) {
        return false;
    }

    if (preproc_option_is(arg, "no-static-dfa")) {
        preproc_config.emit_static_dfa = 0;
        return true;
    }
    if (preproc_option_is(arg, "no-tnfa")) {
        preproc_config.emit_tnfa = 0;
        return true;
    }
    if (preproc_option_is(arg, "no-tdfa")) {
        preproc_config.emit_tdfa = 0;
        return true;
    }
    if (preproc_option_is(arg, "no-tdfa-transition-index")) {
        preproc_config.emit_tdfa_transition_index = 0;
        return true;
    }

    if (parse_option(&value, arg, "emit_static_dfa")
        || parse_option(&value, arg, "emit-static-dfa")
        || parse_option(&value, arg, "enable_static_dfa")
        || parse_option(&value, arg, "enable-static-dfa")) {
        preproc_config.emit_static_dfa
            = preproc_parse_bool("emit_static_dfa", value);
        return true;
    }
    if (parse_option(&value, arg, "emit_tnfa")
        || parse_option(&value, arg, "emit-tnfa")
        || parse_option(&value, arg, "enable_tnfa")
        || parse_option(&value, arg, "enable-tnfa")) {
        preproc_config.emit_tnfa = preproc_parse_bool("emit_tnfa", value);
        return true;
    }
    if (parse_option(&value, arg, "emit_tdfa")
        || parse_option(&value, arg, "emit-tdfa")
        || parse_option(&value, arg, "enable_tdfa")
        || parse_option(&value, arg, "enable-tdfa")) {
        preproc_config.emit_tdfa = preproc_parse_bool("emit_tdfa", value);
        return true;
    }
    if (parse_option(&value, arg, "emit_tdfa_transition_index")
        || parse_option(&value, arg, "emit-tdfa-transition-index")
        || parse_option(&value, arg, "enable_tdfa_transition_index")
        || parse_option(&value, arg, "enable-tdfa-transition-index")) {
        preproc_config.emit_tdfa_transition_index
            = preproc_parse_bool("emit_tdfa_transition_index", value);
        return true;
    }

    if (parse_option(&value, arg, "max_static_dfa_states")
        || parse_option(&value, arg, "max-static-dfa-states")) {
        preproc_config.max_static_dfa_states = preproc_parse_int32(
            "max_static_dfa_states", value, 0, META_MAX_STATIC_DFA_STATES);
        return true;
    }
    if (parse_option(&value, arg, "max_tnfa_tags")
        || parse_option(&value, arg, "max-tnfa-tags")) {
        preproc_config.max_tnfa_tags = preproc_parse_int32(
            "max_tnfa_tags", value, 0, PREPROC_MAX_TNFA_TAGS);
        return true;
    }
    if (parse_option(&value, arg, "max_tnfa_states")
        || parse_option(&value, arg, "max-tnfa-states")) {
        preproc_config.max_tnfa_states = preproc_parse_int32(
            "max_tnfa_states", value, 0, PREPROC_MAX_TNFA_STATES);
        return true;
    }
    if (parse_option(&value, arg, "max_tnfa_transitions")
        || parse_option(&value, arg, "max-tnfa-transitions")) {
        preproc_config.max_tnfa_transitions = preproc_parse_int32(
            "max_tnfa_transitions", value, 0, PREPROC_MAX_TNFA_TRANSITIONS);
        return true;
    }
    if (parse_option(&value, arg, "max_tdfa_states")
        || parse_option(&value, arg, "max-tdfa-states")) {
        preproc_config.max_tdfa_states = preproc_parse_int32(
            "max_tdfa_states", value, 0, PREPROC_MAX_TDFA_STATES);
        return true;
    }
    if (parse_option(&value, arg, "max_tdfa_transitions")
        || parse_option(&value, arg, "max-tdfa-transitions")) {
        preproc_config.max_tdfa_transitions = preproc_parse_int32(
            "max_tdfa_transitions", value, 0, PREPROC_MAX_TDFA_TRANSITIONS);
        return true;
    }
    if (parse_option(&value, arg, "max_tdfa_registers")
        || parse_option(&value, arg, "max-tdfa-registers")) {
        preproc_config.max_tdfa_registers = preproc_parse_int32(
            "max_tdfa_registers", value, 0, PREPROC_MAX_TDFA_REGISTERS);
        return true;
    }
    if (parse_option(&value, arg, "max_tdfa_regops")
        || parse_option(&value, arg, "max-tdfa-regops")) {
        preproc_config.max_tdfa_regops = preproc_parse_int32(
            "max_tdfa_regops", value, 0, PREPROC_MAX_TDFA_REGOPS);
        return true;
    }
    if (parse_option(&value, arg, "max_tdfa_transition_index_entries")
        || parse_option(&value, arg, "max-tdfa-transition-index-entries")) {
        preproc_config.max_tdfa_transition_index_entries
            = preproc_parse_int32("max_tdfa_transition_index_entries", value, 0,
                                  PREPROC_MAX_TDFA_TRANS_INDEX_ENTRIES);
        return true;
    }

    return false;
}

static void
usage(void) {
    fprintf(stderr, "Usage: %s [options] <file.c>\n", program);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --no-static-dfa | --emit_static_dfa=0\n");
    fprintf(stderr, "  --no-tnfa       | --emit_tnfa=0\n");
    fprintf(stderr, "  --no-tdfa       | --emit_tdfa=0\n");
    fprintf(stderr, "  --no-tdfa-transition-index\n");
    fprintf(stderr, "  --max_static_dfa_states=N\n");
    fprintf(stderr, "  --max_tnfa_tags=N\n");
    fprintf(stderr, "  --max_tnfa_states=N\n");
    fprintf(stderr, "  --max_tnfa_transitions=N\n");
    fprintf(stderr, "  --max_tdfa_states=N\n");
    fprintf(stderr, "  --max_tdfa_transitions=N\n");
    fprintf(stderr, "  --max_tdfa_registers=N\n");
    fprintf(stderr, "  --max_tdfa_regops=N\n");
    fprintf(stderr, "  --max_tdfa_transition_index_entries=N\n");
    return;
}

int32
main(int32 argc, char **argv) {
    FILE *input_file = NULL;
    int64 file_size = 0;
    char *buffer = NULL;
    char *filename = NULL;
    program = argv[0];

    if (argc < 2) {
        usage();
        exit(EXIT_FAILURE);
    }

    for (int32 i = 1; i < argc; i += 1) {
        if (preproc_option_is(argv[i], "help")) {
            usage();
            exit(EXIT_SUCCESS);
        }
        if (preproc_parse_config_option(argv[i])) {
            continue;
        }
        filename = argv[i];
    }

    if (filename == NULL) {
        usage();
        exit(EXIT_FAILURE);
    }

    input_file = fopen(filename, "r");
    if (input_file == NULL) {
        error("Error opening %s: %s.\n", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    fseek(input_file, 0, SEEK_END);
    file_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);

    buffer = malloc2(file_size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Error allocating memory.\n");
        exit(EXIT_FAILURE);
    }

    if (fread64(buffer, 1, file_size, input_file) != file_size) {
        fprintf(stderr, "Error reading file.\n");
        exit(EXIT_FAILURE);
    }
    buffer[file_size] = '\0';
    fclose(input_file);

    {
        RegexList parsed_list = parse_source_code(buffer, file_size);
        generate_source_code(buffer, file_size, &parsed_list, stdout);
        free2(parsed_list.items,
              parsed_list.capacity*SIZEOF(*parsed_list.items));
    }

    free2(buffer, file_size + 1);

    exit(EXIT_SUCCESS);
}
