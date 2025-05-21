#ifndef SORTER_H
#define SORTER_H

#include "stddefines.h"

#define COMP_PROC(name) s32 name(u64 size, void *a, void *b)

typedef COMP_PROC(compare_func_t);

COMP_PROC(std_comp_func);

void quick_sort(void *data, u32 element_size, u64 start, u64 stop, compare_func_t *func);

void sorter_tests();

#endif  // SORTER_H
