#include <locale.h>
#include "stddefines.h"

#include "allocator.h"
#include "arena.h"
#include "strings.h"
#include "sorter.h"

#include "compiler.h"
#include "arg_parser.h"
#include "profiler.h"

#define COMPILER_VERSION "1.0a"

allocator_t * default_allocator;
allocator_t __allocator;

#if defined(DEBUG)

#include "talloc.h"
#include "list.h"
#include "hashmap.h"
#include "stack.h"
#include "array.h"

void debug_tests(void) {
    temp_tests();
    stack_tests();
    arena_tests();
    list_tests();
    string_tests();
    array_tests();
    sorter_tests();
    hashmap_tests();
}

#elif defined(NDEBUG)
#define debug_tests(...)
#endif

void init(void) {
    setlocale(LC_ALL, ".utf-8");
    log_push_color(255, 255, 255);
    alloc_init();
    debug_init();
    profiler_init();
    debug_tests();
}

void showhelp(void) {
    log_push_color(INFO_COLOR);
    log_write("usage: prog [options] [files]\n");
    log_write("\n");
    log_write("options:\n");
    log_write("    --help\n");
    log_write("    --no-ansi\n");
    log_write("    --verbose\n");
    log_write("    --version\n");
    log_write("    --output [filename]\n");
    log_pop_color();
}

int main(int argc, char **argv) {
    init();

    if (argc <= 1) {
        showhelp();
        log_reset_color();
        return 0;
    }

    compiler_t state = create_compiler_instance(NULL);
    if (!state.valid) {
        log_error(STRING("Initialization of compiler is failed"));
        return -10;
    }

    f64 start = debug_get_time();

#ifdef DEBUG
    profiler_begin();
    profiler_block_start(STRING("Compilation"));
#endif

    b32 status = true;
    b32 at_least_one_file_loaded = false;
    b32 wait_for_output_filename = false;

    profiler_block_start(STRING("Load and process"));
    for (u64 i = 1; i < (u64)argc; i++) {
        argument_t arg = parse_argument(STRING(argv[i]));

        switch (arg.type) {
            case ARG_ERROR:
                status = false;
                break;
            case ARG_HELP:
                status = false;
                showhelp();
                break;
            case ARG_VERSION:
                status = false;
                log_push_color(INFO_COLOR);
                log_write("Compiler version: ");
                log_write(COMPILER_VERSION);
                log_write("\n");
                log_pop_color();
                break;

            case ARG_NO_ANSI:
                compiler_config.no_ansi_codes = true;
                break;

            case ARG_VERBOSE:
                compiler_config.verbose = true;
                break;

            case ARG_OUTPUT_FILE_NAME:
                wait_for_output_filename = true;
                break;

            default: 
                if (wait_for_output_filename) {
                    compiler_config.filename = arg.content;
                    wait_for_output_filename = false;
                    break;
                } 
                if (load_and_process_file(&state, arg.content)) {
                    at_least_one_file_loaded = true;
                } else {
                    status = false;
                }
                break;
        }

        if (!status) break;
    }
    profiler_block_end();

    if (status) {
        if (wait_for_output_filename) {
            log_error("Not recieved output file name!");
            status = false;
        } else if (!at_least_one_file_loaded) {
            log_error("No files to compile!");
            status = false;
        }
    }

    if (status) compile(&state);

#ifdef DEBUG
    profiler_block_end();
    profiler_end();
#endif

    f64 end = debug_get_time();
    if (status) {
        log_update_color();
        fprintf(stderr, "time: %lf\n", end - start);
        visualize_profiler_state();
    }

    log_reset_color();
    return 0;
}
