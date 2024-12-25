#pragma once 

#include "stddefines.h"
#include "logger.h"
#include "list.h"

#define APPROX_CHAR_PER_LINE (25)

typedef enum {
    TOKEN_IDENT = 256,
    TOKEN_CONST = 257,

    _KW_START,
    // 
    _KW_STOP,

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

    size_t file_index;
    size_t current_line;
    size_t current_char;

    // list of line_tuple_t
    list_t   lines; 
    string_t file;
} scanner_state_t; 

typedef struct {
    token_type_t type;
    size_t l0, l1, c0, c1;

    union {
        string_t str;
    };
} token_t;

bool scan_file(const char* filename, scanner_state_t *state);
