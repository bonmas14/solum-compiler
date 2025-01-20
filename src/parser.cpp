#include "parser.h"

// @todo
//
// 1. parse this file
// 2. create export/import tables
// 3. parse all files from import table = go to 1 for all new files
// 4. analyze types and finish parsing
// 5. codegen


struct local_state_t {
    scanner_state_t *scanner;
    parser_state_t  *parser;
};

/* helpers */

/* parsing */


ast_node_t parse_expression(local_state_t *state);

ast_node_t parse_primary(local_state_t *state) {
    token_t token = advance_token(state->scanner);

    ast_node_t result = {};

    result.type  = AST_LEAF;
    result.token = token;

    switch (token.type) {
        case TOKEN_CONST_FP:
        case TOKEN_CONST_INT:
            break;
        case TOKEN_OPEN_BRACE:
            result = parse_expression(state);

            if (consume_token(TOKEN_CLOSE_BRACE, state->scanner)) {
                result.braced = true;
            } else {
                result.type = AST_ERROR;

                log_error_token(STR("Parser: expected closing brace after expression"), state->scanner, peek_token(state->scanner), 0);
            }
            break;
        default:
            result.type = AST_ERROR;
            log_error_token(STR("Parser: wrong primary token type"),
                            state->scanner, token, 0);
            break;
    }

    return result;
}

ast_node_t parse_unary(local_state_t *state) {
    token_t token = peek_token(state->scanner);

    ast_node_t result = {};

    result.type  = AST_UNARY;
    result.token = token;

    switch (token.type) {
        case '-': {
            advance_token(state->scanner);
            ast_node_t child = parse_unary(state);
            list_add(&state->parser->nodes, (void*)&child);
            result.left_index = state->parser->nodes.count - 1;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = parse_primary(state);
        } break;
    }

    return result;
}
// @todo: different names later!!!
ast_node_t parse_mul(local_state_t *state) {
    return parse_unary(state);
}

// @todo: different names later!!!
ast_node_t parse_add(local_state_t *state) { 
    return parse_mul(state);
}

ast_node_t parse_expression(local_state_t *state) {
    return parse_add(state);
    /*
    ast_node_t left = {};

    left = parse_primary(state);

    ast_node_t right = {};
    */
}

void print_node(local_state_t *local, ast_node_t* node, u32 depth) {
    log_info_token(local->scanner, node->token, depth * LEFT_PAD_STANDART_OFFSET);

    if (node->type == AST_UNARY) {
        ast_node_t* child = (ast_node_t*)list_get(&local->parser->nodes, node->left_index);

        print_node(local, child, depth + 1);
    }
}

b32 parse(scanner_state_t *scanner, parser_state_t* state) {
    local_state_t local = {};

    local.scanner = scanner;
    local.parser  = state;

    if (!list_create(&state->nodes, INIT_NODES_SIZE, sizeof(ast_node_t))) {
        log_error(STR("Parser: Couldn't create nodes list."), 0);
        return false;
    }

    b32 valid_parse = true;
    
    token_t curr = peek_token(local.scanner);

    while (curr.type != TOKEN_EOF && curr.type != TOKEN_ERROR) {
        ast_node_t node = parse_expression(&local);

        if (node.type == AST_ERROR) {
            valid_parse = false; 
            break;
        }

        if (!list_add(&local.parser->nodes, (void*)&node)) {
            log_error(STR("Parser: Couldn't add node to a list."), 0);
            valid_parse = false; 
        } else {
            // @todo just logs for tests
            print_node(&local, &node, 0);
        }

        curr = peek_token(local.scanner);
    }

    return valid_parse;
}
