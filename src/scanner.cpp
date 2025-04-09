#define SCANNER_DEFINITION
#include "scanner.h"
#include "talloc.h"
#include "strings.h"

// @todo for scanner:
// create string builder and replace all of defines

typedef enum {
    PARSE_DIGIT_INT,
    PARSE_HEX_INT,
    PARSE_BIN_INT,
} int_parsing_params;

// --- Processing

static inline u8 advance_char(scanner_t *state) {
    assert(state->file.size > 0);
    assert(state->file.data != 0);
    if (state->file_index >= state->file.size) return 0; 

    u8 curr_code = state->file.data[state->file_index++];
    state->current_char++;

    if (curr_code == '\n') {
        state->current_char = 0;
        state->current_line++;
    }

    return curr_code;
}

static inline void stepback_char(scanner_t *state) {
    assert(state->file.size > 0);
    assert(state->file.data != 0);
    assert(state->file_index > 0);

    state->file_index--;

    if (state->current_char == 0) {
        state->current_char = list_get(&state->lines, --state->current_line)->stop;
    } else {
        state->current_char--;
    }
}

static inline u8 peek_char(scanner_t *state) {
    assert(state->file.data != 0);
    if (state->file.size == 0) return 0;
    if (state->file_index >= state->file.size) return 0; 

    return state->file.data[state->file_index];
}

static inline u8 peek_next_char(scanner_t *state) {
    assert(state->file.data != 0);
    if (state->file.size == 0) return 0;
    if (state->file_index >= (state->file.size - 1)) return 0; 

    return state->file.data[state->file_index + 1];
}

static inline b32 match_char(scanner_t *state, u8 in) {
    return peek_char(state) == in;
}

// --- Helpers

static inline b32 char_is_digit(u8 in) {
    return (in >= '0' && in <= '9');
}

static inline b32 char_is_hex(u8 in) {
    return char_is_digit(in) 
        || (in >= 'A' && in <= 'F')
        || (in >= 'a' && in <= 'f');
}

static inline b32 char_is_bin(u8 in) {
    return (in >= '0' && in <= '1');
}

static inline u8 char_hex_to_int(u8 in) {
    if (!char_is_hex(in)) return 0;
    if (char_is_digit(in))     return in - '0';
    if ((in & 0x20) == 0)       return in - 'A' + 10;
    else                        return in - 'a' + 10;
}

static inline u8 char_bin_to_int(u8 in) {
    if (!char_is_bin(in)) return 0;
    if (in == '1')              return 1;
    else                        return 0;
}

static inline u8 char_digit_to_int(u8 in) {
    if (!char_is_digit(in)) return 0;
    else                     return in - '0';
}

static inline b32 char_is_letter(u8 in) {
    return (in >= 'A' && in <= 'Z')
        || (in >= 'a' && in <= 'z') 
        || (in == '_');
}

static inline b32 char_is_number_or_letter(u8 in) {
    return char_is_digit(in) | char_is_letter(in);
}

// @todo we need to support escape codes
static b32 process_string(scanner_t *state, token_t *token, allocator_t * alloc) {
    assert(alloc != NULL);

    token->type = TOKEN_CONST_STRING;
    token->c0 = state->current_char;
    token->l0 = state->current_line;

    u64 i = 0;
    u64 string_start = state->file_index;

    while (!match_char(state, '"')) {
        token->c1 = state->current_char;
        token->l1 = state->current_line;

        if (match_char(state, 0)) {
            token->type = TOKEN_EOF;

            log_error_token(STRING("Scanner: unexpected End Of Line."), *token);
            return false;
        }

        u8 ch = advance_char(state);

        // case where we just ignore \"
        // we need to parse string instead
        // of just getting reference from array
        // but for now it is okay
        //
        // we will parse it later
        if (ch == '\\') {
            // advance_char(state);
        }

        i++;
    }

    token->c1 = state->current_char; 
    token->l1 = state->current_line;

    advance_char(state);

    string_t identifier = {};

    identifier.size = i;
    identifier.data = (u8*)state->file.data + string_start;

    if (identifier.size != 0) {
        u8 *data = (u8*)mem_alloc(alloc, identifier.size);

        mem_copy(data, identifier.data, identifier.size);

        token->data.string.size = identifier.size;
        token->data.string.data = data;
    } else {
        token->data.string.size = identifier.size;
        token->data.string.data = NULL;
    }

    return true;
}

static token_type_t match_with_keyword(string_t word) {
    u64 matches = 0;

    b32 no_match_flags[_KW_STOP - _KW_START - 1] = { 0 };

    if (word.size >= KEYWORDS_MAX_SIZE) {
        return TOKEN_IDENT;
    }

    for (u64 i = 0; i < (word.size + 1); i++) {
        matches = 0;
        u8 current = word.data[i];

        for (u64 kw = 0; kw < (_KW_STOP - _KW_START - 1); kw++) {
            if (!no_match_flags[kw] && keywords[kw][i] == current) {
                if (current == 0) {
                    return (token_type_t)(kw + _KW_START + 1);
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

static inline u64 int_pow(u64 base, u64 value) {
    u64 result = 1;
    while (value-- > 0) {
        result *= base;
    }
    return result;
}

static u64 parse_const_int(string_t buffer, int_parsing_params type) {
    u64 result = 0;
    
    for (u64 i = 0; i < buffer.size; i++) {
        u64 position = buffer.size - i - 1;

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

static b32 char_is_typed_digit(int_parsing_params parse_type, u8 ch) {
    switch (parse_type) {
        case PARSE_BIN_INT:
            return char_is_bin(ch);
        case PARSE_DIGIT_INT:
            return char_is_digit(ch);
        case PARSE_HEX_INT:
            return char_is_hex(ch);
        default:
            return false;
    }
}

static b32 parse_const_integer(scanner_t *state, string_t *buffer, token_t *token, int_parsing_params parse_type) {
    u64 index = 0;
    u8  ch    = peek_char(state);

    while (ch != 0) {
        if (!char_is_typed_digit(parse_type, ch))
            break;

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
        // @todo better logging
        log_warning(STRING("Scanner: size of a constant is greater than maximum size."));
        buffer->size = (MAX_INT_CONST_SIZE - 1);
        return false;
    } else {
        buffer->size = index;
        return true;
    }
}

static b32 process_number(scanner_t *state, token_t *token) {
    token->c0 = state->current_char;
    token->l0 = state->current_line;

    u8 ch      = peek_char(state);
    u8 next_ch = peek_next_char(state);

    u8 buffer[MAX_INT_CONST_SIZE] = {};
    string_t not_parsed_constant  = {};

    not_parsed_constant.data = (u8*)buffer;

    u64 before_delimeter = 0; 
    u64 after_delimeter  = 0; 

    int_parsing_params type = PARSE_DIGIT_INT;

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

    if (!parse_const_integer(state, &not_parsed_constant, token, type)) {
        token->type = TOKEN_ERROR; 

        log_error(STRING("Couldn't parse integer."));

        return false;
    }

    ch = peek_char(state);
    
    if (ch != '.') {
        token->type = TOKEN_CONST_INT;
        before_delimeter = parse_const_int(not_parsed_constant, type);
        token->data.const_int = before_delimeter;
    } else if (char_is_typed_digit(type, peek_next_char(state))) {
        if (type != PARSE_DIGIT_INT) {
            log_error_token(STRING("floats can be declared only in decimal format."), *token);
            token->type = TOKEN_ERROR;
            return false;
        }

        before_delimeter = parse_const_int(not_parsed_constant, type);

        advance_char(state);
        parse_const_integer(state, &not_parsed_constant, token, type);
        after_delimeter = parse_const_int(not_parsed_constant, type);

        token->type = TOKEN_CONST_FP;

        u64 pow_value = 10;

        switch (type) {
            case PARSE_DIGIT_INT: pow_value = 10; break;
            case PARSE_HEX_INT:   pow_value = 16; break;
            case PARSE_BIN_INT:   pow_value = 2;  break;
        }

        token->data.const_double = (f64)before_delimeter + (f64)after_delimeter / (f64)int_pow(pow_value, not_parsed_constant.size);
    }

    return true;
}

static b32 process_word(scanner_t *state, token_t *token, allocator_t * alloc) {
    assert(alloc != NULL);

    token->c0 = state->current_char;
    token->l0 = state->current_line;

    u8 buffer[MAX_IDENT_SIZE + 1] = { 0 };

    u64 i = 0;

    token->c1 = state->current_char;
    token->l1 = state->current_line;

    while (i < MAX_IDENT_SIZE && char_is_number_or_letter(peek_char(state))) {
        token->c1 = state->current_char;
        token->l1 = state->current_line;
        buffer[i++] = advance_char(state);
    }

    if (i == MAX_IDENT_SIZE) {
        log_error_token(STRING("Scanner: Identifier was too big."), *token);
        token->type = TOKEN_ERROR;
        return false;
    } 

    buffer[i] = 0;

    string_t identifier = {};
    
    identifier.size = i;
    identifier.data = (u8*)buffer;

    token->type = match_with_keyword(identifier);

    if (token->type != TOKEN_IDENT) {
        return true;
    }

    u8 *data = (u8*)mem_alloc(alloc, identifier.size);

    mem_copy(data, identifier.data, identifier.size);

    token->data.string.size = identifier.size;
    token->data.string.data = data;

    return true;
}

void eat_all_spaces(scanner_t *state) {
    u8 ch = peek_char(state);

    while (ch != 0 && (ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t')) {
        advance_char(state); 
        ch = peek_char(state);
    }
}

token_t advance_token(scanner_t *state, allocator_t *alloc) {
    if (alloc == NULL) alloc = default_allocator;
    assert(alloc != NULL);

    eat_all_spaces(state);

    token_t token = {};
    token.from = state;

    if (match_char(state, 0)) {
        token.c0 = token.c1 = state->current_char;
        token.l0 = token.l1 = state->current_line;

        token.type = TOKEN_EOF;
        return token;
    }

    u8 ch = peek_char(state);

    if (ch == '/') {
        if (ch == peek_next_char(state)) {
            while (!(match_char(state, 0) || match_char(state, '\n'))) {
                advance_char(state);
            }

            token = advance_token(state, alloc);
            return token;
        }
    }

    token.c0 = state->current_char;
    token.l0 = state->current_line;
    token.type = advance_char(state); 

    switch(ch) {
        case '/': 
            if ('*' != peek_char(state)) {
                break;
            }

            advance_char(state);

            while (!(match_char(state, 0) || ((peek_char(state) == '*') && (peek_next_char(state) == '/')))) {
                ch = advance_char(state);
            }

            advance_char(state);
            advance_char(state);

            return advance_token(state, alloc);
        case '.': 
        case ',':
        case ':':
        case ';':

        case '[': 
        case ']': 
        case '+': 
        case '*':
        case '%':

        case '(':
        case ')':
        case '{':
        case '}':
        case '@': 
        case '^':
            break;

        case '&': {
            if (match_char(state, '&')) {
                advance_char(state);
                token.type = TOKEN_LOGIC_AND;
            }
        } break;
        case '|': {
            if (match_char(state, '|')) {
                advance_char(state);
                token.type = TOKEN_LOGIC_OR;
            }
        } break;


        case '-': {
            if (match_char(state, '>')) {
                advance_char(state);
                token.type = TOKEN_RET;
            }
        } break;
        case '!': { 
            if (match_char(state, '=')) {
                advance_char(state);
                token.type = TOKEN_NEQ;
            }
        } break;
        case '=': {
            if (match_char(state, '=')) {
                advance_char(state);
                token.type = TOKEN_EQ;
            }
        } break;

        case '<': {
            if (match_char(state, '=')) {
                advance_char(state);
                token.type = TOKEN_LEQ;
            } else if (match_char(state, '<')) {
                advance_char(state);
                token.type = TOKEN_LSHIFT;
            }
        } break;
        case '>': {
            if (match_char(state, '=')) {
                advance_char(state);
                token.type = TOKEN_GEQ;
            } else if (match_char(state, '>')) {
                advance_char(state);
                token.type = TOKEN_RSHIFT;
            }
        } break;

        case '"': {
            process_string(state, &token, alloc); // @todo. do we need to check the value? 
                                           // right now we just ignore the result
                                           // because in failure it returns only TOKEN_EOF
            assert(token.from != NULL);
            return token;
        } break;

        default: {
            if (char_is_letter(ch)) {
                stepback_char(state);
                process_word(state, &token, alloc); // @todo add handlers for false case
            } else if (char_is_digit(ch)) {
                stepback_char(state);
                process_number(state, &token); // @todo add handlers for false case
            } else {
                log_error(STRING("Unexpected symbol in code."));
                token.type = TOKEN_ERROR;
            }
        } break;
    }

    token.c1 = state->current_char;
    token.l1 = state->current_line;

    assert(token.from != NULL);
    return token;
}

// @todo: add caching instead of just cleaning this up 
token_t peek_token(scanner_t *state, allocator_t * alloc) {
    scanner_t peek_state = *state;

    token_t token = advance_token(state, alloc);

    *state = peek_state;

    return token;
}

token_t peek_next_token(scanner_t *state, allocator_t * alloc) {
    scanner_t peek_state = *state;

    token_t token;
    token = advance_token(state, alloc);
    token = advance_token(state, alloc);

    *state = peek_state;
    return token;
}

b32 consume_token(u32 token_type, scanner_t *state, token_t *token, b32 dont_report, allocator_t *alloc) {
    scanner_t peek_state = *state;
    token_t local_token = {};

    if (token == NULL) {
        token  = &local_token;
    }

    *token = peek_token(state, alloc);

    allocator_t *talloc = get_temporary_allocator();

    if (token->type != token_type) {
        if (dont_report) return false;

        string_t name = {};
        name.size = 1;
        name.data = (u8*)&token_type;

        string_t str = string_concat(STRING("Expected token (@todo introspection...)'"),
                       string_concat(name, STRING("' here:"), talloc), talloc);

        log_error_token(str, *token);
        *state = peek_state;
        return false;
    } else {
        advance_token(state, talloc);
    }

    return true;
}

b32 read_file_into_string(string_t filename, allocator_t *alloc, string_t *output) {
    if (alloc == NULL) alloc = default_allocator;

    assert(alloc != NULL);
    assert(output != NULL);
    assert(filename.data != NULL);
    assert(filename.size > 0);

    FILE *file = fopen(string_temp_to_c_string(filename), "rb");

    if (file == NULL) {
        log_error(STRING("Scanner: Could not open file."));
        log_error(filename); 
        return false;
    }

    fseek(file, 0L, SEEK_END);
    u64 file_size = ftell(file);
    rewind(file);

    if (file_size == 0) return false;

    output->data = (u8*)mem_alloc(default_allocator, file_size);

    u64 bytes_read = fread(output->data, sizeof(u8), file_size, file);

    if (bytes_read < file_size) {
        log_error(STRING("Scanner: Could not read file."));
        log_error(filename); 
        return false;
    }

    output->size = file_size;
    fclose(file);

    return true;
}

void scan_lines(scanner_t *state) {
    line_tuple_t line = {};

    for (u64 i = 0; i < state->file.size; i++) {
        if (!(state->file.data[i] == '\n' || state->file.data[i] == 0)) {
            continue;
        }

        line.stop  = i;
        list_add(&state->lines, &line);
        line.start = i + 1;
    }
}

void scanner_close(scanner_t *state) {
    list_delete(&state->lines);
}

b32 scanner_open(string_t *filename, string_t *string, scanner_t *state) {
    state->file = *string;

    u8* buff = (u8*)mem_alloc(default_allocator, filename->size);
    if (buff == NULL) return false;
    mem_copy(buff, filename->data, filename->size);

    state->filename = {};
    state->filename.size = filename->size;
    state->filename.data = buff;

    scan_lines(state);
    return true;
}

void print_lines_of_code(token_t token, s64 start_shift, s64 stop_shift, u64 left_pad) {
    assert(token.l1 >= token.l0);
    scanner_t *state = token.from;

    b32 cancel_empty_line_skip = false;

    s64 line_start = (s64)token.l0;
    s64 line_stop  = (s64)token.l1;

    line_start = (line_start - start_shift) > 0 ? (line_start - start_shift) : line_start;

    u64 stop_pos = line_stop + stop_shift + 1;
    if (stop_pos > state->lines.count) {
        stop_pos = state->lines.count;
    }

    for (u64 i = line_start; i < stop_pos; i++) {
        line_tuple_t line = *list_get(&state->lines, i);

        u64 len   = line.stop - line.start + 1;
        u8 *start = state->file.data + line.start;

        b32 skip_line = true;

        for (u64 j = 0; j < len; j++) {
            if (*(start + j) != ' ' && *(start + j) != '\n' && *(start + j) != '\r') {
                skip_line   = false;
                cancel_empty_line_skip = true;
            }
        }
        
        if (!cancel_empty_line_skip && skip_line && i < token.l0) { 
            cancel_empty_line_skip = true;
            continue;
        }

        add_left_pad(stderr, left_pad);

        if (i < token.l0 || i > token.l1) {
            log_update_color();
            fprintf(stderr, "%4llu | %.*s\n", i + 1, (int)len - 1, start);
        } else {
            u64 token_size = token.c1 - token.c0;

            if (token.l0 != token.l1) {
                log_push_color(ERROR_COLOR); 

                if (i > token.l0 && i < token.l1) {
                    log_update_color();
                    fprintf(stderr, "%4llu | %.*s", i + 1, (int)len, start);
                    log_pop_color();
                } else if (i == token.l0) {
                    log_update_color();
                    fprintf(stderr, "%4llu | %.*s", i + 1, (int)token.c0, start);
                    len -= token.c0;
                    fprintf(stderr, "%.*s", (int)len, start + token.c0);
                    log_pop_color();
                } else if (i == token.l1) {
                    log_update_color();
                    fprintf(stderr, "%4llu | %.*s", i + 1, (int)token.c1, start);
                    len -= token.c1;

                    log_pop_color();
                    log_update_color();
                    fprintf(stderr, "%.*s", (int) len, start + token.c1);
                } else {
                    log_pop_color();
                }
            } else {
                log_update_color();
                fprintf(stderr, "%4llu | %.*s", i + 1, (int)token.c0, start);
                len -= token.c0;

                log_push_color(255, 64, 64); 
                log_update_color();
                fprintf(stderr, "%.*s", (int)token_size, start + token.c0);
                log_pop_color();

                len -= token_size;

                log_update_color();
                fprintf(stderr, "%.*s", (int) len, start + token.c0 + token_size);
            }
        }
    }
}

void print_info(token_t token) {
    log_push_color(255, 255, 255);

    log_update_color();
    fprintf(stderr, "%.*s:%zu,%zu:\n", (int)token.from->filename.size, (char*)token.from->filename.data, token.l0 + 1LL, token.c0 + 1LL);
    log_pop_color();
}

void log_info_token(string_t text, token_t token) {
    log_push_color(255, 255, 255);
    log_info(text);
    print_info(token);
    print_lines_of_code(token, 3, 3, 0);
    log_write(STRING("\n"));
    log_pop_color();
}

void log_warning_token(string_t text, token_t token) {
    log_push_color(WARNING_COLOR);
    log_warning(text);
    print_info(token);

    log_push_color(255, 255, 255);
    print_lines_of_code(token, 3, 3, 0);
    log_write(STRING("\n"));

    log_pop_color();
    log_pop_color();
}

void log_error_token(string_t text, token_t token) {
    log_push_color(ERROR_COLOR);
    log_error(text);
    print_info(token);

    log_push_color(255, 255, 255);
    print_lines_of_code(token, 3, 3, 0);
    log_write(STRING("\n"));

    log_pop_color();
    log_pop_color();
}
