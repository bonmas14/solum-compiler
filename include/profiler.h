#include "stddefines.h"
#include "strings.h"

#define profiler_func_start() profiler_block_start(STRING(__func__));
#define profiler_func_end()   profiler_block_end()

void profiler_begin(void);
void profiler_end(void);
void profiler_block_start(string_t name);
void profiler_block_end(void);
void visualize_profiler_state(void);
