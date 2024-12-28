#pragma once 

#include "stddefines.h"
#include "list.h"

typedef enum {
    AST_BIN_MATH,
    AST_UNARY_MATH,
    AST_ERROR = 2048,
} ast_types_t;

// typedef struct {
// } ast_node_t;

typedef struct {
    bool had_error;

    size_t token_index;

    // tree here
} parser_state_t;


