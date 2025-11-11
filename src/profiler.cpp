#include "stddefines.h"
#include "logger.h"
#include "platform.h"
#include "allocator.h"
#include "talloc.h"
#include "arena.h"
#include "strings.h"
#include "profiler.h"

#include "hashmap.h"
#include "stack.h"

struct Profiler_Context {
    Profile_Data data;
    list_t<Profile_Block*> block_stack;
    b32 running;
};


Profiler_Context profiler_ctx = {};

void profiler_data_delete_impl(Profile_Data *data) {
    if (data->strings.data) {
        mem_delete(&data->strings);
        data->strings = {};
    }

    if (data->blocks.data) {
        mem_delete(&data->blocks);
        data->blocks = {};
    }
}

void profiler_begin_impl(string_t name) {
    profiler_ctx.data = {};

    profiler_ctx.data.blocks  = create_arena_allocator(KB(64));
    profiler_ctx.data.strings = create_arena_allocator(KB(64));

    Profile_Block *block = (Profile_Block*)mem_alloc(&profiler_ctx.data.blocks, sizeof(Profile_Block));

    profiler_ctx.data.block = block;
    block->name = string_copy(name, &profiler_ctx.data.strings);
    block->start_time = debug_get_time();

    list_add(&profiler_ctx.block_stack, &block);
    profiler_ctx.running = true;
}

void add_and_push_block(Profile_Block block) {
    Profile_Block *top = *list_get(&profiler_ctx.block_stack, profiler_ctx.block_stack.count - 1);

    Profile_Block *next = (Profile_Block*)mem_alloc(&profiler_ctx.data.blocks, sizeof(Profile_Block));
    *next = block;

    if (top->first_child) {
        Profile_Block *child = top->first_child;

        while (child->next_in_list) {
            child = child->next_in_list;
        }

        child->next_in_list = next;

    } else {
        top->first_child = next;
    }

    list_add(&profiler_ctx.block_stack, &next);
}

void profiler_push_impl(string_t name) {
    if (!profiler_ctx.running) return;
    Profile_Block block = {};
    block.start_time = debug_get_time();

    block.name = string_copy(name, &profiler_ctx.data.strings);

    add_and_push_block(block);
}

void profiler_pop_impl(string_t name) {
    if (!profiler_ctx.running) return;
    Profile_Block *top = *list_get(&profiler_ctx.block_stack, profiler_ctx.block_stack.count - 1);
    profiler_ctx.block_stack.count--;

    top->stop_time = debug_get_time();

    if (0 != string_compare(top->name, name)) {
        assert(false);
    }
}

Profile_Data profiler_end_impl(void) {
    if (!profiler_ctx.running) return {};
    Profile_Block *top = *list_get(&profiler_ctx.block_stack, profiler_ctx.block_stack.count - 1);
    profiler_ctx.block_stack.count--;

    assert(profiler_ctx.block_stack.count == 0);
    assert(top == profiler_ctx.data.block);
    profiler_ctx.running = false;
    top->stop_time = debug_get_time();
    return profiler_ctx.data;
}

void visualize_profiler_state(Profile_Block *block, u64 depth) {
    if (!compiler_config.verbose) return;

    while (block) {
        fprintf(stdout, "{\"block\":\"%.*s\",\"ns_start\":%.0f, \"ns_stop\": %.0f, \"childs\":", (int)block->name.size, block->name.data, block->start_time * 1000000000.0, block->stop_time * 1000000000.0);

        if (block->first_child) {
            fprintf(stdout, "[");
            visualize_profiler_state(block->first_child, depth + 1);
            fprintf(stdout, "]");
        } else {
            fprintf(stdout, "null");
        }

        fprintf(stdout, "}");
        block = block->next_in_list;
        if (block) {
            fprintf(stdout, ",");
        }
    }
}
