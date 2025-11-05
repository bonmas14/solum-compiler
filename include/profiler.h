#include "stddefines.h"
#include "strings.h"

struct Profile_Block {
    f64 start_time, stop_time;
    string_t name;

    // every block has its own linked list of childs
    // and it can be a part of list too...

    Profile_Block *first_child;
    Profile_Block *next_in_list;
};

struct Profile_Data {
    allocator_t strings;
    allocator_t blocks;
    Profile_Block *block;
};

#define profiler_func_start() profiler_block_start(STRING(__func__));
#define profiler_func_end()   profiler_block_end()


#define profiler_push_func()       profiler_push_impl(STRING(__func__))
#define profiler_pop()             profiler_pop_impl()
#define profiler_begin(name)       profiler_begin_impl(name)
#define profiler_end()             profiler_end_impl()
#define profiler_data_delete(data) profiler_data_delete_impl(data)
#define profiler_block_start(name) profiler_push_impl(name)
#define profiler_block_end()       profiler_pop_impl()

void profiler_begin_impl(string_t name);
void visualize_profiler_state(Profile_Block *block, u64 depth);

void         profiler_begin_impl(string_t name);
Profile_Data profiler_end_impl(void);
void         profiler_data_delete_impl(Profile_Data *data);
void         profiler_push_impl(string_t name);
void         profiler_pop_impl(void);
