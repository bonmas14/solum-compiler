#include "backend.h"
#include <stdlib.h>

struct codegen_state_t {
    bool deez;
};

arena_t * local;
FILE    * file;
ast_node_t * name;

void generate_statement(compiler_t *compiler, ast_node_t *stmt, u64 depth);
void generate_unkn_def(compiler_t *compiler, ast_node_t *root, u64 depth);
void generate_block(compiler_t *compiler, ast_node_t *block, u64 depth);

void print_std_type(token_t token) {
    switch (token.type) {
        case TOK_U8:
            fprintf(file, "u8");
            break;
        case TOK_U16:
            fprintf(file, "u16");
            break;
        case TOK_U32:
            fprintf(file, "u32");
            break;
        case TOK_U64:
            fprintf(file, "u64");
            break;

        case TOK_S8:
            fprintf(file, "s8");
            break;
        case TOK_S16:
            fprintf(file, "s16");
            break;
        case TOK_S32:
            fprintf(file, "s32");
            break;
        case TOK_S64:
            fprintf(file, "s64");
            break;

        case TOK_F32:
            fprintf(file, "f32");
            break;
        case TOK_F64:
            fprintf(file, "f64");
            break;
        case TOK_BOOL32:
            fprintf(file, "b32");
            break;

        default:
            fprintf(file, "err_type");
            break;
    }
}

void generate_type(ast_node_t *type) {
    switch (type->subtype) {
        case SUBTYPE_AST_FUNC_PARAMS:
            {
                ast_node_t * next = type->list_start;
                fprintf(file, "(");
                if (type->child_count == 0) {
                    fprintf(file, "void");
                }

                for (u64 i = 0; i < type->child_count; i++) {
                    if (i > 0) {
                        fprintf(file, ", ");
                    }

                    generate_type(next);

                    next = next->list_next;
                }
                fprintf(file, ")");
            } break;

        case SUBTYPE_AST_PARAM_DEF:
            {
                generate_type(type->left);
                fprintf(file, " %.*s", (int)type->token.data.string.size, type->token.data.string.data);
            } break;

        case SUBTYPE_AST_FUNC_RETURNS: 
            {
                ast_node_t * next = type->list_start;

                if (type->child_count == 0) {
                    fprintf(file, "void");
                    break;
                }

                if (type->child_count == 1) {
                    generate_type(next);
                    break;
                }
                fprintf(file, "struct { ");
                for (u64 i = 0; i <= type->child_count; i++) {
                    if (i > 0) {
                        fprintf(file, "; ");
                    }

                    if (i == type->child_count) 
                        break;

                    generate_type(next);

                    fprintf(file, " rval%zu", i);
                    next = next->list_next;
                }
                fprintf(file, "}");

            } break;


        case SUBTYPE_AST_PTR_TYPE:
            {
                generate_type(type->left);
                fprintf(file, " *");
            } break;

        case SUBTYPE_AST_STD_TYPE: 
            {
                print_std_type(type->token);
            } break;

        case SUBTYPE_AST_FUNC_TYPE:
            {
                generate_type(type->right);

                fprintf(file, " %.*s", (int)name->token.data.string.size, name->token.data.string.data);

                generate_type(type->left);

            } break;

        case SUBTYPE_AST_UNKN_TYPE:
            {
                fprintf(file, " %.*s", (int)type->token.data.string.size, type->token.data.string.data);
            } break;
        default:
            break;
    }
}

void print_primary_value(token_t token) {
    switch (token.type) {
        case TOKEN_CONST_FP:
            fprintf(file, "%f", token.data.const_double);
            break;
        case TOKEN_CONST_INT:
            fprintf(file, "%zu", token.data.const_int);
            break;
        case TOKEN_CONST_STRING:
            fprintf(file, "STR(\"%.*s\")", (int)token.data.string.size, token.data.string.data);
            break;
        case TOKEN_IDENT:
            fprintf(file, "%.*s", (int)token.data.string.size, token.data.string.data);
            break;
        case TOK_DEFAULT:
            fprintf(file, "{}");
            break;
    }
}

void generate_expression_token(token_t token) {
    switch (token.type) {
        case TOKEN_LSHIFT: 
            fprintf(file, "<<");
            break;
        case TOKEN_RSHIFT:
            fprintf(file, ">>");
            break;
        case TOKEN_GEN_GET_SET:
            fprintf(file, ".");
            break;
        case TOKEN_GR: 
            fprintf(file, ">");
            break;
        case TOKEN_LS:
            fprintf(file, "<");
            break;
        case TOKEN_GEQ:
            fprintf(file, ">=");
            break;
        case TOKEN_LEQ:
            fprintf(file, "<=");
            break;
        case TOKEN_EQ:
            fprintf(file, "==");
            break;
        case TOKEN_NEQ: 
            fprintf(file, "!=");
            break;
        case TOKEN_LOGIC_AND:
            fprintf(file, "&&");
            break;
        case TOKEN_LOGIC_OR:
            fprintf(file, "||");
            break;
        default:
            if (token.type > 256) break;
            fprintf(file, "%c", (u8)token.type);
            break;
    }
}

void generate_expression(compiler_t *compiler, ast_node_t *expression) {
    assert(expression->subtype == SUBTYPE_AST_EXPR);

    switch (expression->type) {
        case AST_LEAF:
            print_primary_value(expression->token);
            break;

        case AST_UNARY:
            fprintf(file, "(");
            generate_expression_token(expression->token);
            generate_expression(compiler, expression->left);
            fprintf(file, ")");
            break;

        case AST_BIN:
            fprintf(file, "(");
            generate_expression(compiler, expression->left);
            generate_expression_token(expression->token);
            generate_expression(compiler, expression->right);
            fprintf(file, ")");
            break;
    }
}

void generate_statement(compiler_t *compiler, ast_node_t *stmt, u64 depth) {
    add_left_pad(file, depth * LEFT_PAD_STANDART_OFFSET);

    switch (stmt->subtype) {

        case AST_BLOCK_IMPERATIVE:
            generate_block(compiler, stmt, depth);
            return;
        case SUBTYPE_AST_UNKN_DEF:
            generate_unkn_def(compiler, stmt, depth);
            return;

        case SUBTYPE_AST_EXPR:
            generate_expression(compiler, stmt);
            break;

        case SUBTYPE_AST_RET_STMT:
            fprintf(file, "return ");
            generate_expression(compiler, stmt->left);
            break;

        case SUBTYPE_AST_IF_STMT:
            fprintf(file, "if (");
            generate_expression(compiler, stmt->left);
            fprintf(file, ") ");
            generate_block(compiler, stmt->right, depth);
            return;

        case SUBTYPE_AST_ELIF_STMT:
            fprintf(file, "else if (");
            generate_expression(compiler, stmt->left);
            fprintf(file, ") ");
            generate_block(compiler, stmt->right, depth);
            return;

        case SUBTYPE_AST_ELSE_STMT:
            fprintf(file, "else ");
            generate_block(compiler, stmt->left, depth);
            return;

        case SUBTYPE_AST_WHILE_STMT:
            fprintf(file, "NOT_HERE_YET");
            break;
    }

    fprintf(file, ";\n");
}


void generate_block(compiler_t *compiler, ast_node_t *block, u64 depth) {
    fprintf(file, "{\n");

    ast_node_t * next = block->list_start;

    for (u64 i = 0; i < block->child_count; i++) {
        generate_statement(compiler, next, depth + 1);
        next = next->list_next;
    }

    add_left_pad(file, depth * LEFT_PAD_STANDART_OFFSET);
    fprintf(file, "}\n");
}

void generate_unkn_def(compiler_t *compiler, ast_node_t *root, u64 depth) {

    // int name = ;
    //
    // int func(params) { }

    name = root;
    generate_type(root->left);
    
    if (root->left->subtype != SUBTYPE_AST_FUNC_TYPE) {
        fprintf(file, " %.*s", (int)root->token.data.string.size, root->token.data.string.data);
    }

    if (root->right->subtype == AST_BLOCK_IMPERATIVE) {
        fprintf(file, " ");
        generate_block(compiler, root->right, depth);
    } else {
        fprintf(file, " = ");
        generate_expression(compiler, root->right);
        fprintf(file, ";\n");
    }
    // "int %.s (param)"
    // left type 
    // right expr/block
}
void generate_root(compiler_t *compiler, ast_node_t *root) {

    switch (root->subtype) {

        case SUBTYPE_AST_UNKN_DEF:
            generate_unkn_def(compiler, root, 0);
            break;

        default:
            break;
    }

    fprintf(file, "\n");
}

void generate_code(compiler_t *compiler) {
    local = arena_create(1024);

    log_update_color();
    fprintf(stdout, "WE ARE IN CODEGEN\n");

    file = fopen("./temp/output.cpp", "wb");

    fprintf(file, "#include <stdio.h>\n");
    fprintf(file, "#include \"type_defines.h\"\n");

    for (u64 i = 0; i < compiler->parser->roots.count; i++) {
        ast_node_t * node = *area_get(&compiler->parser->roots, i);

        generate_root(compiler, node);
    }

    fclose(file);
}
