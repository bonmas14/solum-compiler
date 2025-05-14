#include "stddefines.h"


void debug_init(void);
f64 debug_get_time(void);
void debug_break(void);

b32 platform_file_exists(string_t name);
b32 platform_read_file_into_string(string_t filename, allocator_t *alloc, string_t *output);
