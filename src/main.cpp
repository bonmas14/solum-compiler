#include "stddefines.h"
#include "scanner.h"
#include "parser.h"
#include "backend.h"
#include "area_alloc.h"
#include "hashmap.h"

#include <conio.h>
#include <windows.h>

#ifdef NDEBUG 
#define log_info_token(a, b, c)
#endif

void repl(area_t<u8> *area);

void print_node(scanner_t *scanner, parser_t *parser, ast_node_t* node, u32 depth) {
    log_info_token(scanner, node->token, depth * LEFT_PAD_STANDART_OFFSET);

    ast_node_t* child;
    if (node->type == AST_UNARY) {
        child = area_get(&parser->nodes, node->left_index);
        print_node(scanner, parser, child, depth + 1);
    } else if (node->type == AST_BIN) {
        child = area_get(&parser->nodes, node->left_index);
        print_node(scanner, parser, child, depth + 1);

        child = area_get(&parser->nodes, node->right_index);
        print_node(scanner, parser, child, depth + 1);
    }
}

void debug_tests(void) {
    hashmap_tests();
    area_tests();
}

int main(void) {
#ifdef DEBUG
    debug_tests();
#endif

    /*
       scanner_t scanner = {};

       if (!scanner_create(STR("test"), &scanner)) {
       log_error(STR("Main: couldn't open file and load it into memory."), 0);
       return -1;
       }

       parser_t parser = {};

       parse(&scanner, &parser);

       for (u64 i = 0; i < parser.root_indices.count; i++) {
       ast_node_t *root = area_get(&parser.nodes, *area_get(&parser.root_indices, i));

       print_node(&scanner, &parser, root, 0);
       }

       generate_code();
       */

    area_t<u8> area = {};
    area_create(&area, 1000);

    while (true) {
        repl(&area);
    }

    return 0;
}

void repl(area_t<u8> *area) {
    if (_kbhit()) {
        u8 ch = _getch();

        if (ch == '\b') area->count--;
        else            area_add(area, &ch);

        ch = 0;
        area_add(area, &ch);

        fprintf(stderr, "\x1b[1;1f\x1b[0J");
        fprintf(stderr, "%.*s\n", (int)area->count, area->data);

        string_t str = {};

        str.size = area->count;
        area->count--;
        str.data = area->data;

        scanner_t scanner = {};
        parser_t  parser = {};

        if (!scanner_open(&str, &scanner)) return;
        if (!parse(&scanner, &parser)) return;

        for (u64 i = 0; i < parser.root_indices.count; i++) {
            ast_node_t* root = area_get(&parser.nodes, *area_get(&parser.root_indices, i));

            print_node(&scanner, &parser, root, 0);
        }

        scanner_delete(&scanner);
    } 
}
