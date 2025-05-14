#include "parser.h"
#include "arena.h"
#include "talloc.h"

// @todo for parser:
// rewrite expression parser, because it is too complex and plain
//
// @note:
// we need to process all errors on the spot,
// but if it is trailing error, we dont append any logs

struct parser_state_t {
    scanner_t *scanner;

    allocator_t *strings;
    allocator_t *nodes;
};

static ast_node_t parse_type(parser_state_t *state);

static ast_node_t parse_swap_expression(parser_state_t *state, ast_node_t *expr);
static ast_node_t parse_separated_expressions(parser_state_t *state);
static ast_node_t parse_cast_expression(parser_state_t *state);

static ast_node_t parse_func_or_var_declaration(parser_state_t *state, token_t *name);
static ast_node_t parse_struct_declaration(parser_state_t *state, token_t *name);
static ast_node_t parse_union_declaration(parser_state_t *state, token_t *name);
static ast_node_t parse_enum_declaration(parser_state_t *state, token_t *name);
static ast_node_t parse_block(parser_state_t* state, ast_types_t type);

static void panic_skip(parser_state_t *state) {
    allocator_t *talloc = get_temporary_allocator();
    token_t token = peek_token(state->scanner, talloc);

    u64 depth = 1;

    while (depth > 0) {
        if (token.type == TOKEN_EOF || token.type == TOKEN_ERROR) {
            break;
        }

        if (token.type == ';' && depth == 1) depth--;

        if (token.type == '{') depth++;
        if (token.type == '}') depth--;

        if (depth > 0) {
            advance_token(state->scanner, talloc);
            token = peek_token(state->scanner, talloc);
        }
    }
}

static void panic_skip_until_token(u32 value, parser_state_t *state) {
    allocator_t *talloc = get_temporary_allocator();
    token_t token = peek_token(state->scanner, talloc);

    while (token.type != value && token.type != TOKEN_EOF && token.type != TOKEN_ERROR) {
        advance_token(state->scanner, state->strings);
        token = peek_token(state->scanner, talloc);
    }
}

static void add_left_node(parser_state_t *state, ast_node_t *root, ast_node_t *node) {
    assert(root != NULL);
    assert(node != NULL);

    check_value(node->type != AST_ERROR);

    root->left = (ast_node_t*)mem_alloc(state->nodes, sizeof(ast_node_t));
    assert(root->left != NULL);
    *root->left = *node;

    if (node->type == AST_ERROR) {
        root->type = AST_ERROR;
    }
}

static void add_right_node(parser_state_t *state, ast_node_t *root, ast_node_t *node) {
    assert(root != NULL);
    assert(node != NULL);

    check_value(node->type != AST_ERROR);

    root->right = (ast_node_t*)mem_alloc(state->nodes, sizeof(ast_node_t));
    assert(root->right != NULL);
    *root->right = *node;

    if (node->type == AST_ERROR) {
        root->type = AST_ERROR;
    }
}

static void add_center_node(parser_state_t *state, ast_node_t *root, ast_node_t *node) {
    assert(root != NULL);
    assert(node != NULL);

    check_value(node->type != AST_ERROR);

    root->center = (ast_node_t*)mem_alloc(state->nodes, sizeof(ast_node_t));
    assert(root->center != NULL);
    *root->center = *node;

    if (node->type == AST_ERROR) {
        root->type = AST_ERROR;
    }
}

static void add_list_node(parser_state_t *state, ast_node_t *root, ast_node_t *node) {
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

    if (node->type == AST_ERROR) {
        root->type = AST_ERROR;
    }
}

/* parsing */

static ast_node_t parse_primary(parser_state_t *state) {
    ast_node_t result = {};

    allocator_t *talloc = get_temporary_allocator();

    result.type  = AST_PRIMARY;
    result.token = peek_token(state->scanner, talloc);

    switch (result.token.type) {
        case TOKEN_CONST_FP:
        case TOKEN_CONST_INT:
        case TOKEN_CONST_STRING:
        case TOKEN_IDENT:
        case TOK_DEFAULT:
        case TOK_TRUE:
        case TOK_FALSE:
            result.token = advance_token(state->scanner, state->strings);
            break;

        case TOKEN_OPEN_BRACE: {
            advance_token(state->scanner, talloc);
            token_t next = peek_token(state->scanner, talloc);

            // @todo change this to something less ugly
            if (next.type == TOKEN_CLOSE_BRACE) {
                result.type = AST_EMPTY;
                advance_token(state->scanner, talloc);
                break;
            }

            result = parse_cast_expression(state); 
            
            token_t error_token = {};
            if (!consume_token(TOKEN_CLOSE_BRACE, state->scanner, &error_token, false, talloc)) {
                result.type = AST_ERROR;
            }
        } break;

        default:
            result.type = AST_EMPTY;
            break;
    }

    return result;
}

// @todo rewrite
static ast_node_t parse_function_call(parser_state_t *state) {
    ast_node_t node = parse_primary(state);
    if (node.type == AST_EMPTY) return node;

    allocator_t *talloc = get_temporary_allocator();

    ast_node_t result = {};
    token_t next = peek_token(state->scanner, get_temporary_allocator());

    if (next.type == '(') {
        result.type  = AST_FUNC_CALL;
        result.token = advance_token(state->scanner, state->strings);

        add_left_node(state, &result, &node);
        node = parse_separated_expressions(state);

        check_value(node.type != AST_ERROR); // @todo: better messages
        add_right_node(state, &result, &node);
            
        consume_token(')', state->scanner, NULL, true, talloc);
    } else if (next.type == '.') {
        result.type  = AST_MEMBER_ACCESS;
        result.token = advance_token(state->scanner, state->strings);

        add_left_node(state, &result, &node);
        node = parse_function_call(state);
            
        check_value(node.type != AST_EMPTY);
        check_value(node.type != AST_ERROR);

        switch (node.token.type) {
            case TOKEN_IDENT:
                break;

            case TOKEN_CONST_FP:
            case TOKEN_CONST_INT:
            case TOKEN_CONST_STRING:
            case TOK_DEFAULT:
                result.type = AST_ERROR;
                break;
        }

        add_right_node(state, &result, &node);
    } else if (next.type == '[') {
        result.type  = AST_ARRAY_ACCESS;
        result.token = advance_token(state->scanner, state->strings);

        add_left_node(state, &result, &node);

        node = parse_separated_expressions(state);
            
        consume_token(']', state->scanner, NULL, false, talloc);
        check_value(node.type != AST_EMPTY);
        check_value(node.type != AST_ERROR);

        add_right_node(state, &result, &node);
    } else {
        result = node;
    }

    return result;
}

static ast_node_t parse_unary(parser_state_t *state) {
    ast_node_t result = {};
    result.token = peek_token(state->scanner, get_temporary_allocator());

    switch (result.token.type) {
        case '@':
            result.type = AST_UNARY_DEREF;
            result.token = advance_token(state->scanner, state->strings);
            break;

        case '^':
            result.type = AST_UNARY_REF;
            result.token = advance_token(state->scanner, state->strings);
            break;

        case '-':
            result.type = AST_UNARY_NEGATE;
            result.token = advance_token(state->scanner, state->strings);
            break;

        case '!': 
            result.type = AST_UNARY_NOT;
            result.token = advance_token(state->scanner, state->strings);
            break;

        case TOKEN_ERROR: 
            result.type = AST_ERROR;
            advance_token(state->scanner, get_temporary_allocator());
            return result;

        default:
            return parse_function_call(state);
    }

    ast_node_t child = parse_unary(state); // @todo check_value for errors

    if (child.type == AST_EMPTY || child.type == AST_ERROR) {
        check_value(false); // @todo better logging and parsing
    }

    add_left_node(state, &result, &child);

    return result;
}

// @todo rewrite
static ast_node_t parse_shift(parser_state_t *state) {
    ast_node_t node = parse_unary(state);

    if (node.type == AST_EMPTY) return node; 

    ast_node_t result = {};
    result.token = peek_token(state->scanner, get_temporary_allocator());

    switch (result.token.type) {
        case TOKEN_LSHIFT: 
            result.type  = AST_BIN_BIT_LSHIFT; 
            result.token = advance_token(state->scanner, state->strings);
            break;

        case TOKEN_RSHIFT: 
            result.type  = AST_BIN_BIT_RSHIFT; 
            result.token = advance_token(state->scanner, state->strings);
            break;

        case TOKEN_ERROR:
            advance_token(state->scanner, get_temporary_allocator());
            node.type = AST_ERROR;
            return node;

        default:
            return node;
    }

    add_left_node(state, &result, &node);
    node = parse_shift(state); 
    // @todo error check_value
    add_right_node(state, &result, &node);
    return result;
}

// @todo rewrite
static ast_node_t parse_and(parser_state_t *state) {
    ast_node_t node = parse_shift(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    result.token = peek_token(state->scanner, get_temporary_allocator());

    switch (result.token.type) {
        case '&':
            result.type  = AST_BIN_BIT_AND;
            result.token = advance_token(state->scanner, state->strings);
            break;

        case TOKEN_ERROR:
            advance_token(state->scanner, get_temporary_allocator());
            node.type = AST_ERROR;
            return node;
        default: 
            return node;
    }

    add_left_node(state, &result, &node);
    node = parse_and(state);
    add_right_node(state, &result, &node);

    return result;
}

static ast_node_t parse_or(parser_state_t *state) {
    ast_node_t node = parse_and(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    result.token = peek_token(state->scanner, get_temporary_allocator());

    switch (result.token.type) {
        case '|':
            result.type  = AST_BIN_BIT_OR;
            result.token = advance_token(state->scanner, state->strings);
            break;

        case TOKEN_ERROR:
            advance_token(state->scanner, get_temporary_allocator());
            node.type = AST_ERROR;
            return node;

        default:
            return node;
    }

    add_left_node(state, &result, &node);
    node = parse_or(state);
    add_right_node(state, &result, &node);

    return result;
}

static ast_node_t parse_xor(parser_state_t *state) {
    ast_node_t node = parse_or(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    result.token = peek_token(state->scanner, get_temporary_allocator());

    switch (result.token.type) {
        case '^': 
            result.type  = AST_BIN_BIT_XOR;
            result.token = advance_token(state->scanner, state->strings);
            break;

        case TOKEN_ERROR:
            advance_token(state->scanner, get_temporary_allocator());
            result.type = AST_ERROR;
            return result;

        default: 
            return node;
    }

    add_left_node(state, &result, &node);
    node = parse_xor(state);
    add_right_node(state, &result, &node);

    return result;
}

// @todo: different names later!!!
static ast_node_t parse_mul(parser_state_t *state) {
    ast_node_t node = parse_xor(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    result.token = peek_token(state->scanner, get_temporary_allocator());

    switch (result.token.type) {
        case '*': 
            result.type  = AST_BIN_MUL;
            result.token = advance_token(state->scanner, state->strings);
            break;
        case '/':
            result.type = AST_BIN_DIV;
            result.token = advance_token(state->scanner, state->strings);
            break;
        case '%': 
            result.type = AST_BIN_MOD;
            result.token = advance_token(state->scanner, state->strings);
            break;
            
        case TOKEN_ERROR: 
            advance_token(state->scanner, get_temporary_allocator());
            node.type = AST_ERROR;
            return node;

        default: 
            return node;
    }

    add_left_node(state, &result, &node);
    node = parse_mul(state);
    add_right_node(state, &result, &node);

    return result;
}

static ast_node_t parse_add(parser_state_t *state) { 
    ast_node_t node = parse_mul(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    result.token = peek_token(state->scanner, get_temporary_allocator());

    switch (result.token.type) {
        case '+': 
            result.type = AST_BIN_ADD;
            result.token = advance_token(state->scanner, state->strings);
            break;

        case '-': 
            result.type = AST_BIN_SUB;
            result.token = advance_token(state->scanner, state->strings);
            break;
            

        case TOKEN_ERROR:
            advance_token(state->scanner, get_temporary_allocator());
            node.type = AST_ERROR;
            return node;

        default:
            return node;
    }

    add_left_node(state, &result, &node);
    node = parse_add(state);
    add_right_node(state, &result, &node);

    return result;
}

static ast_node_t parse_compare_expression(parser_state_t *state) {
    ast_node_t node = parse_add(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    result.token = peek_token(state->scanner, get_temporary_allocator());

    switch (result.token.type) {
        case TOKEN_GR: 
            result.type = AST_BIN_GR;
            result.token = advance_token(state->scanner, state->strings);
            break;
        case TOKEN_LS:
            result.type = AST_BIN_LS;
            result.token = advance_token(state->scanner, state->strings);
            break;
        case TOKEN_GEQ:
            result.type = AST_BIN_GEQ;
            result.token = advance_token(state->scanner, state->strings);
            break;
        case TOKEN_LEQ:
            result.type = AST_BIN_LEQ;
            result.token = advance_token(state->scanner, state->strings);
            break;
        case TOKEN_EQ:
            result.type = AST_BIN_EQ;
            result.token = advance_token(state->scanner, state->strings);
            break;
        case TOKEN_NEQ: 
            result.type = AST_BIN_NEQ;
            result.token = advance_token(state->scanner, state->strings);
            break;

        case TOKEN_ERROR:
            advance_token(state->scanner, get_temporary_allocator());
            node.type = AST_ERROR;
            return node;

        default:
            return node;
    }

    add_left_node(state, &result, &node);
    node = parse_compare_expression(state);
    add_right_node(state, &result, &node);

    return result;
}

static ast_node_t parse_logic_and_expression(parser_state_t *state) {
    ast_node_t node = parse_compare_expression(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    result.token = peek_token(state->scanner, get_temporary_allocator());

    switch (result.token.type) {
        case TOKEN_LOGIC_AND:
            result.type = AST_BIN_LOG_AND;
            result.token = advance_token(state->scanner, state->strings);
            break;

        case TOKEN_ERROR: 
            advance_token(state->scanner, get_temporary_allocator());
            node.type = AST_ERROR;
            return node;

        default:
            return node;
    }

    add_left_node(state, &result, &node);
    node = parse_logic_and_expression(state);
    add_right_node(state, &result, &node);

    return result;
}

static ast_node_t parse_logic_or_expression(parser_state_t *state) {
    ast_node_t node = parse_logic_and_expression(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    result.token = peek_token(state->scanner, get_temporary_allocator());

    switch (result.token.type) {
        case TOKEN_LOGIC_OR:
            result.type = AST_BIN_LOG_OR;
            result.token = advance_token(state->scanner, state->strings);
            break;

        case TOKEN_ERROR: 
            advance_token(state->scanner, get_temporary_allocator());
            node.type = AST_ERROR;
            return node;

        default: 
            return node;
    }

    add_left_node(state, &result, &node);
    node = parse_logic_or_expression(state);
    add_right_node(state, &result, &node);

    return result;
}

static ast_node_t parse_cast_expression(parser_state_t *state) {
    token_t    cast_token = {};
    ast_node_t result     = {};

    if (consume_token(TOK_CAST, state->scanner, &cast_token, true, state->strings)) {
        consume_token('(', state->scanner, NULL, false, get_temporary_allocator());
        ast_node_t type = parse_type(state);
        consume_token(')', state->scanner, NULL, false, get_temporary_allocator());

        ast_node_t expr = parse_logic_or_expression(state);

        result.type  = AST_BIN_CAST;
        result.token = cast_token;

        if (expr.type == AST_EMPTY) {
            result.type = AST_ERROR;
            log_error_token(STRING("Error while parsing cast. No expression found."), result.token);
            return result;
        }

        add_left_node(state, &result, &type);
        add_right_node(state, &result, &expr);
        return result;
    }

    return parse_logic_or_expression(state);
}

static ast_node_t parse_assignment_expression(parser_state_t *state) {
    ast_node_t node = parse_cast_expression(state);
    if (node.type == AST_EMPTY) return node;

    ast_node_t result = {};
    result.token = peek_token(state->scanner, get_temporary_allocator());

    // here we will do += -= *= /=  and so on
    switch (result.token.type) {
        case '=':
            result.type = AST_BIN_ASSIGN;
            result.token = advance_token(state->scanner, state->strings);
            break;
        case TOKEN_ERROR:
            advance_token(state->scanner, get_temporary_allocator());
            node.type = AST_ERROR;
            return node;
        default:
            return node;
    }

    add_left_node(state, &result, &node);
    node = parse_assignment_expression(state);
    // @todo checks here
    add_right_node(state, &result, &node);

    return result;
}

static ast_node_t parse_separated_primary_expressions(parser_state_t *state) {
    ast_node_t node = parse_primary(state);
    if (node.type == AST_EMPTY) return node;

    token_t current = peek_token(state->scanner, state->strings);

    ast_node_t result = {};
    allocator_t *talloc = get_temporary_allocator();

    result.type  = AST_SEPARATION;
    result.token = current;
    add_list_node(state, &result, &node);

    if (current.type != ',') {
        return result;
    }

    advance_token(state->scanner, state->strings);

    while (current.type == ',' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_primary(state);

        if (node.type == AST_EMPTY) {
            log_error_token(STRING("Separated expression was closed on ','."), current);
            break;
        }

        // @todo check_value for errors
        add_list_node(state, &result, &node);

        current = peek_token(state->scanner, talloc);

        if (current.type != ',') {
            break;
        }

        advance_token(state->scanner, talloc);
    }

    return result;
}

// @todo, default, between these funcs
static ast_node_t parse_separated_expressions(parser_state_t *state) {
    ast_node_t node = parse_cast_expression(state);
    if (node.type == AST_EMPTY) return node;

    token_t current = peek_token(state->scanner, state->strings);

    ast_node_t result = {};
    allocator_t *talloc = get_temporary_allocator();

    result.type  = AST_SEPARATION;
    result.token = current;
    add_list_node(state, &result, &node);

    if (current.type != ',') {
        return result;
    }

    advance_token(state->scanner, state->strings);

    while (current.type == ',' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_cast_expression(state);

        if (node.type == AST_EMPTY) {
            log_error_token(STRING("Separated expression was closed on ','."), current);
            break;
        }

        // @todo check_value for errors
        add_list_node(state, &result, &node);

        current = peek_token(state->scanner, talloc);

        if (current.type != ',') {
            break;
        }

        advance_token(state->scanner, talloc);
    }

    return result;
}

static ast_node_t parse_swap_expression(parser_state_t *state, ast_node_t *expr) {
    ast_node_t node;

    if (expr == NULL) {
        ast_node_t node = parse_separated_primary_expressions(state);
        if (node.type == AST_EMPTY) return node;
    } else {
        node = *expr;
    }
    
    ast_node_t result = {};
    result.token = peek_token(state->scanner, get_temporary_allocator());

    switch (result.token.type) {
        case '=': 
            result.type = AST_BIN_SWAP;
            result.token = advance_token(state->scanner, state->strings);
            break;

        case TOKEN_ERROR:
            advance_token(state->scanner, get_temporary_allocator());
            node.type = AST_ERROR;
            return node;

        default: 
            return node;
    }

    add_left_node(state, &result, &node);
    node = parse_separated_expressions(state); 
    check_value(node.type != AST_EMPTY);
    add_right_node(state, &result, &node);

    return result;
}

static ast_node_t parse_primary_type(parser_state_t *state) {
    ast_node_t result = {};

    result.token = advance_token(state->scanner, state->strings);

    switch (result.token.type) {
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
        case TOK_BOOL8:
        case TOK_BOOL32:
            result.type = AST_STD_TYPE;
            break;

        case TOK_VOID:
            result.type = AST_VOID_TYPE;
            break;

        case TOKEN_IDENT:
            result.type = AST_UNKN_TYPE;
            break;

        default:
            result.type = AST_ERROR;
            log_error_token(STRING("Couldn't use this token as type."), result.token);
            break;
    }

    return result;
}


static ast_node_t parse_type(parser_state_t *state) {
    ast_node_t result = {};

    token_t token = peek_token(state->scanner, get_temporary_allocator());

    switch (token.type) {
        case '[': {
            result.type  = AST_ARR_TYPE;
            result.token = advance_token(state->scanner, state->strings);

            ast_node_t size = parse_separated_expressions(state); 
            check_value(size.type != AST_EMPTY);

            if (!consume_token(']', state->scanner, &token, false, get_temporary_allocator())) {
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

static ast_node_t parse_param_declaration(parser_state_t *state) {
    ast_node_t node = {};

    token_t name, next;

    if (!consume_token(TOKEN_IDENT, state->scanner, &name, false, state->strings)) {
        node.type = AST_ERROR;
        panic_skip_until_token(')', state);
        return node;
    }

    if (!consume_token(':', state->scanner, &next, false, state->strings)) {
        node.type = AST_ERROR;
        panic_skip_until_token(')', state);
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

static ast_node_t parse_parameter_list(parser_state_t *state) {
    ast_node_t result = {};
    allocator_t *talloc = get_temporary_allocator();

    result.type = AST_FUNC_PARAMS;

    token_t current = peek_token(state->scanner, state->strings);
    result.token    = current;

    while (current.type != ')' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_param_declaration(state);

        // @todo check_value for errors
        add_list_node(state, &result, &node);

        current = peek_token(state->scanner, talloc);

        if (current.type == ',') {
            advance_token(state->scanner, talloc);
            current = peek_token(state->scanner, talloc);
        } else if (current.type != ')') {
            log_error_token(STRING("wrong token in parameter list"), current);
            break;
        }
    }

    return result;
}

static ast_node_t parse_return_list(parser_state_t *state) {
    ast_node_t result = {};
    allocator_t *talloc = get_temporary_allocator();

    result.type = AST_FUNC_RETURNS;

    if (!consume_token(TOKEN_RET, state->scanner, &result.token, true, state->strings)) {
        result.type = AST_EMPTY;
        return result;
    }

    token_t current = peek_token(state->scanner, talloc);

    while (current.type != '=' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_type(state);

        if (node.type == AST_ERROR) {
            panic_skip_until_token('=', state);

            result.type = AST_ERROR;
            break;
        }

        add_list_node(state, &result, &node);

        current = peek_token(state->scanner, talloc);

        if (current.type == ',') {
            advance_token(state->scanner, talloc);
            current = peek_token(state->scanner, talloc);
        } else if (current.type != '=') {
            log_error_token(STRING("wrong token in return list"), current);
            break;
        }
    }

    return result;
}

static ast_node_t parse_function_type(parser_state_t *state) {
    token_t token = {};

    if (!consume_token('(', state->scanner, &token, false, state->strings)) {
        assert(false);
    }

    ast_node_t result = {};
    result.type = AST_FUNC_TYPE;
    result.token = token;

    ast_node_t parameters = parse_parameter_list(state);
    // @todo check_value for errors
    add_left_node(state, &result, &parameters);

    if (!consume_token(')', state->scanner, NULL, false, state->strings)) {
        result.type = AST_ERROR;
        return result;
    }

    ast_node_t returns = parse_return_list(state);

    if (returns.type == AST_ERROR) {
        result.type = AST_ERROR;
        log_warning_token(STRING("couldn't parse return list in function."), result.token);
    }

    add_right_node(state, &result, &returns);

    return result;
}

static ast_node_t parse_multiple_types(parser_state_t *state) {
    ast_node_t node = parse_type(state);

    token_t current = peek_token(state->scanner, state->strings);

    if (current.type != ',') {
        return node;
    }

    ast_node_t result = {};

    result.type  = AST_MUL_TYPES;
    result.token = current;
    add_list_node(state, &result, &node);


    while (current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        advance_token(state->scanner, get_temporary_allocator());

        ast_node_t node = parse_type(state);

        if (node.type == AST_ERROR) {
            panic_skip_until_token(',', state);
            result.type = AST_ERROR;
            break;
        }

        add_list_node(state, &result, &node);
        current = peek_token(state->scanner, get_temporary_allocator());
        temp_reset();

        if (current.type != ',') break;
    }

    return result;
}

static ast_node_t parse_declaration_type(parser_state_t *state) {
    ast_node_t node = {};

    token_t token = peek_token(state->scanner, get_temporary_allocator());

    switch (token.type) {
        case '(':
            return parse_function_type(state);

        case '=': 
            node.type  = AST_AUTO_TYPE;
            node.token = peek_token(state->scanner, get_temporary_allocator());
            return node;

        default:
            return parse_type(state);
    }
}

static ast_node_t parse_multiple_var_declaration(parser_state_t *state, ast_node_t *names) {
    ast_node_t node = {}, type = {};

    node.type  = AST_TERN_MULT_DEF;
    node.token = names->token;

    if (peek_token(state->scanner, get_temporary_allocator()).type == '=') {
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
        log_error_token(STRING("You can't define multiple funcitons in same statement."), type.token);
        panic_skip(state);
        return node;
    }

    add_left_node(state, &node, names);

    token_t token = peek_token(state->scanner, get_temporary_allocator());

    if (token.type == ';') {
        node.type = AST_BIN_MULT_DEF; 
        add_right_node(state, &node, &type);
        return node;
    } else if (token.type != '=') {
        node.type = AST_ERROR;
        log_error_token(STRING("expected assignment or semicolon after expression."), type.token);
        panic_skip(state);
        return node;
    }
    add_center_node(state, &node, &type);

    advance_token(state->scanner, get_temporary_allocator());
    ast_node_t data = parse_separated_expressions(state);

    if (data.type == AST_ERROR || data.type == AST_EMPTY) {
        node.type = AST_ERROR;
        panic_skip(state);
        return node;
    }

    add_right_node(state, &node, &data);
    return node;
}

static ast_node_t parse_external_symbol_import(parser_state_t *state) {
    ast_node_t result = {};
    if (!consume_token(TOK_EXTERNAL, state->scanner, &result.token, false, state->strings)) {
        result.type = AST_ERROR;
        panic_skip(state);
        return result;
    }

    result.type = AST_EXT_FUNC_INFO;

    ast_node_t node = parse_primary(state);

    if (node.type == AST_ERROR) {
        log_error_token(STRING("bad library import name."), result.token);
        result.type = AST_ERROR;
        return result;
    } else if (node.token.type != TOKEN_CONST_STRING) {
        log_error_token(STRING("Library import name should be a string."), node.token);
        result.type = AST_ERROR;
    }

    add_left_node(state, &result, &node);

    if (!consume_token(TOK_AS, state->scanner, &result.token, true, state->strings)) {
        return result;
    }

    result.type = AST_NAMED_EXT_FUNC_INFO;

    node = parse_primary(state);

    if (node.type == AST_ERROR) {
        log_error_token(STRING("bad import symbol name."), result.token);
        result.type = AST_ERROR;
        return result;
    } else if (node.token.type != TOKEN_CONST_STRING) {
        log_error_token(STRING("Import symbol name should be a string"), node.token);
        result.type = AST_ERROR;
    }

    // @todo, check if it is valid syntax

    add_right_node(state, &result, &node);

    return result;
}

static ast_node_t parse_func_or_var_declaration(parser_state_t *state, token_t *name) {
    ast_node_t node = {};

    node.type  = AST_BIN_UNKN_DEF;
    node.token = *name;

    ast_node_t type = parse_declaration_type(state);

    if (type.type == AST_ERROR) {
        node.type = AST_ERROR;
        panic_skip_until_token(';', state);
        return node;
    }

    add_left_node(state, &node, &type);

    token_t token = peek_token(state->scanner, get_temporary_allocator());

    if (token.type == ';') {
        node.type = AST_UNARY_VAR_DEF; 
        return node;
    }

    if (!consume_token('=', state->scanner, NULL, false, state->strings)) {
        node.type = AST_ERROR;
        panic_skip(state);
        return node;
    }
    
    ast_node_t data;

    token = peek_token(state->scanner, get_temporary_allocator());

    if (token.type == '{') {
        data = parse_block(state, AST_BLOCK_IMPERATIVE);
    } else if (type.type == AST_FUNC_TYPE && token.type == TOK_EXTERNAL) {
        data = parse_external_symbol_import(state);
    } else {
        data = parse_assignment_expression(state);
    }

    if (data.type == AST_ERROR || data.type == AST_EMPTY) {
        node.type = AST_ERROR;
        panic_skip(state);
        return node;
    }

    add_right_node(state, &node, &data);

    return node;
}

static ast_node_t parse_union_declaration(parser_state_t *state, token_t *name) {
    // @todo better checking_value 
    consume_token(TOK_UNION, state->scanner, NULL, false, get_temporary_allocator());
    consume_token('=', state->scanner, NULL, false, get_temporary_allocator());

    ast_node_t result = {};

    result.type  = AST_UNION_DEF;
    result.token = *name;

    ast_node_t left = parse_block(state, AST_BLOCK_IMPERATIVE);
    // @todo add checks_value
    add_left_node(state, &result, &left);

    return result;
}

static ast_node_t parse_struct_declaration(parser_state_t *state, token_t *name) {
    // @todo better checking_value 
    consume_token(TOK_STRUCT, state->scanner, NULL, false, get_temporary_allocator());
    consume_token('=', state->scanner, NULL, false, get_temporary_allocator());

    ast_node_t result = {};

    result.type  = AST_STRUCT_DEF;
    result.token = *name;

    ast_node_t left = parse_block(state, AST_BLOCK_IMPERATIVE);
    // @todo add checks_value
    add_left_node(state, &result, &left);

    return result;
}

static ast_node_t parse_enum_declaration(parser_state_t *state, token_t *name) {
    ast_node_t result = {};

    allocator_t *talloc = get_temporary_allocator();

    consume_token(TOK_ENUM, state->scanner, NULL, false, talloc);
    consume_token('(', state->scanner, NULL, false, talloc);
    ast_node_t type = parse_type(state);
    consume_token(')', state->scanner, NULL, false, talloc);
    
    if (type.type == AST_STD_TYPE) {
        switch (type.token.type) {
            case TOK_F32:
            case TOK_F64:
            case TOK_BOOL8:
            case TOK_BOOL32:
                log_error_token(STRING("cant use float and bool types in enum definition"), type.token);
                result.type = AST_ERROR;
                return result;

            default:
                break;
        }
    } else {
        log_error_token(STRING("cant use not integer types in enum definition"), type.token);
        result.type = AST_ERROR;
        return result;
    }

    result.type = AST_ENUM_DEF;
    result.token = *name;

    consume_token('=', state->scanner, NULL, false, talloc);

    ast_node_t left = parse_block(state, AST_BLOCK_ENUM);
    if (left.type == AST_ERROR) {
        result.type = AST_ERROR;
        return result;
    }
    add_right_node(state, &result, &type);
    add_left_node(state, &result, &left);

    return result;
}

static ast_node_t parse_statement(parser_state_t *state) {
    ast_node_t node = {};

    node.type = AST_ERROR;

    token_t name = peek_token(state->scanner, get_temporary_allocator());
    token_t next = peek_next_token(state->scanner, get_temporary_allocator());

    b32 ignore_semicolon = false;

    if (name.type == TOKEN_IDENT && next.type == ':') {
        consume_token(TOKEN_IDENT, state->scanner, &name, false, state->strings);
        consume_token(':', state->scanner, &next, false, get_temporary_allocator());

        next = peek_token(state->scanner, get_temporary_allocator());

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
        ast_node_t expr = parse_separated_primary_expressions(state);
        assert(expr.type != AST_EMPTY);

        next = peek_token(state->scanner, get_temporary_allocator());

        if (next.type == ':') {
            advance_token(state->scanner, state->strings);
            node = parse_multiple_var_declaration(state, &expr);
        } else {
            node = parse_swap_expression(state, &expr);
        }

    } else switch (name.type) {
        case  '{': {
            ignore_semicolon = true;
            node = parse_block(state, AST_BLOCK_IMPERATIVE);
        } break;

        case TOK_USE: {
            node.token = advance_token(state->scanner, state->strings);
            token_t token = peek_token(state->scanner, get_temporary_allocator());

            if (token.type == ':') {
                node.type = AST_UNNAMED_MODULE;
                advance_token(state->scanner, state->strings);
                consume_token(TOKEN_CONST_STRING, state->scanner, &node.token, false, state->strings);
            } else if (token.type == TOKEN_IDENT) {
                node.type  = AST_NAMED_MODULE;
                node.token = advance_token(state->scanner, state->strings);
                consume_token(':', state->scanner, NULL, false, state->strings);
                ast_node_t import = {};

                import.type = AST_PRIMARY;
                consume_token(TOKEN_CONST_STRING, state->scanner, &import.token, false, state->strings);
                add_left_node(state, &node, &import);
            } else {
                check_value(false);
                node.type = AST_ERROR;
            }
        } break;

        case TOK_IF: {
            ignore_semicolon = true;
            node.type  = AST_IF_STMT;
            node.token = advance_token(state->scanner, state->strings);

            ast_node_t expr = parse_assignment_expression(state);
            check_value(expr.type != AST_EMPTY);

            if (peek_token(state->scanner, get_temporary_allocator()).type != '{') {
                if (!consume_token(TOK_THEN, state->scanner, NULL, false, get_temporary_allocator())) {
                    node.type = AST_ERROR;
                    break;
                }
            }
            
            ast_node_t stmt = parse_statement(state);
            add_left_node(state, &node, &expr);

            if (!consume_token(TOK_ELSE, state->scanner, NULL, true, get_temporary_allocator())) {
                add_right_node(state, &node, &stmt);
                break;
            } else {
                add_center_node(state, &node, &stmt);
            }

            node.type = AST_IF_ELSE_STMT;
            stmt = parse_statement(state);

            if (node.type == AST_ERROR) {
                node.type = AST_ERROR;
                break;
            }

            add_right_node(state, &node, &stmt);
        } break;

        case TOK_ELSE: {
            log_error_token(STRING("Else statement without if statement before..."), name);
            node.type = AST_ERROR;
            break;
        } break;

        case TOK_FOR: 
        { 
            log_error_token(STRING("@todo for stmt"), name);
            node.type  = AST_ERROR;
            node.token = advance_token(state->scanner, state->strings);
            break;

            ignore_semicolon = true;
            node.type  = AST_WHILE_STMT;
            node.token = advance_token(state->scanner, state->strings);

            ast_node_t expr = parse_assignment_expression(state);
            check_value(expr.type != AST_EMPTY);

            ast_node_t stmt = parse_block(state, AST_BLOCK_IMPERATIVE);
            add_left_node(state, &node, &expr);
            add_right_node(state, &node, &stmt);
        } break;

        case TOK_WHILE: {
            ignore_semicolon = true;
            node.type  = AST_WHILE_STMT;
            node.token = advance_token(state->scanner, state->strings);

            ast_node_t expr = parse_assignment_expression(state);
            check_value(expr.type != AST_EMPTY);

            ast_node_t stmt = parse_block(state, AST_BLOCK_IMPERATIVE);
            add_left_node(state, &node, &expr);
            add_right_node(state, &node, &stmt);
        } break;

        case TOK_RETURN: {
            node.type  = AST_RET_STMT;
            node.token = advance_token(state->scanner, state->strings);

            ast_node_t expr = parse_separated_expressions(state);
            add_left_node(state, &node, &expr);
        } break;

        case TOK_BREAK: {
            node.type  = AST_BREAK_STMT;
            node.token = advance_token(state->scanner, state->strings);
        } break;

        case TOK_CONTINUE: {
            node.type  = AST_CONTINUE_STMT;
            node.token = advance_token(state->scanner, state->strings);
        } break;

        default: {
            node = parse_assignment_expression(state);
        } break;
    }

    if (ignore_semicolon) {
        return node;
    }

    token_t semicolon = {};

    if (!consume_token(';', state->scanner, &semicolon, false, get_temporary_allocator())) {
        node.type = AST_ERROR;
        panic_skip(state);
    }

    return node;
}

static ast_node_t parse_imperative_block(parser_state_t *state) {
    ast_node_t result = {};
    allocator_t *talloc = get_temporary_allocator();

    result.type = AST_BLOCK_IMPERATIVE;

    token_t current = peek_token(state->scanner, talloc);

    while (current.type != '}' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_statement(state);

        if (node.type == AST_EMPTY) {
            current = peek_token(state->scanner, talloc);
            continue;
        }

        add_list_node(state, &result, &node);
        current = peek_token(state->scanner, talloc);
    }

    return result;
}

/*
ast_node_t parse_const_decl(parser_state_t *state) {
    // IDENT :: EXPR;
}
*/

static ast_node_t parse_enum_decl(parser_state_t *state) {
    ast_node_t result = {};

    result.type = AST_ENUM_DECL;

    token_t name = {};

    allocator_t *talloc = get_temporary_allocator();

    if (!consume_token(TOKEN_IDENT, state->scanner, &name, true, state->strings)) {
        log_error_token(STRING("Declaration should start from identifier"), peek_token(state->scanner, talloc));

        result.type = AST_ERROR;
        return result;
    }

    result.token = name;

    if (!consume_token('=', state->scanner, NULL, true, get_temporary_allocator())) {
        log_error_token(STRING("Declaration should have expression"), peek_token(state->scanner, talloc));

        result.type = AST_ERROR;
        return result;
    }

    ast_node_t left = parse_cast_expression(state);
    add_left_node(state, &result, &left);

    return result;
}

static ast_node_t parse_enum_block(parser_state_t *state) {
    ast_node_t result = {};
    allocator_t *talloc = get_temporary_allocator();

    result.type = AST_BLOCK_ENUM;

    token_t current = peek_token(state->scanner, talloc);

    while (current.type != '}' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_enum_decl(state);

        if (node.type == AST_ERROR) {
            result.type = AST_ERROR;
            panic_skip(state);
            break;
        }

        if (node.type == AST_EMPTY) {
            current = peek_token(state->scanner, talloc);
            continue;
        }

        add_list_node(state, &result, &node);
        current = peek_token(state->scanner, talloc);

        if (current.type == ',') {
            advance_token(state->scanner, talloc);
            current = peek_token(state->scanner, talloc);
        }
    }

    return result;
}

static ast_node_t parse_block(parser_state_t *state, ast_types_t type) {
    ast_node_t result = {};
    token_t start, stop;

    if (!consume_token('{', state->scanner, &start, false, get_temporary_allocator())) {
        result.type = AST_ERROR;
        log_error_token(STRING("Expected opening of a block."), start);
        return result;
    }

    if (type == AST_BLOCK_IMPERATIVE) {
        result = parse_imperative_block(state);
    } else if (type == AST_BLOCK_ENUM) {
        result = parse_enum_block(state);
    } else {
        assert(false);
    }

    if (!consume_token('}', state->scanner, &stop, false, get_temporary_allocator())) {
        result.type = AST_ERROR;
        log_error_token(STRING("Expected closing of a block."), stop);
        return result;
    }

    result.token = start;
    result.token.c1 = stop.c1;
    result.token.l1 = stop.l1;

    return result;
}

b32 parse_file(compiler_t *compiler, string_t filename) {
    source_file_t *file = hashmap_get(&compiler->files, filename);

    if (file == NULL) {
        log_error(STRING("no such file in table."));
        return false;
    }

    parser_state_t state = {};
    state.scanner = file->scanner;
    state.nodes   = compiler->nodes;
    state.strings = compiler->strings;

    if (file->parsed_roots.data != NULL) {
        assert(false);
        return false;
    }

    b32 valid_parse = true;
    token_t curr = peek_token(state.scanner, get_temporary_allocator());

    while (curr.type != TOKEN_EOF && curr.type != TOKEN_ERROR) {
        temp_reset();

        ast_node_t node = parse_statement(&state);

        if (node.type == AST_EMPTY) {
            curr = peek_token(state.scanner, get_temporary_allocator());
            continue;
        }

        if (node.type == AST_ERROR) {
            valid_parse = false; 
        }

        ast_node_t *root = (ast_node_t*)mem_alloc(state.nodes, sizeof(ast_node_t));
        *root = node;

        list_add(&file->parsed_roots, &root);
        curr = peek_token(state.scanner, get_temporary_allocator());
    }

    return valid_parse;
}
