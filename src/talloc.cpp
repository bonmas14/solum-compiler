#include "talloc.h"
#include "strings.h"

struct {
    u64 position;
    u8  data[TEMP_MEM_SIZE];
} __talloc_data;

allocator_t __talloc;

ALLOCATOR_PROC(temporary_allocator_proc) {
    UNUSED(p);
    UNUSED(data);

    switch (message) {
        case ALLOC_ALLOCATE:
            assert(data != NULL);
            return temp_allocate(size);
        case ALLOC_DELETE:
            temp_reset();
            break;

        case ALLOC_REALLOCATE:
#ifdef VERBOSE
            log_warning(STRING("Temp allocator is in static memory... we cant reallocate here"));
#endif
            return NULL;

        case ALLOC_DEALLOCATE:
#ifdef VERBOSE
            log_warning(STRING("Temp allocator is in static memory... we cant deallocate here"));
#endif
            return NULL;

        default:
            log_error(STRING("unexpected allocator message."));
            assert(false);
            break;
    }

    return NULL;
}

// @todo : for all allocators
b32 is_inside_of_temp_memory(void *p) {
    return (p >= __talloc_data.data) && (p < (__talloc_data.data + TEMP_MEM_SIZE));
}

allocator_t *get_temporary_allocator(void) {
    if (__talloc.data == NULL) {
        __talloc.proc = temporary_allocator_proc; 
        __talloc.data = &__talloc_data;
    }
    return &__talloc;
}

void *temp_allocate(u64 size) {
    assert(__talloc_data.data != NULL);
    assert(TEMP_MEM_SIZE > __talloc_data.position);

    if (size > TEMP_MEM_SIZE) {
        log_error(STRING("Trying to allocate more memory than exists in __talloc_data."));
        assert(false);
        return NULL;
    }
    
    u64 new_position = __talloc_data.position  + size;
    u8 *address      = __talloc_data.data + __talloc_data.position;

    if (new_position < TEMP_MEM_SIZE) {
        mem_set(address, 0, size);
        __talloc_data.position = new_position;
        return address;
    }

    log_warning(STRING("__talloc_data wrapped."));

    if (new_position > TEMP_MEM_SIZE) {
        __talloc_data.position = size;
    } else {
        __talloc_data.position = 0;
    }
    
    return address;
}

void temp_reset(void) {
    assert(__talloc_data.data != NULL);
    assert(TEMP_MEM_SIZE > 0);
    assert(TEMP_MEM_SIZE > __talloc_data.position);

    __talloc_data.position = 0;
}

void temp_tests(void) {
#ifdef DEBUG
    temp_reset();
    const char* text = "eating burger wit no honey mustard";
    u8* str = STR(text);
    u64 len = c_string_length((char*)str);

    u8* data1 = (u8*)temp_allocate(len);
    mem_copy(data1, STR("eating burger wit no honey mustard"), len);
    assert(mem_compare(data1, str, len) == 0);
    temp_reset();

    u8* data2 = (u8*)temp_allocate(len);
    assert(data1 == data2);
    temp_reset();

    u64 *arr[1024];
    for (u64 i = 0; i < 1024; i++) {
        arr[i] = (u64*)temp_allocate(8);
        *(arr[i]) = i;
    }

    for (u64 i = 0; i < 1024; i++) {
        assert(*(arr[i]) == i);
    }
#endif
}
