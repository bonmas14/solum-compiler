#ifndef SORTER_H
#define SORTER_H

#include "stddefines.h"

#define COMP_PROC(name) s32 name(u64 size, void *a, void *b)

typedef COMP_PROC(compare_func_t);

COMP_PROC(std_comp_func);
COMP_PROC(s32_comp_func);
COMP_PROC(string_comp_func);

template<typename DataType>
void sort_array(DataType *data, u64 size, compare_func_t *func) {
    quick_sort(data, sizeof(DataType), 0, size, func);
}

void quick_sort(void *data, u32 element_size, u64 start, u64 stop, compare_func_t *func);

void sorter_tests();

#endif  // SORTER_H
