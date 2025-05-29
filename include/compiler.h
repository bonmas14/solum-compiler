#ifndef COMPILER_H
#define COMPILER_H

#include "stddefines.h"
#include "allocator.h"
#include "list.h"
#include "array.h"
#include "hashmap.h"
#include "logger.h"

#include "analyzer.h"

#define STRING_ALLOCATOR_INIT_SIZE 4096
#define INIT_NODES_SIZE 2048

struct scanner_t;
struct ast_node_t;

struct source_file_t {
    scanner_t          *scanner;
    list_t<ast_node_t*> parsed_roots;
};

struct compiler_t {
    b32 valid;

    allocator_t *strings;
    allocator_t *nodes;

    string_t     modules_path;

    array_t<hashmap_t<string_t, scope_entry_t>> scopes;
    hashmap_t<string_t, source_file_t> files;
};

source_file_t create_source_file(compiler_t *compiler, allocator_t *alloc);
compiler_t create_compiler_instance(allocator_t *alloc);
void compile(string_t filename);

#endif
