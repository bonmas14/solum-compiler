#include <stdio.h>
#include "stddefines.h"
#include "scanner.h"
#include "parser.h"
#include "analyzer.h"
#include "backend.h"
#include "area_alloc.h"
#include "arena.h"
#include "hashmap.h"

#ifdef NDEBUG 
#define log_info_token(a, b, c)
#endif

arena_t * default_allocator;

void print_node(compiler_t *compiler, ast_node_t* node, u32 depth) {
    log_info_token(compiler->scanner, node->token, depth * LEFT_PAD_STANDART_OFFSET);

    if (node->type == AST_UNARY) {
        print_node(compiler, node->left, depth + 1);
    } else if (node->type == AST_BIN) {
        print_node(compiler, node->left, depth + 1);
        print_node(compiler, node->right, depth + 1);
    } else if (node->type == AST_LIST) {
        ast_node_t* child = node->list_start;

        for (u64 i = 0; i < node->child_count; i++) {
            assert(child != NULL);

            print_node(compiler, child, depth + 1);
            child = child->list_next;
        }
    }
}

void debug_tests(void) {
#ifdef DEBUG
    hashmap_tests();
    list_tests();
    arena_tests();
#endif
}

int main(int argc, char **argv) {

    debug_tests();
    log_push_color(255, 255, 255);

    default_allocator   = arena_create(PAGE_SIZE * 10);
    compiler_t compiler = create_compiler_instance();


    string_t file = {};

    u8* filename = NULL;

    if (argc <= 1) {
        filename = STR("test.slm");
    } else {
        filename = (u8*)argv[1];
    }

    if (!read_file_into_string(filename, &file)) {
        log_error(STR("Main: couldn't open file."), 0);
        return -1;
    }

    if (!scanner_open(&file, compiler.scanner)) {
        log_error(STR("Main: couldn't open file and load it into memory."), 0);
        return -1;
    }

    parse(&compiler);
    analyze_code(&compiler);
    generate_code(&compiler);

    log_update_color();
    return 0;
}
