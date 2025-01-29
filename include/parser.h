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
    SUBTYPE_AST_UNKNOWN   = -1,

    SUBTYPE_AST_EXPR      = 0,

    SUBTYPE_AST_STRUCT_DEF    = 0x10,
    SUBTYPE_AST_UNION_DEF     = 0x11,
    SUBTYPE_AST_ENUM_DEF      = 0x12,
    SUBTYPE_AST_PROTOFUNC_DEF = 0x13,

    SUBTYPE_AST_UNKN_DECL = 0x20,
    SUBTYPE_AST_FUNC_DECL = 0x21,
    SUBTYPE_AST_VAR_DECL  = 0x22,

    AST_BLOCK_IMPERATIVE,
};

enum ast_funciton_flags_t {
    FUNC_EXTERNAL = 0x00010000,
    FUNC_PROTO    = 0x00100000,
    FUNC_GENERIC  = 0x01000000,
};

struct ast_node_t {
    s32 type;
    s32 subtype;
    u32 funciton_flags;

    token_t token;

    u64 left_index;
    u64 right_index;

    u64 list_start_index;
    u64 child_count;

    u64 scope_index;
};

struct scope_entry_t {
    u32 node_index; 
};

struct parser_t {
    area_t<ast_node_t> nodes;
    area_t<u64> root_indices; 
    area_t<hashmap_t<scope_entry_t>> scopes;
};


b32 parse(scanner_t *scanner, parser_t *state);

#endif // PARSER_H
