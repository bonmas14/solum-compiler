#ifndef ANALYZER_H
#define ANALYZER_H

#include "list.h"
#include "hashmap.h"

struct compiler_t;
struct ast_node_t;

#define MAX_COUNT_OF_PARAMS 16
#define MAX_COUNT_OF_RETS 1 

enum types_t {
    TYPE_UNKN,

    TYPE_u8,
    TYPE_u16,
    TYPE_u32,
    TYPE_u64,

    TYPE_s8,
    TYPE_s16,
    TYPE_s32,
    TYPE_s64,
    
    TYPE_f32,
    TYPE_f64,

    TYPE_b8,
    TYPE_b32,

    TYPE_void,
};

struct type_info_t {
    u32 type;
    b32 is_array;
    u32 array_size;
    u32 struct_offset;
    u32 size;
    u32 pointer_depth;
    string_t type_name;
};

enum def_type_t {
    DEF_TYPE_ENUM,
    DEF_TYPE_STRUCT,
    DEF_TYPE_UNION
};

struct scope_entry_t {
    u32 type;
    u32 index;
    b32 uninit;

    ast_node_t *node;
    ast_node_t *stmt; // used in multiple definitions...
    ast_node_t *expr; // can be null

    // ENTRY_TYPE
    u32 def_type;
    hashmap_t<string_t, scope_entry_t> scope;  // info of the struct memebers

    // ENTRY_FUNC
    b32 is_external;
    string_t ext_from, ext_name;
    hashmap_t<string_t, scope_entry_t> func_params;
    list_t<type_info_t> return_typenames;

    // ENTRY_VAR
    type_info_t info;
};

enum entry_type_t {
    ENTRY_ERROR = 0,
    ENTRY_VAR,
    ENTRY_TYPE,
    ENTRY_FUNC,
};

b32 analyzer_preload_all_files(compiler_t *compiler);
b32 analyze(compiler_t *compiler);

b32 load_and_process_file(compiler_t *compiler, string_t filename);

#endif // ANALYZER_H
