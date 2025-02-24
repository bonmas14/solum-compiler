#include "parser.h"
#include "analyzer.h"
// @todo
//
// 1. parse this file
// 2. create export/import tables
// 3. parse all files from import table = go to 1 for all new files
// 4. analyze types and finish parsing
// 5. codegen

// rewrite this parser to something normal that doesnt use BNF 
// because it is too complicated

ast_node_t parse_expression(compiler_t *state);
ast_node_t parse_func_or_var_declaration(compiler_t *state, token_t *name);
ast_node_t parse_struct_declaration(compiler_t *state, token_t *name);
ast_node_t parse_union_declaration(compiler_t *state, token_t *name);
ast_node_t parse_enum_declaration(compiler_t *state, token_t *name);
ast_node_t parse_block(compiler_t* state, ast_subtype_t subtype);

/* helpers */

void panic_skip(compiler_t *state) {
    token_t token = peek_token(state->scanner, state->analyzer->symbols);

    u64 depth = 1;

    while (depth > 0) {
        if (token.type == TOKEN_EOF || token.type == TOKEN_ERROR) {
            break;
        }

        if (token.type == ';' && depth == 1) depth--;

        if (token.type == '{') depth++;
        if (token.type == '}') depth--;

        advance_token(state->scanner, state->analyzer->symbols);
        token = peek_token(state->scanner, state->analyzer->symbols);
    }
}

void panic_skip_until_token(u32 value, compiler_t *state) {
    token_t token = peek_token(state->scanner, state->analyzer->symbols);

    while (token.type != value && token.type != TOKEN_EOF && token.type != TOKEN_ERROR) {
        advance_token(state->scanner, state->analyzer->symbols);
        token = peek_token(state->scanner, state->analyzer->symbols);
    }
}

/* parsing */

ast_node_t parse_primary(compiler_t *state) {
    token_t token = peek_token(state->scanner, state->analyzer->symbols);

    ast_node_t result = {};

    result.type  = AST_LEAF;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case TOKEN_CONST_FP:
        case TOKEN_CONST_INT:
        case TOKEN_CONST_STRING:
        case TOKEN_IDENT:
        case TOK_DEFAULT:
            advance_token(state->scanner, state->analyzer->symbols);
            break;

        case TOKEN_OPEN_BRACE: {
            advance_token(state->scanner, state->analyzer->symbols);
            token_t next = peek_token(state->scanner, state->analyzer->symbols);

            if (next.type == TOKEN_CLOSE_BRACE) {
                result.type = AST_EMPTY;
                advance_token(state->scanner, state->analyzer->symbols);
                break;
            }

            result = parse_expression(state); 
            
            token_t error_token = {};
            if (!consume_token(TOKEN_CLOSE_BRACE, state->scanner, &error_token, state->analyzer->symbols)) {
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
    ast_node_t result = parse_primary(state);

    if (result.type == AST_EMPTY) return result;

    token_t end = {}, next = peek_token(state->scanner, state->analyzer->symbols);

    if (next.type == '(' && result.token.type == TOKEN_IDENT) {
        ast_node_t left = result;
        result = {};

        next = advance_token(state->scanner, state->analyzer->symbols);

        result.type  = AST_BIN;

        result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
        ZERO_CHECK(result.left);
        *result.left = left;

        ast_node_t right = parse_expression(state);
            
        // @todo if right is AST_EMPTY we should change it to something different
        result.right = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
        ZERO_CHECK(result.right);
        *result.right = right;

        check(consume_token(')', state->scanner, &end, state->analyzer->symbols));

        next.type = TOKEN_GEN_FUNC_CALL;

        next.c1 = end.c1;
        next.l1 = end.l1;

        result.token = next;
    }

    return result;
}

ast_node_t parse_unary(compiler_t *state) {
    token_t token = peek_token(state->scanner, state->analyzer->symbols);

    ast_node_t result = {};

    result.type  = AST_UNARY;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case '@':
        case '^':
        case '-':
        case '!': {
            advance_token(state->scanner, state->analyzer->symbols);
            ast_node_t child = parse_unary(state); // @todo check for errors
            
            if (child.type == AST_EMPTY || child.type == AST_ERROR) {
                check(false); // @todo better logging and parsing
            }

            result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.left);
            *result.left = child;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner, state->analyzer->symbols);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
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

    token_t token = peek_token(state->scanner, state->analyzer->symbols);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case TOKEN_LSHIFT: 
        case TOKEN_RSHIFT: {
            advance_token(state->scanner, state->analyzer->symbols);

            result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.left);
            *result.left = left;

            right = parse_shift(state); 

            // @todo error check
            result.right = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.right);
            *result.right = right;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner, state->analyzer->symbols);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
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

    token_t token = peek_token(state->scanner, state->analyzer->symbols);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case '&': {
            advance_token(state->scanner, state->analyzer->symbols);

            result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.left);
            *result.left = left;

            right = parse_and(state);
            result.right = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.right);
            *result.right = right;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner, state->analyzer->symbols);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
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

    token_t token = peek_token(state->scanner, state->analyzer->symbols);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case '|': {
            advance_token(state->scanner, state->analyzer->symbols);

            result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.left);
            *result.left = left;

            right = parse_or(state);
            result.right = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.right);
            *result.right = right;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner, state->analyzer->symbols);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
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

    token_t token = peek_token(state->scanner, state->analyzer->symbols);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case '^': {
            advance_token(state->scanner, state->analyzer->symbols);

            result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.left);
            *result.left = left;

            right = parse_xor(state);
            result.right = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.right);
            *result.right = right;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner, state->analyzer->symbols);
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
ast_node_t parse_mul(compiler_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_xor(state);
    if (left.type == AST_EMPTY) return left;

    token_t token = peek_token(state->scanner, state->analyzer->symbols);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case '*':
        case '/':
        case '%': {
            advance_token(state->scanner, state->analyzer->symbols);

            result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.left);
            *result.left = left;

            right = parse_mul(state);
            result.right = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.right);
            *result.right = right;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner, state->analyzer->symbols);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
}

ast_node_t parse_add(compiler_t *state) { 
    ast_node_t result = {}, left, right;

    left = parse_mul(state);
    if (left.type == AST_EMPTY) return left;

    token_t token = peek_token(state->scanner, state->analyzer->symbols);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case '+':
        case '-': {
            advance_token(state->scanner, state->analyzer->symbols);

            result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.left);
            *result.left = left;

            right = parse_add(state);
            result.right = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.right);
            *result.right = right;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner, state->analyzer->symbols);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
}

ast_node_t parse_compare_expression(compiler_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_add(state);
    if (left.type == AST_EMPTY) return left;

    token_t token = peek_token(state->scanner, state->analyzer->symbols);

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
            advance_token(state->scanner, state->analyzer->symbols);
            result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.left);
            *result.left = left;

            right = parse_compare_expression(state);
            result.right = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.right);
            *result.right = right;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner, state->analyzer->symbols);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
};

ast_node_t parse_logic_and_expression(compiler_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_compare_expression(state);
    if (left.type == AST_EMPTY) return left;

    token_t token = peek_token(state->scanner, state->analyzer->symbols);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case TOKEN_LOGIC_AND: {
            advance_token(state->scanner, state->analyzer->symbols);

            result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.left);
            *result.left = left;

            right = parse_logic_and_expression(state);
            result.right = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.right);
            *result.right = right;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner, state->analyzer->symbols);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = left;
        } break;
    }

    return result;

}
ast_node_t parse_logic_or_expression(compiler_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_logic_and_expression(state);
    if (left.type == AST_EMPTY) return left;

    token_t token = peek_token(state->scanner, state->analyzer->symbols);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case TOKEN_LOGIC_OR: {
            advance_token(state->scanner, state->analyzer->symbols);

            result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.left);
            *result.left = left;

            right = parse_logic_or_expression(state);
            result.right = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.right);
            *result.right = right;
        } break;
        case TOKEN_ERROR: {
            advance_token(state->scanner, state->analyzer->symbols);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;
        default: {
            result = left;
        } break;
    }

    return result;
}

ast_node_t parse_separated_expressions(compiler_t *state) {
    ast_node_t result = parse_logic_or_expression(state);
    if (result.type == AST_EMPTY) return result;

    // @todo change to an ast list later
    // for performance
    token_t expression_separator = {};
    if (consume_token(',', state->scanner, &expression_separator, state->analyzer->symbols)) {
        // @todo check of AST_ERROR
        ast_node_t node = result;

        result = {};

        result.type    = AST_BIN;
        result.subtype = SUBTYPE_AST_EXPR;
        result.token   = expression_separator;

        result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
        ZERO_CHECK(result.left);
        *result.left = node;

        node = parse_separated_expressions(state);
        result.right = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
        ZERO_CHECK(result.right);
        *result.right = node;
    }

    return result;
}

ast_node_t parse_expression(compiler_t *state) {
    ast_node_t result = {}, left, right;

    left = parse_separated_expressions(state);
    if (left.type == AST_EMPTY) return left;

    token_t token = peek_token(state->scanner, state->analyzer->symbols);

    result.type  = AST_BIN;
    result.token = token;
    result.subtype = SUBTYPE_AST_EXPR;

    switch (token.type) {
        case '=': {
            advance_token(state->scanner, state->analyzer->symbols);

            result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.left);
            *result.left = left;

            right = parse_expression(state); 
            check(right.type != AST_EMPTY);

            result.right = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.right);
            *result.right = right;
        } break;

        case TOKEN_ERROR: {
            advance_token(state->scanner, state->analyzer->symbols);
            result.type = AST_ERROR;
            log_error_token(STR("Parser: Error token"), state->scanner, token, 0);
        } break;

        default: {
            result = left;
        } break;
    }

    return result;
}


ast_node_t parse_primary_type(compiler_t *state) {
    ast_node_t result = {};

    token_t token = advance_token(state->scanner, state->analyzer->symbols);

    result.type  = AST_LEAF;
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
            result.subtype = SUBTYPE_AST_STD_TYPE;
            break;

        case TOKEN_IDENT:
            result.subtype = SUBTYPE_AST_UNKN_TYPE;
            break;

        default: {
            result.type = AST_ERROR;
            log_error_token(STR("Type cannot be constant value or keyword"), state->scanner, token, 0);
        } break;
    }

    return result;
}

/*
 
   parse_generic_type()

*/

// @todo dont forget about generics when creating type!
ast_node_t parse_type(compiler_t *state) {
    ast_node_t result = {};

    token_t token = peek_token(state->scanner, state->analyzer->symbols);

    result.type  = AST_UNARY;

    switch (token.type) {
        case '[': {
            advance_token(state->scanner, state->analyzer->symbols);

            result.type = AST_BIN;
            result.subtype = SUBTYPE_AST_ARR_TYPE;
            result.token.type = TOKEN_GEN_ARRAY_CALL;

            ast_node_t size   = parse_expression(state); 
            check(size.type != AST_EMPTY);

            token_t array_end = advance_token(state->scanner, state->analyzer->symbols); 

            if (array_end.type != ']') {
                log_error_token(STR("Array initializer expression should end with ']'."), state->scanner, array_end, 0);
                result.type = AST_ERROR;
                break;
            }

            result.token.c1 = array_end.c1;
            result.token.l1 = array_end.l1;

            result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.left);
            *result.left = size;

            ast_node_t type = parse_type(state);

            if (type.type == AST_ERROR) {
                result.type = AST_ERROR;
                break;
            }

            result.right = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.right);
            *result.right = type;

        } break;
        case '^': {
            result.token = advance_token(state->scanner, state->analyzer->symbols);
            result.subtype = SUBTYPE_AST_PTR_TYPE;

            ast_node_t type = parse_type(state);

            if (type.type == AST_ERROR) {
                result.type = AST_ERROR;
                break;
            }

            result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(result.left);
            *result.left = type;
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

    if (!consume_token(TOKEN_IDENT, state->scanner, &name, state->analyzer->symbols)) {
        node.type = AST_ERROR;
        panic_skip_until_token(')', state);
        log_error_token(STR("parameter name should be identifier."), state->scanner, name, 0);
        return node;
    }

    if (!consume_token(':', state->scanner, &next, state->analyzer->symbols)) {
        node.type = AST_ERROR;
        panic_skip_until_token(')', state);
        log_error_token(STR("':' separator before parameter type is required."), state->scanner, name, 0);
        return node;
    }

    node.token   = name;
    node.type    = AST_UNARY; 
    node.subtype = SUBTYPE_AST_PARAM_DEF;

    ast_node_t type = parse_type(state);

    if (type.type == AST_ERROR) {
        node.type = AST_ERROR;
        panic_skip_until_token(')', state);
        return node;
    }

    node.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
    ZERO_CHECK(node.left);
    *node.left = type;

    return node;
}

ast_node_t parse_generics_list(compiler_t *state) {
    return {};
}

ast_node_t parse_parameter_list(compiler_t *state) {
    ast_node_t result = {};

    result.type    = AST_LIST;
    result.subtype = SUBTYPE_AST_FUNC_PARAMS;

    token_t current   = peek_token(state->scanner, state->analyzer->symbols);
    result.token      = current;
    result.token.type = TOKEN_GEN_PARAM_LIST;

    ast_node_t *previous_node = NULL;

    while (current.type != ')' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_param_declaration(state);

        // @todo check for errors
        
        previous_node  = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
        *previous_node = node;
        previous_node  = previous_node->list_next;

        current = peek_token(state->scanner, state->analyzer->symbols);
        result.child_count++;

        if (current.type != ',' && current.type != ')') {
            log_error_token(STR("wrong token in parameter list"), state->scanner, current, 0);
            break;
        } 

        if (current.type == ',') {
            advance_token(state->scanner, state->analyzer->symbols);
            current = peek_token(state->scanner, state->analyzer->symbols);
        }
    }

    result.token.c1 = current.c0;
    result.token.l1 = current.l0;

    return result;
}

ast_node_t parse_return_list(compiler_t *state) {
    ast_node_t result = {};

    result.type    = AST_LIST;
    result.subtype = SUBTYPE_AST_FUNC_RETURNS;

    if (!consume_token(TOKEN_RET, state->scanner, &result.token, state->analyzer->symbols)) {
        result.type = AST_LEAF;
        return result;
    }

    token_t current = peek_token(state->scanner, state->analyzer->symbols);

    ast_node_t *previous_node = NULL;

    while (current.type != '=' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_type(state);

        // @todo check for errors
        
        previous_node  = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
        *previous_node = node;
        previous_node  = previous_node->list_next;

        current = peek_token(state->scanner, state->analyzer->symbols);
        result.child_count++;

        if (current.type != ',' && current.type != '=') {
            log_error_token(STR("wrong token in return list"), state->scanner, current, 0);
            break;
        }

        if (current.type == ',') {
            advance_token(state->scanner, state->analyzer->symbols);
            current = peek_token(state->scanner, state->analyzer->symbols);
        }
    }

    return result;
}

ast_node_t parse_function_type(compiler_t *state) {
    ast_node_t result = {};

    token_t token = peek_token(state->scanner, state->analyzer->symbols);
    result.token = token;
    result.token.type = TOKEN_GEN_FUNC_DEF;

    if (token.type == '<') {
        panic_skip_until_token('>', state);

        token_t end = advance_token(state->scanner, state->analyzer->symbols);

        token.c1 = end.c1;
        token.l1 = end.l1;

        log_warning_token(STR("generics are not supported."), state->scanner, token, 0);

        // result.type = AST_TERN; @todo
        result.type = AST_LEAF;

        // parse generics right here

    }
    else if (token.type == '(') {
        result.type = AST_BIN; 
    } else {
        assert(false);
    }

    result.subtype = SUBTYPE_AST_FUNC_TYPE;
    result.token.type = TOKEN_GEN_FUNC_DEF;

    // center is generic types

    if (!consume_token('(', state->scanner, NULL, state->analyzer->symbols)) {
        assert(false);
    }

    ast_node_t parameters = parse_parameter_list(state);
    result.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
    ZERO_CHECK(result.left);

    *result.left = parameters;

    // @todo check for errors
    if (!consume_token(')', state->scanner, NULL, state->analyzer->symbols)) {
        result.type = AST_ERROR;
        log_warning_token(STR("waiting for ')'."), state->scanner, result.token, 0);
    }

    ast_node_t returns = parse_return_list(state);

    if (returns.type == AST_ERROR) {
        result.type = AST_ERROR;
        log_warning_token(STR("couldn't parse return list in function."), state->scanner, result.token, 0);
    }

    result.right = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
    ZERO_CHECK(result.right);
    *result.right = returns;

    token_t type_end = peek_token(state->scanner, state->analyzer->symbols);

    if (type_end.type == TOKEN_ERROR) {
        result.type = AST_ERROR;
        log_warning_token(STR("expected an end of a type but got error."), state->scanner, result.token, 0);
    } else {
        result.token.c1 = type_end.c0;
        result.token.l1 = type_end.l0;
    }


    return result;
}

ast_node_t parse_declaration_type(compiler_t *state) {
    ast_node_t node = {};

    token_t token = peek_token(state->scanner, state->analyzer->symbols);

    node.type  = AST_LEAF;
    node.token = token;

    switch (token.type) {
        case '<':
        case '(': { 
            node = parse_function_type(state);
        } break;

        case '=': {
            node.subtype = SUBTYPE_AST_AUTO_TYPE;
        } break;

        default: {
            node = parse_type(state);
        } break;
    }

    return node;
}

ast_node_t parse_func_or_var_declaration(compiler_t *state, token_t *name) {
    ast_node_t node = {};
    node.token      = *name;
    node.type       = AST_BIN; 

    ast_node_t type = parse_declaration_type(state);

    if (type.type == AST_ERROR) {
        node.type = AST_ERROR;
        panic_skip(state);
        return node;
    }

    node.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
    ZERO_CHECK(node.left);
    *node.left = type;

    node.subtype = SUBTYPE_AST_UNKN_DEF;

    if (peek_token(state->scanner, state->analyzer->symbols).type == ';') {
        node.type = AST_UNARY; 
        return node;
    }

    if (!consume_token('=', state->scanner, NULL, state->analyzer->symbols)) {
        node.type = AST_ERROR;
        log_error_token(STR("expected assignment or semicolon after expression."), state->scanner, type.token, 0);
        panic_skip(state);
        return node;
    }
    
    ast_node_t data;

    if (peek_token(state->scanner, state->analyzer->symbols).type == '{') {
        data = parse_block(state, AST_BLOCK_IMPERATIVE);
    } else {
        data = parse_expression(state);
    }

    if (data.type == AST_ERROR || data.type == AST_EMPTY) {
        node.type = AST_ERROR;
        panic_skip(state);
        return node;
    }

    node.right  = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
    ZERO_CHECK(node.right);
    *node.right = data;

    return node;
}

ast_node_t parse_union_declaration(compiler_t *state, token_t *name) {
    ast_node_t result = {};

    result.type = AST_ERROR;
    result.token = *name;

    log_error_token(STR("unions are not realized."), state->scanner, advance_token(state->scanner, state->analyzer->symbols), 0);
    panic_skip(state);

    return result;
}

ast_node_t parse_struct_declaration(compiler_t *state, token_t *name) {
    ast_node_t result = {};

    result.type = AST_ERROR;
    result.token = *name;

    log_error_token(STR("structs are not realized."), state->scanner, advance_token(state->scanner, state->analyzer->symbols), 0);
    panic_skip(state);

    return result;
}

ast_node_t parse_enum_declaration(compiler_t *state, token_t *name) {
    ast_node_t result = {};

    result.type = AST_ERROR;
    result.token = *name;

    log_error_token(STR("enums are not realized."), state->scanner, advance_token(state->scanner, state->analyzer->symbols), 0);
    panic_skip(state);

    return result;
}

ast_node_t parse_statement(compiler_t *state) {
    ast_node_t node = {};

    node.type = AST_ERROR;

    token_t name = peek_token(state->scanner, state->analyzer->symbols);
    token_t next = peek_next_token(state->scanner, state->analyzer->symbols);

    node.token = name;
    b32 ignore_semicolon = false;

    if (name.type == TOKEN_IDENT && next.type == ':') {
        consume_token(TOKEN_IDENT, state->scanner, NULL, state->analyzer->symbols);
        consume_token(':', state->scanner, NULL, state->analyzer->symbols);

        next = peek_token(state->scanner, state->analyzer->symbols);

        switch(next.type) {
            case TOK_UNION: {
                node = parse_union_declaration(state, &name);
            } break;

            case TOK_STRUCT: {
                node = parse_struct_declaration(state, &name);
            } break;

            case TOK_ENUM: {
                node = parse_enum_declaration(state, &name);
            } break;

            default: {
                node = parse_func_or_var_declaration(state, &name);

                if (node.type != AST_BIN) {
                    break;
                }

                if (node.right->subtype != AST_BLOCK_IMPERATIVE) {
                    break;
                }

                ignore_semicolon = true;
            } break;
        }
    } else switch (name.type) {
        case  '{': {
            ignore_semicolon = true;
            node = parse_block(state, AST_BLOCK_IMPERATIVE);
        } break;

        case TOK_IF: {
            ignore_semicolon = true;
            node.type    = AST_BIN;
            node.subtype = SUBTYPE_AST_IF_STMT;
            node.token   = advance_token(state->scanner, state->analyzer->symbols);

            ast_node_t expr = parse_expression(state);
            check(expr.type != AST_EMPTY);
            
            ast_node_t stmt = parse_block(state, AST_BLOCK_IMPERATIVE);

            node.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(node.left);
            *node.left = expr;

            node.right  = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(node.right);
            *node.right = stmt;
        } break;

        case TOK_ELSE: {
            ignore_semicolon = true;
            node.token   = advance_token(state->scanner, state->analyzer->symbols);
            if (peek_token(state->scanner, state->analyzer->symbols).type == TOK_IF) {
                node = parse_statement(state);
                assert(node.subtype == SUBTYPE_AST_IF_STMT);
                node.subtype = SUBTYPE_AST_ELIF_STMT;
            } else {
                node.type    = AST_UNARY; 
                node.subtype = SUBTYPE_AST_ELSE_STMT;

                ast_node_t stmt = parse_block(state, AST_BLOCK_IMPERATIVE);
                node.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
                ZERO_CHECK(node.left);
                *node.left = stmt;
            }
        } break;

        case TOK_WHILE: {
            ignore_semicolon = true;
            node.type    = AST_BIN;
            node.subtype = SUBTYPE_AST_WHILE_STMT;
            node.token   = advance_token(state->scanner, state->analyzer->symbols);

            ast_node_t expr = parse_expression(state);
            check(expr.type != AST_EMPTY);

            ast_node_t stmt = parse_block(state, AST_BLOCK_IMPERATIVE);

            node.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(node.left);
            *node.left = expr;

            node.right = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(node.right);
            *node.right = stmt;
        } break;

        case TOK_RET: {
            node.type    = AST_UNARY;
            node.subtype = SUBTYPE_AST_RET_STMT;
            node.token   = advance_token(state->scanner, state->analyzer->symbols);

            ast_node_t expr = parse_expression(state);

            node.left = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
            ZERO_CHECK(node.left);
            *node.left = expr;
        } break;

        default: {
            node = parse_expression(state); // @todo add check for AST_EMPTY
        } break;
    }

    if (!ignore_semicolon) {
        token_t semicolon = {};

        if (!consume_token(';', state->scanner, &semicolon, state->analyzer->symbols)) {
            node.type = AST_ERROR;

            node.token.c1 = semicolon.c1;
            node.token.l1 = semicolon.l1;

            log_error_token(STR("Missing semicolon after statement."), state->scanner, node.token, 0);
            panic_skip(state);
        }
    }

    return node;
}

ast_node_t parse_imperative_block(compiler_t *state) {
    ast_node_t result = {};

    result.type    = AST_LIST;
    result.subtype = AST_BLOCK_IMPERATIVE;

    token_t current = peek_token(state->scanner, state->analyzer->symbols);

    ast_node_t *previous_node = result.list_next;

    while (current.type != '}' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_statement(state);

        if (node.type == AST_EMPTY) {
            current = peek_token(state->scanner, state->analyzer->symbols);
            continue;
        }

        previous_node  = (ast_node_t*)arena_allocate(state->parser->nodes, sizeof(ast_node_t));
        *previous_node = node;
        previous_node  = previous_node->list_next;

        current = peek_token(state->scanner, state->analyzer->symbols);
        result.child_count++;
    }

    return result;
}

ast_node_t parse_block(compiler_t *state, ast_subtype_t subtype) {
    token_t start, stop, final = {};

    ast_node_t result = {};

    if (!consume_token('{', state->scanner, &start, state->analyzer->symbols)) {
        result.type = AST_ERROR;
        log_error_token(STR("Expected opening of a block."), state->scanner, start, 0);
        return result;
    }

    result = parse_imperative_block(state);

    if (!consume_token('}', state->scanner, &stop, state->analyzer->symbols)) {
        result.type = AST_ERROR;
        log_error_token(STR("Expected closing of a block."), state->scanner, stop, 0);
        return result;
    }

    final.type = TOKEN_GEN_BLOCK; 

    final.c0 = start.c0;
    final.l0 = start.l0;

    final.c1 = stop.c1;
    final.l1 = stop.l1;

    result.token = final;

    return result;
}

b32 parse(compiler_t *compiler) {
	compiler->parser->nodes = arena_create(INIT_NODES_SIZE);

    if (!compiler->parser->nodes) {
        log_error(STR("Parser: Couldn't create nodes arena."), 0);
        return false;
    }

    if (!area_create(&compiler->parser->roots, INIT_NODES_SIZE)) {
        log_error(STR("Parser: Couldn't create root indices list."), 0);
        return false;
    }

    b32 valid_parse = true;
    
    token_t curr = peek_token(compiler->scanner, compiler->analyzer->symbols);

    while (curr.type != TOKEN_EOF && curr.type != TOKEN_ERROR) {
        ast_node_t node = parse_statement(compiler);

        if (node.type == AST_EMPTY) {
            curr = peek_token(compiler->scanner, compiler->analyzer->symbols);
            continue;
        }

        if (node.type == AST_ERROR) {
            valid_parse = false; 
        }

        ast_node_t *root = (ast_node_t*)arena_allocate(compiler->parser->nodes, sizeof(ast_node_t));
        ZERO_CHECK(root);
        *root = node;

        area_add(&compiler->parser->roots, &root);

        curr = peek_token(compiler->scanner, compiler->analyzer->symbols);
    }

    return valid_parse;
}
