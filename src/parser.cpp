#include "parser.h"
#include "arena.h"
#include "talloc.h"
#include "strings.h"

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

static ast_node_t parse_separated_expressions(parser_state_t *state);

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


static b32 add_left_node(parser_state_t *state, ast_node_t *root, ast_node_t *node) {
    profiler_func_start();
    assert(root != NULL);
    assert(node != NULL);

    if (node->type == AST_ERROR) {
        log_error_token("Bad expression: ", root->token);
        profiler_func_end();
        return false;
    }

    root->left = (ast_node_t*)mem_alloc(state->nodes, sizeof(ast_node_t));
    assert(root->left != NULL);
    *root->left = *node;

    if (node->type == AST_ERROR) {
        root->type = AST_ERROR;
    }
    profiler_func_end();
    return true;
}

static b32 add_right_node(parser_state_t *state, ast_node_t *root, ast_node_t *node) {
    profiler_func_start();
    assert(root != NULL);
    assert(node != NULL);

    if (node->type == AST_ERROR) {
        log_error_token("Bad expression: ", root->token);
        profiler_func_end();
        return false;
    }

    root->right = (ast_node_t*)mem_alloc(state->nodes, sizeof(ast_node_t));
    assert(root->right != NULL);
    *root->right = *node;

    if (node->type == AST_ERROR) {
        root->type = AST_ERROR;
    }
    profiler_func_end();
    return true;
}

static b32 add_center_node(parser_state_t *state, ast_node_t *root, ast_node_t *node) {
    profiler_func_start();
    assert(root != NULL);
    assert(node != NULL);

    if (node->type == AST_ERROR) {
        log_error_token("Bad expression: ", root->token);
        profiler_func_end();
        return false;
    }

    root->center = (ast_node_t*)mem_alloc(state->nodes, sizeof(ast_node_t));
    assert(root->center != NULL);
    *root->center = *node;

    if (node->type == AST_ERROR) {
        root->type = AST_ERROR;
    }
    profiler_func_end();
    return true;
}

static b32 add_list_node(parser_state_t *state, ast_node_t *root, ast_node_t *node) {
    profiler_func_start();
    assert(root != NULL);
    assert(node != NULL);

    if (node->type == AST_ERROR) {
        log_error_token("Bad expression: ", root->token);
        profiler_func_end();
        return false;
    }

    if (root->list_start == NULL) {
        root->list_start = (ast_node_t*)mem_alloc(state->nodes, sizeof(ast_node_t));
        *root->list_start = *node;
        root->child_count++;
        profiler_func_end();
        return true;
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
    profiler_func_end();
    return true;
}

/* parsing */
struct bind_power_t {
    s16 left, right;
};

static bind_power_t get_prefix_bind_power(token_t token) {
    switch (token.type) {
        case TOK_CAST: return {0, 3}; // @cast is special expression

        case '@': 
        case '^':
        case '-': 
        case '~': 
        case '!': return {0, 23};

        default:
            log_error_token("Bad prefix operator: ", token);
            break;
    }
    return {-1, -1};
}

static bind_power_t get_infix_bind_power(token_t token) {
    switch (token.type) {
        case '=':   return {2, 1};

        case TOKEN_LOGIC_OR:
                    return {5, 6};

        case TOKEN_LOGIC_AND:
                    return {7, 8};
        
        case TOKEN_GR: 
        case TOKEN_LS:
        case TOKEN_GEQ:
        case TOKEN_LEQ:
        case TOKEN_EQ:
        case TOKEN_NEQ: 
                  return {9, 10};

        case '+':
        case '-': return {11, 12};

        case '*':
        case '/':
        case '%': return {13, 14};

        case TOKEN_LSHIFT: 
        case TOKEN_RSHIFT: 
                  return {15, 16};

        case '^': return {17, 18};
        case '|': return {19, 20};
        case '&': return {21, 22};

        // prefixes are here
        // postfixes are here
        case '.': return {28, 27};

        default:
            break;
    }
    return { -1, -1 };
}

static bind_power_t get_postfix_bind_power(token_t token) {
    switch (token.type) {
        case '[': return {25, 0};
        case '(': return {25, 0};

        default:
            break;
    }
    return {-1, -1};
}

static u32 get_ast_type_based_on_token(token_t token) {
    switch (token.type) {
        case TOKEN_CONST_FP:
        case TOKEN_CONST_INT:
        case TOKEN_CONST_STRING:
        case TOKEN_IDENT:
        case TOK_TRUE:
        case TOK_FALSE:
            return AST_PRIMARY;

        default:
            log_error_token("Unknown primary type: ", token);
            return AST_ERROR;
    }

    return AST_ERROR;
}

static u32 get_ast_type_based_on_prefix_token(token_t token) {
    switch (token.type) {
        case '@': return AST_UNARY_DEREF;  break;
        case '^': return AST_UNARY_REF;    break;
        case '-': return AST_UNARY_NEGATE; break;
        case '!': return AST_UNARY_NOT;    break;
        case '~': return AST_UNARY_INVERT; break;

        case TOK_CAST: return AST_BIN_CAST; break;

        default:
            log_error_token("Unknown prefix operator type: ", token);
            return AST_ERROR;
    }

    return AST_ERROR;
}

static u32 get_ast_type_based_on_infix_token(token_t token) {
    switch (token.type) {
        case '=': return AST_BIN_ASSIGN;

        case '+': return AST_BIN_ADD;
        case '-': return AST_BIN_SUB;

        case '*': return AST_BIN_MUL;
        case '/': return AST_BIN_DIV;
        case '%': return AST_BIN_MOD;

        case '.': return AST_MEMBER_ACCESS;

        case TOKEN_GR:  return AST_BIN_GR;
        case TOKEN_LS:  return AST_BIN_LS;
        case TOKEN_GEQ: return AST_BIN_GEQ;
        case TOKEN_LEQ: return AST_BIN_LEQ;
        case TOKEN_EQ:  return AST_BIN_EQ;
        case TOKEN_NEQ: return AST_BIN_NEQ;

        case TOKEN_LOGIC_AND: return AST_BIN_LOG_AND;
        case TOKEN_LOGIC_OR:  return AST_BIN_LOG_OR;

        case '&': return AST_BIN_BIT_AND;
        case '^': return AST_BIN_BIT_XOR;
        case '|': return AST_BIN_BIT_OR;
        case TOKEN_LSHIFT: return AST_BIN_BIT_LSHIFT;
        case TOKEN_RSHIFT: return AST_BIN_BIT_RSHIFT;

        default:
            log_error_token("Unknown infix operator type: ", token);
            return AST_ERROR;
    }

    return AST_ERROR;
}

static u32 get_ast_type_based_on_postfix_token(token_t token) {
    switch (token.type) {
        case '[': return AST_ARRAY_ACCESS; break;
        case '(': return AST_FUNC_CALL; break;

        default:
            log_error_token("Unknown postfix operator type: ", token);
            return AST_ERROR;
    }

    return AST_ERROR;
}

static ast_node_t parse_expression(parser_state_t *state, s16 min_bind_power = 0) {
    profiler_func_start();
    assert(state != NULL);
    allocator_t *talloc = get_temporary_allocator();
    ast_node_t result = {};

    token_t left = peek_token(state->scanner, talloc);
    result.token = left;

    switch (left.type) {
        case TOKEN_CONST_FP:
        case TOKEN_CONST_INT:
        case TOKEN_CONST_STRING:
        case TOKEN_IDENT:
        case TOK_TRUE:
        case TOK_FALSE:
            result.token = advance_token(state->scanner, state->strings);
            result.type  = get_ast_type_based_on_token(left);
            break; // Atom

        case '-':
        case '!':
        case '@':
        case '^':
        case '~':
            {
                result.token    = advance_token(state->scanner, state->strings);
                result.type     = get_ast_type_based_on_prefix_token(left);
                bind_power_t bp = get_prefix_bind_power(left);
                ast_node_t lhs  = parse_expression(state, bp.right);
                if (!add_left_node(state, &result, &lhs)) {
                    result.type = AST_ERROR;
                }
            } break; // prefix operations

        case '(':
            {
                result.token = advance_token(state->scanner, state->strings);
                result = parse_expression(state, 0);
                if (!consume_token(TOKEN_CLOSE_BRACE, state->scanner, NULL, false, talloc)) {
                    result.type = AST_ERROR;
                }
            } break;

        case TOK_CAST:
            {
                result.token = advance_token(state->scanner, state->strings);
                if (!consume_token('(', state->scanner, NULL, false, get_temporary_allocator())) {
                    break;
                }

                ast_node_t lhs = parse_type(state); // @todo, make pratt type parser?

                if (!consume_token(')', state->scanner, NULL, false, get_temporary_allocator())) {
                    break;
                }

                result.type = get_ast_type_based_on_prefix_token(left);
                bind_power_t bp = get_prefix_bind_power(left);
                ast_node_t rhs = parse_expression(state, bp.right);

                if (!add_left_node(state, &result, &lhs))  { result.type = AST_ERROR; } 
                if (!add_right_node(state, &result, &rhs)) { result.type = AST_ERROR; } 
            } break;

        default:
            result.type  = AST_EMPTY;
            profiler_func_end();
            return result;
    }

    while (true) {
        token_t op = peek_token(state->scanner, state->strings);

        if (op.type == TOKEN_EOF) 
            break;

        bind_power_t power; 

        power = get_postfix_bind_power(op);

        if (power.left >= 0 && power.right >= 0) {
            if (power.left < min_bind_power)
                break;

            advance_token(state->scanner, talloc);

            ast_node_t lhs = result;
            ast_node_t rhs = parse_separated_expressions(state);

            result.token = {};
            result.token = op;
            result.type  = get_ast_type_based_on_postfix_token(op);

            if (op.type == '[') {
                if (!consume_token(']', state->scanner, NULL, false, talloc)) {
                    result.type = AST_ERROR;
                }
            } else if (op.type == '(') {
                if (!consume_token(TOKEN_CLOSE_BRACE, state->scanner, NULL, false, talloc)) {
                    result.type = AST_ERROR;
                }
            } else {
                assert(false);
            }

            if (!add_left_node(state, &result, &lhs))  { result.type = AST_ERROR; } 
            if (!add_right_node(state, &result, &rhs)) { result.type = AST_ERROR; } 
            continue;
        }

        power = get_infix_bind_power(op);

        if (power.left >= 0 && power.right >= 0) {
            if (power.left < min_bind_power)
                break;

            advance_token(state->scanner, talloc);

            ast_node_t lhs = result;
            ast_node_t rhs = parse_expression(state, power.right);

            result = {};
            result.token = op;
            result.type  = get_ast_type_based_on_infix_token(op);

            if (!add_left_node(state, &result, &lhs))  { result.type = AST_ERROR; } 
            if (!add_right_node(state, &result, &rhs)) { result.type = AST_ERROR; } 
            continue;
        }

        break;
    }

    profiler_func_end();
    return result;
}

static ast_node_t parse_separated_primary_expressions(parser_state_t *state) {
    profiler_func_start();
    ast_node_t node = parse_expression(state, 100);
    if (node.type == AST_EMPTY) {
        profiler_func_end();
        return node;
    }

    token_t current = peek_token(state->scanner, state->strings);

    ast_node_t result = {};
    allocator_t *talloc = get_temporary_allocator();

    result.type  = AST_SEPARATION;
    result.token = current;
    add_list_node(state, &result, &node);

    if (current.type != ',') {
        profiler_func_end();
        return result;
    }

    advance_token(state->scanner, state->strings);

    while (current.type == ',' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_expression(state, 100);

        if (node.type == AST_EMPTY) {
            log_error_token("Separated expression was closed on ','.", current);
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

    profiler_func_end();
    return result;
}

// @todo, default, between these funcs
static ast_node_t parse_separated_expressions(parser_state_t *state) {
    profiler_func_start();
    token_t parsing_level = {};
    parsing_level.type = '=';

    ast_node_t node = parse_expression(state, get_infix_bind_power(parsing_level).left);
    if (node.type == AST_EMPTY) {
        profiler_func_end();
        return node;
    }

    token_t current = peek_token(state->scanner, state->strings);

    ast_node_t result = {};
    allocator_t *talloc = get_temporary_allocator();

    result.type  = AST_SEPARATION;
    result.token = current;
    add_list_node(state, &result, &node);

    if (current.type != ',') {
        profiler_func_end();
        return result;
    }

    advance_token(state->scanner, state->strings);

    while (current.type == ',' && current.type != TOKEN_EOF && current.type != TOKEN_ERROR) {
        ast_node_t node = parse_expression(state, get_infix_bind_power(parsing_level).left);

        if (node.type == AST_EMPTY) {
            log_error_token("Separated expression was closed on ','.", current);
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

    profiler_func_end();
    return result;
}

static ast_node_t parse_swap_expression(parser_state_t *state, ast_node_t *expr) {
    profiler_func_start();
    ast_node_t node = {};

    if (expr == NULL) {
        node = parse_separated_expressions(state);
        if (node.type == AST_EMPTY) {
            profiler_func_end();
            return node;
        }
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
            profiler_func_end();
            return node;

        default: 
            profiler_func_end();
            return node;
    }

    add_left_node(state, &result, &node);
    node = parse_separated_expressions(state); 
    check_value(node.type != AST_EMPTY);
    add_right_node(state, &result, &node);

    profiler_func_end();
    return result;
}
static ast_node_t parse_primary_type(parser_state_t *state) {
    profiler_func_start();
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
            log_error_token("Couldn't use this token as type.", result.token);
            break;
    }

    profiler_func_end();
    return result;
}


static ast_node_t parse_type(parser_state_t *state) {
    profiler_func_start();
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

    profiler_func_end();
    return result;
}

static ast_node_t parse_param_declaration(parser_state_t *state) {
    profiler_func_start();

    ast_node_t node = {};

    token_t name, next;

    if (!consume_token(TOKEN_IDENT, state->scanner, &name, false, state->strings)) {
        node.type = AST_ERROR;
        panic_skip_until_token(')', state);
        profiler_func_end();
        return node;
    }

    if (!consume_token(':', state->scanner, &next, false, state->strings)) {
        node.type = AST_ERROR;
        panic_skip_until_token(')', state);
        profiler_func_end();
        return node;
    }

    node.type  = AST_PARAM_DEF;
    node.token = name;

    ast_node_t type = parse_type(state);

    if (type.type == AST_ERROR) {
        node.type = AST_ERROR;
        panic_skip_until_token(')', state);
        profiler_func_end();
        return node;
    }

    add_left_node(state, &node, &type);
    profiler_func_end();
    return node;
}

static ast_node_t parse_parameter_list(parser_state_t *state) {
    profiler_func_start();
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
            log_error_token("wrong token in parameter list", current);
            break;
        }
    }

    profiler_func_end();
    return result;
}

static ast_node_t parse_return_list(parser_state_t *state) {
    profiler_func_start();
    ast_node_t result = {};
    allocator_t *talloc = get_temporary_allocator();

    result.type = AST_FUNC_RETURNS;

    if (!consume_token(TOKEN_RET, state->scanner, &result.token, true, state->strings)) {
        result.type = AST_EMPTY;
        profiler_func_end();
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
            log_error_token("wrong token in return list", current);
            break;
        }
    }

    profiler_func_end();
    return result;
}

static ast_node_t parse_function_type(parser_state_t *state) {
    profiler_func_start();
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
        profiler_func_end();
        return result;
    }

    ast_node_t returns = parse_return_list(state);

    if (returns.type == AST_ERROR) {
        result.type = AST_ERROR;
        log_warning_token("couldn't parse return list in function.", result.token);
    }

    add_right_node(state, &result, &returns);

    profiler_func_end();
    return result;
}

static ast_node_t parse_multiple_types(parser_state_t *state) {
    profiler_func_start();
    ast_node_t node = parse_type(state);

    token_t current = peek_token(state->scanner, state->strings);

    if (current.type != ',') {
        profiler_func_end();
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

    profiler_func_end();
    return result;
}

static ast_node_t parse_declaration_type(parser_state_t *state) {
    profiler_func_start();
    ast_node_t node = {};

    token_t token = peek_token(state->scanner, get_temporary_allocator());

    switch (token.type) {
        case '(':
            profiler_func_end();
            return parse_function_type(state);

        case '=': 
            node.type  = AST_AUTO_TYPE;
            node.token = peek_token(state->scanner, get_temporary_allocator());
            profiler_func_end();
            return node;

        default:
            profiler_func_end();
            return parse_type(state);
    }
}

static ast_node_t parse_multiple_var_declaration(parser_state_t *state, ast_node_t *names) {
    profiler_func_start();
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
        profiler_func_end();
        return node;
    }

    if (type.type == AST_FUNC_TYPE) {
        node.type = AST_ERROR;
        log_error_token("You can't define multiple funcitons in same statement.", type.token);
        panic_skip(state);
        profiler_func_end();
        return node;
    }

    add_left_node(state, &node, names);

    token_t token = peek_token(state->scanner, get_temporary_allocator());

    if (token.type == ';') {
        node.type = AST_BIN_MULT_DEF; 
        add_right_node(state, &node, &type);
        profiler_func_end();
        return node;
    } else if (token.type != '=') {
        node.type = AST_ERROR;
        log_error_token("expected assignment or semicolon after expression.", type.token);
        panic_skip(state);
        profiler_func_end();
        return node;
    }
    add_center_node(state, &node, &type);

    advance_token(state->scanner, get_temporary_allocator());
    ast_node_t data = parse_separated_expressions(state);

    if (data.type == AST_ERROR || data.type == AST_EMPTY) {
        node.type = AST_ERROR;
        panic_skip(state);
        profiler_func_end();
        return node;
    }

    add_right_node(state, &node, &data);
    profiler_func_end();
    return node;
}

static ast_node_t parse_external_symbol_import(parser_state_t *state) {
    profiler_func_start();
    ast_node_t result = {};
    if (!consume_token(TOK_EXTERNAL, state->scanner, &result.token, false, state->strings)) {
        result.type = AST_ERROR;
        panic_skip(state);
        profiler_func_end();
        return result;
    }

    result.type = AST_EXT_FUNC_INFO;

    ast_node_t node = parse_expression(state, 100);

    if (node.type == AST_ERROR) {
        log_error_token("bad library import name.", result.token);
        result.type = AST_ERROR;
        profiler_func_end();
        return result;
    } else if (node.token.type != TOKEN_CONST_STRING) {
        log_error_token("Library import name should be a string.", node.token);
        result.type = AST_ERROR;
    }

    add_left_node(state, &result, &node);

    if (!consume_token(TOK_AS, state->scanner, &result.token, true, state->strings)) {
        profiler_func_end();
        return result;
    }

    result.type = AST_NAMED_EXT_FUNC_INFO;

    node = parse_expression(state, 100);

    if (node.type == AST_ERROR) {
        log_error_token("bad import symbol name.", result.token);
        result.type = AST_ERROR;
        profiler_func_end();
        return result;
    } else if (node.token.type != TOKEN_CONST_STRING) {
        log_error_token("Import symbol name should be a string", node.token);
        result.type = AST_ERROR;
    }

    // @todo, check if it is valid syntax

    add_right_node(state, &result, &node);

    profiler_func_end();
    return result;
}

static ast_node_t parse_func_or_var_declaration(parser_state_t *state, token_t *name) {
    profiler_func_start();
    ast_node_t node = {};

    node.type  = AST_BIN_UNKN_DEF;
    node.token = *name;

    ast_node_t type = parse_declaration_type(state);

    if (type.type == AST_ERROR) {
        node.type = AST_ERROR;
        panic_skip_until_token(';', state);
        profiler_func_end();
        return node;
    }

    add_left_node(state, &node, &type);

    token_t token = peek_token(state->scanner, get_temporary_allocator());

    if (token.type == ';') {
        node.type = AST_UNARY_VAR_DEF; 
        profiler_func_end();
        return node;
    }

    if (!consume_token('=', state->scanner, NULL, false, state->strings)) {
        node.type = AST_ERROR;
        panic_skip(state);
        profiler_func_end();
        return node;
    }
    
    ast_node_t data;

    token = peek_token(state->scanner, get_temporary_allocator());

    if (token.type == '{') {
        data = parse_block(state, AST_BLOCK_IMPERATIVE);
    } else if (type.type == AST_FUNC_TYPE && token.type == TOK_EXTERNAL) {
        data = parse_external_symbol_import(state);
    } else {
        data = parse_expression(state, 0);
    }

    if (data.type == AST_ERROR || data.type == AST_EMPTY) {
        node.type = AST_ERROR;
        panic_skip(state);
        profiler_func_end();
        return node;
    }

    add_right_node(state, &node, &data);

    profiler_func_end();
    return node;
}

static ast_node_t parse_union_declaration(parser_state_t *state, token_t *name) {
    profiler_func_start();
    // @todo better checking_value 
    consume_token(TOK_UNION, state->scanner, NULL, false, get_temporary_allocator());
    consume_token('=', state->scanner, NULL, false, get_temporary_allocator());

    ast_node_t result = {};

    result.type  = AST_UNION_DEF;
    result.token = *name;

    ast_node_t left = parse_block(state, AST_BLOCK_IMPERATIVE);
    // @todo add checks_value
    add_left_node(state, &result, &left);

    profiler_func_end();
    return result;
}

static ast_node_t parse_struct_declaration(parser_state_t *state, token_t *name) {
    profiler_func_start();
    // @todo better checking_value 
    consume_token(TOK_STRUCT, state->scanner, NULL, false, get_temporary_allocator());
    consume_token('=', state->scanner, NULL, false, get_temporary_allocator());

    ast_node_t result = {};

    result.type  = AST_STRUCT_DEF;
    result.token = *name;

    ast_node_t left = parse_block(state, AST_BLOCK_IMPERATIVE);
    // @todo add checks_value
    add_left_node(state, &result, &left);

    profiler_func_end();
    return result;
}

static ast_node_t parse_enum_declaration(parser_state_t *state, token_t *name) {
    profiler_func_start();
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
                log_error_token("cant use float and bool types in enum definition", type.token);
                result.type = AST_ERROR;
                profiler_func_end();
                return result;

            default:
                break;
        }
    } else {
        log_error_token("cant use not integer types in enum definition", type.token);
        result.type = AST_ERROR;
        profiler_func_end();
        return result;
    }

    result.type = AST_ENUM_DEF;
    result.token = *name;

    consume_token('=', state->scanner, NULL, false, talloc);

    ast_node_t left = parse_block(state, AST_BLOCK_ENUM);
    if (left.type == AST_ERROR) {
        result.type = AST_ERROR;
        profiler_func_end();
        return result;
    }
    add_right_node(state, &result, &type);
    add_left_node(state, &result, &left);

    profiler_func_end();
    return result;
}

static ast_node_t parse_statement(parser_state_t *state) {
    profiler_func_start();
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

            ast_node_t expr = parse_expression(state, 0);
            check_value(expr.type != AST_EMPTY);

            if (peek_token(state->scanner, get_temporary_allocator()).type != '{') {
                node.type = AST_ERROR;
                break;
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
            log_error_token("Else statement without if statement before...", name);
            node.type = AST_ERROR;
            break;
        } break;

        case TOK_FOR: 
        { 
            log_error_token("@todo for stmt", name);
            node.type  = AST_ERROR;
            node.token = advance_token(state->scanner, state->strings);
            break;

            ignore_semicolon = true;
            node.type  = AST_WHILE_STMT;
            node.token = advance_token(state->scanner, state->strings);

            ast_node_t expr = parse_expression(state, 0);
            check_value(expr.type != AST_EMPTY);

            ast_node_t stmt = parse_block(state, AST_BLOCK_IMPERATIVE);
            add_left_node(state, &node, &expr);
            add_right_node(state, &node, &stmt);
        } break;

        case TOK_WHILE: {
            ignore_semicolon = true;
            node.type  = AST_WHILE_STMT;
            node.token = advance_token(state->scanner, state->strings);

            ast_node_t expr = parse_expression(state, 0);
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
            node = parse_swap_expression(state, NULL);
        } break;
    }

    if (ignore_semicolon) {
        profiler_func_end();
        return node;
    }

    token_t semicolon = {};

    if (!consume_token(';', state->scanner, &semicolon, false, get_temporary_allocator())) {
        node.type = AST_ERROR;
        panic_skip(state);
    }

    profiler_func_end();
    return node;
}

static ast_node_t parse_imperative_block(parser_state_t *state) {
    profiler_func_start();
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

    profiler_func_end();
    return result;
}

/*
ast_node_t parse_const_decl(parser_state_t *state) {
    // IDENT :: EXPR;
}
*/

static ast_node_t parse_enum_decl(parser_state_t *state) {
    profiler_func_start();
    ast_node_t result = {};

    result.type = AST_ENUM_DECL;

    token_t name = {};

    allocator_t *talloc = get_temporary_allocator();

    if (!consume_token(TOKEN_IDENT, state->scanner, &name, true, state->strings)) {
        log_error_token("Declaration should start from identifier", peek_token(state->scanner, talloc));

        result.type = AST_ERROR;
        profiler_func_end();
        return result;
    }

    result.token = name;

    if (!consume_token('=', state->scanner, NULL, true, get_temporary_allocator())) {
        log_error_token("Declaration should have expression", peek_token(state->scanner, talloc));

        result.type = AST_ERROR;
        profiler_func_end();
        return result;
    }

    ast_node_t left = parse_expression(state, 0);
    add_left_node(state, &result, &left);

    profiler_func_end();
    return result;
}

static ast_node_t parse_enum_block(parser_state_t *state) {
    profiler_func_start();
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

    profiler_func_end();
    return result;
}

static ast_node_t parse_block(parser_state_t *state, ast_types_t type) {
    ast_node_t result = {};
    token_t start, stop;

    if (!consume_token('{', state->scanner, &start, false, get_temporary_allocator())) {
        result.type = AST_ERROR;
        log_error_token("Expected opening of a block.", start);
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
        log_error_token("Expected closing of a block.", stop);
        return result;
    }

    result.token = start;
    result.token.c1 = stop.c1;
    result.token.l1 = stop.l1;

    return result;
}

string_t get_expression_string(ast_node_t *node) {
    allocator_t *talloc = get_temporary_allocator();

    string_t t = {};

    t = string_copy(STRING("("), talloc);

    u64 size = 0;

    if (node->token.l0 != node->token.l1) {
        size = 1;
    } else {
        size = node->token.c1 - node->token.c0;
    }

    line_tuple_t line = *list_get(&node->token.from->lines, node->token.l0);
    u8 *p = node->token.from->file.data + line.start + node->token.c0;
    t = string_concat(t, { size, p }, talloc);

    if (node->left) {
        t = string_concat(t, STRING(" "), talloc);
        t = string_concat(t, get_expression_string(node->left), talloc);
    }

    if (node->center) {
        t = string_concat(t, STRING(" "), talloc);
        t = string_concat(t, get_expression_string(node->center), talloc);
    }

    if (node->right) {
        t = string_concat(t, STRING(" "), talloc);
        t = string_concat(t, get_expression_string(node->right), talloc);
    }

    if (node->list_start) {
        t = string_concat(t, STRING(" ["), talloc);

        ast_node_t *next = node->list_start;
        for (u64 i = 0; i < node->child_count; i++) {
            t = string_concat(t, STRING(" "), talloc);
            t = string_concat(t, get_expression_string(next), talloc);

            next = next->list_next;
        }

        t = string_concat(t, STRING("]"), talloc);
    }

    t = string_concat(t, STRING(")"), talloc);
    return t;
}

b32 parse_file(compiler_t *compiler, string_t filename) {
    profiler_func_start();
    source_file_t *file = hashmap_get(&compiler->files, filename);

    if (file == NULL) {
        log_error("no such file in table.");
        profiler_func_end();
        return false;
    }

    parser_state_t state = {};
    state.scanner = file->scanner;
    state.nodes   = compiler->nodes;
    state.strings = compiler->strings;

    if (file->parsed_roots.data != NULL) {
        profiler_func_end();
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

    profiler_func_end();
    return valid_parse;
}
