#include "analyzer.h"

#include "stack.h"
#include "logger.h"
#include "compiler.h"
#include "scanner.h"
#include "parser.h"

#include "talloc.h"
#include "strings.h"

// @todo @fix ... in parser check if all nodes in separated expression are just identifiers (AST_PRIMARY). caching the tokens or just changing them all to use temp alloc
// so in multiple definitions we getting multiple errors that are for one entry_type
// but it happens like this:
//    a, a : s32 = 123;

struct analyzer_state_t {
    scanner_t  *scanner;
    compiler_t *compiler;
    stack_t<hashmap_t<string_t, scope_entry_t>*> *scopes;
};

// ------------ helpers

hashmap_t<string_t, scope_entry_t> create_scope(void) {
    hashmap_t<string_t, scope_entry_t> scope = {};
    scope.hash_func    = get_string_hash;
    scope.compare_func = compare_string_keys;
    return scope;
}

void report_already_used(analyzer_state_t *state, string_t key, ast_node_t *already_used_node) {
    string_t decorated_name = string_temp_concat(string_temp_concat(STRING("The identifier '"), key), STRING("' "));
    string_t info = string_temp_concat(decorated_name, STRING("is already used before.\n\0"));
    log_error_token((u8*)string_to_c_string(info, get_temporary_allocator()), state->scanner, already_used_node->token, 0);
}

b32 check_if_unique(scope_entry_t *entry, ast_node_t *new_node) {
    if (entry->node == NULL) return true;

    if (mem_compare((u8*)entry->node, (u8*)new_node, sizeof(ast_node_t)))
        return false;

    return true;
}

void add_blank_entry(hashmap_t<string_t, scope_entry_t> *scope, string_t key, scope_entry_t **output) {
    assert(output != NULL);
    assert(scope  != NULL);

    scope_entry_t entry = {};
    if (!hashmap_add(scope, key, &entry))
        assert(false);

    *output = hashmap_get(scope, key);
    assert(*output != NULL);
}

b32 aquire_entry(hashmap_t<string_t, scope_entry_t> *scope, string_t key, ast_node_t *node, scope_entry_t **output) {
    UNUSED(output);

    if (hashmap_contains(scope, key)) {
        scope_entry_t *entry = hashmap_get(scope, key);

        if (!check_if_unique(entry, node)) {
            return false;
        }

        *output = entry;

        return true;
    }

    add_blank_entry(scope, key, output);
    return true;
}

b32 get_if_exists(analyzer_state_t *state, string_t key, scope_entry_t **output) {
    UNUSED(output);
    // scan up to global scope

    for (u64 i = 0; i < state->scopes->index; i++) {
        if (!hashmap_contains(state->scopes->data[i], key))
            continue;

        *output = hashmap_get(state->scopes->data[i], key);
        return true;
    }

    return false;
}

// -----------------

b32 analyze_unary_var_def(analyzer_state_t *state, ast_node_t *node, b32 *should_wait) {
    assert(node->type == AST_UNARY_VAR_DEF);
    assert(state       != NULL);
    assert(node        != NULL);
    assert(should_wait != NULL);

    string_t key = node->token.data.string;
    scope_entry_t *entry = {};

    if (!aquire_entry(stack_peek(state->scopes), key, node, &entry)) {
        report_already_used(state, key, node);
        return false;
    }

    if (!entry->configured) {
        entry->node = node;
        entry->type = ENTRY_VAR;
    }

    if (node->left->type == AST_UNKN_TYPE) {
        string_t type_key = node->left->token.data.string;

        scope_entry_t *type;

        if (get_if_exists(state, type_key, &type)) {
            switch (type->type) {
                case ENTRY_TYPE:
                    break;

                case ENTRY_PROTOTYPE: {
                    log_error_token(STR("Couldn't create prototype realization without body."), state->scanner, node->token, 0);
                    
                    log_push_color(INFO_COLOR);
                    string_t decorated_name = string_temp_concat(string_temp_concat(STRING("---- '"), key), STRING("' "));
                    string_t info = string_temp_concat(decorated_name, STRING("is defined as a prototype of:\n\0"));

                    log_print(info);
                    log_pop_color();

                    log_push_color(255, 255, 255);
                    print_lines_of_code(state->scanner, 2, 1, type->node->token, 0);
                    log_pop_color();
                } return false;

                case ENTRY_FUNC: {
                    log_error_token(STR("Couldn't create variable with function type."), state->scanner, node->token, 0);
                    
                    log_push_color(INFO_COLOR);
                    string_t decorated_name = string_temp_concat(string_temp_concat(STRING("---- '"), type_key), STRING("' "));
                    string_t info = string_temp_concat(decorated_name, STRING("is defined as a function here:\n\0"));

                    log_print(info);
                    log_pop_color();

                    log_push_color(255, 255, 255);
                    print_lines_of_code(state->scanner, 2, 1, type->node->token, 0);
                    log_pop_color();
                } return false;

                default:
                    log_error(STR("Unexpected type..."));
                    return false;
            }
        } else {
            *should_wait = true;
        }
    }

    if (!*should_wait) {
        node->analyzed = true;
    }

    return true;
}


/*
b32 scan_bin_var_defs(analyzer_state_t *state, ast_node_t *node, b32 *should_wait) {
    assert(node->type == AST_BIN_MULT_DEF);
    assert(state != NULL);
    assert(node  != NULL);
    assert(should_wait != NULL);

    b32 result = true;
    b32 is_not_resolved = false;

    if ((node->right->type == AST_UNKN_TYPE) || (node->right->type == AST_AUTO_TYPE)) {
        is_not_resolved = true;
    }

    if (node->right->type != AST_MUL_TYPES) {
        ast_node_t * next = node->left->list_start;

        for (u64 i = 0; i < node->left->child_count; i++) {
            scope_entry_t entry = {};
            string_t key = next->token.data.string;

            entry.entry_type = ENTRY_VAR;
            entry.node = node;
            entry.unknown_type = is_not_resolved;

            if (!add_symbol_to_scope(state, key, &entry)) 
                result = false;

            next = next->list_next;
        }

        return result;
    }

    if (node->left->child_count != node->right->child_count) {
        ast_node_t * next = node->left->list_start;

        while (next->list_next != NULL) {
            next = next->list_next;
        }

        log_error_token(STR("This variable didn't have it's own type:"), state->scanner, next->token, 0);
        return false;
    }

    ast_node_t * name = node->left->list_start;
    ast_node_t * type = node->right->list_start;

    for (u64 i = 0; i < node->left->child_count; i++) {
        scope_entry_t entry = {};
        string_t key = name->token.data.string;

        entry.entry_type = ENTRY_VAR;
        entry.node = node;

        if (type->type == AST_UNKN_TYPE) {
            entry.unknown_type = true;
        }

        if (type->type == AST_VOID_TYPE) {
            log_push_color(ERROR_COLOR);
            string_t decorated_name = string_temp_concat(string_temp_concat(STRING("The variable '"), key), STRING("' "));
            string_t info = string_temp_concat(decorated_name, STRING("is defined with void type:\n\0"));
            log_print(info);
            log_pop_color();

            log_push_color(255, 255, 255);
            print_lines_of_code(state->scanner, 0, 0, name->token, 0);
            log_write(STR("\n"));
            log_pop_color();
            result = false;
        }

        if (!add_symbol_to_scope(state, key, &entry)) 
            result = false;

        name = name->list_next;
        type = type->list_next;
    }

    return result;
} 
*/

b32 analyze_unkn_def(analyzer_state_t *state, ast_node_t *node, b32 *should_wait) {
    assert(node->type == AST_BIN_UNKN_DEF);
    assert(state != NULL);
    assert(node  != NULL);
    assert(should_wait != NULL);

    string_t key = node->token.data.string;
    scope_entry_t *entry = {};

    if (!aquire_entry(stack_peek(state->scopes), key, node, &entry)) {
        report_already_used(state, key, node);
        return false;
    }

    if (!entry->configured) {
        entry->node = node;
    }

    if (node->left->type == AST_UNKN_TYPE) {
        string_t type_key = node->left->token.data.string;

        scope_entry_t *type = {};

        // this could be: prototype, or var def
        if (get_if_exists(state, type_key, &type)) {
            switch (type->type) {
                case ENTRY_TYPE:
                    break;

                case ENTRY_PROTOTYPE:
                    break;

                case ENTRY_FUNC: 
                    log_error(STR("Cant use FUNC as a type of variable [@better_message]"));
                    entry->type = ENTRY_ERROR;
                    break;

                default:
                    log_error(STR("Unexpected type..."));
                    return false;
            }
        } else {
            *should_wait = true;
        }
    }


    switch (node->left->type) {
        case AST_FUNC_TYPE:
            entry->type = ENTRY_FUNC;
            // analyze func
            break;

            // ast_type == primary
        case AST_AUTO_TYPE:
        case AST_STD_TYPE:
            entry->type = ENTRY_VAR;

            if (node->left) {
            }

            break;
            // ast_type == unary
        case AST_ARR_TYPE:
        case AST_PTR_TYPE:
            log_error(STR("we dont support pointers and arrays right now"));
            entry->type = ENTRY_ERROR;
            return false;
            break;

        default:
            log_error(STR("unexpected type of ast node. in scan unkn def"));
            return false;
    } 

    return true;
}

b32 analyze_and_add_struct_members(analyzer_state_t *state, scope_entry_t *entry) {
    ast_node_t *block = entry->node->left;

    ast_node_t * next = block->list_start;

    b32 should_wait = false;

    for (u64 i = 0; i < block->child_count; i++) {
        analyze_unary_var_def(state, next, &should_wait);

        if (should_wait) return true;

        next = next->list_next;
    }

    entry->node->analyzed = true;
    return true;
}

b32 analyze_struct(analyzer_state_t *state, ast_node_t *node) {
    assert(node->type == AST_STRUCT_DEF);
    if (node->analyzed) return true; 

    string_t key = node->token.data.string;

    scope_entry_t *entry = NULL;

    if (!aquire_entry(stack_peek(state->scopes), key, node, &entry)) {
        report_already_used(state, key, node);
        return false;
    }

    if (!entry->configured) {
        entry->node = node;
        entry->type = ENTRY_TYPE;
        entry->scope = create_scope();
    }


    b32 result = false;
    stack_push(state->scopes, &entry->scope);
    result = analyze_and_add_struct_members(state, entry); 
    stack_pop(state->scopes);

    return result;
}


/*
b32 scan_union_def(analyzer_state_t *state, ast_node_t *node) {
    assert(node->type == AST_UNION_DEF);
    if (node.analyzed) return true; 

    scope_entry_t entry = {};
    string_t key = node->token.data.string;

    entry.entry_type = ENTRY_TYPE;
    entry.node = node;
    
    b32 result = add_symbol_to_scope(state, key, &entry);
    node.analyzed = true;
    return result;
}

b32 scan_enum_def(analyzer_state_t *state, ast_node_t *node) {
    assert(node->type == AST_ENUM_DEF);
    if (node.analyzed) return true; 

    string_t key = node->token.data.string;

    scope_entry_t entry = {};
    entry.entry_type = ENTRY_TYPE;
    entry.node       = node;
    
    b32 result    = add_symbol_to_scope(state, key, &entry);
    node.analyzed = true;
    return result;
}

b32 scan_prototype_def(analyzer_state_t *state, ast_node_t *node) {
    assert(node->type == AST_UNARY_PROTO_DEF);
    if (node.analyzed) return true; 

    scope_entry_t entry = {};
    string_t key = node->token.data.string;

    entry.entry_type = ENTRY_PROTOTYPE;
    entry.node = node;

    b32 result = add_symbol_to_scope(state, key, &entry);
    node.analyzed = true;
    return result;
}


b32 scan_tern_var_defs(analyzer_state_t *state, ast_node_t *node) {
    assert(state != NULL);
    assert(node  != NULL);
    assert(node->type == AST_TERN_MULT_DEF);

    b32 result = true;
    b32 is_not_resolved = false;

    if ((node->center->type == AST_UNKN_TYPE) || (node->center->type == AST_AUTO_TYPE)) {
        is_not_resolved = true;
    }

    if (node->center->type != AST_MUL_TYPES) {
        ast_node_t * next = node->left->list_start;

        for (u64 i = 0; i < node->left->child_count; i++) {
            scope_entry_t entry = {};
            string_t key = next->token.data.string;

            entry.entry_type = ENTRY_VAR;
            entry.node = node;
            entry.unknown_type = is_not_resolved;

            if (!add_symbol_to_scope(state, key, &entry)) 
                result = false;

            next = next->list_next;
        }

        return result;
    }

    if (node->left->child_count != node->center->child_count) {
        ast_node_t * next;
        u64 least_size = MIN(node->left->child_count, node->center->child_count);

        if (least_size >= node->left->child_count) {
            next = node->center->list_start;
            for (u64 i = 0; i < least_size; i++) {
                next = next->list_next;
            }
            log_error_token(STR("Too much types in definition. Starting from this one:"), state->scanner, next->token, 0);
        } else {
            next = node->left->list_start;
            for (u64 i = 0; i < least_size; i++) {
                next = next->list_next;
            }
            log_error_token(STR("Starting from this variable name, there's no enough types in definition:"), state->scanner, next->token, 0);
        }

        return false;
    }

    ast_node_t * name = node->left->list_start;
    ast_node_t * type = node->center->list_start;

    for (u64 i = 0; i < node->left->child_count; i++) {
        scope_entry_t entry = {};
        string_t key = name->token.data.string;

        entry.entry_type = ENTRY_VAR;
        entry.node = node;

        if (type->type == AST_UNKN_TYPE) {
            entry.unknown_type = true;
        }

        if (type->type == AST_VOID_TYPE) {
            log_push_color(ERROR_COLOR);
            string_t decorated_name = string_temp_concat(string_temp_concat(STRING("The variable '"), key), STRING("' "));
            string_t info = string_temp_concat(decorated_name, STRING("is defined with void type:\n\0"));
            log_print(info);
            log_pop_color();

            log_push_color(255, 255, 255);
            print_lines_of_code(state->scanner, 0, 0, name->token, 0);
            log_write(STR("\n"));
            log_pop_color();
            result = false;
        }

        if (!add_symbol_to_scope(state, key, &entry)) 
            result = false;

        name = name->list_next;
        type = type->list_next;
    }

    if (node->right->type != AST_SEPARATION) {
        return true;
    }

    if (node->left->child_count != node->right->child_count) {
        ast_node_t * next;
        u64 least_size = MIN(node->left->child_count, node->right->child_count);

        if (least_size >= node->left->child_count) {
            next = node->right->list_start;
            for (u64 i = 0; i < least_size; i++) {
                next = next->list_next;
            }

            log_error_token(STR("Trailing expression without its variable:"), state->scanner, next->token, 0);
        } else {
            next = node->left->list_start;
            for (u64 i = 0; i < least_size; i++) {
                next = next->list_next;
            }
            log_error_token(STR("This variable didn't have it's expression:"), state->scanner, next->token, 0);
        }


        return false;
    }

    return result;
}


*/

#define GREEN_COLOR 63, 255, 63

// --------------------------------------------------- FILE LOADING


string_t construct_source_name(string_t path, string_t name, allocator_t *alloc) {
    return string_swap(string_concat(path, string_concat(name, STRING(".slm"), alloc), alloc), SWAP_SLASH, (u8)HOST_SYSTEM_SLASH, alloc);
}

string_t construct_module_name(string_t path, string_t name, allocator_t *alloc) {
    return string_swap(string_concat(path, string_concat(name, STRING("/module.slm"), alloc), alloc), SWAP_SLASH, (u8)HOST_SYSTEM_SLASH, alloc);
}

b32 is_file_exists(string_t name) {
    FILE *file = fopen(string_to_c_string(name, get_temporary_allocator()), "rb");

    if (file == NULL) return false;

    fclose(file);
    return true;
}

b32 load_and_process_file(compiler_t *compiler, string_t filename) {
    source_file_t file = create_source_file(compiler, NULL);

    string_t source;

    if (!read_file_into_string(filename, default_allocator, &source)) {
        return false;
    }

    if (!scanner_open(&filename, &source, file.scanner)) {
        return false;
    }

    if (!hashmap_add(&compiler->files, filename, &file)) {
        log_error(STR("Couldn't add file to work with."));
        return false;
    }

    if (!parse_file(compiler, filename)) {
        return false;
    }

    return true;
}

b32 add_file_if_exists(compiler_t *compiler, string_t file) {
    log_write(STR("TRY: "));
    log_print(file);

    if (!is_file_exists(file)) { 
        log_push_color(ERROR_COLOR);
        log_write(STR(" FAIL\n"));
        log_pop_color();
        return false;
    }

    if (is_file_exists(file)) { 
        log_push_color(GREEN_COLOR);
        log_write(STR(" OK\n"));
        log_pop_color();

        load_and_process_file(compiler, string_copy(file, compiler->strings));
    }

    return true;
}

b32 find_and_add_file(compiler_t *compiler, analyzer_state_t *state, ast_node_t *node) {
    allocator_t *talloc = get_temporary_allocator();

    // ./<file>.slm
    // ./<file>/module.slm
    // /<compiler>/<file>.slm
    // /<compiler>/<file>/module.slm

    string_t directory, name = node->token.data.string;

    s64 slash = index_of_last_file_slash(state->scanner->filename);
    if (slash == -1) {
        directory = string_copy(STRING("./"), talloc);
    } else {
        directory = string_substring(state->scanner->filename, 0, slash + 1, talloc);
    }

    if (add_file_if_exists(compiler, construct_source_name(directory, name, talloc))) {
        node->analyzed = true;
        return true;
    }

    if (add_file_if_exists(compiler, construct_module_name(directory, name, talloc))) {
        node->analyzed = true;
        return true;
    }

    if (add_file_if_exists(compiler, construct_source_name(compiler->modules_path, name, talloc))) {
        node->analyzed = true;
        return true;
    }

    if (add_file_if_exists(compiler, construct_module_name(compiler->modules_path, name, talloc))) {
        node->analyzed = true;
        return true;
    }

    log_error_token(STR("Couldn't find corresponding file to this declaration."), state->scanner, node->token, 0);
    node->analyzed = true;
    return false;
}

// --------------------

b32 analyze_statement(compiler_t *compiler, analyzer_state_t *state, ast_node_t *node) {
    temp_reset();

    assert(!node->analyzed);
    b32 should_wait = false;

    switch (node->type) {
        case AST_UNNAMED_MODULE:
                return find_and_add_file(compiler, state, node);

        case AST_STRUCT_DEF: 
                return analyze_struct(state, node);

        case AST_UNARY_VAR_DEF:
                return analyze_unary_var_def(state, node, &should_wait);
                
        case AST_BIN_UNKN_DEF: 
                return analyze_unkn_def(state, node, &should_wait);

                             /*
        case AST_NAMED_MODULE:
            // ENTRY_NAMESPACE
            log_error_token(STR("Named modules dont work right now."), state->scanner, node->token, 0);
            node->analyzed = true;
            break;


        case AST_UNION_DEF:  return scan_union_def(state, node);
        case AST_ENUM_DEF:   return scan_enum_def(state, node);

        case AST_UNARY_PROTO_DEF: return scan_prototype_def(state, node);

        case AST_BIN_MULT_DEF:  return scan_bin_var_defs(state, node);
        case AST_TERN_MULT_DEF: return scan_tern_var_defs(state, node);
                             */

        default:
            log_error_token(STR("Wrong type of construct."), state->scanner, node->token, 0);
            break;
    }

    return false;
}

// priorities of compiling funcitons
//
// #run 
// other code
//

b32 analyze_and_compile(compiler_t *compiler) {
    assert(compiler != NULL);
    allocator_t *talloc = get_temporary_allocator();

    b32 result       = true;
    b32 not_finished = true;

    stack_t<hashmap_t<string_t, scope_entry_t>*> scopes = {};

    stack_push(&scopes, &compiler->scope);

    // we analyze and typecheck and prepare to compile this
    u64 curr_index = 0;
    while (not_finished) {
        not_finished = false;

        log_push_color(GREEN_COLOR);
        log_update_color();
        fprintf(stderr, "step: %zu\n", curr_index);
        log_pop_color();
        curr_index++;

        for (u64 i = 0; i < compiler->files.capacity; i++) {
            kv_pair_t<string_t, source_file_t> pair = compiler->files.entries[i];

            if (!pair.occupied) continue;
            if (pair.deleted)   continue;

            analyzer_state_t state = {};
            state.scanner  = pair.value.scanner;
            state.compiler = compiler;
            state.scopes   = &scopes;

            // we just need to try to compile, if we cant resolve something we add it, but just
            for (u64 j = 0; j < pair.value.parsed_roots.count; j++) {
                ast_node_t *node = *list_get(&pair.value.parsed_roots, j);

                if (node->analyzed) {
                    continue;
                } 

                not_finished = true;

                if (!analyze_statement(compiler, &state, node)) {
                    result = false;
                }
            }

            if (!result) {
                not_finished = false;
                break;
            }
        }
    }

    if (!result) {
        log_error(STR("Had an analyze error, wont go further."));
        return false;
    }

    log_info(STR("Everything is okay, compiling."));

    // compiling to IR aka bytecode

    stack_pop(&scopes);

    log_update_color();

    fprintf(stderr, "--------GLOBAL-SCOPE---------\n");

    for (u64 i = 0; i < compiler->scope.capacity; i++) {
        kv_pair_t<string_t, scope_entry_t> pair = compiler->scope.entries[i];

        if (!pair.occupied) continue;
        if (pair.deleted)   continue;

        log_update_color();
        fprintf(stderr, "%zu -> %s\n", i, string_to_c_string(pair.key, talloc));
    }

    fprintf(stderr, "-----------------------------\n");


    return result;
}

// first thing is to add all types into one place and resolve them all
// then we will do same to prototypes and variables
// after that main part. functions and variables
// first thing - we resolve all of type info, before starting going deep
// and then we run resolving for every func body.
//
//
// use: "core";
//
// release_build : b32 = false;
//
// entry : (args : []string) -> s32 = {
// }
//
// #run entry();
//

// then we will make dependency "graph" for compiling, but we need to start somewhere
// 
// for example we start with some definition and try to resolve it's type, 
// if we cant do that, we will go
