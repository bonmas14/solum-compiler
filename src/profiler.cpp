#include "stddefines.h"
#include "logger.h"
#include "platform.h"
#include "allocator.h"
#include "talloc.h"
#include "arena.h"
#include "strings.h"

#include "hashmap.h"
#include "stack.h"

struct profile_block_t {
    u64 depth;
    u64 hits;
    f64 time;
    f64 max_time;
};

struct profiler_entry_t {
    f64 start_time;
    profile_block_t *block;
};

struct {
    b32 running;
    u64 current_depth;
    hashmap_t<string_t, profile_block_t> blocks;
    stack_t<profiler_entry_t> start_time;
} profiler;

void profiler_begin(void) {
    assert(!profiler.running);

    if (profiler.blocks.entries != NULL) {
        hashmap_delete(&profiler.blocks);
        profiler.blocks = {};
    }

    if (profiler.start_time.data != NULL) {
        profiler.start_time.index = 0;
    } else {
        stack_create(&profiler.start_time, 1024);
    }

    hashmap_create(&profiler.blocks, 1024, NULL, NULL);
    profiler.running = true;
}

void profiler_end(void) {
    assert(profiler.running);
    profiler.running = false;
}

void profiler_block_start(string_t name) {
    if (!profiler.running) return;
    profiler.current_depth++;

    if (!hashmap_contains(&profiler.blocks, name)) {
        profile_block_t b = {};
        hashmap_add(&profiler.blocks, name, &b);
    }

    profile_block_t *entry = hashmap_get(&profiler.blocks, name);
    entry->hits++;
    entry->depth = profiler.current_depth;

    stack_push(&profiler.start_time, { debug_get_time(), entry });
}

void profiler_block_end(void) {
    if (!profiler.running) return;
    profiler.current_depth--;
    f64 end_time = debug_get_time();

    profiler_entry_t entry = stack_pop(&profiler.start_time);
    f64 time = end_time - entry.start_time;
    if (time > entry.block->max_time) entry.block->max_time = time;
    entry.block->time += time;
}

void visualize_profiler_state(void) {
    if (profiler.running) return;

    log_push_color(192, 255, 255);

    u64 amount = profiler.blocks.load;
    u64 depth = 0;

    while (amount > 0) {
        for (u64 i = 0; i < profiler.blocks.capacity; i++) {
            kv_pair_t<string_t, profile_block_t> pair = profiler.blocks.entries[i];

            if (!pair.occupied) continue;
            if (pair.deleted) continue;

            log_update_color();

            if (pair.value.depth == depth) {
                fprintf(stderr, "Block:");

                for (u64 j = 0; j < depth; j++) {
                    fprintf(stderr, " ");
                }

                s32 offset = 40 - depth;
                if (offset < 0) offset = 0;

                
                fprintf(stderr, "%-*.*s ",
                        (int)offset,
                        (int)pair.key.size,
                        (char*)pair.key.data);
 

                fprintf(stderr, " %10llu Hits, %.4lf total(sec), %.4lf longest(sec), T/H: %.4lf\n",
                        pair.value.hits,
                        pair.value.time, 
                        pair.value.max_time, 
                        pair.value.time / (u64)pair.value.hits);

                amount--;
            }
        }

        depth += 1;
    }

    log_pop_color();
    log_update_color();
}
