#include "compiler.h"
#include "analyzer.h"
#include "ir.h"

ir_function_t compile_function(compiler_t *compiler, scope_entry_t entry) {
    // 
}

ir_t compile_program(compiler_t *compiler) {
    assert(compiler       != NULL);

    ir_t ir = {};

    for (u64 i = 0; i < compiler->scope.capacity; i++) {
        kv_pair_t<string_t, scope_entry_t> pair = compiler->scope.entries[i];

        if (!pair.occupied) continue;
        if (pair.deleted)   continue;

    }

    return ir;
}

