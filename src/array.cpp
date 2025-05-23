#include "array.h"
#include "allocator.h"
#include "talloc.h"

void array_tests(void) {
#ifdef DEBUG
    {
        array_t<u64> values = {};
        allocator_t *talloc = get_temporary_allocator();
        array_create(&values, 128, *talloc);

        for (u64 i = 0; i < 128; i++) {
            array_add(&values, i);
        }

        for (u64 i = 0; i < 128; i++) {
            assert(*array_get(&values, i) == i);
        }

        array_delete(&values);
    }
    {
        array_t<u64> values = {};
        allocator_t *talloc = get_temporary_allocator();
        array_create(&values, 128, *talloc);

        for (u64 i = 0; i < 384; i++) {
            array_add(&values, i);
        }

        for (u64 i = 0; i < 384; i++) {
            assert(*array_get(&values, i) == i);
        }

        array_delete(&values);
    }
#endif//
}
