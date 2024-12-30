#pragma once 

#include "stddefines.h"
#include "logger.h"
#include "list.h"

#define APPROX_CHAR_PER_LINE (25)

typedef enum {
    TOKEN_IDENT        = 256,
    TOKEN_CONST_INT    = 257,
    TOKEN_CONST_FP     = 258,
    TOKEN_CONST_STRING = 259,

    _KW_START,
    // 
    _KW_STOP,

    TOKEN_EOF   = 2047,
    TOKEN_ERROR = 2048,
} token_type_t;

typedef struct {
    uint32_t size;
    char    *data;
} string_t;

typedef struct  {
    size_t start;
    size_t stop;
} line_tuple_t;

typedef struct {
    bool had_error;
    bool at_the_end;

    size_t file_index;
    size_t current_line;
    size_t current_char;

    // list of line_tuple_t
    list_t   lines; 
    string_t file;
} scanner_state_t; 

typedef struct {
    uint32_t type;
    size_t c0, c1, l0, l1; // address in code. without human readable offsets (add 1 or so on)

} token_t;

bool scan_file(const char* filename, scanner_state_t *state);

token_t advance_token(scanner_state_t *state);
token_t peek_token(scanner_state_t *state, size_t offset);

// --- logging for scanner

void log_info_token(const char *text, scanner_state_t *state, token_t token);
void log_warning_token(const char *text, scanner_state_t *state, token_t token);
void log_error_token(const char *text, scanner_state_t *state, token_t token);

