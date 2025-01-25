#ifndef PARSER_H
#define PARSER_H

#include "stddefines.h"
#include "area_alloc.h"
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

enum ast_subtype_t {
    AST_COMP_EQ, // ==
    AST_COMP_NEQ, // !=
    AST_COMP_GEQ, // >=
    AST_COMP_LEQ, // <=
    AST_COMP_GR, // >
    AST_COMP_LS, // <

    AST_BLOCK_IMPERATIVE,
};

struct ast_node_t {
    s32     type;
    token_t token;

    b32 braced; // @todo change name to something normal. if it even needed

    u64 left_index;
    u64 right_index;

    u64 list_start_index;
    u64 child_count;
};

struct parser_t {
    b32 had_error;

    area_t<ast_node_t> nodes;
    area_t<u64> root_indices; 
};

b32 parse(scanner_t *scanner, parser_t *state);

#endif // PARSER_H
