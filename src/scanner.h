#pragma once 

#include "stddefines.h"
#include "logger.h"
#include "list.h"

#define APPROX_CHAR_PER_LINE (25)
#define KEYWORDS_MAX_SIZE (11)

// do we even need this???
#define MAX_IDENT_SIZE (256)

typedef enum {
    TOKEN_IDENT        = 256,
    TOKEN_CONST_INT    = 257,
    TOKEN_CONST_FP     = 258,
    TOKEN_CONST_STRING = 259,

    _KW_START = 1024,

    TOK_STRUCT,
    TOK_UNION,

    TOK_U8,
    TOK_U16,
    TOK_U32,
    TOK_U64,

    TOK_S8,
    TOK_S16,
    TOK_S32,
    TOK_S64,

    TOK_F32,
    TOK_F64,

    TOK_BOOL,
    TOK_NULL,
    TOK_DEFAULT,

    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,

    TOK_MUT,

    TOK_PROTOTYPE,
    TOK_EXTERNAL,
    // TOK_

    TOK_MODULE,
    TOK_USE,

    _KW_STOP,

    TOKEN_EOF   = 2047,
    TOKEN_ERROR = 2048,
} token_type_t;

#ifdef SCANNER_DEFINITION
char keywords [_KW_STOP - _KW_START - 1][KEYWORDS_MAX_SIZE] = {
    "struct", "union",

    "u8", "u16", "u32", "u64",
    "s8", "s16", "s32", "s64", 
    "f32", "f64",

    "bool", "null", "default",

    "if", "else", "while", "for",

    "mut", "prototype", "external",

    "module", "use"
};
#endif

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
    // address in code. without human readable offsets (add 1 or so on)
    size_t c0, c1, l0, l1;
} token_t;

bool scan_file(const char* filename, scanner_state_t *state);

token_t advance_token(scanner_state_t *state);
token_t peek_token(scanner_state_t *state, size_t offset);

// --- logging for scanner

void log_info_token(const char *text, scanner_state_t *state, token_t token, size_t left_pad);
void log_warning_token(const char *text, scanner_state_t *state, token_t token, size_t left_pad);
void log_error_token(const char *text, scanner_state_t *state, token_t token, size_t left_pad);

