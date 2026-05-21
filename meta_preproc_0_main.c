#include "meta_preproc.h"
#include "meta_preproc_1_parse_regex.c"
#include "meta_preproc_1_parse_tnfa.c"
#include "meta_preproc_1_parse_tdfa.c"
#include "meta_preproc_1_parse_source.c"
#include "meta_preproc_2_gen.c"

static void
usage(void) {
    fprintf(stderr, "Usage: %s [options] <file.c>\n", program);
    return;
}

int32
main(int32 argc, char **argv) {
    FILE *input_file = NULL;
    int64 file_size = 0;
    char *buffer = NULL;
    char *filename = NULL;
    char *option;
    char *c;
    program = argv[0];

    if (argc < 2) {
        usage();
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i <= argc; i += 1) {
        PARSE_OPTION(argv[i], option)
        PARSE_OPTION(argv[i], c)
        filename = argv[i];
    }

    if (filename == NULL) {
        usage();
        exit(EXIT_FAILURE);
    }

    if ((input_file = fopen(filename, "r")) == NULL) {
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
