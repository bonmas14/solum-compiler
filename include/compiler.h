#ifndef COMPILER_H
#define COMPILER_H

#include "stddefines.h"
#include "allocator.h"
#include "list.h"
#include "hashmap.h"
#include "logger.h"

#include "analyzer.h"

#define STRING_ALLOCATOR_INIT_SIZE 4096
#define INIT_NODES_SIZE 1024

struct scanner_t;
struct analyzer_t;
struct codegen_t;
struct ast_node_t;

struct source_file_t {
    b32 loaded;
    b32 parsed;
    b32 analyzed;
    
    scanner_t *scanner;
    list_t<ast_node_t*> parsed_roots;
    hashmap_t<string_t, scope_entry_t> scope;
};

struct compiler_t {
    b32 is_valid;

    allocator_t *strings;
    allocator_t *nodes;

    analyzer_t *analyzer;
    codegen_t  *codegen;

    hashmap_t<string_t, source_file_t> files;
};

source_file_t create_source_file(compiler_t *compiler, allocator_t *alloc);
compiler_t create_compiler_instance(allocator_t *alloc);

#endif
