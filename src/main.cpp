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
        child = (ast_node_t*)area_get(&parser->nodes, node->left_index);
        print_node(scanner, parser, child, depth + 1);
    } else if (node->type == AST_BIN) {
        child = (ast_node_t*)area_get(&parser->nodes, node->left_index);
        print_node(scanner, parser, child, depth + 1);

        child = (ast_node_t*)area_get(&parser->nodes, node->right_index);
        print_node(scanner, parser, child, depth + 1);
    }
}
enum types_t {
    T_UNKNOWN,
    T_UINT,
    T_INT,
    T_FLOAT,
};

struct interop_state_t {
    types_t type;
    b32     valid;

    union {
        u64 u;
        s64 s;
        f64 f;
    } container;
};
interop_state_t interop_node(parser_t *parser, ast_node_t *node) {
    interop_state_t state = { .valid = true };
    ast_node_t* child;

    if (node->type == AST_LEAF) {
        switch (node->token.type) {
            case TOKEN_CONST_INT: 
                state.type = T_UINT;
                state.container.u = node->token.data.const_int;
                break;
            case TOKEN_CONST_FP: 
                state.type = T_FLOAT;
                state.container.f = node->token.data.const_double;
                break;
            default:
                state.valid = false;
                break;
        }

    } else if (node->type == AST_UNARY) {
        child = (ast_node_t*)area_get(&parser->nodes, node->left_index);
        interop_state_t ret = interop_node(parser, child);

        switch (node->token.type) {
            case '!': 
                if (ret.type == T_UINT) {
                    state.type = T_UINT;
                    state.container.u = !ret.container.u;
                } else if (ret.type == T_INT) {
                    state.type = T_INT;
                    state.container.s = !ret.container.s;
                } else {
                    state.valid = false;
                }
                break;
            case '-': 
                state.type = ret.type;
                if (ret.type == T_UINT) {
                    state.type = T_INT;
                    state.container.s = -ret.container.u;
                    if (((u64)(-state.container.s)) != ret.container.u) {
                        state.valid = false;
                    }
                } else if (ret.type == T_INT) {
                    state.container.s = -ret.container.s;
                } else if (ret.type == T_FLOAT) {
                    state.container.f = -ret.container.f;
                } else {
                    state.valid = false;
                }
                break;
            default:
                state.valid = false;
                break;
        }
    } else if (node->type == AST_BIN) {
        child = (ast_node_t*)area_get(&parser->nodes, node->left_index);
        interop_state_t left  = interop_node(parser, child);

        child = (ast_node_t*)area_get(&parser->nodes, node->right_index);
        interop_state_t right = interop_node(parser, child);

        switch (node->token.type) {
            case '+':
                break;

            case '-':
                break;

            case TOKEN_LSHIFT: 
            case TOKEN_RSHIFT:
                break;

            case '&':
            case '|':
            case '^':
                break;

            case '*':
            case '/':
            case '%':
                break;
            case TOKEN_GR: 
            case TOKEN_LS:
            case TOKEN_GEQ:
            case TOKEN_LEQ:
            case TOKEN_EQ:
            case TOKEN_NEQ: 
                break;

            case TOKEN_LOGIC_AND:
            case TOKEN_LOGIC_OR:
                break;
        }
    }
    state.valid = false;
    return state;
}

int main(void) {
    hashmap_tests();
    area_tests();

    scanner_t scanner = {};

    if (!scanner_create(STR("test"), &scanner)) {
        log_error(STR("Main: couldn't open file and load it into memory."), 0);
        return -1;
    }

    parser_t parser = {};

    parse(&scanner, &parser);

    for (u64 i = 0; i < parser.root_indices.count; i++) {
        ast_node_t *root = (ast_node_t*)area_get(&parser.nodes, *(u64*)area_get(&parser.root_indices, i));

        print_node(&scanner, &parser, root, 0);
        interop_state_t res = interop_node(&parser, root);

        if (res.type == T_INT) {
            fprintf(stdout, "---------------------- %zd\n", res.container.s);
        } else if (res.type == T_UINT) {
            fprintf(stdout, "---------------------- %zu\n", res.container.u);
        } else if (res.type == T_FLOAT) {
            fprintf(stdout, "---------------------- %f\n",  res.container.f);
        }
    }

    generate_code();
    return 0;
}
