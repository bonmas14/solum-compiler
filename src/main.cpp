
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
}

int main(int argc, char **argv) {
    init();
    string_t str1 = {}, str2 = {};

    str1.data = STR("Concating some strings... ");
    str1.size = strlen((const char*)str1.data);

    str2.data = STR("Right now!\n");
    str2.size = strlen((const char*)str2.data);

    string_t str3 = string_temp_concat(str1, str2);

    log_info(str3.data);

    log_push_color(255, 255, 255);

    compiler_t compiler = create_compiler_instance();

    string_t file = {};
    u8* filename  = NULL;

    if (argc <= 1) {
        filename = STR("test.slm");
    } else {
        filename = (u8*)argv[1];
    }

    if (!read_file_into_string((u8*)filename, &file)) {
        log_error(STR("Main: couldn't open file."));
        return -1;
    }

    if (!scanner_open(&file, compiler.scanner)) {
        log_error(STR("Main: couldn't open file and load it into memory."));
        return -1;
    }

    log_info(STR("Parsing..."));
    if (!parse(&compiler)) { 
        log_info(STR("Parsing error"));
        log_update_color();
        return -1;
    }

    log_info(STR("Analyzing..."));
    if (!analyze_code(&compiler)) {
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
