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
        case TOK_NULL:
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

ast_node_t parse_primary_type(local_state_t *state) {
    ast_node_t result = {};

    token_t token = peek_token(state->scanner);

    result.type  = AST_LEAF;
    result.token = token;

    switch (token.type) {
        case '=':
            result.subtype = SUBTYPE_AST_AUTO_TYPE;
            break;

        case TOK_U8:
        case TOK_U16:
        case TOK_U32:
        case TOK_U64:

        case TOK_S8:
        case TOK_S16:
        case TOK_S32:
        case TOK_S64:

        case TOK_F32:
        case TOK_F64:
        case TOK_BOOL32:
            advance_token(state->scanner);
            result.subtype = SUBTYPE_AST_STD_TYPE;
            break;

        case TOKEN_IDENT:
            advance_token(state->scanner);
            result.subtype = SUBTYPE_AST_UNKN_TYPE;
            break;

        default: {
            advance_token(state->scanner);
            result.type = AST_ERROR;
            log_error_token(STR("Unknown type"), state->scanner, token, 0);
        } break;
    }

    return result;
}

ast_node_t parse_type(local_state_t *state) {
    ast_node_t result = {};

    token_t token = peek_token(state->scanner);

    result.type  = AST_UNARY;

    switch (token.type) {
        case '[': {
            result.type = AST_BIN;

            result.token = advance_token(state->scanner);

            /*
            if (peek_token(state->scanner).type != ']') { // @todo add the handle for [] without expression inside
            }
            */

            ast_node_t size   = parse_expression(state);
            token_t array_end = advance_token(state->scanner); 

            if (array_end.type != ']') {
                log_error_token(STR("Array initializer expression should end with ']'."), state->scanner, array_end, 0);
                result.type = AST_ERROR;
                break;
            }

            result.token.c1 = array_end.c1;
            result.token.l1 = array_end.l1;

            area_add(&state->parser->nodes, &size);
            result.left_index = state->parser->nodes.count - 1;

            // @todo this will allow for this case: [] ^ [] ^ [] s32
            // this doesnt make any sence, so we need to delete it in analyzer step

            ast_node_t type = parse_type(state);

            // @todo check for AST_ERROR
            if (type.subtype == SUBTYPE_AST_AUTO_TYPE) {
                result.type = AST_ERROR;
                log_error_token(STR("Type cannot be automatic when it specified as an array."), state->scanner, result.token, 0);
                break;
            }

            area_add(&state->parser->nodes, &type);
            result.right_index = state->parser->nodes.count - 1;

        } break;
        case '^': {
            result.token = advance_token(state->scanner);

            ast_node_t node = parse_type(state);
            // @todo check for AST_ERROR
            if (node.subtype == SUBTYPE_AST_AUTO_TYPE) {
                result.type = AST_ERROR;
                log_error_token(STR("Type cannot be automatic when it specified as a pointer."), state->scanner, result.token, 0);
                break;
            }
            area_add(&state->parser->nodes, &node);
            result.left_index = state->parser->nodes.count - 1;
        } break;

        default: {
            result = parse_primary_type(state);
        } break;
    }

    return result;
}

ast_node_t parse_declaration_type(local_state_t *state) {
    ast_node_t node = {};

    token_t token = peek_token(state->scanner);

    node.type  = AST_LEAF;
    node.token = token;

    switch (token.type) {
        // case '<':
        case '(': { 
            node.type = AST_BIN;

            consume_token('(', state->scanner);

            // parser_parameter_list();
            //     parse_parameter();
            consume_token(')', state->scanner);

            if (consume_token(TOKEN_RET, state->scanner)) {
                // we have return values
            } else {
                // we dont return anything
            }

            /*
            if (token.type == '<') {
                node.type = AST_TERN;
                // parse_generic_type_list();
                // CENTER
            }
            */

            // left = parse_parameter_list();
            // right = parse_return_types_list();
            //
            // center = imperative block or expression
        } break;

        default: {
            node = parse_type(state);
        } break;
    }

    return node;
}

ast_node_t parse_global_statement(local_state_t *state) {
    ast_node_t node = {};

    node.type = AST_ERROR;

    token_t name = peek_token(state->scanner);
    token_t next = peek_next_token(state->scanner);

    node.token = name;

    if (name.type == TOKEN_IDENT && next.type == ':') {
        consume_token(TOKEN_IDENT, state->scanner);
        consume_token(':', state->scanner);

        next = peek_token(state->scanner);

        switch(next.type) {
            case TOK_UNION: {
                //

            };
            case TOK_STRUCT: {
                // SUBTYPE_AST_AST_TYPE_DEFINITION
            } break;

            case TOK_ENUM: {
            } break;


            default: {
                ast_node_t type = parse_declaration_type(state);

                if (type.type == AST_ERROR) {
                    node.type = AST_ERROR;
                    break;
                }

                area_add(&state->parser->nodes, &type);
                node.left_index = state->parser->nodes.count - 1;

                node.token = name;

                if (consume_token(';', state->scanner)) {
                    node.type = AST_UNARY; 
                    node.subtype = SUBTYPE_AST_VAR;
                    return node;
                }

                node.type = AST_BIN; 

                if (!consume_token('=', state->scanner)) {
                    node.type = AST_ERROR;
                    log_error_token(STR("expected assignment or semicolon after expression."), state->scanner, type.token, 0);
                    break;
                }

                // @todo check for:
                // - function declaration
                // - prototype declaration
                //
                // right now we assume we only have expresions here
                ast_node_t expresion = parse_expression(state);
                area_add(&state->parser->nodes, &expresion);
                node.right_index = state->parser->nodes.count - 1;

                // we got specific decl
                //
                // it could be type definition also

                // return node;
            } break;
        }
    } else {
        // this is where expression is, we care only now.
        // we will delete top level statements
        // @todo log error here
        node = parse_expression(state);
    }


    token_t semicolon = advance_token(state->scanner);
    // @todo we dont need semicolon everywhere
    if (semicolon.type != ';') {
        node.type = AST_ERROR;

        log_error_token(STR("Missing semicolon after expression."), state->scanner, semicolon, 0);
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

        // only adding AST_VAR
        if (node.subtype == SUBTYPE_AST_VAR) {
            scope_entry_t entry = {};

            entry.type       = SCOPE_VAR;
            entry.node_index = root_index;

            hashmap_t<scope_entry_t> *scope = area_get(&state->scopes, 0);

            if (hashmap_contains(scope, node.token.data.symbol)) {
                log_error_token(STR("the name of declaration is already in use."), local.scanner, node.token, 0);
                valid_parse = false;
                break;
            } else {
                hashmap_add(scope, node.token.data.symbol, &entry);
            }
        }


        curr = peek_token(local.scanner);
    }

    return valid_parse;
}
