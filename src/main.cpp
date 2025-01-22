#include "stddefines.h"
#include "scanner.h"
#include "parser.h"
#include "backend.h"

#ifdef NDEBUG 
#define log_info_token(a, b, c, d)
#endif


void print_node(scanner_state_t *scanner, parser_state_t *parser, ast_node_t* node, u32 depth) {
    log_info_token(scanner, node->token, depth * LEFT_PAD_STANDART_OFFSET);

    ast_node_t* child;
    if (node->type == AST_UNARY) {
        child = (ast_node_t*)list_get(&parser->nodes, node->left_index);
        print_node(scanner, parser, child, depth + 1);
    } else if (node->type == AST_BIN) {
        child = (ast_node_t*)list_get(&parser->nodes, node->left_index);
        print_node(scanner, parser, child, depth + 1);

        child = (ast_node_t*)list_get(&parser->nodes, node->right_index);
        print_node(scanner, parser, child, depth + 1);
    }
}

int main(void) {
    scanner_state_t scanner = {};

    if (!scan_file(STR("test"), &scanner)) {
        log_error(STR("Main: couldn't open file and load it into memory."), 0);
        return -1;
    }

    parser_state_t parser = {};

    parse(&scanner, &parser);

    for (u64 i = 0; i < (parser.root_indices.count - 1); i++) {
        ast_node_t *root = (ast_node_t*)list_get(&parser.nodes, *(u64*)list_get(&parser.root_indices, i));

        print_node(&scanner, &parser, root, 0);
    }

    generate_code();
    /* 
    // create context, module and builder
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef  module  = LLVMModuleCreateWithNameInContext("hello", context);
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);

    // types
    LLVMTypeRef int8t      = LLVMInt8TypeInContext(context);
    LLVMTypeRef int8t_ptr  = LLVMPointerType(int8t, 0);
    LLVMTypeRef int32t     = LLVMInt32TypeInContext(context);

    // puts function
    LLVMTypeRef puts_args[] = { int8t_ptr };

    LLVMTypeRef  puts_ft = LLVMFunctionType(int32t, puts_args, 1, false);
    LLVMValueRef puts_f  = LLVMAddFunction(module, "puts", puts_ft);
    // end

    // main function
    LLVMTypeRef  main_ft = LLVMFunctionType(int32t, NULL, 0, false);
    LLVMValueRef main_f  = LLVMAddFunction(module, "main", main_ft);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, main_f, "entry");
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef puts_f_args[] = {
        LLVMBuildPointerCast(builder, LLVMBuildGlobalString(builder, "Hello, World!", "hello"), int8t_ptr, "0")
    };

    LLVMBuildCall2(builder, puts_ft, puts_f, puts_f_args, 1, "i");
    LLVMBuildRet(builder, LLVMConstInt(int32t, 0, false));
    // end

    // LLVMDumpModule(module); // dump module to STDOUT
    LLVMPrintModuleToFile(module, "hello.ll", NULL);

    // clean memory
    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(module);
    LLVMContextDispose(context);
*/
    return 0;
}
