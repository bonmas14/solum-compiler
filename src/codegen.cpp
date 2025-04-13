#include "backend.h"
#include "parser.h"
#include "scanner.h"
#include "analyzer.h"

#include <stdlib.h>

struct codegen_t {
    FILE     *file;
    string_t *current_identifier_name;
};

codegen_t *codegen_create(allocator_t *allocator) {
    codegen_t *result = (codegen_t*)mem_alloc(allocator, sizeof(codegen_t));

    assert(result != NULL);

    return result;
}

void generate_code(compiler_t *compiler) {
}
