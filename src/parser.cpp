#include "parser.h"

// @todo
//
// 1. parse this file
// 2. create export/import tables
// 3. parse all files from import table = go to 1 for all new files
// 4. analyze types and finish parsing
// 5. codegen



/* helpers */

/* parsing */

ast_node_t parse_expression(compiler_t *state);
ast_node_t parse_func_or_var_declaration(compiler_t *state, token_t *name);
ast_node_t parse_struct_declaration(compiler_t *state, token_t *name);
ast_node_t parse_union_declaration(compiler_t *state, token_t *name);
ast_node_t parse_enum_declaration(compiler_t *state, token_t *name);
ast_node_t parse_block(compiler_t* state, ast_subtype_t subtype);

void panic_skip(compiler_t *state) {
    token_t token = peek_token(state->scanner);

    u64 depth = 1;

    while (depth > 0) {
        if (token.type == TOKEN_EOF || token.type == TOKEN_ERROR) {
            break;
        }

        if (token.type == ';' && depth == 1) depth--;

        if (token.type == '{') depth++;
        if (token.type == '}') depth--;

        advance_token(state->scanner);
        token = peek_token(state->scanner);
    }
}

void panic_skip_until_token(u32 value, compiler_t *state) {
    token_t token = peek_token(state->scanner);

    while (token.type != value && token.type != TOKEN_EOF && token.type != TOKEN_ERROR) {
        advance_token(state->scanner);
        token = peek_token(state->scanner);
    }
}

ast_node_t parse_primary(compiler_t *state) {
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
        case TOK_DEFAULT:
        case TOK_PROTOTYPE:
            break;

        case TOKEN_OPEN_BRACE: {
            token_t next = peek_token(state->scanner);
            if (next.type == TOKEN_CLOSE_BRACE) {
                result.type = AST_ERROR;
                log_error_token(STR("Expected expression"), state->scanner, next, 0);
                break;
            }

            result = parse_expression(state);

            token_t error_token = {};
            if (!consume_token(TOKEN_CLOSE_BRACE, state->scanner, &error_token)) {
                result.type = AST_ERROR;

                log_error_token(STR("Parser: expected closing brace after expression"), state->scanner, error_token, 0);
            }
        } break;

        default:
            result.type = AST_ERROR;
            log_error_token(STR("Parser: wrong primary token type"),
                            state->scanner, token, 0);
            break;
    }

    return result;
}

ast_node_t parse_function_call(compiler_t *state) {
    return parse_primary(state);
}

ast_node_t parse_unary(compiler_t *state) {
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
            ast_node_t child = parse_unary(state); // @todo check for errors
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

ast_node_t parse_shift(compiler_t *state) {
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

ast_node_t parse_and(compiler_t *state) {
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

ast_node_t parse_or(compiler_t *state) {
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

ast_node_t parse_xor(compiler_t *state) {
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
ast_node_t parse_mul(compiler_t *state) {
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

ast_node_t parse_add(compiler_t *state) { 
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

ast_node_t parse_compare_expression(compiler_t *state) {
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

ast_node_t parse_logic_and_expression(compiler_t *state) {
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
ast_node_t parse_logic_or_expression(compiler_t *state) {
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

// @todo: add mulitple return statements assignment
ast_node_t parse_assignment_expression(compiler_t *state) {
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

            right = parse_assignment_expression(state);
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

ast_node_t parse_expression(compiler_t *state) {
    ast_node_t result = parse_assignment_expression(state);

    // @todo change to an ast list later
    // for performance
    token_t expression_separator = {};
    if (consume_token(',', state->scanner, &expression_separator)) {
        // @todo check of AST_ERROR
        ast_node_t node = result;

        result = {};

        result.type    = AST_BIN;
        result.subtype = SUBTYPE_AST_EXPR;
        result.token   = expression_separator;

        area_add(&state->parser->nodes, &node);
        result.left_index = state->parser->nodes.count - 1;

        node = parse_expression(state);
        area_add(&state->parser->nodes, &node);
        result.right_index = state->parser->nodes.count - 1;
    }

    return result;
}


ast_node_t parse_primary_type(compiler_t *state) {
    ast_node_t result = {};

    token_t token = advance_token(state->scanner);

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
            log_error_token(STR("Unknown type"), state->scanner, token, 0);
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

ast_node_t parse_param_declaration(compiler_t *state) {
    ast_node_t node = {};

    token_t name, next;

    if (!consume_token(TOKEN_IDENT, state->scanner, &name)) {
        node.type = AST_ERROR;
        panic_skip_until_token(')', state);
        log_error_token(STR("parameter name should be identifier."), state->scanner, name, 0);
        return node;
    }

    if (!consume_token(':', state->scanner, &next)) {
        node.type = AST_ERROR;
        panic_skip_until_token(')', state);
        log_error_token(STR("':' separator before parameter type is required."), state->scanner, name, 0);
        return node;
    }

    node.token   = name;
    node.type    = AST_UNARY; 
    node.subtype = SUBTYPE_AST_VAR;

    ast_node_t type = parse_type(state);

    if (type.type == AST_ERROR) {
        node.type = AST_ERROR;
        panic_skip_until_token(')', state);
        return node;
    }

    area_add(&state->parser->nodes, &type);
    node.left_index = state->parser->nodes.count - 1;

    return node;
}

ast_node_t parse_generics_list(compiler_t *state) {
    return {};
}

ast_node_t parse_parameter_list(compiler_t *state) {
    ast_node_t result = {};

    result.type    = AST_LIST;
    result.subtype = SUBTYPE_AST_FUNC_PARAMS;

    token_t current = peek_token(state->scanner);
    result.token = current;

    b32 first_time = true;
    u64 previous_node_index = 0;

    while (current.type != ')' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_param_declaration(state);

        // @todo check for errors
        
        area_add(&state->parser->nodes, &node);

        if (first_time) {
            first_time = false;
            result.list_next_node = state->parser->nodes.count - 1;
            previous_node_index = result.list_next_node;
        } else {
            ast_node_t *prev = area_get(&state->parser->nodes, previous_node_index);
            prev->list_next_node = state->parser->nodes.count - 1;
            previous_node_index = prev->list_next_node;
        }

        current = peek_token(state->scanner);
        result.child_count++;

        if (current.type != ',' && current.type != ')') {
            log_error_token(STR("wrong token in parameter list"), state->scanner, current, 0);
            break;
        } 

        if (current.type == ',') {
            advance_token(state->scanner);
            current = peek_token(state->scanner);
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

    if (!consume_token(TOKEN_RET, state->scanner, &result.token)) {
        result.type = AST_LEAF;
        return result;
    }

    token_t current = peek_token(state->scanner);

    b32 first_time = true;
    u64 previous_node_index = 0;

    while (current.type != '=' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_type(state);

        // @todo check for errors
        
        area_add(&state->parser->nodes, &node);

        if (first_time) {
            first_time = false;
            result.list_next_node = state->parser->nodes.count - 1;
            previous_node_index = result.list_next_node;
        } else {
            ast_node_t *prev = area_get(&state->parser->nodes, previous_node_index);
            prev->list_next_node = state->parser->nodes.count - 1;
            previous_node_index = prev->list_next_node;
        }

        current = peek_token(state->scanner);
        result.child_count++;

        if (current.type != ',' && current.type != '=') {
            log_error_token(STR("wrong token in return list"), state->scanner, current, 0);
            break;
        }

        if (current.type == ',') {
            advance_token(state->scanner);
            current = peek_token(state->scanner);
        }
    }

    return result;
}

ast_node_t parse_function_type(compiler_t *state) {
    ast_node_t result = {};

    token_t token = peek_token(state->scanner);
    result.token = token;

    if (token.type == '<') {
        panic_skip_until_token('>', state);

        token_t end = advance_token(state->scanner);

        token.c1 = end.c1;
        token.l1 = end.l1;

        log_warning_token(STR("generics are not supported."), state->scanner, token, 0);

        // result.type = AST_TERN; @todo
        result.type = AST_LEAF;

        // parse generics right here

    }
    else if (token.type == '(') {
        result.type = AST_BIN; 
        result.token = token;
    } else {
        assert(false);
    }

    result.subtype = SUBTYPE_AST_FUNC_TYPE;

    // center is generic types

    if (!consume_token('(', state->scanner, NULL)) {
        assert(false);
    }

    ast_node_t parameters = parse_parameter_list(state);
    area_add(&state->parser->nodes, &parameters);
    result.left_index = state->parser->nodes.count - 1;

    // @todo check for errors
    consume_token(')', state->scanner, NULL);

    ast_node_t returns = parse_return_list(state);
    area_add(&state->parser->nodes, &returns);
    result.right_index = state->parser->nodes.count - 1;

    token_t type_end = peek_token(state->scanner);
    result.token.c1 = type_end.c0;
    result.token.l1 = type_end.l0;

    return result;
}

ast_node_t parse_declaration_type(compiler_t *state) {
    ast_node_t node = {};

    token_t token = peek_token(state->scanner);

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

    area_add(&state->parser->nodes, &type);
    node.left_index = state->parser->nodes.count - 1;


    if (peek_token(state->scanner).type == ';') {
        node.type = AST_UNARY; 
        node.subtype = SUBTYPE_AST_VAR;
        return node;
    }

    if (!consume_token('=', state->scanner, NULL)) {
        node.type = AST_ERROR;
        log_error_token(STR("expected assignment or semicolon after expression."), state->scanner, type.token, 0);
        panic_skip(state);
        return node;
    }
    
    ast_node_t data;

    if (peek_token(state->scanner).type == '{') {
        data = parse_block(state, AST_BLOCK_IMPERATIVE);
    } else {
        data = parse_expression(state);
    }

    if (data.type == AST_ERROR) {
        node.type = AST_ERROR;
        panic_skip(state);
        return node;
    }

    area_add(&state->parser->nodes, &data);
    node.right_index = state->parser->nodes.count - 1;

    return node;
}

ast_node_t parse_union_declaration(compiler_t *state, token_t *name) {
    ast_node_t result = {};

    result.type = AST_ERROR;
    result.token = *name;

    log_error_token(STR("unions are not realized."), state->scanner, advance_token(state->scanner), 0);
    panic_skip(state);

    return result;
}

ast_node_t parse_struct_declaration(compiler_t *state, token_t *name) {
    ast_node_t result = {};

    result.type = AST_ERROR;
    result.token = *name;

    log_error_token(STR("structs are not realized."), state->scanner, advance_token(state->scanner), 0);
    panic_skip(state);

    return result;
}

ast_node_t parse_enum_declaration(compiler_t *state, token_t *name) {
    ast_node_t result = {};

    result.type = AST_ERROR;
    result.token = *name;

    log_error_token(STR("enums are not realized."), state->scanner, advance_token(state->scanner), 0);
    panic_skip(state);

    return result;
}

ast_node_t parse_statement(compiler_t *state) {
    ast_node_t node = {};

    node.type = AST_ERROR;

    token_t name = peek_token(state->scanner);
    token_t next = peek_next_token(state->scanner);

    node.token = name;
    b32 ignore_semicolon = false;

    if (name.type == TOKEN_IDENT && next.type == ':') {
        consume_token(TOKEN_IDENT, state->scanner, NULL);
        consume_token(':', state->scanner, NULL);

        next = peek_token(state->scanner);

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
            node.token   = advance_token(state->scanner);

            ast_node_t expr = parse_expression(state);
            ast_node_t stmt = parse_block(state, AST_BLOCK_IMPERATIVE);

            area_add(&state->parser->nodes, &expr);
            node.left_index = state->parser->nodes.count - 1;

            area_add(&state->parser->nodes, &stmt);
            node.right_index = state->parser->nodes.count - 1;
        } break;

        case TOK_ELSE: {
            ignore_semicolon = true;
            node.token   = advance_token(state->scanner);
            if (peek_token(state->scanner).type == TOK_IF) {
                node = parse_statement(state);
                assert(node.subtype == SUBTYPE_AST_IF_STMT);
                node.subtype = SUBTYPE_AST_ELIF_STMT;
            } else {
                node.type    = AST_UNARY; 
                node.subtype = SUBTYPE_AST_ELSE_STMT;

                ast_node_t stmt = parse_block(state, AST_BLOCK_IMPERATIVE);
                area_add(&state->parser->nodes, &stmt);
                node.left_index = state->parser->nodes.count - 1;
            }
        } break;

        case TOK_WHILE: {
            ignore_semicolon = true;
            node.type    = AST_BIN;
            node.subtype = SUBTYPE_AST_WHILE_STMT;
            node.token   = advance_token(state->scanner);

            ast_node_t expr = parse_expression(state);
            ast_node_t stmt = parse_block(state, AST_BLOCK_IMPERATIVE);

            area_add(&state->parser->nodes, &expr);
            node.left_index = state->parser->nodes.count - 1;

            area_add(&state->parser->nodes, &stmt);
            node.right_index = state->parser->nodes.count - 1;
        } break;

        case TOK_RET: {
            node.type    = AST_UNARY;
            node.subtype = SUBTYPE_AST_RET_STMT;
            node.token   = advance_token(state->scanner);

            ast_node_t expr = parse_expression(state);
            area_add(&state->parser->nodes, &expr);
            node.left_index = state->parser->nodes.count - 1;
        } break;

        default: {
            node = parse_expression(state);
        } break;
    }

    if (!ignore_semicolon) {
        token_t semicolon = {};

        if (!consume_token(';', state->scanner, &semicolon)) {
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

    token_t current = peek_token(state->scanner);

    b32 first_time = true;
    u64 previous_node_index = 0;

    while (current.type != '}' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_statement(state);

        area_add(&state->parser->nodes, &node);

        if (first_time) {
            first_time = false;
            result.list_next_node = state->parser->nodes.count - 1;
            previous_node_index = result.list_next_node;
        } else {
            ast_node_t *prev = area_get(&state->parser->nodes, previous_node_index);
            prev->list_next_node = state->parser->nodes.count - 1;
            previous_node_index = prev->list_next_node;
        }

        current = peek_token(state->scanner);
        result.child_count++;
    }

    return result;
}

ast_node_t parse_block(compiler_t *state, ast_subtype_t subtype) {
    token_t start, stop, final = {};

    ast_node_t result = {};

    if (!consume_token('{', state->scanner, &start)) {
        result.type = AST_ERROR;
        log_error_token(STR("Expected opening of a block."), state->scanner, start, 0);
        return result;
    }

    result = parse_imperative_block(state);

    if (!consume_token('}', state->scanner, &stop)) {
        result.type = AST_ERROR;
        log_error_token(STR("Expected closing of a block."), state->scanner, stop, 0);
        return result;
    }

    final.type = '{'; // TOKEN_BLOCK

    final.c0 = start.c0;
    final.l0 = start.l0;

    final.c1 = stop.c1;
    final.l1 = stop.l1;

    result.token = final;

    return result;
}

b32 parse(compiler_t *compiler) {
    if (!area_create(&compiler->parser->nodes, INIT_NODES_SIZE)) {
        log_error(STR("Parser: Couldn't create nodes list."), 0);
        return false;
    }

    if (!area_create(&compiler->parser->root_indices, INIT_NODES_SIZE)) {
        log_error(STR("Parser: Couldn't create root indices list."), 0);
        return false;
    }

    b32 valid_parse = true;
    
    token_t curr = peek_token(compiler->scanner);

    while (curr.type != TOKEN_EOF && curr.type != TOKEN_ERROR) {
        ast_node_t node = parse_statement(compiler);

        if (node.type == AST_ERROR) {
            valid_parse = false; 
        }

        if (!area_add(&compiler->parser->nodes, &node)) {
            log_error(STR("Parser: Couldn't add node to a list."), 0);
            valid_parse = false; 
            break;
        } 

        u64 root_index = compiler->parser->nodes.count - 1;
        area_add(&compiler->parser->root_indices, &root_index);

        curr = peek_token(compiler->scanner);
    }

    return valid_parse;
}
