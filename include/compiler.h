#ifndef COMPILER_H
#define COMPILER_H

#include "stddefines.h"
#include "allocator.h"
#include "list.h"
#include "hashmap.h"
#include "logger.h"

#define STRING_ALLOCATOR_INIT_SIZE 4096
#define INIT_NODES_SIZE 1024

#define MAX_COUNT_OF_PARAMS 16
#define MAX_COUNT_OF_RETS 1 

struct scanner_t;
struct analyzer_t;
struct codegen_t;
struct ast_node_t;

struct file_t {
    b32 loaded;
    b32 parsed;
    b32 analyzed;
    
    scanner_t *scanner;
    list_t<ast_node_t*> parsed_roots;
};

struct compiler_t {
    b32 is_valid;

    allocator_t *strings;
    allocator_t *nodes;

    scanner_t *current_scanner;

    analyzer_t *analyzer;
    codegen_t  *codegen;

    hashmap_t<string_t, file_t> files;
};

compiler_t create_compiler_instance(void);

#endif
