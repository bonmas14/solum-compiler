#include <stdio.h>
#include <string.h>

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

b32 add_file(compiler_t *compiler, char *filename) {
    if (filename == NULL) return false;

    file_t file  = {};
    file.scanner = (scanner_t*)mem_alloc(default_allocator, sizeof(scanner_t));

    string_t source, name;

    name.size = strlen(filename);
    name.data = (u8*)filename;

    if (!read_file_into_string(name.data, &source)) {
        log_error(STR("Main: couldn't open file."));
        log_update_color();
        return false;
    }

    if (!scanner_open(&name, &source, file.scanner)) {
        log_update_color();
        return false;
    }

    if (!hashmap_add(&compiler->files, name, &file)) {
        log_error(STR("Couldn't add file to work with."));
        log_update_color();
        return false;
    }

    return true;
}

int main(int argc, char **argv) {
    UNUSED(argc);
    init();

    compiler_t compiler = create_compiler_instance();

    u64 arg_index = 1;
    while (add_file(&compiler, argv[arg_index])) {
        arg_index++;
    }

    if (!parse_all_files(&compiler)) { 
        log_info(STR("Parsing error"));
        log_update_color();
        return -1;
    }

    if (!analyze_all_files(&compiler)) {
        log_info(STR("Analyzing error"));
        log_update_color();
        return -1;
    }

    log_info(STR("IR..."));
    generate_ir(&compiler);

    log_info(STR("Generating..."));
    generate_code(&compiler);

    log_update_color();
    return 0;
}
