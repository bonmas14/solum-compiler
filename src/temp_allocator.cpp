#include "temp_allocator.h"
#include <stdlib.h>
#include <string.h> 
#include <memory.h> 

struct {
    u64 size;
    u64 position;
    void *data;
} temp_allocator;

void temp_init(u64 size) {
    assert(size != 0);

    temp_allocator.data = calloc(1, size);

    if (temp_allocator.data != NULL) {
        temp_allocator.size = size;
    } else {
        assert(false);
    }
}

void temp_deinit(void) {
    if (temp_allocator.data != NULL) {
        free(temp_allocator.data);
    }

    temp_allocator = {};
}

void *temp_allocate(u64 size) {
    assert(temp_allocator.data != NULL);
    assert(temp_allocator.size > 0);
    assert(temp_allocator.size > temp_allocator.position);

    if (size > temp_allocator.size) {
        log_error(STR("Trying to allocate more memory than exists in temp_allocator."), 0);
        assert(false);
        return NULL;
    }
    
    u64 new_position = temp_allocator.position  + size;
    void *address    = (u8*)temp_allocator.data + temp_allocator.position;

    if (new_position < temp_allocator.size) {
        memset(address, 0, size);
        temp_allocator.position = new_position;
        return address;
    }

    log_warning(STR("temp_allocator wrapped."), 0);

    if (new_position > temp_allocator.size) {
        temp_allocator.position = size;
    } else {
        temp_allocator.position = 0;
    }
    
    return address;
}

void temp_reset(void) {
    assert(temp_allocator.data != NULL);
    assert(temp_allocator.size > 0);
    assert(temp_allocator.size > temp_allocator.position);

    temp_allocator.position = 0;
}

void temp_tests(void) {
    temp_init(64); 
    
    const char* text = "eating burger wit no honey mustard";
    u8* str = STR(text);
    u64 len = strlen((char*)str);

    u8* data1 = (u8*)temp_allocate(len);
    memcpy(data1, "eating burger wit no honey mustard", len);
    assert(memcmp(data1, str, len) == 0);
    temp_reset();

    u8* data2 = (u8*)temp_allocate(len);
    assert(data1 == data2);
    temp_deinit();

    log_info(STR("Temp_Allocator: OK"), 0);
}
