#ifndef PARSER_H
#define PARSER_H

#include "stddefines.h"
#include "list.h"
#include "scanner.h"
#include "logger.h"

#define INIT_NODES_SIZE (100)

enum ast_types_t {
    AST_LEAF,
    AST_UNARY,
    AST_BIN,
    AST_LIST,
    AST_ERROR = -1,
};

struct ast_node_t {
    s32     type;
    token_t token;

    u64 left_index;
    u64 right_index;

    u64 list_start_index;
    u64 child_count;
};

struct parser_state_t {
    b32 had_error;

    // tree here
    ast_node_t root_node;
    list_t     nodes;
};

b32 parse(scanner_state_t *scanner, parser_state_t *state);

#endif // PARSER_H
