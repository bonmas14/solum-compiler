#ifndef BACKEND_H
#define BACKEND_H

#include "compiler.h"
#include "scanner.h"
#include "parser.h"
#include "analyzer.h"

codegen_t *codegen_create(allocator_t *allocator);
void generate_code(compiler_t *compiler);

#endif // BACKEND_H
