#include <locale.h>
#include "stddefines.h"

#include "allocator.h"
#include "arena.h"
#include "strings.h"

#include "compiler.h"
#include "profiler.h"

allocator_t * default_allocator;
allocator_t __allocator;

#if defined(DEBUG)

#include "talloc.h"
#include "list.h"
#include "hashmap.h"
#include "stack.h"

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
    debug_init();
    debug_tests();
    default_allocator = preserve_allocator_from_stack(create_arena_allocator(PG(10)));
    log_push_color(255, 255, 255);
}

int main(int argc, char **argv) {
    setlocale(LC_ALL, ".utf-8");

    UNUSED(argc);
    init();

    profiler_begin();

    if (argc < 1) { 
        assert(argc > 0); 
        return -1; 
    }

    if (argc <= 1) {
        log_info(STRING("usage: prog [FILE]"));
        log_color_reset();
        return 0;
    }

    string_t file = string_copy(STRING(argv[1]), default_allocator);

    profiler_block_start(STRING("Compile Action"));
    compile(file);
    profiler_block_end();

    log_update_color();
    profiler_end();
    visualize_profiler_state();

    log_color_reset();
    return 0;
}
