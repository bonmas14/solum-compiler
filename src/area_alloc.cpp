#include "area_alloc.h" 

void list_tests(void) {
    list_t<u64> list = {};

    list_create(&list, 100);

    assert(list.data != 0);
    assert(list.count == 0);
    assert(list.raw_size == 100);
    assert(list.grow_size == 200);

    u64 data1 = 404;
    u64 data2 = 804;
    u64 data3 = 104;

    list_add(&list, &data1);
    list_add(&list, &data2);
    list_add(&list, &data3);

    assert(data1 == *list_get(&list, 0));
    assert(data2 == *list_get(&list, 1));
    assert(data3 == *list_get(&list, 2));

    u64 index = 0;
    list_allocate(&list, 100, &index);

    u64 test[100];
    memset(test, 0xab, 100);
    list_fill(&list, (u64*)test, 100, index);

    assert(*list_get(&list, 100) == test[100 - 3]);
    assert(*list_get(&list, 10) == test[10 - 3]);

    assert(index == 3);

    assert(list.count == 103);
    list_delete(&list);
    log_info(STR("Area: OK"), 0);
}
