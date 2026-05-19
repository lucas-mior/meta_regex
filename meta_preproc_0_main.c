#include "meta_preproc.h"
#include "meta_preproc_1_parse.c"
#include "meta_preproc_2_gen.c"

int32
main(int32 argc, char **argv) {
    FILE *input_file = NULL;
    int64 file_size = 0;
    char *buffer = NULL;

    if (argc < 2) {
        fprintf(stderr, "Usage: preprocessor <file.c>\n");
        exit(EXIT_FAILURE);
    }

    input_file = fopen(argv[1], "r");
    if (input_file == NULL) {
        fprintf(stderr, "Error opening file.\n");
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
