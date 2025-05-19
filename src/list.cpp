#include "list.h" 
#include "memctl.h" 

void list_tests(void) {
    list_t<u64> list = {};

    u64 data1 = 404;
    u64 data2 = 804;
    u64 data3 = 104;

    list += data1;
    list += data2;
    list += data3;

    assert(data1 == *list_get(&list, 0));
    assert(data2 == *list_get(&list, 1));
    assert(data3 == *list_get(&list, 2));

    // adding multiple elements + there will be allocation inside
    u64 index = 0;
    list_allocate(&list, 100, &index);

    // checking if allocating didnt erase all of the values
    assert(data1 == list[0]);
    assert(data2 == list[1]);
    assert(data3 == list[2]);

    u64 test[100];
    mem_set((u8*)test, 0xab, sizeof(u64) * 100);
    list_fill(&list, (u64*)test, 100, index);

    assert(*list_get(&list, 100) == test[100 - 3]);
    assert(*list_get(&list, 10) == test[10 - 3]);

    assert(index == 3);

    assert(list.count == 103);
    list_delete(&list);
}
