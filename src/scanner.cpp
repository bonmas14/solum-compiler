#define SCANNER_DEFINITION
#include "scanner.h"

typedef enum {
    PARSE_DIGIT_INT,
    PARSE_HEX_INT,
    PARSE_BIN_INT,
} int_parsing_parameters;

// --- Processing

u8 advance_char(scanner_state_t *state) {
    if (state->at_the_end) return 0; 

    u8 curr_code = state->file.data[state->file_index++];
    u8 peek_code = state->file.data[state->file_index];

    if (curr_code == 0 || peek_code == 0) {
        state->at_the_end = true;
    }

    state->current_char++;

    if (curr_code == '\n') {
        state->current_char = 0;
        state->current_line++;
    }

    return curr_code;
}

u8 peek_char(scanner_state_t *state) {
    return state->file.data[state->file_index];
}

u8 peek_next_char(scanner_state_t *state) {
    if (!state->at_the_end) return state->file.data[state->file_index + 1];
    else                    return 0;
}

bool match_char(scanner_state_t *state, u8 in) {
    return peek_char(state) == in;
}

// --- Helpers

bool char_is_digit(u8 in) {
    return (in >= '0' && in <= '9');
}

bool char_is_hex_digit(u8 in) {
    return char_is_digit(in) 
        || (in >= 'A' && in <= 'F')
        || (in >= 'a' && in <= 'f');
}

bool char_is_bin_digit(u8 in) {
    return (in >= '0' && in <= '1');
}
// @todo make all of them inline for @speed

uint8_t char_hex_to_int(u8 in) {
    if (!char_is_hex_digit(in)) return 0;
    if (char_is_digit(in))     return in - '0';
    if ((in & 0x20) == 0)       return in - 'A' + 10;
    else                        return in - 'a' + 10;
}

uint8_t char_bin_to_int(u8 in) {
    if (!char_is_bin_digit(in)) return 0;
    if (in == '1')              return 1;
    else                        return 0;
}

uint8_t char_digit_to_int(u8 in) {
    if (!char_is_digit(in)) return 0;
    else                     return in - '0';
}

bool char_is_letter(u8 in) {
    return (in >= 'A' && in <= 'Z')
        || (in >= 'a' && in <= 'z') 
        || (in == '_');
}

bool char_is_number_or_letter(u8 in) {
    return char_is_digit(in) || char_is_letter(in);
}

bool char_is_special(u8 in) {
    // null is not a special symbol of ascii, it is OUR null terminator
    return (in > 0 && in <= 31) || in == 127;
}

// --- Scanning
/*
token_t create_token_at(scanner_state_t *state, u64 index) {
    u64 local_index = index;
    
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

        if (match_char(state, 0)) {
            state->had_error = true;
            result.type = TOKEN_EOF;

            log_error_token("Scanner: unexpected End Of Line.", state, result, 0);
            break;
        }

        u8 ch = advance_char(state);

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
    u64 matches = 0;

    bool no_match_flags[_KW_STOP - _KW_START - 1] = { 0 };

    if (word.size >= KEYWORDS_MAX_SIZE) {
        return TOKEN_IDENT;
    }

    for (u64 i = 0; i < (word.size + 1); i++) {
        matches = 0;
        u8 current = word.data[i];

        for (u64 kw = 0; kw < (_KW_STOP - _KW_START - 1); kw++) {
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

uint64_t int_pow(uint64_t base, uint64_t value) {
    uint64_t result = 1;
    while (value-- > 0) {
        result *= base;
    }
    return result;
}

uint64_t parse_const_int(string_t buffer, int_parsing_parameters type) {
    uint64_t result = 0;
    
    for (u64 i = 0; i < buffer.size; i++) {
        uint64_t position = buffer.size - i - 1;

        switch (type) {
            case PARSE_HEX_INT: 
                result += int_pow(16, position) * char_hex_to_int(buffer.data[i]);
                break;
            case PARSE_DIGIT_INT:
                result += int_pow(10, position) * char_digit_to_int(buffer.data[i]);
                break;
            case PARSE_BIN_INT:
                result += int_pow(2,  position) * char_bin_to_int(buffer.data[i]);
                break;
        }
    }

    return result;
}


void get_const_int_string(scanner_state_t *state, string_t *buffer, token_t *token, int_parsing_parameters parse_type) {
    u64 index = 0;
    u8 ch      = peek_char(state);

    while (ch != 0) {
        if (parse_type == PARSE_BIN_INT) {
            if (!char_is_bin_digit(ch))
                break;
        }

        if (parse_type == PARSE_DIGIT_INT) {
            if (!char_is_digit(ch)) 
                break;
        }

        if (parse_type == PARSE_HEX_INT) {
            if (!char_is_hex_digit(ch))
                break;
        }

        token->c1 = state->current_char;
        token->l1 = state->current_line;

        ch = advance_char(state);

        if (index < (MAX_INT_CONST_SIZE - 1)) {
            buffer->data[index] = ch; 
        }

        ch = peek_char(state);
        index++;
    }

    if (index >= (MAX_INT_CONST_SIZE - 1)) {
        log_warning("Scanner: size of a constant is greater than maximum size.", 0);
        index = (MAX_INT_CONST_SIZE - 1);
    }

    buffer->size = index;
}

token_t process_number(scanner_state_t *state) {
    token_t token = { 0 };

    token.c0 = state->current_char;
    token.l0 = state->current_line;

    u8 ch      = peek_char(state);
    u8 next_ch = peek_next_char(state);

    u8 buffer[MAX_INT_CONST_SIZE] = { 0 };
    string_t not_parsed_constant = { .data = buffer };

    uint64_t before_delimeter = 0; 
    uint64_t after_delimeter  = 0; 

    int_parsing_parameters type = PARSE_DIGIT_INT;

    if (ch == '0') {
        switch (next_ch) {
            case 'b':
                advance_char(state); 
                advance_char(state); 
                type = PARSE_BIN_INT;
                break;
            case 'x':
                advance_char(state); 
                advance_char(state); 
                type = PARSE_HEX_INT;
                break;
            default: 
                break;
        }
    }

    get_const_int_string(state, &not_parsed_constant, &token, type);

    ch = peek_char(state);
    
    if (ch != '.') {
        token.type = TOKEN_CONST_INT;
        before_delimeter = parse_const_int(not_parsed_constant, type);
        token.data.const_int = before_delimeter;
    } else {
        if (type != PARSE_DIGIT_INT) {
            // what is he cooking?? 
            // 0b0110.0101
            // damn, this is fire 
            //
            // @todo fix it?
            log_warning("Scanner: what? i allow this.", 0);
        }

        before_delimeter = parse_const_int(not_parsed_constant, type);

        advance_char(state);
        get_const_int_string(state, &not_parsed_constant, &token, type);
        after_delimeter = parse_const_int(not_parsed_constant, type);

        token.type = TOKEN_CONST_FP;

        uint64_t pow_value = 10;

        switch (type) {
            case PARSE_DIGIT_INT: pow_value = 10; break;
            case PARSE_HEX_INT:   pow_value = 16; break;
            case PARSE_BIN_INT:   pow_value = 2;  break;
        }

        token.data.const_double = (double)before_delimeter + (double)after_delimeter / (double)int_pow(pow_value, not_parsed_constant.size);
    }

    /*
    if (state->had_error) {
        token.type = TOKEN_ERROR;
    } 
    */

    return token;
}
token_t process_word(scanner_state_t *state) {
    token_t token = { 0 };

    token.c0 = state->current_char;
    token.l0 = state->current_line;

    u8 buffer[MAX_IDENT_SIZE + 1] = { 0 };

    u64 i = 0;
    buffer[i++] = advance_char(state);

    token.c1 = state->current_char;
    token.l1 = state->current_line;

    while (i < MAX_IDENT_SIZE && char_is_number_or_letter(peek_char(state))) {
        token.c1 = state->current_char;
        token.l1 = state->current_line;
        buffer[i++] = advance_char(state);
    }

    if (i == MAX_IDENT_SIZE) {
        log_error_token("Scanner: Identifier was too big.", state, token, 0);

        state->had_error = true;
    } 

    buffer[i] = 0;

    string_t identifier = { .size = i, .data = &buffer };

    if (state->had_error)
        token.type = TOKEN_ERROR;
    else 
        token.type = match_with_keyword(identifier);

    return token;
}

void eat_all_spaces(scanner_state_t *state) {
    u8 ch = peek_char(state);

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

    u8 ch = peek_char(state);


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
            if (char_is_letter(ch)) {
                token = process_word(state);
            } else if (char_is_digit(ch)) {
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
token_t peek_token(scanner_state_t *state, u64 offset) {
    token_t peeked = create_token_at(state, state->file_index);

    u64 file_offset = 0;
    for (u64 i = 0; i < offset; i++) {
        file_offset += peeked.c1 - peeked.c0;

        token_t peeked = create_token_at(state, state->file_index + file_offset);
    }

    return peeked;
}
*/

// --- Reading file and null terminating it

bool read_file(const u8 *filename, string_t *output) {
    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        log_error("Scanner: Could not open file.", 0);
        log_error(filename, 0); // @cleanup
        return false;
    }

    fseek(file, 0L, SEEK_END);
    u64 file_size = ftell(file);
    rewind(file);

    output->data = (u8*)malloc(file_size + 1);

    if (output->data == NULL) {
        log_error("Scanner: Couldn't allocate memory for a file.", 0);
        return false;
    }

    u64 bytes_read = fread(output->data, sizeof(u8), file_size, file);

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

    u64 init_size = state->file.size / APPROX_CHAR_PER_LINE;

    if (!list_create(&state->lines, init_size, sizeof(line_tuple_t))) {
        log_error("Scanner: Couldn't create list.", 0);
        state->had_error = true;
        return false;
    }

    line_tuple_t line = { 0 };
    line.start        = 0;

    for (u64 i = 0; i < state->file.size; i++) {
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

bool scan_file(u8* filename, scanner_state_t *state) {
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
void get_token_name(u8 *buffer, token_t token) {
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
            if (char_is_number_or_letter((u8)token.type)) {
                sprintf(buffer, "%c", token.type);
            } else {
                sprintf(buffer, "%s", "UNKNOWN");
            }
            break;
    }
}

void get_token_info(u8 *buffer, token_t token) {
    switch (token.type) {
        case TOKEN_CONST_INT:
            sprintf(buffer, "%zu", token.data.const_int);
            break;
        case TOKEN_CONST_FP:
            sprintf(buffer, "%f", token.data.const_double);
            break;
        case TOKEN_CONST_STRING:
            sprintf(buffer, "%s", "TOKEN_CONST_STRING");
            break;
        default:
            sprintf(buffer, "NODATA");
            break;
    }
}

void print_token_info(token_t token, u64 left_pad) {
    u8 buffer[256];
    u8 token_name[100];
    u8 token_info[100];

    get_token_name((&token_name, token);
    get_token_info((&token_info, token);
    
    sprintf(buffer, "TOK: %.100s. DATA: %.100s", token_name, token_info);
    log_no_dec(buffer, left_pad);
}

void print_code_lines(scanner_state_t *state, token_t token, u64 line_start_offset, u64 line_stop_offset, u64 left_pad) {
    u64 start_line = token.l0 - line_start_offset;

    if (start_line > token.l0) {
        start_line = 0;
    }

    u64 stop_line = token.l1 + line_stop_offset + 1; 

    if (stop_line > state->lines.count) {
        stop_line = state->lines.count;
    }

    for (u64 i = start_line; i < stop_line; i++) {
        line_tuple_t *line = (line_tuple_t*)list_get(&state->lines, i);

        u64 len    = line->stop - line->start + 1;
        u8  *start = state->file.data + line->start;

        for (u64 j = 0; j < left_pad; j++) {
            fprintf(stderr, " ");
        }

        if (i != token.l0) {
            fprintf(stderr, "    %.4zu | %.*s", i, (int)len, start);
        } else {
            fprintf(stderr, " -> %.4zu | %.*s", i, (int)len, start);

            for (u64 j = 0; j < (left_pad + 8); j++) fprintf(stderr, " ");

            fprintf(stderr, " * ");

            for (u64 j = 0; j < token.c0; j++) fprintf(stderr, " ");
            
            u64 line_print_size = 0;

            if (token.c1 < token.c0) {
                u64 token_size = line->stop - token.c0;

                if (token_size != 0) {
                    line_print_size = token_size - 1;
                }
            } else {
                u64 token_size = token.c1 - token.c0;

                if (token_size != 0) {
                    line_print_size = token_size - 1;
                }
            }

            if (line_print_size == 0) {
                fprintf(stderr, "^\n");
                continue;
            }

            fprintf(stderr, "^");
            for (u64 j = 0; j < line_print_size; j++) {
                fprintf(stderr, "-");
            }

            fprintf(stderr, "\n");
        }
    }
}

void log_info_token(u8 *text, scanner_state_t *state, token_t token, u64 left_pad) {
    log_info(text, left_pad);
    print_token_info(token, left_pad + LEFT_PAD_STANDART_OFFSET);
    log_no_dec("", 0);
    print_code_lines(state, token, 0, 0, left_pad + LEFT_PAD_STANDART_OFFSET);
    log_no_dec("", 0);
}

void log_warning_token(u8 *text, scanner_state_t *state, token_t token, u64 left_pad) {
    log_warning(text, left_pad);
    print_token_info(token, left_pad + LEFT_PAD_STANDART_OFFSET);
    log_no_dec("", 0);
    print_code_lines(state, token, 2, 0, left_pad + LEFT_PAD_STANDART_OFFSET);
    log_no_dec("", 0);
}

void log_error_token(u8 *text, scanner_state_t *state, token_t token, u64 left_pad) {
    log_error(text, left_pad);
    print_token_info(token, left_pad + LEFT_PAD_STANDART_OFFSET);
    log_no_dec("", 0);
    print_code_lines(state, token, 4, 0, left_pad + LEFT_PAD_STANDART_OFFSET);
    log_no_dec("", 0);
}
