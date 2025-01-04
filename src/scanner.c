#define SCANNER_DEFINITION
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

bool char_is_number(char in) {
    return (in >= '0' && in <= '9');
}

bool char_is_hex_digit(char in) {
    return char_is_number(in) 
        || (in >= 'A' && in <= 'F')
        || (in >= 'a' && in <= 'f');
}

bool char_is_bin_digit(char in) {
    return (in >= '0' && in <= '1');
}

uint8_t char_hex_to_int(char in) {
    if (!char_is_hex_digit(in)) return 0;
    if (char_is_number(in))      return in - '0';
    if ((in & 0x20) == 0)  return in - 'A' + 10;
    else                   return in - 'a' + 10;
}

uint8_t char_bin_to_int(char in) {
    if (!char_is_bin_digit(in)) return 0;
    if (in == '1')         return 1;
    else                   return 0;
}

uint8_t char_digit_to_int(char in) {
    if (!char_is_number(in)) return 0;
    else               return in - '0';
}

bool char_alpha(char in) {
    return (in >= 'A' && in <= 'Z')
        || (in >= 'a' && in <= 'z') 
        || (in == '_');
}

bool char_is_number_or_alpha(char in) {
    return char_is_number(in) || char_alpha(in);
}

bool char_is_special(char in) {
    // null is not a special symbol of ascii, it is OUR null terminator
    return (in > 0 && in <= 31) || in == 127 || in == EOF;
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
        result.c1 = state->current_char;
        result.l1 = state->current_line;

        if (match_char(state, EOF)) {
            state->had_error = true;
            result.type = TOKEN_EOF;

            log_error_token("Scanner: unexpected End Of Line.", state, result, 0);
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
            advance_char(state);
        }
    }

    result.c1 = state->current_char; 
    result.l1 = state->current_line;

    advance_char(state);

    return result;
}

token_type_t match_with_keyword(string_t word) {
    size_t matches = 0;

    bool no_match_flags[_KW_STOP - _KW_START - 1] = { 0 };

    if (word.size >= KEYWORDS_MAX_SIZE) {
        return TOKEN_IDENT;
    }

    for (size_t i = 0; i < (word.size + 1); i++) {
        matches = 0;
        char current = word.data[i];

        for (size_t kw = 0; kw < (_KW_STOP - _KW_START - 1); kw++) {
            if (!no_match_flags[kw] && keywords[kw][i] == current) {
                if (current == 0) {
                    return kw + _KW_START + 1;
                }
                matches++;
            } else {
                no_match_flags[kw] = true;
            }
        }

        if (matches == 0)
            break;
    }

    return TOKEN_IDENT;
}

// @todo
//
// available variants of ints/floats
//
//  ints:
//      0          123    53439      5469450645
//      0x0FAA4B   0xFF   0xAFDEBBFF
//      0b10010111 0b0101
//      
//  floats:
//      0. 0.12 0.325 213.543 
//      0. 0. 0.123d 9.   
//
token_t process_number(scanner_state_t *state) {

    log_info("Scanner: FINISH NUMBER PARSING!", 40);

    token_t token = { 0 };

    token.c0 = state->current_char;
    token.l0 = state->current_line;

    char ch = peek_char(state);

    size_t index = 0;

    while (ch != 0 && char_is_number(ch)) {
        token.c1 = state->current_char;
        token.l1 = state->current_line;

        advance_char(state); // just skip for now
        ch = peek_char(state);
    }

    // or TOKEN_CONST_FP
    token.type = TOKEN_CONST_INT; 

    return token;
}
token_t process_word(scanner_state_t *state) {
    token_t token = { 0 };

    token.c0 = state->current_char;
    token.l0 = state->current_line;

    char buffer[MAX_IDENT_SIZE + 1] = { 0 };

    size_t i = 0;
    buffer[i++] = advance_char(state);

    token.c1 = state->current_char;
    token.l1 = state->current_line;

    while (i < MAX_IDENT_SIZE && char_is_number_or_alpha(peek_char(state))) {
        token.c1 = state->current_char;
        token.l1 = state->current_line;
        buffer[i++] = advance_char(state);
    }

    if (i == MAX_IDENT_SIZE) {
        log_error_token("Scanner: Identifier was too big.", state, token, 0);

        state->had_error = true;
    } 

    buffer[i] = 0;

    string_t identifier = { .size = i, .data = (char*)&buffer };

    if (state->had_error)
        token.type = TOKEN_ERROR;
    else 
        token.type = match_with_keyword(identifier);

    return token;
}

void eat_all_spaces(scanner_state_t *state) {
    char ch = peek_char(state);

    while (ch != 0 && (ch == ' ' || char_is_special(ch))) {
        advance_char(state); 
        ch = peek_char(state);
    }
}

token_t advance_token(scanner_state_t *state) {
    token_t token = { 0 };

    state->had_error = false; // i dont really know do we need to skip? 

    eat_all_spaces(state);

    if (match_char(state, 0)) {
        token.type = TOKEN_EOF;
        return token;
    }

    char ch = peek_char(state);


    if (ch == '/') {
        if (ch == peek_next_char(state)) {
            // comment here 
            
            while (!(match_char(state, 0) || match_char(state, '\n'))) {
                advance_char(state);
            }

            token = advance_token(state);
            return token;
        }
    }

    token.c0 = state->current_char;
    token.l0 = state->current_line;

    switch(ch) {
        case '+': { token.type = advance_char(state); } break;
        case '*': { token.type = advance_char(state); } break;
        case '(': { token.type = advance_char(state); } break;
        case ')': { token.type = advance_char(state); } break;
        case '-': { token.type = advance_char(state); } break;
        case '@': { token.type = advance_char(state); } break;
        case '/': { token.type = advance_char(state); } break;

        case '"': {
            advance_char(state);
            token = process_string(state);
            return token; // @todo error handle errors in process string 
        } break;

        default: {
            if (char_alpha(ch)) {
                token = process_word(state);
            } else if (char_is_number(ch)) {
                token = process_number(state);
            } else {
                advance_char(state);
                token.type = TOKEN_ERROR;
                state->had_error = true;
            }
        } break;
    }

    token.c1 = state->current_char;
    token.l1 = state->current_line;

    if (state->had_error) {
        log_error_token("Scanner: error scanning token.", state, token, 0);
    }

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

// --- Reading file and null terminating it

bool read_file(const char *filename, string_t *output) {
    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        log_error("Scanner: Could not open file.", 0);
        log_error(filename, 0); // @cleanup
        return false;
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    output->data = (char*)malloc(file_size + 1);

    if (output->data == NULL) {
        log_error("Scanner: Couldn't allocate memory for a file.", 0);
        return false;
    }

    size_t bytes_read = fread(output->data, sizeof(char), file_size, file);

    output->data[bytes_read] = 0;

    if (bytes_read < file_size) {
        log_error("Scanner: Could not read file.", 0);
        log_error(filename, 0); // @cleanup
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
        log_error("Scanner: Couldn't create list.", 0);
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
        log_error("Scan: couldn't add another line segment to a list.", 0);
        return false;
    }

    return true;
}

bool scan_file(const char* filename, scanner_state_t *state) {
    if (!read_file(filename, &state->file)) {
        log_error("Scanner: couldn't read file.", 0);
        state->had_error = true;
        return false;
    }

    if (!scan(state)) {
        log_error("Scanner: couldn't scan file.", 0);
        return false;
    }

    return true;
}

// introspect later
void get_token_name(char *buffer, token_t token) {
    switch (token.type) {
        case TOKEN_IDENT:
            sprintf(buffer, "%s", "TOKEN_IDENT");
            break;
        case TOKEN_CONST_INT:
            sprintf(buffer, "%s", "TOKEN_CONST_INT");
            break;
        case TOKEN_CONST_FP:
            sprintf(buffer, "%s", "TOKEN_CONST_FP");
            break;
        case TOKEN_CONST_STRING:
            sprintf(buffer, "%s", "TOKEN_CONST_STRING");
            break;
        case TOK_STRUCT:
            sprintf(buffer, "%s", "TOK_STRUCT");
            break;
        case TOK_UNION:
            sprintf(buffer, "%s", "TOK_UNION");
            break;
        case TOK_U8:
            sprintf(buffer, "%s", "TOK_U8");
            break;
        case TOK_U16:
            sprintf(buffer, "%s", "TOK_U16");
            break;
        case TOK_U32:
            sprintf(buffer, "%s", "TOK_U32");
            break;
        case TOK_U64:
            sprintf(buffer, "%s", "TOK_U64");
            break;
        case TOK_S8:
            sprintf(buffer, "%s", "TOK_S8");
            break;
        case TOK_S16:
            sprintf(buffer, "%s", "TOK_S16");
            break;
        case TOK_S32:
            sprintf(buffer, "%s", "TOK_S32");
            break;
        case TOK_S64:
            sprintf(buffer, "%s", "TOK_S64");
            break;
        case TOK_F32:
            sprintf(buffer, "%s", "TOK_F32");
            break;
        case TOK_F64:
            sprintf(buffer, "%s", "TOK_F64");
            break;
        case TOK_BOOL:
            sprintf(buffer, "%s", "TOK_BOOL");
            break;
        case TOK_NULL:
            sprintf(buffer, "%s", "TOK_NULL");
            break;
        case TOK_DEFAULT:
            sprintf(buffer, "%s", "TOK_DEFAULT");
            break;
        case TOK_IF:
            sprintf(buffer, "%s", "TOK_IF");
            break;
        case TOK_ELSE:
            sprintf(buffer, "%s", "TOK_ELSE");
            break;
        case TOK_WHILE:
            sprintf(buffer, "%s", "TOK_WHILE");
            break;
        case TOK_FOR:
            sprintf(buffer, "%s", "TOK_FOR");
            break;
        case TOK_MUT:
            sprintf(buffer, "%s", "TOK_MUT");
            break;
        case TOK_PROTOTYPE:
            sprintf(buffer, "%s", "TOK_PROTOTYPE");
            break;
        case TOK_EXTERNAL:
            sprintf(buffer, "%s", "TOK_EXTERNAL");
            break;
        case TOK_MODULE:
            sprintf(buffer, "%s", "TOK_MODULE");
            break;
        case TOK_USE:
            sprintf(buffer, "%s", "TOK_USE");
            break;
        case TOKEN_EOF :
            sprintf(buffer, "%s", "TOKEN_EOF");
            break;
        case TOKEN_ERROR:
            sprintf(buffer, "%s", "TOKEN_ERROR");
            break;
        default:
            if (char_is_number_or_alpha(token.type)) {
                sprintf(buffer, "%c", token.type);
            } else {
                sprintf(buffer, "%s", "UNKNOWN");
            }
            break;
    }
}

void print_token_info(token_t token, size_t left_pad) {
    char buffer[256];
    char token_name[100];

    get_token_name((char*)&token_name, token);

    sprintf(buffer, "TOK: %.100s", token_name);
    log_no_dec(buffer, left_pad);
}

void print_code_lines(scanner_state_t *state, token_t token, size_t line_start_offset, size_t line_stop_offset, size_t left_pad) {
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

        for (size_t j = 0; j < left_pad; j++) {
            fprintf(stderr, " ");
        }

        if (i != token.l0) {
            fprintf(stderr, "    %.4zu | %.*s", i, (int)len, start);
        } else {
            fprintf(stderr, " -> %.4zu | %.*s", i, (int)len, start);

            for (size_t j = 0; j < (left_pad + 8); j++) fprintf(stderr, " ");

            fprintf(stderr, " * ");

            for (size_t j = 0; j < token.c0; j++) fprintf(stderr, " ");
            
            size_t line_print_size = 0;

            if (token.c1 < token.c0) {
                size_t token_size = line->stop - token.c0;

                if (token_size != 0) {
                    line_print_size = token_size - 1;
                }
            } else {
                size_t token_size = token.c1 - token.c0;

                if (token_size != 0) {
                    line_print_size = token_size - 1;
                }
            }

            if (line_print_size == 0) {
                fprintf(stderr, "^\n");
                continue;
            }

            fprintf(stderr, "^");
            for (size_t j = 0; j < line_print_size; j++) {
                fprintf(stderr, "-");
            }

            fprintf(stderr, "\n");
        }
    }
}

void log_info_token(const char *text, scanner_state_t *state, token_t token, size_t left_pad) {
    log_info(text, left_pad);
    print_token_info(token, left_pad + LEFT_PAD_STANDART_OFFSET);
    log_no_dec("", 0);
    print_code_lines(state, token, 0, 0, left_pad + LEFT_PAD_STANDART_OFFSET);
    log_no_dec("", 0);
}

void log_warning_token(const char *text, scanner_state_t *state, token_t token, size_t left_pad) {
    log_warning(text, left_pad);
    print_token_info(token, left_pad + LEFT_PAD_STANDART_OFFSET);
    log_no_dec("", 0);
    print_code_lines(state, token, 2, 0, left_pad + LEFT_PAD_STANDART_OFFSET);
    log_no_dec("", 0);
}

void log_error_token(const char *text, scanner_state_t *state, token_t token, size_t left_pad) {
    log_error(text, left_pad);
    print_token_info(token, left_pad + LEFT_PAD_STANDART_OFFSET);
    log_no_dec("", 0);
    print_code_lines(state, token, 4, 0, left_pad + LEFT_PAD_STANDART_OFFSET);
    log_no_dec("", 0);
}
