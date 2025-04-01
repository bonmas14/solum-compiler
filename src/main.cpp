#include "stddefines.h"
#include "strings.h"

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

allocator_t * default_allocator;
allocator_t __allocator;

#if defined(DEBUG)

void debug_tests(void) {
    hashmap_tests();
    list_tests();
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

b32 add_file(compiler_t *state, string_t filename) {
    source_file_t file = {};

    file = create_source_file(state, NULL);

    string_t source;

    if (!read_file_into_string(filename, default_allocator, &source)) {
        log_error(STR("Main: couldn't open file."));
        return false;
    }

    if (!scanner_open(&filename, &source, file.scanner)) {
        return false;
    }

    file.loaded = true;

    if (!hashmap_add(&state->files, filename, &file)) {
        log_error(STR("Couldn't add file to work with."));
        return false;
    }

    return true;
}

b32 compile(string_t filename) {
    compiler_t state = create_compiler_instance(NULL);
    add_file(&state, filename);

    b32 result = true;
    b32 finished = false;

    while (!finished) {
        finished = true;

        for (u64 i = 0; i < state.files.capacity; i++) {
            kv_pair_t<string_t, source_file_t> *pair = &state.files.entries[i];
            if (!pair->occupied) continue;
            if (pair->deleted)   continue;

            if (!pair->value.loaded) {
                finished = false;
                string_t source = {};

                if (!read_file_into_string(pair->key, default_allocator, &source)) {
                    log_error(STR("Pickle?!"));
                    return false;
                }

                if (!scanner_open(&pair->key, &source, pair->value.scanner)) {
                    log_error(STR("Pickle no good maan"));
                    return false;
                }

                pair->value.loaded = true;
            }

            if (!pair->value.parsed) {
                finished = false;
                if (!parse_file(&state, pair->key)) {
                    result = false;
                }

                pair->value.parsed = true;
            }

            if (!pair->value.analyzed) {
                finished = false;
                if (!analyze_file(&state, pair->key)) {
                    result = false;
                }

                pair->value.analyzed = true;
            }
        }
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

    // thats because in some systems argv[0] is not a filename of executable
    assert(argc > 0);

    if (argc > 1) {
        compile(string_copy(STRING(argv[1]), default_allocator)); 
    } else {
        log_info(STR("usage: prog [FILE]"));
    }

    log_color_reset();
    return 0;
}
