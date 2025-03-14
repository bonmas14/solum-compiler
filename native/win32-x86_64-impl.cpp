#include "backend.h"
#include "parser.h"
#include "scanner.h"
#include "analyzer.h"

#include <stdlib.h>

struct codegen_t {
    FILE     *file;
    string_t *current_identifier_name;
};

codegen_t *codegen_create(arena_t *allocator) {
    assert(allocator != NULL);

    codegen_t *result = (codegen_t*)arena_allocate(allocator, sizeof(codegen_t));

    assert(result != NULL);

    return result;
}

void generate_statement(compiler_t *compiler, ast_node_t *stmt, u64 depth);
void generate_unkn_def(compiler_t *compiler, ast_node_t *root, u64 depth);
void generate_struct_def(compiler_t *compiler, ast_node_t *root, u64 depth);
void generate_union_def(compiler_t *compiler, ast_node_t *root, u64 depth);
void generate_block(compiler_t *compiler, ast_node_t *block, u64 depth);

void print_std_type(compiler_t *compiler, token_t token) {
    switch (token.type) {
        case TOK_U8:  fprintf(compiler->codegen->file, "u8");  break;
        case TOK_U16: fprintf(compiler->codegen->file, "u16"); break;
        case TOK_U32: fprintf(compiler->codegen->file, "u32"); break;
        case TOK_U64: fprintf(compiler->codegen->file, "u64"); break;

        case TOK_S8:  fprintf(compiler->codegen->file, "s8");  break;
        case TOK_S16: fprintf(compiler->codegen->file, "s16"); break;
        case TOK_S32: fprintf(compiler->codegen->file, "s32"); break;
        case TOK_S64: fprintf(compiler->codegen->file, "s64"); break;

        case TOK_F32: fprintf(compiler->codegen->file, "f32"); break;
        case TOK_F64: fprintf(compiler->codegen->file, "f64"); break;

        case TOK_BOOL32: fprintf(compiler->codegen->file, "b32"); break;

        default: fprintf(compiler->codegen->file, "err_type"); break;
    }
}

void generate_type(compiler_t *compiler, ast_node_t *type) {
    switch (type->subtype) {
        case SUBTYPE_AST_FUNC_PARAMS:
            {
                ast_node_t * next = type->list_start;
                fprintf(compiler->codegen->file, "(");
                if (type->child_count == 0) {
                    fprintf(compiler->codegen->file, "void");
                }

                for (u64 i = 0; i < type->child_count; i++) {
                    if (i > 0) {
                        fprintf(compiler->codegen->file, ", ");
                    }

                    generate_type(compiler, next);

                    next = next->list_next;
                }
                fprintf(compiler->codegen->file, ")");
            } break;

        case SUBTYPE_AST_PARAM_DEF:
            {
                generate_type(compiler, type->left);
                fprintf(compiler->codegen->file, " %.*s", (int)type->token.data.string.size, type->token.data.string.data);
            } break;

        case SUBTYPE_AST_FUNC_RETURNS: 
            {
                ast_node_t * next = type->list_start;

                if (type->child_count == 0) {
                    fprintf(compiler->codegen->file, "void");
                    break;
                }

                if (type->child_count == 1) {
                    generate_type(compiler, next);
                    break;
                }
                fprintf(compiler->codegen->file, "struct { ");
                for (u64 i = 0; i <= type->child_count; i++) {
                    if (i > 0) {
                        fprintf(compiler->codegen->file, "; ");
                    }

                    if (i == type->child_count) 
                        break;

                    generate_type(compiler, next);

                    fprintf(compiler->codegen->file, " rval%zu", i);
                    next = next->list_next;
                }
                fprintf(compiler->codegen->file, "}");

            } break;


        case SUBTYPE_AST_PTR_TYPE:
            {
                generate_type(compiler, type->left);
                fprintf(compiler->codegen->file, " *");
            } break;

        case SUBTYPE_AST_STD_TYPE: 
            {
                print_std_type(compiler, type->token);
            } break;

        case SUBTYPE_AST_FUNC_TYPE:
            {
                generate_type(compiler, type->right);

                fprintf(compiler->codegen->file, " %.*s", (int)compiler->codegen->current_identifier_name->size, compiler->codegen->current_identifier_name->data);

                generate_type(compiler, type->left);

            } break;

        case SUBTYPE_AST_UNKN_TYPE:
            {
                fprintf(compiler->codegen->file, "%.*s", (int)type->token.data.string.size, type->token.data.string.data);
            } break;
        default:
            break;
    }
}

void print_primary_value(compiler_t *compiler, token_t token) {
    switch (token.type) {
        case TOKEN_CONST_FP:
            fprintf(compiler->codegen->file, "%f", token.data.const_double);
            break;
        case TOKEN_CONST_INT:
            fprintf(compiler->codegen->file, "%zu", token.data.const_int);
            break;
        case TOKEN_CONST_STRING:
            fprintf(compiler->codegen->file, "__internal_STR(\"%.*s\")", (int)token.data.string.size, token.data.string.data);
            break;
        case TOKEN_IDENT:
            fprintf(compiler->codegen->file, "%.*s", (int)token.data.string.size, token.data.string.data);
            break;
        case TOK_DEFAULT:
            fprintf(compiler->codegen->file, "{}");
            break;
    }
}

void generate_expression_token(compiler_t *compiler, token_t token) {
    switch (token.type) {
        case '@':
            fprintf(compiler->codegen->file, "*");
            break;
        case '^':
            fprintf(compiler->codegen->file, "&");
            break;

        case TOKEN_LSHIFT: 
            fprintf(compiler->codegen->file, "<<");
            break;
        case TOKEN_RSHIFT:
            fprintf(compiler->codegen->file, ">>");
            break;
        case TOKEN_GEN_GET_SET:
            fprintf(compiler->codegen->file, ".");
            break;
        case TOKEN_GR: 
            fprintf(compiler->codegen->file, ">");
            break;
        case TOKEN_LS:
            fprintf(compiler->codegen->file, "<");
            break;
        case TOKEN_GEQ:
            fprintf(compiler->codegen->file, ">=");
            break;
        case TOKEN_LEQ:
            fprintf(compiler->codegen->file, "<=");
            break;
        case TOKEN_EQ:
            fprintf(compiler->codegen->file, "==");
            break;
        case TOKEN_NEQ: 
            fprintf(compiler->codegen->file, "!=");
            break;
        case TOKEN_LOGIC_AND:
            fprintf(compiler->codegen->file, "&&");
            break;
        case TOKEN_LOGIC_OR:
            fprintf(compiler->codegen->file, "||");
            break;
        default:
            if (token.type > 256) break;
            fprintf(compiler->codegen->file, "%c", (u8)token.type);
            break;
    }
}

void generate_expression(compiler_t *compiler, ast_node_t *expression) {
    if (expression->token.type == TOK_CAST) {
        fprintf(compiler->codegen->file, "(");
        generate_type(compiler, expression->left);  
        fprintf(compiler->codegen->file, ")");

        generate_expression(compiler, expression->right);
        return;
    }

    assert(expression->subtype == SUBTYPE_AST_EXPR);

    switch (expression->type) {
        case AST_LEAF:
            print_primary_value(compiler, expression->token);
            break;

        case AST_UNARY:
            // fprintf(compiler->codegen->file, "(");
            generate_expression_token(compiler, expression->token);
            generate_expression(compiler, expression->left);
            // fprintf(compiler->codegen->file, ")");
            break;

        case AST_BIN:
            // fprintf(compiler->codegen->file, "(");
            generate_expression(compiler, expression->left);
            generate_expression_token(compiler, expression->token);
            
            if (expression->token.type == TOKEN_GEN_FUNC_CALL) {
                fprintf(compiler->codegen->file, "(");
                generate_expression(compiler, expression->right);
                fprintf(compiler->codegen->file, ")");
            } else if (expression->token.type == TOKEN_GEN_ARRAY_GET_SET) {
                fprintf(compiler->codegen->file, "[");
                generate_expression(compiler, expression->right);
                fprintf(compiler->codegen->file, "]");
            } else {
                generate_expression(compiler, expression->right);
            }

            // fprintf(compiler->codegen->file, ")");
            break;
    }
}

void generate_statement(compiler_t *compiler, ast_node_t *stmt, u64 depth) {
    add_left_pad(compiler->codegen->file, depth * LEFT_PAD_STANDART_OFFSET);

    switch (stmt->subtype) {

        case AST_BLOCK_IMPERATIVE:
            generate_block(compiler, stmt, depth);
            fprintf(compiler->codegen->file, "\n");
            return;
        case SUBTYPE_AST_STRUCT_DEF:
            generate_struct_def(compiler, stmt, depth);
            return;
        case SUBTYPE_AST_UNION_DEF:
            generate_union_def(compiler, stmt, depth);
            return;
        case SUBTYPE_AST_UNKN_DEF:
            generate_unkn_def(compiler, stmt, depth);
            return;

        case SUBTYPE_AST_EXPR:
            generate_expression(compiler, stmt);
            break;

        case SUBTYPE_AST_RET_STMT:
            fprintf(compiler->codegen->file, "return ");
            generate_expression(compiler, stmt->left);
            break;

        case SUBTYPE_AST_IF_STMT:
            fprintf(compiler->codegen->file, "if (");
            generate_expression(compiler, stmt->left);
            fprintf(compiler->codegen->file, ") ");
            generate_block(compiler, stmt->right, depth);
            fprintf(compiler->codegen->file, "\n");
            return;

        case SUBTYPE_AST_ELIF_STMT:
            fprintf(compiler->codegen->file, "else if (");
            generate_expression(compiler, stmt->left);
            fprintf(compiler->codegen->file, ") ");
            generate_block(compiler, stmt->right, depth);
            fprintf(compiler->codegen->file, "\n");
            return;

        case SUBTYPE_AST_ELSE_STMT:
            fprintf(compiler->codegen->file, "else ");
            generate_block(compiler, stmt->left, depth);
            fprintf(compiler->codegen->file, "\n");
            return;

        case SUBTYPE_AST_WHILE_STMT:
            fprintf(compiler->codegen->file, "while (");
            generate_expression(compiler, stmt->left);
            fprintf(compiler->codegen->file, ") ");
            generate_block(compiler, stmt->right, depth);
            fprintf(compiler->codegen->file, "\n");
            return;

        default:
            fprintf(compiler->codegen->file, "NOT_HERE_YET");
            break;
    }

    fprintf(compiler->codegen->file, ";\n");
}


void generate_block(compiler_t *compiler, ast_node_t *block, u64 depth) {
    fprintf(compiler->codegen->file, "{\n");

    ast_node_t * next = block->list_start;

    for (u64 i = 0; i < block->child_count; i++) {
        generate_statement(compiler, next, depth + 1);
        next = next->list_next;
    }

    add_left_pad(compiler->codegen->file, depth * LEFT_PAD_STANDART_OFFSET);
    fprintf(compiler->codegen->file, "}");
}

void generate_unkn_def(compiler_t *compiler, ast_node_t *root, u64 depth) {
    compiler->codegen->current_identifier_name = &root->token.data.string;

    generate_type(compiler, root->left);
    
    if (root->left->subtype != SUBTYPE_AST_FUNC_TYPE) {
        fprintf(compiler->codegen->file, " %.*s", (int)root->token.data.string.size, root->token.data.string.data);
    }

    if (root->right == NULL) {
        fprintf(compiler->codegen->file, ";\n");
        return;
    }

    if (root->right->subtype == AST_BLOCK_IMPERATIVE) {
        fprintf(compiler->codegen->file, " ");
        generate_block(compiler, root->right, depth);
    } else {
        fprintf(compiler->codegen->file, " = ");
        generate_expression(compiler, root->right);
        fprintf(compiler->codegen->file, ";\n");
    }
}

void generate_union_def(compiler_t *compiler, ast_node_t *root, u64 depth) {
    fprintf(compiler->codegen->file, "union ");

    string_t name = root->token.data.string;

    if (name.size != 1 || name.data[0] != '_') {
        fprintf(compiler->codegen->file, "%.*s ", (int)name.size, name.data);
    }

    generate_block(compiler, root->left, depth);
    fprintf(compiler->codegen->file, ";\n");
}

void generate_struct_def(compiler_t *compiler, ast_node_t *root, u64 depth) {
    fprintf(compiler->codegen->file, "struct ");

    string_t name = root->token.data.string;

    if (name.size != 1 || name.data[0] != '_') {
        fprintf(compiler->codegen->file, "%.*s ", (int)name.size, name.data);
    }

    generate_block(compiler, root->left, depth);
    fprintf(compiler->codegen->file, ";\n");
}


void generate_root(compiler_t *compiler, ast_node_t *root) {
    switch (root->subtype) {
        case SUBTYPE_AST_UNION_DEF:
            generate_union_def(compiler, root, 0);
            break;
        case SUBTYPE_AST_STRUCT_DEF:
            generate_struct_def(compiler, root, 0);
            break;
        case SUBTYPE_AST_UNKN_DEF:
            generate_unkn_def(compiler, root, 0);
            break;

        default:
            break;
    }

    fprintf(compiler->codegen->file, "\n");
}

void generate_code(compiler_t *compiler) {
    log_update_color();
    fprintf(stdout, "WE ARE IN CODEGEN\n");

    compiler->codegen->file = fopen("./output.cpp", "wb");

    fprintf(compiler->codegen->file, "#include <stdio.h>\n");
    fprintf(compiler->codegen->file, "#include \"type_defines.h\"\n");

    for (u64 i = 0; i < compiler->parser->parsed_roots.count; i++) {
        ast_node_t * node = *list_get(&compiler->parser->parsed_roots, i);

        generate_root(compiler, node);
    }

    fclose(compiler->codegen->file);
}
