#include "area_alloc.h" 

void area_tests(void) {
    area_t<u64> list = {};

    area_create(&list, 100);

    assert(list.data != 0);
    assert(list.count == 0);
    assert(list.raw_size == 100);
    assert(list.grow_size == 200);

    u64 data1 = 404;
    u64 data2 = 804;
    u64 data3 = 104;

    assert(area_add(&list, &data1));
    assert(area_add(&list, &data2));
    assert(area_add(&list, &data3));

    assert(data1 == *area_get(&list, 0));
    assert(data2 == *area_get(&list, 1));
    assert(data3 == *area_get(&list, 2));

    u64 index = 0;
    assert(area_allocate(&list, 10000, &index));

    assert(index == 3);

    assert(list.count == 10003);
    area_delete(&list);
    log_info(STR("Area: OK"), 0);
}
