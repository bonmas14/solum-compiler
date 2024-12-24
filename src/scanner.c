#include "scanner.h"

bool read_file(const char *filename, string_t *output) {
    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        log_error("Scanner: Could not open file.");
        log_error(filename); // @cleanup
        return false;
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    output->data = (char*)malloc(file_size + 1);

    if (output->data == NULL) {
        log_error("Scanner: Couldn't allocate memory for a file.");
        return false;
    }

    size_t bytes_read = fread(output->data, sizeof(char), file_size, file);

    output->data[bytes_read] = 0;

    if (bytes_read < file_size) {
        log_error("Scanner: Could not read file.");
        log_error(filename); // @cleanup
        return false;
    }

    output->size = file_size;

    fclose(file);
    return true;
}

bool scan_file(const char* filename, scanner_state_t *state) {
    if (!read_file(filename, &state->file)) {
        state->had_error = true;
        return false;
    }

    /*
    dynlist_t tokens = init_dynlist(INIT_TOKEN_COUNT, sizeof(token_t));

    while (!match('\0')) {
        if (tokens.count >= (file_size / sizeof(char))) break;

        token_t tok = scan_next();

        dynlist_add(&tokens, &tok);
    }


    state.count = tokens.count;
    state.tokens = tokens.data;

    scanner_state_t output = state;
    state = (scanner_state_t) { 0 };
    */
    return true;
}
