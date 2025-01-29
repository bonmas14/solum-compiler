#include "stddefines.h"
#include "scanner.h"
#include "parser.h"
#include "backend.h"
#include "hashmap.h"

#ifdef NDEBUG 
#define log_info_token(a, b, c)
#endif

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
    return 0;
}
