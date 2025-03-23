#ifndef PARSER_H
#define PARSER_H

#include "compiler.h"
#include "scanner.h"

struct ast_node_t {
    s32 type;
    b32 analyzed;

    token_t token;

    ast_node_t *left;
    ast_node_t *center;
    ast_node_t *right;

    ast_node_t *list_next;
    ast_node_t *list_start;

    u64 child_count;
};

struct parser_t {
    list_t<ast_node_t*> parsed_roots;
};

enum ast_types_t {
    AST_ERROR,
    AST_EMPTY,

    AST_PRIMARY,

    AST_BIN_ASSIGN,
    AST_BIN_SEPARATION,

    AST_BIN_CAST,
    AST_BIN_COMP,

    AST_BIN_LOG_OR,
    AST_BIN_LOG_AND,

    AST_BIN_ADD,
    AST_BIN_MUL,

    AST_BIN_BIT_XOR,
    AST_BIN_BIT_OR,
    AST_BIN_BIT_AND,
    AST_BIN_BIT_SHIFT,

    AST_UNARY,

    AST_FUNC_CALL,
    AST_MEMBER_ACCESS,
    AST_ARRAY_ACCESS,


    AST_IF_STMT,
    AST_ELSE_STMT,
    AST_ELIF_STMT,
    AST_WHILE_STMT,
    AST_RET_STMT,

    AST_BLOCK_IMPERATIVE,
    AST_BLOCK_ENUM,

    // DECLARATIONS 
    // VALUE NAME , L TYPE , R EXPR / BLOCK (DATA, CODE) / default / keyword

    AST_PARAM_DEF, 

    AST_UNARY_UNKN_DEF, // uninitialize definitions... never a function
    AST_BIN_UNKN_DEF,
    AST_TERN_UNKN_DEF, // multiple definitions... never a function

    AST_STRUCT_DEF,
    AST_UNION_DEF,
    AST_ENUM_DEF,


    // types
    AST_MUL_AUTO,
    AST_MUL_TYPES,

    // 
    AST_AUTO_TYPE,

    AST_STD_TYPE,
    AST_UNKN_TYPE,
    AST_FUNC_TYPE,

    AST_ARR_TYPE,
    AST_PTR_TYPE,

    AST_FUNC_PARAMS,
    AST_FUNC_RETURNS,
};

b32 parse(compiler_t *compiler);

#endif // PARSER_H
