#include "compiler.h"
#include "analyzer.h"
#include "parser.h"
#include "ir.h"

#include "allocator.h"
#include "talloc.h"
#include "strings.h"

ir_function_t compile_function(compiler_t *compiler, scope_entry_t entry) {
    UNUSED(compiler);
    UNUSED(entry);

    return {};
}

// void evaluate_expression(compiler_t *compiler, );

ir_t compile_program(compiler_t *compiler) {
    UNUSED(compiler);

    assert(compiler != NULL);

    ir_t ir = {};

    log_info(STRING("----------"));
    for (u64 i = 0; i < compiler->scope.capacity; i++) {
        kv_pair_t<string_t, scope_entry_t> pair = compiler->scope.entries[i];

        if (!pair.occupied) continue;
        if (pair.deleted)   continue;

        string_t t = string_temp_concat(STRING("Symbol: "), pair.value.node->token.data.string);
        log_info(t);
    }

    return ir;
}

