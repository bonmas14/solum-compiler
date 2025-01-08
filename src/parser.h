#ifndef PARSER_H
#define PARSER_H

#include "stddefines.h"
#include "list.h"

enum ast_types_t {
    AST_UNARY,
    AST_BIN,
    AST_TERNARY,
    AST_LIST,
    AST_ERROR = -1,
};

struct ast_node_t {
    s32 type;
};

typedef struct parser_state_t {
    b32 had_error;

    s64 token_index;

    // tree here
    list_t elements;
};


#endif // PARSER_H
