#ifndef SCANNER_H
#define SCANNER_H

#include "list.h"

#define MINIMAL_SIZE (50)
#define KEYWORDS_MAX_SIZE (11)

// do we even need this???
#define MAX_IDENT_SIZE (256)
#define MAX_INT_CONST_SIZE (256)

struct line_tuple_t {
    u64 start;
    u64 stop;
};

struct scanner_t {
    u64 file_index;
    u64 current_line;
    u64 current_char;

    list_t<line_tuple_t> lines; 
    string_t filename;
    string_t file;
}; 

struct token_t {
    u32 type;

    scanner_t *from;
    u32 c0, c1, l0, l1; //  without human readable offsets
    
    union {
        u64 const_int;
        f64 const_double;
        string_t string;
    } data;
};

enum token_type_t {
    TOKEN_GR = '>',
    TOKEN_LS = '<',

    TOKEN_OPEN_BRACE   = '(',
    TOKEN_CLOSE_BRACE  = ')',

    TOKEN_IDENT        = 256,
    TOKEN_CONST_INT    = 257,
    TOKEN_CONST_FP     = 258,
    TOKEN_CONST_STRING = 259,

    TOKEN_EQ  = 300, // ==
    TOKEN_NEQ = 301, // != 
    TOKEN_GEQ = 302, // >=
    TOKEN_LEQ = 303, // <=

    TOKEN_LOGIC_AND = 304, // &&
    TOKEN_LOGIC_OR  = 305, // ||
    TOKEN_LSHIFT    = 306, // <<
    TOKEN_RSHIFT    = 307, // >>

    TOKEN_RET = 400, // ->

    _KW_START = 1024,

    TOK_STRUCT,
    TOK_UNION,
    TOK_ENUM,

    TOK_VOID,
    TOK_TRUE,
    TOK_FALSE,

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

    TOK_BOOL8,
    TOK_BOOL32,

    TOK_CAST,

    TOK_IF,
    TOK_THEN,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_RETURN,

    TOK_BREAK,
    TOK_CONTINUE,

    TOK_EXTERNAL,
    TOK_IT,
    TOK_AS,

    TOK_USE,

    _KW_STOP,

    TOKEN_EOF   = 2047,
    TOKEN_ERROR = 2048,
};

#ifdef SCANNER_DEFINITION
u8 keywords [_KW_STOP - _KW_START - 1][KEYWORDS_MAX_SIZE] = {
    "struct", "union", "enum",

    "void", "true", "false",

    "u8", "u16", "u32", "u64",
    "s8", "s16", "s32", "s64", 
    "f32", "f64",

    "b8", "b32", 

    "cast",

    "if", "then", "else", "while", "for", "return",

    "break", "continue",

    "external", "it", "as",

    "use"
};
#endif

b32  scanner_open(string_t *filename, string_t *string, scanner_t *state);
void scanner_close(scanner_t *state);

token_t advance_token(scanner_t *state, allocator_t * allocator);
b32     consume_token(u32 token_type, scanner_t *state, token_t *token, b32 dont_report, allocator_t * allocator);

token_t peek_token(scanner_t *state, allocator_t * allocator);
token_t peek_next_token(scanner_t *state, allocator_t * allocator);

// --- logging for scanner

void print_lines_of_code(token_t token, s64 start_shift, s64 stop_shift, u64 left_pad);
void log_info_token(string_t text, token_t token);
void log_warning_token(string_t text, token_t token);
void log_error_token(string_t text, token_t token);
#endif // SCANNER_H
