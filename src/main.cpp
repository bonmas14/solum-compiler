#include "stddefines.h"
#include "strings.h"

#include "allocator.h"

#include "talloc.h"
#include "arena.h"

#include "list.h"
#include "hashmap.h"
#include "stack.h"


#include "scanner.h"
#include "parser.h"
#include "analyzer.h"
#include "ir_generator.h"
#include "backend.h"

allocator_t * default_allocator;
allocator_t __allocator;

#if defined(DEBUG)

void debug_tests(void) {
    hashmap_tests();
    list_tests();
    stack_tests();
    arena_tests();
    temp_tests();
    string_tests();
}

#elif defined(NDEBUG)
#define debug_tests(...)
#endif

void init(void) {
    debug_tests();
    default_allocator = preserve_allocator_from_stack(create_arena_allocator(PG(10)));
    log_push_color(255, 255, 255);
}

int main(int argc, char **argv) {
    UNUSED(argc);
    init();

    if (argc < 1) { 
        assert(argc > 0); 
        return -1; 
    }

    if (argc <= 1) {
        log_info(STR("usage: prog [FILE]"));
        log_color_reset();
        return 0;
    }

    compiler_t state = create_compiler_instance(NULL);

    if (!state.valid) {
        return 100;
    }

    string_t filename = string_copy(STRING(argv[1]), default_allocator);

    load_and_process_file(&state, filename);
    analyze_and_compile(&state);

    log_color_reset();
    return 0;
}
