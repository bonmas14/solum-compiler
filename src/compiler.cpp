#include "compiler.h"

#include "arena.h"
#include "hashmap.h"
#include "parser.h"
#include "scanner.h"
#include "analyzer.h"
#include "backend.h"

source_file_t create_source_file(compiler_t *compiler, allocator_t *alloc) {
    if (alloc == NULL) alloc = default_allocator;
    assert(alloc != NULL);
    assert(compiler != NULL);

    // here we will initialize source file

    source_file_t file = {};

    file.scanner = (scanner_t*)mem_alloc(alloc, sizeof(scanner_t));

    if (file.scanner == NULL) { 
        log_error(STR("Buy more ram, or provide normal alloc [create_source_file]."));
    }

    file.scope.hash_func    = get_string_hash;
    file.scope.compare_func = compare_string_keys;

    return file; 
}

void create_standart_types(analyzer_t *analyzer) {
    types_t type = {};

    type.type = TYPE_STD;

    type.std.is_void = true;
    list_add(&analyzer->types, &type);
    type.std.is_void = false;

    // U8-64
    type.std.is_unsigned = true;

    for (u64 i = 1; i < 8; i++) {
        type.size = i;
        type.alignment = i;

        list_add(&analyzer->types, &type);
    }

    // S8-64
    type.std.is_unsigned = false;

    for (u64 i = 1; i < 8; i++) {
        type.size = i;
        type.alignment = i;
        list_add(&analyzer->types, &type);
    }

    // B8 B32
    type.std.is_unsigned = true;
    
    type.size = 1;
    type.alignment = 1;
    list_add(&analyzer->types, &type);

    type.size = 4;
    type.alignment = 4;
    list_add(&analyzer->types, &type);

    // f32 f64
    type.std.is_float    = true;
    type.std.is_unsigned = false;

    type.size      = 4;
    type.alignment = 4;
    list_add(&analyzer->types, &type);

    type.size      = 8;
    type.alignment = 8;
    list_add(&analyzer->types, &type);
}

compiler_t create_compiler_instance(allocator_t *alloc) {
    if (alloc == NULL) alloc = default_allocator;
    assert(alloc != NULL);

    compiler_t compiler = {};

	compiler.nodes   = preserve_allocator_from_stack(create_arena_allocator(sizeof(ast_node_t) * INIT_NODES_SIZE));
    compiler.strings = preserve_allocator_from_stack(create_arena_allocator(4096));

    compiler.analyzer = (analyzer_t*)mem_alloc(alloc, sizeof(analyzer_t));

    if (compiler.analyzer == NULL) { 
        log_error(STR("Buy more ram, or provide normal alloc [create_compiler_instance]."));
    }

    compiler.analyzer->global_scope.hash_func    = get_string_hash;
    compiler.analyzer->global_scope.compare_func = compare_string_keys;

    compiler.codegen  = codegen_create(alloc);
    compiler.is_valid = true;

    return compiler;
}
