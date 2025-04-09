#ifndef ANALYZER_H
#define ANALYZER_H

#include "list.h"
#include "hashmap.h"

struct compiler_t;
struct ast_node_t;

#define MAX_COUNT_OF_PARAMS 16
#define MAX_COUNT_OF_RETS 1 

// func : (a : s32) -> s32 = { return a; }
//
// -> func
//    -> a
//       s32
//    -> s32
//    -> {}
//
// var : s32 = 43;
//
// var
//     s32
//
// test : struct = {
//     a : s32;
//     b : s32;
//     c : u64;
// }
//
// v : test;
// v.a =
//

struct var_type_info_t {
    u32 pointer_depth;
    b32 is_std;
    string_t type_name;

    struct {
        b32 is_unsigned;
        b32 is_boolean;
        b32 is_float;
        u32 size; // 0 = void
    } std;
};

struct scope_entry_t {
    b32 configured;
    u32 type;

    ast_node_t *node;
    union {
        // NOTE: if it is not in union, you need to change creation in analyzer
        hashmap_t<string_t, scope_entry_t> scope; 

        // ENTRY_FUNC
        struct {
            hashmap_t<string_t, scope_entry_t> func_params;
            list_t<var_type_info_t> return_typenames;
            b32 from_prototype;
            string_t prototype_name;
        };
    };

    // ENTRY_TYPE
    u32 def_type; // types_t

    union {
        struct {
            u32 size;
            u32 align;

            // members are in scope!
        } comp_type;

        struct {
            u32 count_of_elements;
        } enum_type;
    };

    // ENTRY_VAR
    
    // b32 array_type;
    // u32 array_count;
    
    u32 pointer_depth;
    b32 is_std;

    struct {
        b32 is_unsigned;
        b32 is_boolean;
        b32 is_float;
        u32 size; // 0 = void
    } std;

    string_t type_name;
};

enum types_t {
    TYPE_ERROR = 0,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
    TYPE_PROTO,
};

enum entry_type_t {
    ENTRY_ERROR = 0,
    ENTRY_VAR,
    ENTRY_TYPE,
    ENTRY_PROTOTYPE,
    ENTRY_FUNC,
    ENTRY_NAMESPACE,
};

b32 analyze_and_compile(compiler_t *compiler);
b32 load_and_process_file(compiler_t *compiler, string_t filename);

#endif // ANALYZER_H
