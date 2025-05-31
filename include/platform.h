#include "stddefines.h"

void debug_init(void);
f64  debug_get_time(void);
void debug_break(void);

string_t platform_get_current_directory(allocator_t *alloc);
b32      platform_file_exists(string_t name);
b32      platform_write_file(string_t name, string_t content);
b32      platform_read_file_into_string(string_t filename, allocator_t *alloc, string_t *output);
