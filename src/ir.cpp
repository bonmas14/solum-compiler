#include "ir.h"

#include "analyzer.h"
#include "parser.h"

#include "allocator.h"
#include "talloc.h"
#include "arena.h"

#include "profiler.h"

#include "strings.h"
#include "memctl.h"

// just for now this will be here
// #define VERBOSE

#define EXPR_CASE(cs, tok) case cs: {\
            ir_expression_t rhs = compile_expression(state, node->right, shadow);\
            ir_expression_t lhs = compile_expression(state, node->left,  shadow);\
            UNUSED(rhs);\
            expr = lhs;\
            emit_op(state, tok, node->token, 0);\
            } break;

#define EXPR_UN_CASE(cs, tok) case cs: {\
            ir_expression_t lhs = compile_expression(state, node->left,  shadow);\
            expr = lhs;\
            emit_op(state, tok, node->token, 0);\
            } break;


struct ir_state_t {
    compiler_t *compiler;
    ir_t ir;
    ir_function_t *current_function;
    stack_t<ir_opcode_t*> continue_stmt;
    stack_t<ir_opcode_t*> break_stmt;
    stack_t<ast_node_t*>  reverse;
    stack_t<hashmap_t<string_t, scope_entry_t>*> search_scopes;
    list_t<hashmap_t<string_t, scope_entry_t>>   local_scopes;
};

// ------ //

const char* ir_code_to_string(u64 code) {
    switch (code) {
        case IR_NOP:         return "NOP";
        case IR_SETUP_GLOBAL: return "SETUP_GLOBAL";
        case IR_PUSH_SIGN:   return "PUSH_SIGN";
        case IR_PUSH_UNSIGN: return "PUSH_UNSIGN";
        case IR_PUSH_STACK:  return "PUSH_STACK";
        case IR_PUSH_GLOBAL: return "PUSH_GLOBAL";
        case IR_PUSH_GEA:     return "PUSH_GEA";
        case IR_PUSH_SEA:     return "PUSH_SEA";
        case IR_POP:         return "POP";
        case IR_CLONE:       return "CLONE";
        case IR_STACK_FRAME_PUSH: return "SF_PUSH";
        case IR_STACK_FRAME_POP:  return "SF_POP";
        case IR_ALLOC:       return "ALLOC";
        case IR_FREE:        return "FREE";
        case IR_LOAD:        return "LOAD";
        case IR_STORE:       return "STORE";
        case IR_ADD:         return "ADD";
        case IR_SUB:         return "SUB";
        case IR_MUL:         return "MUL";
        case IR_DIV:         return "DIV";
        case IR_MOD:         return "MOD";
        case IR_NEG:         return "NEG";
        case IR_BIT_AND:     return "BIT_AND";
        case IR_BIT_OR:      return "BIT_OR";
        case IR_BIT_XOR:     return "BIT_XOR";
        case IR_BIT_NOT:     return "BIT_NOT";
        case IR_SHIFT_LEFT:  return "SHIFT_LEFT";
        case IR_SHIFT_RIGHT: return "SHIFT_RIGHT";
        case IR_CMP_EQ:      return "CMP_EQ";
        case IR_CMP_NEQ:     return "CMP_NEQ";
        case IR_CMP_LT:      return "CMP_LT";
        case IR_CMP_GT:      return "CMP_GT";
        case IR_CMP_LTE:     return "CMP_LTE";
        case IR_CMP_GTE:     return "CMP_GTE";
        case IR_LOG_NOT:     return "LOG_NOT";
        case IR_JUMP:        return "JUMP";
        case IR_JUMP_IF:     return "JUMP_IF";
        case IR_JUMP_IF_NOT: return "JUMP_IF_NOT";
        case IR_RET:         return "RET";
        case IR_CALL:        return "CALL";
        case IR_BRK:         return "BRK";
        case IR_INVALID:     return "INVALID";
        default:             return "UNKNOWN_OP";
    }
}

void print_ir_opcode(ir_opcode_t op) {
    const char* op_name = ir_code_to_string(op.operation);
    
    log_update_color();
    printf("[IR] | %-14s | %lld\n", op_name, (long long) op.s_operand);
}

string_t get_ir_opcode_info(ir_opcode_t op) {
    const char* op_name = ir_code_to_string(op.operation);
    
    return string_format(get_temporary_allocator(), STRING(" --- %s | %d"), STRING(op_name), op.s_operand);
}

#ifdef NDEBUG
#define print_ir_opcode(...)
#endif

ir_opcode_t *emit_op(ir_state_t *state, u64 op, token_t debug, u64 data) {
    ir_opcode_t o = {};
    o.operation = op;
    o.info = debug;
    o.u_operand = data;

    array_add(&state->current_function->code, o);
    ir_opcode_t *out = array_get(&state->current_function->code, state->current_function->code.count - 1);
    out->index = state->current_function->code.count - 1;
    return out;
}

ir_opcode_t *emit_op(ir_state_t *state, u64 op, token_t debug, s64 data, u64 dummy) {
    UNUSED(dummy);
    ir_opcode_t o = {};
    o.operation = op;
    o.info = debug;
    o.s_operand = data;

    array_add(&state->current_function->code, o);
    ir_opcode_t *out = array_get(&state->current_function->code, state->current_function->code.count - 1);
    out->index = state->current_function->code.count - 1;
    return out;
}

ir_opcode_t *emit_op(ir_state_t *state, u64 op, token_t debug, string_t data) {
    ir_opcode_t o = {};
    o.operation = op;
    o.info = debug;
    o.string = data;

    array_add(&state->current_function->code, o);
    ir_opcode_t *out = array_get(&state->current_function->code, state->current_function->code.count - 1);
    out->index = state->current_function->code.count - 1;
    return out;
}

// ------ // 

u64 compile_statement(ir_state_t *state, ast_node_t *node, u64 alloc_count);

scope_entry_t *search_identifier(ir_state_t *state, string_t key, string_t shadow) {
    b32 shadowed = false;
    for (s64 i = (state->search_scopes.index - 1); i >= 0; i--) {
        hashmap_t<string_t, scope_entry_t> *search_scope = state->search_scopes.data[i];
        
        if (!hashmap_contains(search_scope, key))
            continue;

        if (!shadowed && string_compare(shadow, key) == 0) {
            shadowed = true;
            continue;
        }

        return hashmap_get(search_scope, key);
    }

    assert(false);
    return NULL;
}

b32 add_identifier_type_to_search(ir_state_t *state, string_t key) {
    scope_entry_t *entry = search_identifier(state, key, {});
    if (entry->info.type != TYPE_UNKN) {
        return false;
    }

    string_t type_name   = entry->info.type_name;
    scope_entry_t *type  = search_identifier(state, type_name, {});

    switch (type->type) {
        case ENTRY_VAR:
        case ENTRY_FUNC:
            assert(false); 
            break;

        case ENTRY_TYPE:
            stack_push(&state->search_scopes, &type->scope);
            break;

        default:
            assert(false);
            break;
    }

    return true;
}

struct ir_expression_t {
    b32            accessable;
    type_info_t    type;
    s64            offset;
    ir_opcode_t   *emmited_op;
    stack_t<hashmap_t<string_t, scope_entry_t>*> search_info;
};

ir_expression_t compile_expression(ir_state_t *state, ast_node_t *node, string_t shadow) {
    assert(state->current_function != NULL);
    UNUSED(state);
    UNUSED(node);

    ir_expression_t expr = {};

    switch (node->type) {
        EXPR_UN_CASE(AST_UNARY_INVERT, IR_BIT_NOT);
        EXPR_UN_CASE(AST_UNARY_NEGATE, IR_NEG);
        EXPR_UN_CASE(AST_UNARY_NOT,    IR_LOG_NOT);

        EXPR_CASE(AST_BIN_GR,  IR_CMP_GT);
        EXPR_CASE(AST_BIN_LS,  IR_CMP_LT);
        EXPR_CASE(AST_BIN_GEQ, IR_CMP_GTE);
        EXPR_CASE(AST_BIN_LEQ, IR_CMP_LTE);
        EXPR_CASE(AST_BIN_EQ,  IR_CMP_EQ);
        EXPR_CASE(AST_BIN_NEQ, IR_CMP_NEQ);
        EXPR_CASE(AST_BIN_ADD, IR_ADD);
        EXPR_CASE(AST_BIN_SUB, IR_SUB);
        EXPR_CASE(AST_BIN_MUL, IR_MUL);
        EXPR_CASE(AST_BIN_DIV, IR_DIV);
        EXPR_CASE(AST_BIN_MOD, IR_MOD);
        EXPR_CASE(AST_BIN_BIT_XOR, IR_BIT_XOR);
        EXPR_CASE(AST_BIN_BIT_OR,  IR_BIT_OR);
        EXPR_CASE(AST_BIN_BIT_AND, IR_BIT_AND);
        EXPR_CASE(AST_BIN_BIT_LSHIFT, IR_SHIFT_LEFT);
        EXPR_CASE(AST_BIN_BIT_RSHIFT, IR_SHIFT_RIGHT);

        case AST_PRIMARY: switch (node->token.type)
            {
                case TOKEN_CONST_FP:
                    log_error("AST_PRIMARY:TOKEN_IDENT compilation TODO");
                    emit_op(state, IR_INVALID, node->token, 0);
                    break;
                case TOKEN_CONST_INT:
                    emit_op(state, IR_PUSH_SIGN, node->token, (s64)node->token.data.const_int);
                    set_std_info(TOK_U64, &expr.type);
                    break;
                case TOKEN_CONST_STRING:
                    log_error("AST_PRIMARY:TOKEN_STRING compilation TODO");
                    emit_op(state, IR_INVALID, node->token, 0);
                    break;
                case TOKEN_IDENT: 
                    {
                        scope_entry_t *entry = search_identifier(state, node->token.data.string, shadow);

                        if (entry->type == ENTRY_TYPE) {
                            log_error_token("Cant use types in expression", node->token);
                            emit_op(state, IR_INVALID, node->token, 0);
                            state->ir.is_valid = false;
                            break;
                        }

                        expr.type       = entry->info;
                        expr.accessable = true;
                        expr.offset     = entry->offset;

                        if (entry->on_stack) {
                            expr.emmited_op = emit_op(state, IR_PUSH_STACK,  node->token, expr.offset);
                        } else {
                            expr.emmited_op = emit_op(state, IR_PUSH_GLOBAL, node->token, expr.offset);
                        }

                        expr.emmited_op->string = node->token.data.string;
                    } break;
                case TOK_TRUE:
                    emit_op(state, IR_PUSH_SIGN, node->token, 1);
                    set_std_info(TOK_BOOL32, &expr.type);
                    break;
                case TOK_FALSE:
                    emit_op(state, IR_PUSH_SIGN, node->token, 0);
                    set_std_info(TOK_BOOL32, &expr.type);
                    break;
                default:
                    emit_op(state, IR_INVALID, node->token, 0xFFFFFFFFFFFFFFFF);
                    break;
            } break;
            break;

        case AST_UNARY_REF:
            expr = compile_expression(state, node->left, shadow);

            if (!expr.accessable) {
                log_error_token("Cant get address of unknown variable", node->left->token);
                state->ir.is_valid = false;
            } else {
                if (expr.emmited_op->operation == IR_PUSH_GLOBAL) {
                    expr.emmited_op->operation = IR_PUSH_GEA;
                } else if (expr.emmited_op->operation == IR_PUSH_STACK) {
                    expr.emmited_op->operation = IR_PUSH_SEA;
                } else {
                    expr.emmited_op->operation = IR_INVALID;
                }
            }

            expr.type.pointer_depth++;
            break;

        case AST_UNARY_DEREF: {
            ir_expression_t value = compile_expression(state, node->left, shadow);

            if (value.type.pointer_depth == 0) {
                log_warning_token("Trying to dereference non-pointer variable.", node->left->token);
            }

            expr.emmited_op = emit_op(state, IR_LOAD, node->token, 0);
            expr.accessable = true;
        } break;


        case AST_BIN_LOG_OR: 
        {
            expr = compile_expression(state, node->left,  shadow);

            emit_op(state, IR_CLONE, node->token, 0);
            ir_opcode_t *end = emit_op(state, IR_JUMP_IF, node->token, 0);
            emit_op(state, IR_POP, node->token, 0);

            ir_expression_t rhs = compile_expression(state, node->right, shadow);
            UNUSED(rhs);
            end->s_operand = state->current_function->code.count - 1 - end->index;
        } break;
        case AST_BIN_LOG_AND:
        {
            expr = compile_expression(state, node->left,  shadow);

            emit_op(state, IR_CLONE, node->token, 0);
            ir_opcode_t *end = emit_op(state, IR_JUMP_IF_NOT, node->token, 0);
            emit_op(state, IR_POP, node->token, 0);

            ir_expression_t rhs = compile_expression(state, node->right, shadow);
            UNUSED(rhs);
            end->s_operand = state->current_function->code.count - 1 - end->index;
        } break;

        case AST_BIN_CAST:
            // @todo
            //
            // log_error("AST_BIN_CAST compilation TODO");
            expr = compile_expression(state, node->right, shadow);
            // compile_expression(state, node->left);
            // emit_op(state, IR_INVALID, node->token, 0);
            break;

        case AST_FUNC_CALL:
            compile_expression(state, node->right, shadow);
            expr = compile_expression(state, node->left, shadow);
            expr.emmited_op->operation = IR_CALL;
            break;

        case AST_MEMBER_ACCESS:
            log_error("Structs TODO:");
            break;

            /*
            assert(node->left->type       == AST_PRIMARY);
            assert(node->left->token.type == TOKEN_IDENT);

            if (!add_identifier_type_to_search(state, node->left->token.data.string)) {
                log_error_token("Identifier is a primitive type: ", node->left->token);
                state->ir.is_valid = false;
                break;
            }

            expr = compile_expression(state, node->right, shadow);
            stack_pop(&state->search_scopes);

            if (!expr.accessable) {
                log_error_token("Bad member access expression: ", node->left->token);
                state->ir.is_valid = false;
                break;
            }

            if ((expr.emmited_op->operation != IR_PUSH_GLOBAL) && (expr.emmited_op->operation != IR_PUSH_STACK)) {
                log_error_token("Bad access target: ", node->left->token);
                state->ir.is_valid = false;
                break;
            }

            */

            // TODO: here we set code for loading real address,
            // because before we added offset of variable in type!
            // not the real offset
            //
            // so get (struct addr) + (offset, size)

            return expr;

        case AST_ARRAY_ACCESS: // @todo finish
            // log_error("Cant use arrays");
            break;

            /*
            compile_expression(state, node->right, shadow);
            expr = compile_expression(state, node->left, shadow);

            if (!expr.accessable) {
                log_error_token("Bad array access expression: ", node->left->token);
                state->ir.is_valid = false;
            } else {
                expr.emmited_op->operation = IR_STORE;
            }
            */
            return expr;

        case AST_BIN_ASSIGN:
            compile_expression(state, node->right, shadow);
            expr = compile_expression(state, node->left, shadow);

            if (!expr.accessable) {
                log_error_token("Bad assignment expression: ", node->left->token);
                state->ir.is_valid = false;
            } else {
                if (expr.emmited_op->operation == IR_PUSH_GLOBAL) {
                    expr.emmited_op->operation = IR_PUSH_GEA;
                } else if (expr.emmited_op->operation == IR_PUSH_STACK) {
                    expr.emmited_op->operation = IR_PUSH_SEA;
                } else if (expr.emmited_op->operation == IR_LOAD) {
                    expr.emmited_op->operation = IR_STORE;
                    expr.emmited_op->u_operand = 0;
                    break;
                } else {
                    expr.emmited_op->operation = IR_INVALID;
                }

                emit_op(state, IR_STORE, node->left->token, 0);
            }
            break;

        case AST_BIN_SWAP:
            {
                compile_expression(state, node->right, shadow);
                ast_node_t *next = node->left->list_start;

                for (u64 i = 0; i < node->left->child_count; i++) {
                    ir_expression_t expr = compile_expression(state, next, shadow);

                    if (!expr.accessable) {
                        log_error_token("Bad swap part: ", next->token);
                        state->ir.is_valid = false;
                    } else {
                        if (expr.emmited_op->operation == IR_PUSH_GLOBAL) {
                            expr.emmited_op->operation = IR_PUSH_GEA;
                        } else if (expr.emmited_op->operation == IR_PUSH_STACK) {
                            expr.emmited_op->operation = IR_PUSH_SEA;
                        } else if (expr.emmited_op->operation == IR_LOAD) {
                            expr.emmited_op->operation = IR_STORE;
                            break;
                        } else {
                            expr.emmited_op->operation = IR_INVALID;
                        }

                        emit_op(state, IR_STORE, next->token, 0);
                    }

                    next = next->list_next;
                }
            }
            break;

        case AST_SEPARATION:
            {
                ast_node_t *next = node->list_start;

                state->reverse.index = 0;
                for (u64 i = 0; i < node->child_count; i++) {
                    stack_push(&state->reverse, next);
                    next = next->list_next;
                }

                for (u64 i = 0; i < node->child_count; i++) {
                    compile_expression(state, stack_pop(&state->reverse), shadow);
                }
            }
            break;

        case AST_EMPTY:
            break;

        default:
            log_error_token("UNKN!!!", node->token);
            break;
    }

    if (expr.search_info.data != NULL) {
        stack_delete(&expr.search_info);
    }

    return expr;
}

void compile_block(ir_state_t *state, ast_node_t *node) {
    hashmap_t<string_t, scope_entry_t> *block = array_get(&state->compiler->scopes, node->scope_index);
    stack_push(&state->search_scopes, block);
    u64 si = state->current_function->stack_index;

    ast_node_t *stmt = node->list_start;

    u64 alloc_count = 0;
    for (u64 i = 0; i < node->child_count; i++) {
        alloc_count += compile_statement(state, stmt, alloc_count);
        
        if (stmt->list_next) {
            stmt = stmt->list_next;
        }
    }

    if (alloc_count) {
        emit_op(state, IR_FREE, stmt->token, alloc_count);
    }

    state->current_function->stack_index = si;
    stack_pop(&state->search_scopes);
}

u64 compile_variable(ir_state_t *state, ast_node_t *node, b32 is_global = false) {
    assert(state->current_function != NULL);

    scope_entry_t *entry = search_identifier(state, node->token.data.string, {});

    if (entry->expr) {
        ir_expression_t expr = compile_expression(state, entry->expr, node->token.data.string);
        UNUSED(expr);
    } else {
        emit_op(state, IR_PUSH_UNSIGN, node->token, 0);
    }

    u64 size = 1;

    if (entry->info.is_array) { 
        size *= 100 * 100;
        entry->info.pointer_depth += 1;
    }

    if (is_global) {
        entry->offset   = state->current_function->stack_index;
        state->current_function->stack_index += size;
        entry->on_stack = false;
        emit_op(state, IR_SETUP_GLOBAL, node->token, 0);
    } else {
        state->current_function->stack_index += size;
        entry->offset   = state->current_function->stack_index;
        entry->on_stack = true;
        emit_op(state, IR_ALLOC, node->token, size);
        emit_op(state, IR_STORE, node->token, 0);
    }

    return size;
}

u64 compile_mul_variables(ir_state_t *state, ast_node_t *node, b32 is_global = false) {
    assert(state->current_function != NULL);

    u64 alloc_count = 0;
    ast_node_t *next = node->left->list_start;
    
    for (u64 i = 0; i < node->left->child_count; i++) {
        alloc_count += compile_variable(state, next, is_global);
        next = next->list_next;
    }

    return alloc_count;
}

u64 compile_statement(ir_state_t *state, ast_node_t *node, u64 alloc_count = 0) {
    switch (node->type) {
        case AST_BIN_UNKN_DEF:  return compile_variable(state, node); break;
        case AST_UNARY_VAR_DEF: return compile_variable(state, node); break;
        case AST_BIN_MULT_DEF:  return compile_mul_variables(state, node); break;
        case AST_TERN_MULT_DEF: return compile_mul_variables(state, node); break;

        case AST_BLOCK_IMPERATIVE: compile_block(state, node); break;

        case AST_IF_STMT: 
            {
                compile_expression(state, node->left, {});
                ir_opcode_t *end = emit_op(state, IR_JUMP_IF_NOT, node->token, 0);
                compile_block(state, node->right);
                end->s_operand = state->current_function->code.count - 1 - end->index; // @todo, this could be a problem...
            }
            break;
        case AST_IF_ELSE_STMT:
            {
                ir_opcode_t *branch, *end;
                compile_expression(state, node->left, {});
                branch = emit_op(state, IR_JUMP_IF_NOT, node->token, 0);

                compile_block(state, node->center);

                end = emit_op(state, IR_JUMP, node->token, 0);
                branch->s_operand = end->index - branch->index;

                compile_statement(state, node->right);
                end->s_operand = state->current_function->code.count - 1 - end->index; // @todo, this could be a problem...
            }
            break;
        case AST_WHILE_STMT: 
            {
                ir_opcode_t *end;
                
                u64 pos = state->current_function->code.count;

                u64 start_continue_index = state->continue_stmt.index;
                u64 start_break_index    = state->break_stmt.index;

                compile_expression(state, node->left, {});
                end = emit_op(state, IR_JUMP_IF_NOT, node->token, 0);
                compile_block(state, node->right);
                emit_op(state, IR_JUMP, node->token, -((state->current_function->code.count + 1) - pos), 0);
                end->s_operand = state->current_function->code.count - end->index - 1; // @todo, this could be a problem...
            
                while (start_continue_index < state->continue_stmt.index) {
                    ir_opcode_t *op = stack_pop(&state->continue_stmt);
                    op->s_operand = state->current_function->code.count - 1 - op->index;
                }

                while (start_break_index < state->break_stmt.index) {
                    ir_opcode_t *op = stack_pop(&state->break_stmt);
                    op->s_operand = state->current_function->code.count - 1 - op->index;
                }
            }
            break;
        case AST_RET_STMT: 
            {
                compile_expression(state, node->left, {});
                if (alloc_count) emit_op(state, IR_FREE, node->token, alloc_count);
                emit_op(state, IR_STACK_FRAME_POP, node->token, 0);
                emit_op(state, IR_RET, node->token, 0);
            }
            break;
        case AST_BREAK_STMT: 
            {
                if (alloc_count) emit_op(state, IR_FREE, node->token, alloc_count);
                stack_push(&state->break_stmt, emit_op(state, IR_JUMP, node->token, 0)); // jump outa loop
            }
            break;
        case AST_CONTINUE_STMT:
            {
                if (alloc_count) emit_op(state, IR_FREE, node->token, alloc_count);
                stack_push(&state->continue_stmt, emit_op(state, IR_JUMP, node->token, 0)); // jump to start of loop
            }
            break;

        default:
            compile_expression(state, node, {});
            break;
    }

    return 0;
}

void compile_function(ir_state_t *state, string_t key, scope_entry_t *entry) {
    profiler_func_start();

    assert(state->current_function == NULL);

    {
        ir_function_t func = {};
        hashmap_add(&state->ir.functions, key, &func);
    }

    state->current_function = hashmap_get(&state->ir.functions, key);

    array_create(&state->current_function->code, 8, state->ir.code);
    if (entry->is_external) {
        state->current_function->is_external = true;
        profiler_func_end();
        state->current_function = NULL;
        return;
    }  

    stack_push(&state->search_scopes, &entry->func_params);
    {
        emit_op(state, IR_STACK_FRAME_PUSH, entry->node->token, 0);
        //                      def -> type -> params
        ast_node_t *node = entry->node->left->left;
        ast_node_t *next = node->list_start;

        for (u64 i = 0; i < node->child_count; i++) {
            scope_entry_t *entry = search_identifier(state, next->token.data.string, {});

            u64 size = 1;

            if (entry->info.is_array) { 
                size *= 100 * 100;
                entry->info.pointer_depth += 1;
            }

            state->current_function->stack_index += size;
            entry->offset   = state->current_function->stack_index;
            entry->on_stack = true;

            emit_op(state, IR_ALLOC, node->token, size); 
            emit_op(state, IR_STORE, next->token, 0);
            next = next->list_next;
        }

        compile_block(state, entry->expr);

        if (entry->return_typenames.count == 0) {
            emit_op(state, IR_STACK_FRAME_POP, node->token, 0);
            emit_op(state, IR_RET, entry->node->token, 0);
        } else {
            emit_op(state, IR_INVALID, entry->node->token, 0);
        }

#ifdef VERBOSE
        u64 last_line = 0;

        for (u64 i = 0; i < state->current_function->code.count; i++) {
            if (last_line != state->current_function->code[i].info.l0) {
                log_write("\n");
                print_lines_of_code(state->current_function->code[i].info, 0, 0, 0);
                last_line = state->current_function->code[i].info.l0;
            }

            print_ir_opcode(state->current_function->code[i]);
        }
#endif
    }
    stack_pop(&state->search_scopes);
    state->current_function = NULL;
    profiler_func_end();
}

void compile_globals(ir_state_t *state) {
    string_t key = string_copy(STRING("compile_globals"), default_allocator);
    hashmap_t<string_t, scope_entry_t> *scope = stack_peek(&state->search_scopes);
    
    {
        ir_function_t func = {};
        hashmap_add(&state->ir.functions, key, &func);
    }

    state->current_function = hashmap_get(&state->ir.functions, key);

    array_create(&state->current_function->code, 8, state->ir.code);
    {
        emit_op(state, IR_STACK_FRAME_PUSH, {}, 0);

        u64 global_size = 0;

        for (u64 i = 0; i < scope->capacity; i++) {
            kv_pair_t<string_t, scope_entry_t> *pair = scope->entries + i;
        
            if (!pair->occupied) continue;
            if (pair->deleted)   continue;

            if (pair->value.type != ENTRY_VAR) {
                continue;
            }

            switch (pair->value.stmt->type) {
                case AST_BIN_UNKN_DEF:  global_size += compile_variable(state,      pair->value.node, true); break;
                case AST_UNARY_VAR_DEF: global_size += compile_variable(state,      pair->value.node, true); break;
                case AST_BIN_MULT_DEF:  global_size += compile_mul_variables(state, pair->value.node, true); break;
                case AST_TERN_MULT_DEF: global_size += compile_mul_variables(state, pair->value.node, true); break;
                default: assert(false); break;
            }
        }
    
        for (u64 i = 0; i < global_size; i++) {
            array_add(&state->ir.globals, (s64) 0);
        }

        // code for initing the globals
        emit_op(state, IR_STACK_FRAME_POP, {}, 0);
        emit_op(state, IR_RET,             {}, 0);

// #ifdef VERBOSE
        u64 last_line = 0;

        for (u64 i = 0; i < state->current_function->code.count; i++) {
            if (last_line != state->current_function->code[i].info.l0) {
                if (state->current_function->code[i].info.from != NULL) {
                    log_write("\n");
                    print_lines_of_code(state->current_function->code[i].info, 0, 0, 0);
                    last_line = state->current_function->code[i].info.l0;
                }
            }

            print_ir_opcode(state->current_function->code[i]);
        }
// #endif
    }
    state->current_function = NULL;
}

ir_t compile_program(compiler_t *compiler) {
    profiler_func_start();
    UNUSED(compiler);

    assert(compiler != NULL);

    ir_state_t state = {};

    state.ir.code     = create_arena_allocator(1024 * sizeof(ir_opcode_t));
    state.compiler    = compiler;
    state.ir.is_valid = true;

    hashmap_t<string_t, scope_entry_t> *scope = array_get(&compiler->scopes, 0);
    stack_push(&state.search_scopes, scope);

    // compiling globals
    compile_globals(&state);

    // compiling code, functions
    for (u64 i = 0; i < scope->capacity; i++) {
        kv_pair_t<string_t, scope_entry_t> *pair = scope->entries + i;
    
        if (!pair->occupied) continue;
        if (pair->deleted)   continue;

        if (pair->value.type == ENTRY_TYPE || pair->value.type == ENTRY_VAR) {
            continue;
        }

        assert(pair->value.stmt->type == AST_BIN_UNKN_DEF);
        assert(pair->value.type == ENTRY_FUNC);
        compile_function(&state, pair->key, &pair->value);
    }

    stack_delete(&state.search_scopes);

    profiler_func_end();
    return state.ir;
}

