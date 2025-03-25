#include "parser.h"

// @todo for parser:
// rewrite expression parser, because it is too complex and plain
//
// @note:
// we need to process all errors on the spot,
// but if it is trailing error, we dont append any logs

ast_node_t parse_type(compiler_t *state);
ast_node_t parse_expression(compiler_t *state);
ast_node_t parse_func_or_var_declaration(compiler_t *state, token_t *name);
ast_node_t parse_struct_declaration(compiler_t *state, token_t *name);
ast_node_t parse_union_declaration(compiler_t *state, token_t *name);
ast_node_t parse_enum_declaration(compiler_t *state, token_t *name);
ast_node_t parse_block(compiler_t* state, ast_types_t type);

/* helpers */

void panic_skip(compiler_t *state) {
    token_t token = peek_token(state->scanner, state->strings);

    u64 depth = 1;

    while (depth > 0) {
        if (token.type == TOKEN_EOF || token.type == TOKEN_ERROR) {
            break;
        }

        if (token.type == ';' && depth == 1) depth--;

        if (token.type == '{') depth++;
        if (token.type == '}') depth--;

        advance_token(state->scanner, state->strings);
        token = peek_token(state->scanner, state->strings);
    }
}

void panic_skip_until_token(u32 value, compiler_t *state) {
    token_t token = peek_token(state->scanner, state->strings);

    while (token.type != value && token.type != TOKEN_EOF && token.type != TOKEN_ERROR) {
        advance_token(state->scanner, state->strings);
        token = peek_token(state->scanner, state->strings);
    }
}

void add_left_node(compiler_t *state, ast_node_t *root, ast_node_t *node) {
    assert(root != NULL);
    assert(node != NULL);

    check_value(node->type != AST_ERROR);

    root->left = (ast_node_t*)mem_alloc(state->nodes, sizeof(ast_node_t));
    assert(root->left != NULL);
    *root->left = *node;
}

void add_right_node(compiler_t *state, ast_node_t *root, ast_node_t *node) {
    assert(root != NULL);
    assert(node != NULL);

    check_value(node->type != AST_ERROR);

    root->right = (ast_node_t*)mem_alloc(state->nodes, sizeof(ast_node_t));
    assert(root->right != NULL);
    *root->right = *node;
}

void add_center_node(compiler_t *state, ast_node_t *root, ast_node_t *node) {
    assert(root != NULL);
    assert(node != NULL);

    check_value(node->type != AST_ERROR);

    root->center = (ast_node_t*)mem_alloc(state->nodes, sizeof(ast_node_t));
    assert(root->center != NULL);
    *root->center = *node;
}

void add_list_node(compiler_t *state, ast_node_t *root, ast_node_t *node) {
    assert(root != NULL);
    assert(node != NULL);

    check_value(node->type != AST_ERROR);

    if (root->list_start == NULL) {
        root->list_start = (ast_node_t*)mem_alloc(state->nodes, sizeof(ast_node_t));
        *root->list_start = *node;
        root->child_count++;
        return;
    }

    ast_node_t *list_node = root->list_start;

    while (list_node->list_next != NULL) {
        list_node = list_node->list_next;
    }

    list_node->list_next  = (ast_node_t*)mem_alloc(state->nodes, sizeof(ast_node_t));
    *list_node->list_next = *node;

    root->child_count++;
}

/* parsing */

ast_node_t parse_primary(compiler_t *state) {
    token_t token = peek_token(state->scanner, state->strings);

    ast_node_t result = {};

    result.type  = AST_PRIMARY;
    result.token = token;

    switch (token.type) {
        case TOKEN_CONST_FP:
        case TOKEN_CONST_INT:
        case TOKEN_CONST_STRING:
        case TOKEN_IDENT:
        case TOK_DEFAULT:
            advance_token(state->scanner, state->strings);
            break;

        case TOKEN_OPEN_BRACE: {
            advance_token(state->scanner, state->strings);
            token_t next = peek_token(state->scanner, state->strings);

            if (next.type == TOKEN_CLOSE_BRACE) {
                result.type = AST_EMPTY;
                advance_token(state->scanner, state->strings);
                break;
            }

            result = parse_expression(state); 
            
            token_t error_token = {};
            if (!consume_token(TOKEN_CLOSE_BRACE, state->scanner, &error_token, state->strings)) {
                result.type = AST_ERROR;

                log_error_token(STR("Parser: expected closing brace after expression"), state->scanner, error_token, 0);
            }
        } break;

        default:
            result.type = AST_EMPTY;
            break;
    }

    return result;
}

ast_node_t parse_function_call(compiler_t *state) {
    ast_node_t node = parse_primary(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    token_t next = peek_token(state->scanner, state->strings);

    if (next.type == '(') {
        result.type = AST_FUNC_CALL;
        result.token = next;

        advance_token(state->scanner, state->strings);

        add_left_node(state, &result, &node);
        node = parse_expression(state);

        check_value(node.type != AST_ERROR); // @todo: better messages
        add_right_node(state, &result, &node);
            
        check_value(consume_token(')', state->scanner, NULL, state->strings));
    } else if (next.type == '.') {
        result.type = AST_MEMBER_ACCESS;
        result.token = next;

        advance_token(state->scanner, state->strings);

        add_left_node(state, &result, &node);
        node = parse_function_call(state);
            
        check_value(node.type != AST_EMPTY);
        check_value(node.type != AST_ERROR);
        check_value(node.token.type == TOKEN_IDENT || node.type == AST_MEMBER_ACCESS);

        add_right_node(state, &result, &node);
    } else if (next.type == '[') {
        result.type  = AST_ARRAY_ACCESS;
        result.token = next;

        advance_token(state->scanner, state->strings);

        add_left_node(state, &result, &node);

        node = parse_expression(state);
            
        check_value(consume_token(']', state->scanner, NULL, state->strings));
        check_value(node.type != AST_EMPTY);
        check_value(node.type != AST_ERROR);

        add_right_node(state, &result, &node);
    } else {
        result = node;
    }

    return result;
}

ast_node_t parse_unary(compiler_t *state) {
    token_t token = peek_token(state->scanner, state->strings);

    ast_node_t result = {};

    result.type  = AST_UNARY;
    result.token = token;

    switch (token.type) {
        case '@':
        case '^':
        case '-':
        case '!': {
            advance_token(state->scanner, state->strings);
            ast_node_t child = parse_unary(state); // @todo check_value for errors
            
            if (child.type == AST_EMPTY || child.type == AST_ERROR) {
                check_value(false); // @todo better logging and parsing
            }

            add_left_node(state, &result, &child);
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner, state->strings);
            result.type = AST_ERROR; 
        } break;
        default: {
            result = parse_function_call(state);
        } break;
    }

    return result;
}

ast_node_t parse_shift(compiler_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_unary(state);

    if (left.type == AST_EMPTY) return left;

    token_t token = peek_token(state->scanner, state->strings);

    result.type  = AST_BIN_BIT_SHIFT; // @change
    result.token = token;

    switch (token.type) {
        case TOKEN_LSHIFT: 
        case TOKEN_RSHIFT: {
            advance_token(state->scanner, state->strings);

            add_left_node(state, &result, &left);
            right = parse_shift(state); 
            // @todo error check_value
            add_right_node(state, &result, &right);
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner, state->strings);
            result.type = AST_ERROR;
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
}

ast_node_t parse_and(compiler_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_shift(state);
    if (left.type == AST_EMPTY) return left;

    token_t token = peek_token(state->scanner, state->strings);

    result.type  = AST_BIN_BIT_AND;
    result.token = token;

    switch (token.type) {
        case '&': {
            advance_token(state->scanner, state->strings);

            add_left_node(state, &result, &left);
            right = parse_and(state);
            add_right_node(state, &result, &right);
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner, state->strings);
            result.type = AST_ERROR;
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
}

ast_node_t parse_or(compiler_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_and(state);
    if (left.type == AST_EMPTY) return left;

    token_t token = peek_token(state->scanner, state->strings);

    result.type  = AST_BIN_BIT_OR;
    result.token = token;

    switch (token.type) {
        case '|': {
            advance_token(state->scanner, state->strings);

            add_left_node(state, &result, &left);
            right = parse_or(state);
            add_right_node(state, &result, &right);
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner, state->strings);
            result.type = AST_ERROR;
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
}

ast_node_t parse_xor(compiler_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_or(state);
    if (left.type == AST_EMPTY) return left;

    token_t token = peek_token(state->scanner, state->strings);

    result.type  = AST_BIN_BIT_XOR;
    result.token = token;

    switch (token.type) {
        case '^': {
            advance_token(state->scanner, state->strings);

            add_left_node(state, &result, &left);
            right = parse_xor(state);
            add_right_node(state, &result, &right);
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner, state->strings);
            result.type = AST_ERROR;
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
}

// @todo: different names later!!!
ast_node_t parse_mul(compiler_t *state) {
    ast_node_t node = parse_xor(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    result.token = peek_token(state->scanner, state->strings);

    switch (result.token.type) {
        case '*': 
            result.type = AST_BIN_MUL;
            break;
        case '/':
            result.type = AST_BIN_DIV;
            break;
        case '%': 
            result.type = AST_BIN_MOD;
            break;
            
        case TOKEN_ERROR: 
            advance_token(state->scanner, state->strings);
            result.type = AST_ERROR;
            return result;

        default: 
            return node;
    }

    advance_token(state->scanner, state->strings);

    add_left_node(state, &result, &node);
    node = parse_mul(state);
    add_right_node(state, &result, &node);

    return result;
}

ast_node_t parse_add(compiler_t *state) { 
    ast_node_t node = parse_mul(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    result.token = peek_token(state->scanner, state->strings);

    switch (result.token.type) {
        case '+': 
            result.type = AST_BIN_ADD;
            break;

        case '-': 
            result.type = AST_BIN_SUB;
            break;
            

        case TOKEN_ERROR:
            advance_token(state->scanner, state->strings);
            result.type = AST_ERROR;
            return result;

        default:
            return node;
    }

    advance_token(state->scanner, state->strings);

    add_left_node(state, &result, &node);
    node = parse_add(state);
    add_right_node(state, &result, &node);

    return result;
}

ast_node_t parse_compare_expression(compiler_t *state) {
    ast_node_t node = parse_add(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    result.token = peek_token(state->scanner, state->strings);

    switch (result.token.type) {
        case TOKEN_GR: 
            result.type = AST_BIN_GR;
            break;
        case TOKEN_LS:
            result.type = AST_BIN_LS;
            break;
        case TOKEN_GEQ:
            result.type = AST_BIN_GEQ;
            break;
        case TOKEN_LEQ:
            result.type = AST_BIN_LEQ;
            break;
        case TOKEN_EQ:
            result.type = AST_BIN_EQ;
            break;
        case TOKEN_NEQ: 
            result.type = AST_BIN_NEQ;
            break;

        case TOKEN_ERROR:
            advance_token(state->scanner, state->strings);
            result.type = AST_ERROR;
            return result;

        default:
            return node;
    }

    advance_token(state->scanner, state->strings);

    add_left_node(state, &result, &node);
    node = parse_compare_expression(state);
    add_right_node(state, &result, &node);

    return result;
};

ast_node_t parse_logic_and_expression(compiler_t *state) {
    ast_node_t node = parse_compare_expression(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    result.token = peek_token(state->scanner, state->strings);

    switch (result.token.type) {
        case TOKEN_LOGIC_AND:
            result.type = AST_BIN_LOG_AND;
            break;

        case TOKEN_ERROR: 
            advance_token(state->scanner, state->strings);
            result.type = AST_ERROR;
            return result;

        default:
            return node;
    }

    advance_token(state->scanner, state->strings);

    add_left_node(state, &result, &node);
    node = parse_logic_and_expression(state);
    add_right_node(state, &result, &node);

    return result;

}
ast_node_t parse_logic_or_expression(compiler_t *state) {
    ast_node_t node = parse_logic_and_expression(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    result.token = peek_token(state->scanner, state->strings);

    switch (result.token.type) {
        case TOKEN_LOGIC_OR:
            result.type = AST_BIN_LOG_OR;
            break;

        case TOKEN_ERROR: 
            advance_token(state->scanner, state->strings);
            result.type = AST_ERROR;
            return result;

        default: 
            return node;
    }

    advance_token(state->scanner, state->strings);

    add_left_node(state, &result, &node);
    node = parse_logic_or_expression(state);
    add_right_node(state, &result, &node);

    return result;
}

ast_node_t parse_cast_exression(compiler_t *state) {
    token_t    cast_token = {};
    ast_node_t result     = {};

    if (consume_token(TOK_CAST, state->scanner, &cast_token, state->strings)) {
        check_value(consume_token('(', state->scanner, NULL, state->strings));
        ast_node_t type = parse_type(state);
        check_value(consume_token(')', state->scanner, NULL, state->strings));

        ast_node_t expr = parse_logic_or_expression(state);

        result.type  = AST_BIN_CAST;
        result.token = cast_token;

        if (expr.type == AST_EMPTY) {
            result.type = AST_ERROR;
            log_error_token(STR("Error while parsing cast. No expression found."), state->scanner, result.token, 0);
            return result;
        }

        add_left_node(state, &result, &type);
        add_right_node(state, &result, &expr);
        return result;
    }

    return parse_logic_or_expression(state);
}

ast_node_t parse_separated_expressions(compiler_t *state) {
    ast_node_t result = parse_cast_exression(state);
    if (result.type == AST_EMPTY) return result;

    // @todo change to an ast list later
    // for performance
    token_t expression_separator = {};
    if (consume_token(',', state->scanner, &expression_separator, state->strings)) {
        // @todo check_value of AST_ERROR
        ast_node_t node = result;

        result = {};

        result.type    = AST_BIN_SEPARATION;
        result.token   = expression_separator;

        add_left_node(state, &result, &node);
        node = parse_separated_expressions(state);
        add_right_node(state, &result, &node);
    }

    return result;
}

ast_node_t parse_expression(compiler_t *state) {
    ast_node_t node = parse_separated_expressions(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    result.token = peek_token(state->scanner, state->strings);

    switch (result.token.type) {
        case '=': 
            result.type = AST_BIN_ASSIGN;
            break;

        case TOKEN_ERROR:
            advance_token(state->scanner, state->strings);
            result.type = AST_ERROR;
            return result;

        default: 
            return node;
    }

    advance_token(state->scanner, state->strings);

    add_left_node(state, &result, &node);
    node = parse_expression(state); 
    check_value(node.type != AST_EMPTY);
    add_right_node(state, &result, &node);

    return result;
}


ast_node_t parse_primary_type(compiler_t *state) {
    ast_node_t result = {};

    token_t token = advance_token(state->scanner, state->strings);

    result.token = token;

    switch (token.type) {
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
            result.type = AST_STD_TYPE;
            break;

        case TOKEN_IDENT:
            result.type = AST_UNKN_TYPE;
            break;

        default: {
            result.type = AST_ERROR;
            log_error_token(STR("Type cannot be constant value or keyword"), state->scanner, token, 0);
        } break;
    }

    return result;
}


ast_node_t parse_type(compiler_t *state) {
    ast_node_t result = {};

    token_t token = peek_token(state->scanner, state->strings);

    switch (token.type) {
        case '[': {
            result.type  = AST_ARR_TYPE;
            result.token = advance_token(state->scanner, state->strings);

            ast_node_t size = parse_expression(state); 
            check_value(size.type != AST_EMPTY);

            token_t array_end = advance_token(state->scanner, state->strings); 

            if (array_end.type != ']') {
                log_error_token(STR("Array initializer expression should end with ']'."), state->scanner, array_end, 0);
                result.type = AST_ERROR;
                break;
            }

            add_left_node(state, &result, &size);
            ast_node_t type = parse_type(state);

            if (type.type == AST_ERROR) {
                result.type = AST_ERROR;
                break;
            }

            add_right_node(state, &result, &type);
        } break;
        case '^': {
            result.type  = AST_PTR_TYPE;
            result.token = advance_token(state->scanner, state->strings);

            ast_node_t type = parse_type(state);

            if (type.type == AST_ERROR) {
                result.type = AST_ERROR;
                break;
            }

            add_left_node(state, &result, &type);
        } break;

        default: {
            result = parse_primary_type(state);
        } break;
    }

    return result;
}

ast_node_t parse_param_declaration(compiler_t *state) {
    ast_node_t node = {};

    token_t name, next;

    if (!consume_token(TOKEN_IDENT, state->scanner, &name, state->strings)) {
        node.type = AST_ERROR;
        panic_skip_until_token(')', state);
        log_error_token(STR("parameter name should be identifier."), state->scanner, name, 0);
        return node;
    }

    if (!consume_token(':', state->scanner, &next, state->strings)) {
        node.type = AST_ERROR;
        panic_skip_until_token(')', state);
        log_error_token(STR("':' separator before parameter type is required."), state->scanner, name, 0);
        return node;
    }

    node.type  = AST_PARAM_DEF;
    node.token = name;

    ast_node_t type = parse_type(state);

    if (type.type == AST_ERROR) {
        node.type = AST_ERROR;
        panic_skip_until_token(')', state);
        return node;
    }

    add_left_node(state, &node, &type);
    return node;
}

ast_node_t parse_parameter_list(compiler_t *state) {
    ast_node_t result = {};

    result.type = AST_FUNC_PARAMS;

    token_t current = peek_token(state->scanner, state->strings);
    result.token    = current;

    while (current.type != ')' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_param_declaration(state);

        // @todo check_value for errors
        add_list_node(state, &result, &node);

        current = peek_token(state->scanner, state->strings);

        if (current.type == ',') {
            advance_token(state->scanner, state->strings);
            current = peek_token(state->scanner, state->strings);
        } else if (current.type != ')') {
            log_error_token(STR("wrong token in parameter list"), state->scanner, current, 0);
            break;
        }
    }

    return result;
}

ast_node_t parse_return_list(compiler_t *state) {
    ast_node_t result = {};

    result.type = AST_FUNC_RETURNS;

    if (!consume_token(TOKEN_RET, state->scanner, &result.token, state->strings)) {
        result.type = AST_EMPTY;
        return result;
    }

    token_t current = peek_token(state->scanner, state->strings);

    while (current.type != '=' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_type(state);

        // @todo check_value for errors
        add_list_node(state, &result, &node);

        current = peek_token(state->scanner, state->strings);

        if (current.type == ',') {
            advance_token(state->scanner, state->strings);
            current = peek_token(state->scanner, state->strings);
        } else if (current.type != '=') {
            log_error_token(STR("wrong token in return list"), state->scanner, current, 0);
            break;
        }
    }

    return result;
}

ast_node_t parse_function_type(compiler_t *state) {
    token_t token = {};

    if (!consume_token('(', state->scanner, &token, state->strings)) {
        assert(false);
    }

    ast_node_t result = {};
    result.type = AST_FUNC_TYPE;
    result.token = token;

    ast_node_t parameters = parse_parameter_list(state);
    // @todo check_value for errors
    add_left_node(state, &result, &parameters);

    if (!consume_token(')', state->scanner, NULL, state->strings)) {
        result.type = AST_ERROR;
        log_warning_token(STR("waiting for ')'."), state->scanner, result.token, 0);
    }

    ast_node_t returns = parse_return_list(state);

    if (returns.type == AST_ERROR) {
        result.type = AST_ERROR;
        log_warning_token(STR("couldn't parse return list in function."), state->scanner, result.token, 0);
    }

    add_right_node(state, &result, &returns);

    return result;
}

ast_node_t parse_multiple_types(compiler_t *state) {
    ast_node_t node = parse_type(state);

    if (consume_token(',', state->scanner, NULL, state->strings)) {
        ast_node_t result = {};

        result.type  = AST_MUL_TYPES;
        result.token = node.token;

        add_left_node(state, &result, &node);
        node = parse_multiple_types(state);
        check_value(node.type != AST_ERROR);
        add_right_node(state, &result, &node);

        return result;
    }

    return node;
}

ast_node_t parse_declaration_type(compiler_t *state) {
    ast_node_t node = {};

    token_t token = peek_token(state->scanner, state->strings);
    node.token = token;

    switch (token.type) {
        case '(':
            return parse_function_type(state);

        case '=': 
            node.type = AST_AUTO_TYPE;
            return node;

        default:
            return parse_type(state);
    }
}

ast_node_t parse_multiple_var_declaration(compiler_t *state, ast_node_t *expr) {
    ast_node_t node = {}, type = {};

    node.type = AST_TERN_MULT_DEF;

    if (peek_token(state->scanner, state->strings).type == '=') {
        token_t token = peek_token(state->scanner, state->strings);

        type.type = AST_MUL_AUTO;
        type.token = token;
    } else {
        type = parse_multiple_types(state);
    }

    if (type.type == AST_ERROR) {
        node.type = AST_ERROR;
        panic_skip(state);
        return node;
    }

    if (type.type == AST_FUNC_TYPE) {
        node.type = AST_ERROR;
        log_error_token(STR("You can't define multiple funcitons in same statement."), state->scanner, type.token, 0);
        panic_skip(state);
        return node;
    }

    // left is our type, right is names expressions, center is expression

    add_left_node(state, &node, &type);
    add_right_node(state, &node, expr);

    token_t token = peek_token(state->scanner, state->strings);

    if (token.type == ';') {
        node.type = AST_BIN_MULT_DEF; 
        return node;
    } else if (token.type != '=') {
        node.type = AST_ERROR;
        log_error_token(STR("expected assignment or semicolon after expression."), state->scanner, type.token, 0);
        panic_skip(state);
        return node;
    }

    advance_token(state->scanner, state->strings);
    ast_node_t data = parse_expression(state);

    if (data.type == AST_ERROR || data.type == AST_EMPTY) {
        node.type = AST_ERROR;
        panic_skip(state);
        return node;
    }

    add_center_node(state, &node, &data);
    return node;
}

ast_node_t parse_func_or_var_declaration(compiler_t *state, token_t *name) {
    ast_node_t node = {};

    node.type  = AST_BIN_UNKN_DEF;
    node.token = *name;

    ast_node_t type = parse_declaration_type(state);

    if (type.type == AST_ERROR) {
        node.type = AST_ERROR;
        panic_skip(state);
        return node;
    }

    add_left_node(state, &node, &type);

    if (peek_token(state->scanner, state->strings).type == ';') {
        node.type = AST_UNARY_VAR_DEF; 
        return node;
    }

    if (!consume_token('=', state->scanner, NULL, state->strings)) {
        node.type = AST_ERROR;
        log_error_token(STR("expected assignment or semicolon after expression."), state->scanner, type.token, 0);
        panic_skip(state);
        return node;
    }
    
    ast_node_t data;

    if (peek_token(state->scanner, state->strings).type == '{') {
        data = parse_block(state, AST_BLOCK_IMPERATIVE);
    } else {
        data = parse_expression(state);
    }

    if (data.type == AST_ERROR || data.type == AST_EMPTY) {
        node.type = AST_ERROR;
        panic_skip(state);
        return node;
    }

    add_right_node(state, &node, &data);

    return node;
}

ast_node_t parse_union_declaration(compiler_t *state, token_t *name) {
    // @todo better checking_value 
    check_value(consume_token(TOK_UNION, state->scanner, NULL, state->strings));
    check_value(consume_token('=', state->scanner, NULL, state->strings));

    ast_node_t result = {};

    result.type  = AST_UNION_DEF;
    result.token = *name;

    ast_node_t left = parse_block(state, AST_BLOCK_IMPERATIVE);
    // @todo add checks_value
    add_left_node(state, &result, &left);
    return result;
}

ast_node_t parse_struct_declaration(compiler_t *state, token_t *name) {
    // @todo better checking_value 
    check_value(consume_token(TOK_STRUCT, state->scanner, NULL, state->strings));
    check_value(consume_token('=', state->scanner, NULL, state->strings));

    ast_node_t result = {};


    result.type  = AST_STRUCT_DEF;
    result.token = *name;

    ast_node_t left = parse_block(state, AST_BLOCK_IMPERATIVE);
    // @todo add checks_value
    add_left_node(state, &result, &left);

    return result;
}

ast_node_t parse_enum_declaration(compiler_t *state, token_t *name) {
    ast_node_t result = {};

    result.type = AST_ERROR;
    result.token = *name;

    log_error_token(STR("enums are not realized."), state->scanner, advance_token(state->scanner, state->strings), 0);
    panic_skip(state);

    return result;
}

ast_node_t parse_statement(compiler_t *state) {
    ast_node_t node = {};

    node.type = AST_ERROR;

    token_t name = peek_token(state->scanner, state->strings);
    token_t next = peek_next_token(state->scanner, state->strings);

    node.token = name;
    b32 ignore_semicolon = false;

    if (name.type == TOKEN_IDENT && next.type == ':') {
        consume_token(TOKEN_IDENT, state->scanner, NULL, state->strings);
        consume_token(':', state->scanner, NULL, state->strings);

        next = peek_token(state->scanner, state->strings);

        switch(next.type) {
            case TOK_UNION: {
                node = parse_union_declaration(state, &name);
                ignore_semicolon = true;
            } break;

            case TOK_STRUCT: {
                node = parse_struct_declaration(state, &name);
                ignore_semicolon = true;
            } break;

            case TOK_ENUM: {
                node = parse_enum_declaration(state, &name);
                ignore_semicolon = true;
            } break;

            default: {
                node = parse_func_or_var_declaration(state, &name);

                if (node.type != AST_BIN_UNKN_DEF) {
                    break;
                }

                if (node.right->type != AST_BLOCK_IMPERATIVE) {
                    break;
                }

                ignore_semicolon = true;
            } break;
        }
    } else if (name.type == TOKEN_IDENT && next.type == ',') {
        ast_node_t expr = parse_expression(state);
        assert(expr.type != AST_EMPTY);

        next = peek_token(state->scanner, state->strings);

        if (next.type == ':') {
            advance_token(state->scanner, state->strings);
            node = parse_multiple_var_declaration(state, &expr);
        } else {
            node = expr;
        }

    } else switch (name.type) {
        case  '{': {
            ignore_semicolon = true;
            node = parse_block(state, AST_BLOCK_IMPERATIVE);
        } break;

        case TOK_IF: {
            ignore_semicolon = true;
            node.type = AST_IF_STMT;
            node.token = advance_token(state->scanner, state->strings);

            ast_node_t expr = parse_expression(state);
            check_value(expr.type != AST_EMPTY);
            
            ast_node_t stmt = parse_block(state, AST_BLOCK_IMPERATIVE);
            add_left_node(state, &node, &expr);
            add_right_node(state, &node, &stmt);
        } break;

        case TOK_ELSE: {
            ignore_semicolon = true;
            node.token   = advance_token(state->scanner, state->strings);
            if (peek_token(state->scanner, state->strings).type == TOK_IF) {
                node = parse_statement(state);
                assert(node.type == AST_IF_STMT);
                node.type = AST_ELIF_STMT;
            } else {
                node.type = AST_ELSE_STMT;

                ast_node_t stmt = parse_block(state, AST_BLOCK_IMPERATIVE);
                add_left_node(state, &node, &stmt);
            }
        } break;

        case TOK_WHILE: {
            ignore_semicolon = true;
            node.type  = AST_WHILE_STMT;
            node.token = advance_token(state->scanner, state->strings);

            ast_node_t expr = parse_expression(state);
            check_value(expr.type != AST_EMPTY);

            ast_node_t stmt = parse_block(state, AST_BLOCK_IMPERATIVE);
            add_left_node(state, &node, &expr);
            add_right_node(state, &node, &stmt);
        } break;

        case TOK_RETURN: {
            node.type  = AST_RET_STMT;
            node.token = advance_token(state->scanner, state->strings);

            ast_node_t expr = parse_expression(state);
            add_left_node(state, &node, &expr);
        } break;

        default: {
            node = parse_expression(state);
        } break;
    }

    if (!ignore_semicolon) {
        token_t semicolon = {};

        if (!consume_token(';', state->scanner, &semicolon, state->strings)) {
            node.type = AST_ERROR;

            log_error_token(STR("We expected semicolon after this token."), state->scanner, node.token, 0);
            panic_skip(state);
        }
    }

    return node;
}

ast_node_t parse_imperative_block(compiler_t *state) {
    ast_node_t result = {};

    result.type = AST_BLOCK_IMPERATIVE;

    token_t current = peek_token(state->scanner, state->strings);

    while (current.type != '}' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_statement(state);

        if (node.type == AST_EMPTY) {
            current = peek_token(state->scanner, state->strings);
            continue;
        }

        add_list_node(state, &result, &node);
        current = peek_token(state->scanner, state->strings);
    }

    return result;
}

ast_node_t parse_block(compiler_t *state, ast_types_t type) {
    ast_node_t result = {};
    token_t start, stop;

    if (!consume_token('{', state->scanner, &start, state->strings)) {
        result.type = AST_ERROR;
        log_error_token(STR("Expected opening of a block."), state->scanner, start, 0);
        return result;
    }

    if (type == AST_BLOCK_IMPERATIVE) {
        result = parse_imperative_block(state);
    } else if (type == AST_BLOCK_ENUM) {
        assert(false);
    } else {
        assert(false);
    }

    if (!consume_token('}', state->scanner, &stop, state->strings)) {
        result.type = AST_ERROR;
        log_error_token(STR("Expected closing of a block."), state->scanner, stop, 0);
        return result;
    }

    return result;
}

b32 parse(compiler_t *compiler) {
    if (compiler->parser->parsed_roots.data != NULL) {
        list_delete(&compiler->parser->parsed_roots);
    }

    if (!list_create(&compiler->parser->parsed_roots, INIT_NODES_SIZE)) {
        log_error(STR("Parser: Couldn't create root indices list."));
        return false;
    }

    b32 valid_parse = true;
    
    token_t curr = peek_token(compiler->scanner, compiler->strings);

    while (curr.type != TOKEN_EOF && curr.type != TOKEN_ERROR) {
        ast_node_t node = parse_statement(compiler);

        if (node.type == AST_EMPTY) {
            curr = peek_token(compiler->scanner, compiler->strings);
            continue;
        }

        if (node.type == AST_ERROR) {
            valid_parse = false; 
        }

        ast_node_t *root = (ast_node_t*)mem_alloc(compiler->nodes, sizeof(ast_node_t));
        *root = node;

        list_add(&compiler->parser->parsed_roots, &root);

        curr = peek_token(compiler->scanner, compiler->strings);
    }

    return valid_parse;
}
