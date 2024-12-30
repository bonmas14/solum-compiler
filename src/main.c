#include "stddefines.h"
#include "scanner.h"

int main(void) {
    scanner_state_t state = { 0 };

    if (!scan_file("test", &state)) {
        log_error("Main: couldn't open file and load it into memory.");
        return -1;
    }

    token_t token = advance_token(&state);

    while (token.type != TOKEN_EOF) {
        log_info_token("token", &state, token);
        token = advance_token(&state);
    }

    // fprintf(stdout, "%s", state.file.data);

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
