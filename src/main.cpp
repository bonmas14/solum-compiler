#include "stddefines.h"
#include "scanner.h"
#include "parser.h"
#include "analyzer.h"
#include "backend.h"
#include "area_alloc.h"
#include "arena.h"
#include "hashmap.h"

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else 
#include <stdio.h>
#endif

#ifdef NDEBUG 
#define log_info_token(a, b, c)
#endif

void repl(area_t<u8> *area);

void print_node(compiler_t *compiler, ast_node_t* node, u32 depth) {
    log_info_token(compiler->scanner, node->token, depth * LEFT_PAD_STANDART_OFFSET);

    if (node->type == AST_UNARY) {
        print_node(compiler, node->left, depth + 1);
    } else if (node->type == AST_BIN) {
        print_node(compiler, node->left, depth + 1);
        print_node(compiler, node->right, depth + 1);
    } else if (node->type == AST_LIST) {
        b32 first_time = true;

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
    area_tests();
    arena_tests();
#endif
}

int main(void) {
    debug_tests();

    log_push_color(255, 255, 255);

    analyzer_t analyzer = {};
    analyzer_create(&analyzer);

    compiler_t compiler = {};
    scanner_t  scanner  = {};
    parser_t   parser   = {};

    compiler.scanner  = &scanner;
    compiler.parser   = &parser;
    compiler.analyzer = &analyzer;

    if (!scanner_create((u8[]) {"test"}, &scanner)) {
        log_error(STR("Main: couldn't open file and load it into memory."), 0);
        return -1;
    }

    parse(&compiler);
    analyze_code(&compiler);

    for (u64 i = 0; i < parser.roots.count; i++) {
        ast_node_t * node = *area_get(&parser.roots, i);

        print_node(&compiler, node, 0);
    }
    generate_code(&compiler);

    log_update_color();
    return 0;
}
