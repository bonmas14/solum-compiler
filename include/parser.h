#ifndef PARSER_H
#define PARSER_H

#include "stddefines.h"
#include "area_alloc.h"
#include "hashmap.h"
#include "scanner.h"
#include "logger.h"

#define INIT_NODES_SIZE (100)

enum ast_types_t {
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
    SUBTYPE_AST_UNKN_DECL  = 0x10,

    SUBTYPE_AST_STRUCT_DEF = 0x11,
    SUBTYPE_AST_UNION_DEF  = 0x12,
    SUBTYPE_AST_ENUM_DEF   = 0x13,
    SUBTYPE_AST_PROTO_DEF  = 0x14,

    SUBTYPE_AST_FUNC = 0x15, 
    SUBTYPE_AST_VAR  = 0x16, 

    // types
    SUBTYPE_AST_AUTO_TYPE = 0x20,
    SUBTYPE_AST_STD_TYPE  = 0x21,
    SUBTYPE_AST_UNKN_TYPE = 0x22,
    SUBTYPE_AST_FUNC_TYPE = 0x23,

    SUBTYPE_AST_FUNC_PARAMS   = 0x30,
    SUBTYPE_AST_FUNC_RETURNS  = 0x31,
    SUBTYPE_AST_FUNC_GENERICS = 0x32,
};

enum funciton_flags_t {
    SCOPE_IS_GENERIC    = 0x00100000,
    SCOPE_FUNC_EXTERNAL = 0x00010000,
    SCOPE_FUNC_PROTO    = 0x00020000,
};

enum scope_entry_types_t {
    SCOPE_VAR,
    SCOPE_FUNC, // generic or not
    SCOPE_TYPE // proto, enum, struct, union
};

struct ast_node_t {
    s32 type;
    s32 subtype;

    token_t token;

    u64 left_index;
    u64 center_index;
    u64 right_index;

    u64 list_next_node;
    u64 child_count;

    u64 scope_index;
};

struct scope_entry_t {
    u32 type;
    u32 flags;
    u32 node_index;
};

struct scope_tuple_t {
    b32 is_global;
    u64 parent_scope;
    hashmap_t<scope_entry_t> scope;
};

struct parser_t {
    area_t<ast_node_t> nodes;
    area_t<u64> root_indices; 
    area_t<scope_tuple_t> scopes;
};


b32 parse(scanner_t *scanner, parser_t *state);

#endif // PARSER_H
