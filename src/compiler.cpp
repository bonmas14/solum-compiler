#include "compiler.h"

#include "arena.h"

#include "list.h"
#include "stack.h"
#include "hashmap.h"

#include "parser.h"
#include "scanner.h"
#include "analyzer.h"
#include "backend.h"

#include "ir.h"

#include "strings.h"
#include "profiler.h"

#define MODULES_PATH "SOLUM_MODULES"

string_t get_global_modules_search_path(void) {
    assert(default_allocator != NULL);

    char *env = getenv(MODULES_PATH);

    if (env == NULL) {
        log_error(STRING("Couldn't find environment variable for modules path... please run install script"));
        return {};
    }

    return string_copy(STRING(env), default_allocator);
}

source_file_t create_source_file(compiler_t *compiler, allocator_t *alloc) {
    if (alloc == NULL) alloc = default_allocator;
    assert(alloc    != NULL);
    assert(compiler != NULL);

    source_file_t file = {};

    file.scanner = (scanner_t*)mem_alloc(alloc, sizeof(scanner_t));

    if (file.scanner == NULL) { 
        log_error(STRING("Buy more ram, or provide normal alloc [create_source_file]."));
    }

    return file; 
}

compiler_t create_compiler_instance(allocator_t *alloc) {
    if (alloc == NULL) alloc = default_allocator;
    assert(alloc != NULL);

    compiler_t compiler = {};
    compiler.modules_path = get_global_modules_search_path();
    if (compiler.modules_path.data == NULL) { 
        compiler.valid = false;
        return compiler;
    }

	compiler.nodes   = preserve_allocator_from_stack(create_arena_allocator(sizeof(ast_node_t) * INIT_NODES_SIZE));
    compiler.strings = preserve_allocator_from_stack(create_arena_allocator(4096));

    compiler.codegen  = codegen_create(alloc);
    compiler.valid = true;

    hashmap_t<string_t, scope_entry_t> hm = {};
    list_add(&compiler.scopes, &hm);

    return compiler;
}

void compile(string_t filename) {
    compiler_t state = create_compiler_instance(NULL);

    if (!state.valid) {
        log_error(STRING("Initialization of compiler is failed"));
        return;
    }

    profiler_block_start(STRING("Load and process"));
    if (!load_and_process_file(&state, filename)) {
        profiler_block_end();
        return;
    }
    profiler_block_end();

    profiler_block_start(STRING("Preload all files"));
    if (!analyzer_preload_all_files(&state)) {
        profiler_block_end();
        return;
    }
    profiler_block_end();

    profiler_block_start(STRING("Analyze"));
    if (!analyze(&state)) {
        profiler_block_end();
        return;
    }
    profiler_block_end();

    ir_t result = compile_program(&state);

    // result will return final IR
    // everything should be compiled 
    // or there is an error

    if (result.is_valid) {
        log_info(STRING("Compiled successfully!!!!!"));
    } else {
        log_error(STRING("NOT COMPILED!@!!!LJ#LKj !!!!!"));
    }

    // codegen from IR here
    return;
}

