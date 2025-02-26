#ifndef PARSER_H
#define PARSER_H

#include "compiler.h"
#include "arena.h"
#include "area_alloc.h"
#include "hashmap.h"
#include "scanner.h"

#define INIT_NODES_SIZE (100)

enum ast_types_t {
    AST_EMPTY,
    AST_LEAF,
    AST_UNARY,
    AST_BIN,
    AST_TERN,
    AST_LIST,
    AST_ERROR = -1,
};

enum ast_subtype_t {
    SUBTYPE_AST_EXPR       = 0x00,
    SUBTYPE_AST_IF_STMT    = 0x01,
    SUBTYPE_AST_ELSE_STMT  = 0x02,
    SUBTYPE_AST_ELIF_STMT  = 0x03,
    SUBTYPE_AST_WHILE_STMT = 0x04,
    SUBTYPE_AST_RET_STMT   = 0x05,

    AST_BLOCK_IMPERATIVE   = 0x06,

    // DECLARATIONS 
    // VALUE NAME , L TYPE , R EXPR / BLOCK (DATA, CODE) / default / keyword

    SUBTYPE_AST_UNKN_DEF   = 0x10,

    SUBTYPE_AST_STRUCT_DEF = 0x11,
    SUBTYPE_AST_UNION_DEF  = 0x12,
    SUBTYPE_AST_ENUM_DEF   = 0x13,

    SUBTYPE_AST_PARAM_DEF  = 0x14, 

    // types
    SUBTYPE_AST_AUTO_TYPE = 0x20,
    SUBTYPE_AST_STD_TYPE  = 0x21,
    SUBTYPE_AST_UNKN_TYPE = 0x22,
    SUBTYPE_AST_FUNC_TYPE = 0x23,
    SUBTYPE_AST_ARR_TYPE  = 0x24,
    SUBTYPE_AST_PTR_TYPE  = 0x25,

    SUBTYPE_AST_FUNC_PARAMS   = 0x30,
    SUBTYPE_AST_FUNC_RETURNS  = 0x31,
    SUBTYPE_AST_FUNC_GENERICS = 0x32,
};


struct ast_node_t {
    s32 type;
    s32 subtype;

    token_t token;

    ast_node_t *left;
    ast_node_t *center;
    ast_node_t *right;

    ast_node_t *list_next;
    ast_node_t *list_start;
    u64 child_count;
};

struct parser_t {
    arena_t *nodes;
    area_t<ast_node_t*> roots; 
};

b32 parse(compiler_t *compiler);

#endif // PARSER_H
