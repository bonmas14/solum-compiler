#include "stddefines.h"

#include "allocator.h"

#include "list.h"
#include "talloc.h"
#include "arena.h"

#include "hashmap.h"

#include "scanner.h"
#include "parser.h"
#include "analyzer.h"
#include "ir_generator.h"
#include "backend.h"

#ifdef NDEBUG 
#define log_info_token(a, b, c)
#endif

allocator_t * default_allocator;
allocator_t __allocator;

void debug_tests(void) {
    hashmap_tests();
    list_tests();
    arena_tests();
    temp_tests();
}

#ifdef NDEBUG
#define debug_tests(...)
#endif

void init(void) {
    debug_tests();
    default_allocator = preserve_allocator_from_stack(create_arena_allocator(PG(10)));

    string_t str1 = {}, str2 = {};

    str1.data = STR("Concating some strings... ");
    str1.size = strlen((const char*)str1.data);

    str2.data = STR("Right now!\n");
    str2.size = strlen((const char*)str2.data);

    string_t str3 = string_temp_concat(str1, str2);

    log_info(str3.data);
    log_push_color(255, 255, 255);
}

b32 add_file(compiler_t *state, char *filename) {
    if (filename == NULL) return false;

    file_t file  = {};
    file.scanner = (scanner_t*)mem_alloc(default_allocator, sizeof(scanner_t));

    string_t source, name;

    name.size = strlen(filename);
    name.data = (u8*)filename;

    if (!read_file_into_string(name.data, &source)) {
        log_error(STR("Main: couldn't open file."));
        return false;
    }

    if (!scanner_open(&name, &source, file.scanner)) {
        return false;
    }

    if (!hashmap_add(&state->files, name, &file)) {
        log_error(STR("Couldn't add file to work with."));
        return false;
    }

    return true;
}

b32 compile(char *filename) {
    if (filename == 0) return false;

    compiler_t state = create_compiler_instance();

    add_file(&state, filename);

    string_t file = STRING(filename);
    assert(strlen(filename) == file.size);
    if (!parse_file(&state, file)) { 
        log_info(STR("Parsing error"));
        log_update_color();
        return -1;
    }

    b32 result = true;
    
    for (u64 i = 0; i < state.files.capacity; i++) {
        kv_pair_t<string_t, file_t> pair = state.files.entries[i];
        if (!pair.occupied) continue;
        if (pair.deleted)   continue;

        /* loading files... when we do 'use: "filename"'
        if (!pair.value.loaded) {
            // find_file_by_key()
            // <file>
            // <file>.slm
            // <dir>/module.slm
            // <slm>/<dir>/module.slm
            //
            // delete key from hashmap
            // add filename

            // add_file();
            // pair.value.loaded = true;
        }
        */

        if (!pair.value.parsed) {
            if (!parse_file(&state, pair.key)) {
                result = false;
            }
        }

        if (!analyze_file(&state, pair.key)) {
            result = false;
        }

        pair.value.analyzed = true;
    }

    log_info(STR("IR..."));
    generate_ir(&state);

    log_info(STR("Generating..."));
    generate_code(&state);

    return result;
}

int main(int argc, char **argv) {
    UNUSED(argc);
    init();

    assert(argc > 0);
    // you can just pass that because C allows that,
    // next index will be null so you dont need
    // to use argc
    compile(argv[1]); 

    log_update_color();
    return 0;
}
