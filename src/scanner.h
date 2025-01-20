#ifndef SCANNER_H
#define SCANNER_H

#include "stddefines.h"
#include "logger.h"
#include "list.h"

#define APPROX_CHAR_PER_LINE (25)
#define MINIMAL_SIZE (50)
#define KEYWORDS_MAX_SIZE (11)

// do we even need this???
#define MAX_IDENT_SIZE (256)
#define MAX_INT_CONST_SIZE (256)

enum token_type_t {
    TOKEN_OPEN_BRACE   = '(',
    TOKEN_CLOSE_BRACE  = ')',

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
};

#ifdef SCANNER_DEFINITION
u8 keywords [_KW_STOP - _KW_START - 1][KEYWORDS_MAX_SIZE] = {
    "struct", "union",

    "u8", "u16", "u32", "u64",
    "s8", "s16", "s32", "u64", 
    "f32", "f64",

    "b32", "null", "default",

    "if", "else", "while", "for",

    "mut", "prototype", "external",

    "module", "use"
};
#endif

// @todo: we need to move it away from scanner
struct string_t {
    u64 size;
    u8 *data;
};

struct line_tuple_t {
    u64 start;
    u64 stop;
};

struct scanner_state_t {
    b32 had_error;
    b32 at_the_end;

    u64 file_index;
    u64 current_line;
    u64 current_char;

    // list of line_tuple_t
    list_t   lines; 
    string_t file;
}; 

struct token_t {
    u32 type;
    // address in code. without human readable offsets (add 1 or so on)
    u64 c0, c1, l0, l1;
    union {
        u64 const_int;
        f64 const_double;
    } data;
};

b32 scan_file(const u8* filename, scanner_state_t *state);

token_t advance_token(scanner_state_t *state);
b32     consume_token(u32 token_type, scanner_state_t *state);
token_t peek_token(scanner_state_t *state);

// --- logging for scanner

void log_info_token(const u8 *text, scanner_state_t *state, token_t token, u64 left_pad);
void log_warning_token(const u8 *text, scanner_state_t *state, token_t token, u64 left_pad);
void log_error_token(const u8 *text, scanner_state_t *state, token_t token, u64 left_pad);
#endif // SCANNER_H
