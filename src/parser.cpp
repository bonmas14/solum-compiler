#include "parser.h"

// @todo
//
// 1. parse this file
// 2. create export/import tables
// 3. parse all files from import table = go to 1 for all new files
// 4. analyze types and finish parsing
// 5. codegen


struct local_state_t {
    scanner_t *scanner;
    parser_t  *parser;
};

/* helpers */

/* parsing */

ast_node_t parse_expression(local_state_t *state);

ast_node_t parse_primary(local_state_t *state) {
    token_t token = advance_token(state->scanner);

    ast_node_t result = {};

    result.type  = AST_LEAF;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case TOKEN_CONST_FP:
        case TOKEN_CONST_INT:
        case TOKEN_CONST_STRING:
        case TOKEN_IDENT:
            break;
        case TOKEN_OPEN_BRACE:
            result = parse_expression(state);

            if (!consume_token(TOKEN_CLOSE_BRACE, state->scanner)) {
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

ast_node_t parse_function_call(local_state_t *state) {
    return parse_primary(state);
}

ast_node_t parse_unary(local_state_t *state) {
    token_t token = peek_token(state->scanner);

    ast_node_t result = {};

    result.type  = AST_UNARY;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case '@':
        case '^':
        case '-':
        case '!': {
            advance_token(state->scanner);
            ast_node_t child = parse_unary(state);
            area_add(&state->parser->nodes, &child);
            result.left_index = state->parser->nodes.count - 1;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = parse_function_call(state);
        } break;
    }

    return result;
}

ast_node_t parse_shift(local_state_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_unary(state);

    token_t token = peek_token(state->scanner);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case TOKEN_LSHIFT: 
        case TOKEN_RSHIFT: {
            advance_token(state->scanner);

            area_add(&state->parser->nodes, &left);
            result.left_index = state->parser->nodes.count - 1;

            right = parse_shift(state);
            area_add(&state->parser->nodes, &right);
            result.right_index = state->parser->nodes.count - 1;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
}

ast_node_t parse_and(local_state_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_shift(state);

    token_t token = peek_token(state->scanner);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case '&': {
            advance_token(state->scanner);

            area_add(&state->parser->nodes, &left);
            result.left_index = state->parser->nodes.count - 1;

            right = parse_and(state);
            area_add(&state->parser->nodes, &right);
            result.right_index = state->parser->nodes.count - 1;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
}

ast_node_t parse_or(local_state_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_and(state);

    token_t token = peek_token(state->scanner);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case '|': {
            advance_token(state->scanner);

            area_add(&state->parser->nodes, &left);
            result.left_index = state->parser->nodes.count - 1;

            right = parse_or(state);
            area_add(&state->parser->nodes, &right);
            result.right_index = state->parser->nodes.count - 1;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
}

ast_node_t parse_xor(local_state_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_or(state);

    token_t token = peek_token(state->scanner);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case '^': {
            advance_token(state->scanner);

            area_add(&state->parser->nodes, &left);
            result.left_index = state->parser->nodes.count - 1;

            right = parse_xor(state);
            area_add(&state->parser->nodes, &right);
            result.right_index = state->parser->nodes.count - 1;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
}

// @todo: different names later!!!
ast_node_t parse_mul(local_state_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_xor(state);

    token_t token = peek_token(state->scanner);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case '*':
        case '/':
        case '%': {
            advance_token(state->scanner);

            area_add(&state->parser->nodes, &left);
            result.left_index = state->parser->nodes.count - 1;

            right = parse_mul(state);
            area_add(&state->parser->nodes, &right);
            result.right_index = state->parser->nodes.count - 1;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
}

ast_node_t parse_add(local_state_t *state) { 
    ast_node_t result = {}, left, right;

    left = parse_mul(state);

    token_t token = peek_token(state->scanner);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case '+':
        case '-': {
            advance_token(state->scanner);

            area_add(&state->parser->nodes, &left);
            result.left_index = state->parser->nodes.count - 1;

            right = parse_add(state);
            area_add(&state->parser->nodes, &right);
            result.right_index = state->parser->nodes.count - 1;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
}

ast_node_t parse_compare_expression(local_state_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_add(state);

    token_t token = peek_token(state->scanner);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case TOKEN_GR: 
        case TOKEN_LS:
        case TOKEN_GEQ:
        case TOKEN_LEQ:
        case TOKEN_EQ:
        case TOKEN_NEQ: {
            advance_token(state->scanner);
            area_add(&state->parser->nodes, &left);
            result.left_index = state->parser->nodes.count - 1;

            right = parse_compare_expression(state);
            area_add(&state->parser->nodes, &right);
            result.right_index = state->parser->nodes.count - 1;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
};

ast_node_t parse_logic_and_expression(local_state_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_compare_expression(state);

    token_t token = peek_token(state->scanner);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case TOKEN_LOGIC_AND: {
            advance_token(state->scanner);

            area_add(&state->parser->nodes, &left);
            result.left_index = state->parser->nodes.count - 1;

            right = parse_logic_and_expression(state);
            area_add(&state->parser->nodes, &right);
            result.right_index = state->parser->nodes.count - 1;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = left;
        } break;
    }

    return result;

}
ast_node_t parse_logic_or_expression(local_state_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_logic_and_expression(state);

    token_t token = peek_token(state->scanner);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case TOKEN_LOGIC_OR: {
            advance_token(state->scanner);

            area_add(&state->parser->nodes, &left);
            result.left_index = state->parser->nodes.count - 1;

            right = parse_logic_or_expression(state);
            area_add(&state->parser->nodes, &right);
            result.right_index = state->parser->nodes.count - 1;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
}

ast_node_t parse_assing_expression(local_state_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_logic_or_expression(state);

    token_t token = peek_token(state->scanner);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case '=': {
            advance_token(state->scanner);

            area_add(&state->parser->nodes, &left);
            result.left_index = state->parser->nodes.count - 1;

            right = parse_assing_expression(state);
            area_add(&state->parser->nodes, &right);
            result.right_index = state->parser->nodes.count - 1;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
}

ast_node_t parse_expression(local_state_t *state) {
    return parse_assing_expression(state);
    /*
    ast_node_t left = {};

    left = parse_primary(state);

    ast_node_t right = {};
    */
}

// ast_node_t parse_block(local_state_t *state, ast_subtype_t subtype) {
    // check for open brace and closing one
    // and then do 
// }
//


ast_node_t parse_global_statement(local_state_t *state) {
    ast_node_t node = {};

    node.type = AST_ERROR;

    token_t name = peek_token(state->scanner);
    token_t next = peek_next_token(state->scanner);

    if (name.type == TOKEN_IDENT && next.type == ':') {
        consume_token(TOKEN_IDENT, state->scanner);
        consume_token(':', state->scanner);

        next = peek_token(state->scanner);

        switch(next.type) {
            case '=': {
                // evaluate on the right size
                //
                // SUBTYPE_AST_UNKN_DECL
            } break;

            case TOKEN_IDENT: {
                // prototype function definition
                //
                // SUBTYPE_AST_UNKN_DECL
            } break;

            case TOK_UNION: {
                //

            };
            case TOK_STRUCT: {
                // SUBTYPE_AST_AST_TYPE_DEFINITION

            } break;

            case TOK_ENUM: {
            } break;

            case '<':       // <T>(a : T) -> T   = {}
            case '(': {     // (a : s32)  -> s32 = {}
                // FUNC_GENERIC
                // SUBTYPE_AST_TYPE_DEFINITION
                // function/prototype definition
                //
                // @todo we can do generic prototypes btw.
                // but this could slow down compilation a bit
                //
                // t : <T>(a : T, b : T) -> T = prototype;
                //
                // f : t = {
                //     return a + b;
                // }
                // 
                // mul_by_2 : <B>(a : t<B>, b : B) = {
                //     a(b, b);
                // }
            } break;


            case TOK_BOOL32:

            case TOK_U8:
            case TOK_U16:
            case TOK_U32:
            case TOK_U64:

            case TOK_S8:
            case TOK_S16:
            case TOK_S32:
            case TOK_S64:

            case TOK_F32:
            case TOK_F64: {
                node.token   = advance_token(state->scanner);

                if (consume_token(';', state->scanner)) {
                    // name
                    node.type = AST_BIN; 
                } else {
                    node.type = AST_TERN; 
                }

                node.subtype = SUBTYPE_AST_EXPR;

                // @todo we cant just string from token... 
                // because it is in symbols table.
                // 
                // how to solve:
                //  create scope_t and use it instead of bare hashmaps
                //  so we could do extra dereference
                //
                //  scope_add(string_t symbol);

                // parser->
                // = name.data.symbol.table_index;
            } break;

            default: {
                         // @todo log error here
            } break;
        }

        // @nocheckin 
        node = parse_expression(state);

    } else {
        // this is where expression is, we care only now.
        // we will delete top level statements
        // @todo log error here
        node = parse_expression(state);
    }


    // @todo we dont need semicolon everywhere
    if (!consume_token(';', state->scanner)) {
        node.type = AST_ERROR;

        log_error_token(STR("Missing semicolon after expression."), state->scanner, node.token, 0);
    }


    return node;

}

b32 parse(scanner_t *scanner, parser_t* state) {
    local_state_t local = {};

    local.scanner = scanner;
    local.parser  = state;

    if (!area_create(&state->nodes, INIT_NODES_SIZE)) {
        log_error(STR("Parser: Couldn't create nodes list."), 0);
        return false;
    }

    if (!area_create(&state->root_indices, INIT_NODES_SIZE)) {
        log_error(STR("Parser: Couldn't create root indices list."), 0);
        return false;
    }

    if (!area_create(&state->scopes, INIT_NODES_SIZE)) {
        log_error(STR("Parser: Couldn't create scope indices list."), 0);
        return false;
    }

    u64 global_scope_index = 0;

    if (!area_allocate(&state->scopes, 1, &global_scope_index)) {
        log_error(STR("Parser: Couldn't create global scope."), 0);
        return false;
    }

    if (!hashmap_create(area_get(&state->scopes, global_scope_index), &scanner->symbols, 100)) {
        log_error(STR("Parser: Couldn't init global scope hashmap."), 0);
        return false;
    }

    b32 valid_parse = true;
    
    token_t curr = peek_token(local.scanner);

    while (curr.type != TOKEN_EOF && curr.type != TOKEN_ERROR) {
        ast_node_t node = parse_global_statement(&local);

        if (node.type == AST_ERROR) {
            valid_parse = false; 
            break;
        }

        if (!area_add(&local.parser->nodes, &node)) {
            log_error(STR("Parser: Couldn't add node to a list."), 0);
            valid_parse = false; 
            break;
        } 

        u64 root_index = local.parser->nodes.count - 1;
        area_add(&local.parser->root_indices, &root_index);

        curr = peek_token(local.scanner);
    }

    return valid_parse;
}
