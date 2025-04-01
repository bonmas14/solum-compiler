#ifndef ANALYZER_H
#define ANALYZER_H

#include "list.h"
#include "hashmap.h"

struct compiler_t;
struct ast_node_t;

#define MAX_COUNT_OF_PARAMS 16
#define MAX_COUNT_OF_RETS 1 

struct scope_entry_t {
    b32 unknown_type;
    u32 entry_type;
    ast_node_t *node;

    union {
        u32 type_index;

        struct {
            u32 type_index;
            u32 pointer_depth;
            u32 array_dimensions;
        } variable_def;

        struct {
            list_t<u32> params;
            list_t<u32> returns;
        } func;

        string_t use_filename;
    };
};

enum entry_type_t {
    ENTRY_ERROR = 0,
    ENTRY_VAR,
    ENTRY_TYPE,
    ENTRY_PROTOTYPE,
    ENTRY_FUNC,
    ENTRY_NAMESPACE,
};

enum type_t {
    TYPE_ERROR,
    TYPE_STD,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
};

struct types_t {
    u32 type;
    u32 size;
    u32 alignment;

    union {
        list_t<u32> struct_members;
        list_t<u32> union_members;
        list_t<u32> enum_members;

        struct {
            b32 is_void;
            b32 is_float;
            b32 is_unsigned;
        } std;
    };
};

struct analyzer_t {
    hashmap_t<string_t, scope_entry_t> global_scope;
    list_t<types_t> types;
};

b32 pre_analyze_file(compiler_t *compiler, string_t filename);
b32 resolve_everything(compiler_t *compiler);

#endif // ANALYZER_H
