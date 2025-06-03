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
#include "interop.h"

#include "strings.h"
#include "profiler.h"
#include "allocator.h"
#include "talloc.h"

#define MODULES_PATH "SOLUM_MODULES"

compiler_configuration_t compiler_config = {};

string_t get_global_modules_search_path(void) {
    assert(default_allocator != NULL);

    char *env = getenv(MODULES_PATH);

    if (env == NULL) {
        log_error(STRING("Couldn't find environment variable for modules path... please run install script"));
        return {};
    }

    return string_copy(STRING(env), default_allocator);
}

source_file_t create_source_file(allocator_t *alloc) {
    if (alloc == NULL) alloc = default_allocator;
    assert(alloc    != NULL);

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
    compiler.valid   = true;

    array_create(&compiler.scopes, 32, create_arena_allocator(32 * sizeof(hashmap_t<string_t, scope_entry_t>)));

    hashmap_t<string_t, scope_entry_t> hm = {};
    array_add(&compiler.scopes, hm);

    return compiler;
}

void compile(compiler_t *state) {
    profiler_block_start(STRING("Preload all files"));
    if (!analyzer_preload_all_files(state)) {
        profiler_block_end();
        return;
    }
    profiler_block_end();

    profiler_block_start(STRING("Analyze"));
    if (!analyze(state)) {
        profiler_block_end();
        return;
    }
    profiler_block_end();

    profiler_block_start(STRING("IR gen"));
    ir_t result = compile_program(state);
    profiler_block_end();

    if (result.is_valid) {
        string_t key = STRING("__internal_compile_globals");

        b32 interp_state = true;

        profiler_block_start(STRING("Interpretation")); {
            ir_function_t *func = result.functions[key];

            assert(func);

            interp_state = interop_func(&result, key);

            if (hashmap_remove(&result.functions, STRING("__internal_compile_globals"))) {
                array_delete(&func->code);
            }

        } profiler_block_end();

        if (!interp_state) {
            log_error("Error interpreting code!");
            return;
        }


        profiler_block_start(STRING("Nasm backend generation"));
        backend_t backend = nasm_compile_program(&result);
        profiler_block_end();

        string_t content = { c_string_length((char*)backend.code.data), backend.code.data };

        string_t filename = compiler_config.filename.data ? compiler_config.filename : STRING("output");

        string_t backend_config = string_format(get_temporary_allocator(), STRING("%s.nasm"), filename);

        string_t nasm_config = string_format(get_temporary_allocator(), STRING("-g -f win64 %s.nasm -o %s.obj"), filename, filename);

        string_t link_config = STRING("/MACHINE:X64 /DYNAMICBASE /SUBSYSTEM:CONSOLE /DEBUG:FULL /ENTRY:mainCRTStartup kernel32.lib");

        link_config          = string_format(get_temporary_allocator(), 
                                    STRING("%s /OUT:%s.exe %s.obj %s"), 
                                    link_config, filename, filename,
                                    compiler_config.show_link_time ? STRING("/TIME") : STRING(" "));

        profiler_block_start(STRING("Assembling"));
        platform_write_file(backend_config, content);
        if (platform_run_process(STRING("nasm.exe"),     nasm_config) != 0) { profiler_block_end(); return; }
        profiler_block_end();

        profiler_block_start(STRING("Linking"));
        if (platform_run_process(STRING("lld-link.exe"), link_config) != 0) { profiler_block_end(); return; }
        profiler_block_end();

    } else {
        log_error(STRING("Compilation error"));
    }
}

