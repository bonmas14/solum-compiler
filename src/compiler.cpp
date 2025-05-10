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

#ifdef _WIN32
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

#else 

// @todo also make it environment variable
#define MODULES_PATH "/usr/local/lib/solum/modules/"

string_t get_global_modules_search_path(void) {
    assert(default_allocator != NULL);
    return string_copy(STRING(MODULES_PATH), default_allocator);
}

#endif

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

    return compiler;
}

void compile(compiler_t *compiler) {
    if (!analyzer_preload_all_files(compiler)) {
        return;
    }

    if (!analyze(compiler)) {
        return;
    }

    ir_t result = compile_program(compiler);

    // result will return final IR
    // everything should be compiled 
    // or there is an error


    if (result.is_valid) {
        log_info(STRING("Compiled successfully!!!!!"));
    } else {
        log_error(STRING("NOT COMPILED!@!!!LJ#LKj !!!!!"));
    }

    // codegen from IR here
}

