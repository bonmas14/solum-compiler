#include "ir_generator.h"

#include "allocator.h"
#include "talloc.h"

#include "scanner.h"
#include "parser.h"
#include "list.h"

#include <stdio.h>


// we already need to know where is the data defined, because when we access it, we should do relative links to it

void gen_stmt_ir(compiler_t *state, list_t<ir_opcode_t> *opcodes, ast_node_t *node);
void gen_bin_unkn_ir(compiler_t *state, list_t<ir_opcode_t> *opcodes, ast_node_t *node);
void gen_block_ir(compiler_t *state, list_t<ir_opcode_t> *opcodes, ast_node_t *block);

void gen_expr_ir(compiler_t *state, list_t<ir_opcode_t> *opcodes, ast_node_t *expr) {
    switch (expr->type) {
        case AST_PRIMARY: 
            {
                ir_opcode_t op = {};
                op.operation = IR_PUSH;

                if (expr->token.type == TOKEN_CONST_INT) {
                    op.arg1 = expr->token.data.const_int;
                } else {
                    // if identifier, we push value from hashtable,
                    // aka just doing mov x, [rsp - value pos on stack]
                }

                list_add(opcodes, &op);
            } break;

        case AST_UNARY_DEREF:
        case AST_UNARY_REF:
        case AST_UNARY_NEGATE:
        case AST_UNARY_NOT:
            gen_expr_ir(state, opcodes, expr->left);
            break;

        case AST_BIN_CAST:
            // left is type
        case AST_FUNC_CALL:
        case AST_MEMBER_ACCESS:
        case AST_ARRAY_ACCESS:
            log_error(STR("Not supported at all..."));
            break;
        case AST_SEPARATION:
            break;

        case AST_BIN_ADD:
        case AST_BIN_SUB:
        case AST_BIN_ASSIGN:
        case AST_BIN_GR:
        case AST_BIN_LS:
        case AST_BIN_GEQ:
        case AST_BIN_LEQ:
        case AST_BIN_EQ:
        case AST_BIN_NEQ:
        case AST_BIN_LOG_OR:
        case AST_BIN_LOG_AND:
        case AST_BIN_MUL:
        case AST_BIN_DIV:
        case AST_BIN_MOD:
        case AST_BIN_BIT_XOR:
        case AST_BIN_BIT_OR:
        case AST_BIN_BIT_AND:
        case AST_BIN_BIT_LSHIFT:
        case AST_BIN_BIT_RSHIFT:
            gen_expr_ir(state, opcodes, expr->left);
            gen_expr_ir(state, opcodes, expr->right);
            log_error(STR("Not supported..."));
            break;

        default:
            log_error(STR("Not supported at all..."));
            break;
    }
}

void gen_stmt_ir(compiler_t *state, list_t<ir_opcode_t> *opcodes, ast_node_t *node) {
    switch (node->type) {
        case AST_BLOCK_IMPERATIVE:
            gen_block_ir(state, opcodes, node);
            break;

        case AST_UNION_DEF:
        case AST_STRUCT_DEF:
            break;

        case AST_UNARY_VAR_DEF:
        case AST_BIN_UNKN_DEF:
        case AST_BIN_MULT_DEF:
        case AST_TERN_MULT_DEF:
            break;

        case AST_RET_STMT:
            gen_expr_ir(state, opcodes, node->left);
            break;

        case AST_IF_STMT:
            break;

        case AST_ELIF_STMT:
            break;

        case AST_ELSE_STMT:
            break;

        case AST_WHILE_STMT:
            break;

        default:
            break;
    }
}


void gen_block_ir(compiler_t *state, list_t<ir_opcode_t> *opcodes, ast_node_t *block) {
    ast_node_t * next = block->list_start;

    for (u64 i = 0; i < block->child_count; i++) {
        gen_stmt_ir(state, opcodes, next);
        next = next->list_next;
    }
}

void gen_bin_unkn_ir(compiler_t *state, list_t<ir_opcode_t> *opcodes, ast_node_t *node) {
    if (node->left->type != AST_FUNC_TYPE) {
        return;
    }

    assert(node->left  != NULL);
    assert(node->right != NULL);

    if (node->right->type != AST_BLOCK_IMPERATIVE) {
        assert(false);
        return;
    }

    gen_block_ir(state, opcodes, node->right);
}

list_t<ir_opcode_t> gen_statement_ir(compiler_t *state, ast_node_t *statement) {
    list_t<ir_opcode_t> ir_code = {};

    // so here we will generate code for statements
    //
    // supported things:
    //
    //



    for (u64 i = 0; i < ir_code.count; i++) {
        print_ir_opcode(list_get(&ir_code, i));
    }

    return ir_code;
}

void print_ir_opcode(ir_opcode_t *code) {
    allocator_t *temp = get_temporary_allocator();

    u8* buffer = (u8*)mem_alloc(temp, 128);

    sprintf((char*)buffer, "ir_code: OP: [%llu] [%llu],[%llu],[%llu]\n", code->operation, code->arg1, code->arg2, code->arg3);
    log_info(buffer);
}
