
#include "sorter.h"
#include "logger.h"
#include "list.h"
#include "allocator.h"
#include "talloc.h"
#include "memctl.h"

COMP_PROC(std_comp_func) {
    assert(a != NULL);
    assert(b != NULL);
    return mem_compare((u8*)a, (u8*)b, size);
}

COMP_PROC(s32_comp_func) {
    assert(a != NULL);
    assert(b != NULL);
    assert(size == sizeof(s32));

    s32 va = *(s32*)a;
    s32 vb = *(s32*)b;

    if (va > vb)  return 1;
    if (va < vb)  return -1;
    return 0;
}

void sort_swap(void *data, u32 size, u64 a, u64 b) {
    u8* a_addr = (u8*)data + a * size;
    u8* b_addr = (u8*)data + b * size;

    u8* temp = (u8*)mem_alloc(get_temporary_allocator(), size);

    mem_copy(temp,   a_addr, size);
    mem_copy(a_addr, b_addr, size);
    mem_copy(b_addr, temp,   size);
}

// start - in elements, inclusive
// stop  - in elements, exclusive
void quick_sort(void *data, u32 element_size, u64 start, u64 stop, compare_func_t *comp = std_comp_func) {
    s64 size = stop - start;

    if (size < 2) {
        return;
    }

    if (size == 2) {
        if (comp(element_size, ((u8*)data) + start * element_size, ((u8*)data) + (stop - 1) * element_size) > 0)
            sort_swap(data, element_size, start, stop - 1);
        return;
    }     


    // u64 pivot_index = start + (stop - start) / 2; // center
    u64 pivot_index = stop-1; // center
    u8* pivot = (u8*)mem_alloc(get_temporary_allocator(), element_size);
    mem_copy(pivot, (u8*)data + pivot_index * element_size, element_size);

    u64 begin_index = start;
    u64 end_index = stop - 1;

    while (begin_index < end_index) {
        u8 *begin = ((u8*)data) + begin_index * element_size;
        u8 *end   = ((u8*)data) + end_index * element_size;

        if (comp(element_size, begin, pivot) < 0) {
            begin_index++;
            continue;
        }

        if (comp(element_size, end, pivot) > 0) {
            end_index--;
            continue;
        }

        if (comp(element_size, begin, end) == 0) {
            begin_index++;
            end_index--;
            continue;
        }

        sort_swap(data, element_size, begin_index, end_index);
        if (begin_index == pivot_index) {
            pivot_index = end_index;
        } else if (end_index == pivot_index) {
            pivot_index = begin_index;
        }
    }

    quick_sort(data, element_size, start, pivot_index, comp);
    quick_sort(data, element_size, pivot_index + 1, stop, comp);
}


b32 sort_test_case(s32 *data, s32 *expect, u64 size) {
    quick_sort(data, sizeof(s32), 0, size, s32_comp_func);
    
    return mem_compare((u8*)data, (u8*)expect, size * sizeof(s32)) == 0;
}

void sorter_tests() {
    log_push_color(255,255,255);
    {
        s32 data[]     = {5, 4, 3, 2, 1};
        s32 expected[] = {1, 2, 3, 4, 5};
        assert(sort_test_case(data, expected, sizeof(data) / sizeof(s32)));
    }
    {
        s32 data[]     = {1, 2, 3, 4, 5};
        s32 expected[] = {1, 2, 3, 4, 5};
        assert(sort_test_case(data, expected, sizeof(data) / sizeof(s32)));
    }
    {
        s32 data[]     = {3, 1, 4, 5, 2};
        s32 expected[] = {1, 2, 3, 4, 5};
        assert(sort_test_case(data, expected, sizeof(data) / sizeof(s32)));
    }
    {
        s32 data[]     = {2, 3, 2, 1, 3};
        s32 expected[] = {1, 2, 2, 3, 3};
        assert(sort_test_case(data, expected, sizeof(data) / sizeof(s32)));
    }
    {
        s32 data[]     = {42};
        s32 expected[] = {42};
        assert(sort_test_case(data, expected, sizeof(data) / sizeof(s32)));
    }
    {
        s32 data[]     = {};
        s32 expected[] = {};
        assert(sort_test_case(data, expected, sizeof(data) / sizeof(s32)));
    }
    {
        s32 data[]     = {-3, 1, -2, 0, -1};
        s32 expected[] = {-3, -2, -1, 0, 1};
        assert(sort_test_case(data, expected, sizeof(data) / sizeof(s32)));
    }
    {
        s32 data[]     = {7, 7, 7, 7, 7};
        s32 expected[] = {7, 7, 7, 7, 7};
        assert(sort_test_case(data, expected, sizeof(data) / sizeof(s32)));
    }
    {
        s32 data[]     = {9, 3, 7, 1, 8, 2, 5, 4, 6, 0};
        s32 expected[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        assert(sort_test_case(data, expected, sizeof(data) / sizeof(s32)));
    }
}
