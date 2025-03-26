#ifndef ANALYZER_H
#define ANALYZER_H

#include "compiler.h"
#include "parser.h"
#include "list.h"
#include "hashmap.h"
#include "scanner.h"

struct scope_entry_t {
    b32 not_resolved;
    u32 type;
    ast_node_t *node;

    union {
        u32 type_table_index;

        struct {
            u32 type;
            u32 pointer_depth;
            u32 array_dimensions;
        } variable_def;
    };
};

enum entry_type_t {
    ENTRY_ERROR = 0,
    ENTRY_VAR,
    ENTRY_TYPE,
    ENTRY_PROTOTYPE,
    ENTRY_FUNC,
    ENTRY_UNKN,
};

enum type_t {
    TYPE_ERROR,
    TYPE_STD,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
    TYPE_FUNC,
};

struct scope_t {
    u64 parent_scope;
    hashmap_t<string_t, scope_entry_t> table;
};

struct types_t {
    u32 type;
    u32 size;
    u32 alignment;
    // b32 is_standart;

    ast_node_t *declared_at;

    union {
        list_t<u32> struct_members;
        list_t<u32> union_members;
        list_t<u32> enum_members;
        struct {
            list_t<u32> params;
            list_t<u32> returns;
        } func;

        struct {
            b32 is_void;
            b32 is_float;
            b32 is_unsigned;
        } std;
    };
};

struct analyzer_t {
    list_t<scope_t> scopes;
    list_t<types_t> types;
};

b32 analyze_code(compiler_t *compiler);

#endif // ANALYZER_H
