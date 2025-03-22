#include "compiler.h"
#include "analyzer.h"
#include "parser.h"
#include "scanner.h"
#include "logger.h"
#include "area_alloc.h"
#include "hashmap.h"

// plan @todo:
    // error at every global scope expression
    // scan tree and create all scopes + add all symbols
    // create compile units and after that type check all of them
    // create final IR of every scope
    //
    // codegen
    //
    //
    // count elements on left and rignt in a, b = b, a;
    // typecheck
    //
    // check functions to not contain assingment expresions! because it will break ','
    //
    // block in general := structure is not allowed if it is not an function def



// It is just check of all top-level symbols only after 
// it resolved we start to generate IR and when we generate IR
// we typecheck. Easy!
//
// So we just need to create global hashtable and after that 
// when we will be generating IR we will generate all of the other things
// like lambdas?

/*
b32 analyze_type(compiler_t *compiler, ast_node_t *node) {
    assert(node.type != AST_ERROR);

    switch (node.subtype) {
        case SUBTYPE_AST_STD_TYPE:
        case SUBTYPE_AST_AUTO_TYPE:
            return true;
            
        case SUBTYPE_AST_UNKN_TYPE:
            hashmap_contains();
            break;

        case SUBTYPE_AST_FUNC_TYPE:
            break;
        case SUBTYPE_AST_ARR_TYPE:
            break;
        case SUBTYPE_AST_PTR_TYPE:
            break;

        default:
            log_error(STR("Not supported yet."), 0);
            break;
    }
}
*/

b32 analyze_unkn_def(compiler_t *compiler, ast_node_t *node) {
    scope_tuple_t *scope = list_get(&compiler->analyzer->scopes, 0); // @todo replace to variable if need

    assert(scope->is_global);

    string_t key = node->token.data.string;

    if (hashmap_contains(&scope->table, key)) {
        return false;
    } else {
        scope_entry_t entry = {};
        ast_node_t *type = node->left;

        // b32 result = analyze_type(compiler, node->left);

        entry.node = node;

        if (node->right->subtype == AST_BLOCK_IMPERATIVE) {
            entry.type = ENTRY_FUNC;
            // entry.func = ;
        } else if (node->right->subtype == SUBTYPE_AST_EXPR) {
            check_value(type->subtype == SUBTYPE_AST_STD_TYPE || type->subtype == SUBTYPE_AST_AUTO_TYPE || type->subtype == SUBTYPE_AST_UNKN_TYPE);
            // process type somehow

            entry.type = ENTRY_VAR;
            entry.data.var.is_const = false;

        } else assert(false);

        // entry.var.type = 

        


        hashmap_add(&scope->table, key, &entry);
    }

    /*
    if (node->type == AST_BIN || node->type == AST_UNARY) {
        

        scope->table

    } else if (node->type == AST_TERN) { 
        
    }

    */

    return false;
}

b32 analyze_root_stmt(compiler_t *compiler, ast_node_t *root) {
    assert(compiler != NULL);
    assert(root     != NULL);

    if (root->type == AST_EMPTY)
        return true;

    if (root->type == AST_ERROR) {
        log_error(STR("Got unexpected value @error"), 0);
        assert(false);
        return false;
    }

    switch (root->subtype) {
        case SUBTYPE_AST_UNKN_DEF:
            return analyze_unkn_def(compiler, root);

        case SUBTYPE_AST_STRUCT_DEF:
            break;

        case SUBTYPE_AST_UNION_DEF:
            break;

        case SUBTYPE_AST_ENUM_DEF:
            break;

        case SUBTYPE_AST_EXPR:
            log_error_token(STR("Expression is not valid construct in the global scope."), compiler->scanner, root->token, 0);
            break;

        default:
            log_error_token(STR("Wrong type of construct in global scope."), compiler->scanner, root->token, 0);
            assert(false);
            break;
    }

    return false;
}

// first thing we do is add all of symbols in table
// then we can set 'ast_node_t.analyzed' to 'true'
// and then we can already analyze the code

b32 analyze_code(compiler_t *compiler) {
    for (u64 i = 0; i < compiler->parser->parsed_roots.count; i++) {
        ast_node_t *node = *list_get(&compiler->parser->parsed_roots, i);

        if (node->analyzed) continue;

        if (!analyze_root_stmt(compiler, node)) {
            // we got error there
            // it was already reported
            // so we just quit
            return false;
        }
    }

    return false;
}
