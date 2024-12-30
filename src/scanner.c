#include "scanner.h"

// --- Processing

char advance_char(scanner_state_t *state) {
    if (state->at_the_end) return 0; 

    char curr_code = state->file.data[state->file_index++];
    char peek_code = state->file.data[state->file_index];

    if (curr_code == 0 || curr_code == EOF || peek_code == 0 || peek_code == EOF) {
        state->at_the_end = true;
    }

    state->current_char++;

    if (curr_code == '\n') {
        state->current_char = 0;
        state->current_line++;
    }

    return curr_code;
}

char peek_char(scanner_state_t *state) {
    return state->file.data[state->file_index];
}

char peek_next_char(scanner_state_t *state) {
    if (!state->at_the_end) return state->file.data[state->file_index + 1];
    else                    return EOF;
}

bool match_char(scanner_state_t *state, char in) {
    return peek_char(state) == in;
}

// --- Helpers

bool char_is_digit(char in) {
    return (in >= '0' && in <= '9');
}

bool char_is_hex_digit(char in) {
    return char_is_digit(in) 
        || (in >= 'A' && in <= 'F')
        || (in >= 'a' && in <= 'f');
}

bool char_is_bin_digit(char in) {
    return (in >= '0' && in <= '1');
}

uint8_t char_hex_to_int(char in) {
    if (!char_is_hex_digit(in)) return 0;
    if (char_is_digit(in))      return in - '0';
    if ((in & 0x20) == 0)  return in - 'A' + 10;
    else                   return in - 'a' + 10;
}

uint8_t char_bin_to_int(char in) {
    if (!char_is_bin_digit(in)) return 0;
    if (in == '1')         return 1;
    else                   return 0;
}

uint8_t char_digit_to_int(char in) {
    if (!char_is_digit(in)) return 0;
    else               return in - '0';
}

bool char_is_letter(char in) {
    return (in >= 'A' && in <= 'Z')
        || (in >= 'a' && in <= 'z') 
        || (in == '_');
}

bool char_is_digit_or_letter(char in) {
    return char_is_digit(in) || char_is_letter(in);
}

bool char_is_special(char in) {
    return (in >= 0 && in <= 31) || in == 127 || in == EOF;
}

// --- Scanning
/*
token_t create_token_at(scanner_state_t *state, size_t index) {
    size_t local_index = index;
    
}
*/

// @todo we need to support escape codes
token_t process_string(scanner_state_t *state) {
    token_t result = { 0 };

    result.type = TOKEN_CONST_STRING;
    result.c0 = state->current_char;  // it is already after "
    result.l0 = state->current_line;

    while (!match_char(state, '"')) {
        if (match_char(state, EOF)) {
            state->had_error = true;
            result.type = TOKEN_EOF;

            result.c1 = state->current_char;
            result.l1 = state->current_line;

            log_error_token("Scanner: unexpected End Of Line.", state, result);
            break;
        }

        char ch = advance_char(state);

        // case where we just ignore \"
        // we need to parse string instead
        // of just getting reference from array
        // but for now it is okay
        //
        // we will parse it later
        if (ch == '\\') {
            if (!match_char(state, '"')) {
                continue;
            }

            advance_char(state);
        }
    }

    result.c1 = state->current_char; 
    result.l1 = state->current_line;

    advance_char(state);

    return result;
}

token_t advance_token(scanner_state_t *state) {
    token_t token = { 0 };

consume:
    state->had_error = false; 
    token    = (token_t){ 0 };

    token.c0 = state->current_char;
    token.l0 = state->current_line;

    char ch = advance_char(state);

    if (ch == 0) {
        token.type = TOKEN_EOF;
        return token;
    }

    if (char_is_special(ch) || ch == ' ') 
        goto consume;

    token.type = ch;

    switch(ch) {
        case '+': break;
        case '*': break;

        case '(': break;
        case ')': break;

        case '-': break;
        case '^': break;
        case '@': break;
        case '"': 
            token = process_string(state);
            break;

        case '/':
            if (!match_char(state, '/'))
                break;

            while (!match_char(state, '\n') && !state->at_the_end)
                advance_char(state);
            goto consume;

        default:
            if (char_is_letter(ch)) {
                // token = process_word(state);
            } else if (char_is_digit(ch)) {
                // token = process_digit(state);
            } else {
                token.type = TOKEN_ERROR;

                token.c1 = state->current_char;
                token.l1 = state->current_line;

                state->had_error = true;

                log_error_token("Scanner: unexpected token.", state, token);
            }
            break;
    }

    token.c1 = state->current_char;
    token.l1 = state->current_line;

    return token;
}

/*
// use peek_token(state, 0); to peek token that is at current index
token_t peek_token(scanner_state_t *state, size_t offset) {
    token_t peeked = create_token_at(state, state->file_index);

    size_t file_offset = 0;
    for (size_t i = 0; i < offset; i++) {
        file_offset += peeked.c1 - peeked.c0;

        token_t peeked = create_token_at(state, state->file_index + file_offset);
    }

    return peeked;
}
*/

// --- Reading file 

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

bool scan(scanner_state_t *state) {
    state->lines = (list_t){ 0 };

    size_t init_size = state->file.size / APPROX_CHAR_PER_LINE;

    if (!list_create(&state->lines, init_size, sizeof(line_tuple_t))) {
        log_error("Scanner: Couldn't create list.");
        state->had_error = true;
        return false;
    }

    line_tuple_t line = { 0 };
    line.start        = 0;

    for (size_t i = 0; i < state->file.size; i++) {
        if (state->file.data[i] != '\n') {
            continue;
        }

        line.stop  = i;

        if (!list_add(&state->lines, (void*)&line)) {
            state->had_error = true;
            break;
        }

        line.start = i + 1;
    }

    if (state->had_error) {
        log_error("Scan: couldn't add another line segment to a list.");
        return false;
    }

    return true;
}

bool scan_file(const char* filename, scanner_state_t *state) {
    if (!read_file(filename, &state->file)) {
        log_error("Scanner: couldn't read file.");
        state->had_error = true;
        return false;
    }

    if (!scan(state)) {
        log_error("Scanner: couldn't scan file.");
        return false;
    }

    return true;
}

void print_token_info(scanner_state_t *state, token_t token, size_t line_start_offset, size_t line_stop_offset) {

    size_t start_line = token.l0 - line_start_offset;

    if (start_line > token.l0) {
        start_line = 0;
    }

    size_t stop_line = token.l1 + line_stop_offset + 1; 

    if (stop_line > state->lines.count) {
        stop_line = state->lines.count;
    }

    for (size_t i = start_line; i < stop_line; i++) {

        line_tuple_t *line  = (line_tuple_t*)list_get(&state->lines, i);
        size_t        len   = line->stop - line->start + 1;
        char         *start = state->file.data + line->start;

        if (i != token.l0) {
            fprintf(stdout, "    %.3zu |%.*s", i, (int)len, start);
        } else {
            fprintf(stdout, " -> %.3zu |%.*s", i, (int)len, start);

            fprintf(stdout, "        *");

            for (size_t j = 0; j < token.c0; j++) {
                fprintf(stdout, " ");
            }

            size_t token_size = token.c1 - token.c0;

            if (token_size == 1) {
                fprintf(stdout, "^\n");
                continue;
            }

            fprintf(stdout, "^");
            for (size_t j = 0; j < (token_size - 2); j++) {
                fprintf(stdout, "-");
            }

            fprintf(stdout, "\n");
        }
    }
}

void log_info_token(const char *text, scanner_state_t *state, token_t token) {
    log_info(text);
    print_token_info(state, token, 1, 0);
}

void log_warning_token(const char *text, scanner_state_t *state, token_t token) {
    log_warning(text);
    print_token_info(state, token, 2, 0);
}
void log_error_token(const char *text, scanner_state_t *state, token_t token) {
    log_error(text);
    print_token_info(state, token, 3, 0);
}
