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
    hashmap_t<string_t, profile_block_t> blocks;

    stack_t<profiler_entry_t> start_time;
} profiler;

void profiler_begin(void) {
    assert(!profiler.running);

    if (profiler.blocks.entries != NULL) {
        hashmap_clear(&profiler.blocks);
    }

    profiler.start_time.index = 0;
    profiler.running = true;
}

void profiler_end(void) {
    assert(profiler.running);
    profiler.running = false;
}

void profiler_block_start(string_t name) {
    if (!profiler.running) return;

    if (!hashmap_contains(&profiler.blocks, name)) {
        profile_block_t b = {};
        hashmap_add(&profiler.blocks, name, &b);
    }

    profile_block_t *entry = hashmap_get(&profiler.blocks, name);
    entry->hits++;

    stack_push(&profiler.start_time, { debug_get_time(), entry });
}

void profiler_block_end(void) {
    if (!profiler.running) return;
    f64 end_time = debug_get_time();

    profiler_entry_t entry = stack_pop(&profiler.start_time);
    f64 time = end_time - entry.start_time;
    if (time > entry.block->max_time) entry.block->max_time = time;
    entry.block->time += time;
}

void visualize_profiler_state(void) {
    if (profiler.running) return;

    log_push_color(192, 255, 255);
    for (u64 i = 0; i < profiler.blocks.capacity; i++) {
        kv_pair_t<string_t, profile_block_t> pair = profiler.blocks.entries[i];
        
        if (!pair.occupied) continue;
        if (pair.deleted) continue;

        log_update_color();

        fprintf(stderr, "Block: %-40.*s --- %10llu Hits,     %.4lf total(sec),     %lf longest(sec),     T/H: %lf\n",
                (int)pair.key.size,
                (char*)pair.key.data,
                pair.value.hits,
                pair.value.time, 
                pair.value.max_time, 
                pair.value.time / (u64)pair.value.hits);
    }

    log_pop_color();
    log_update_color();
}
