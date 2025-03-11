#include "arena.h"

arena_t * arena_create(u64 init_size) {
    arena_t * data = (arena_t*)ALLOC(init_size + sizeof(arena_t));

    if (data == NULL) {
        log_error(STR("Arena, couldn't create arena."), 0);
        return NULL;
    }

    data->size  = init_size;
    data->start = data + 1;

    return data;
}

void arena_delete(arena_t *cont) {
    assert(cont != NULL);

    if (cont->next != NULL)
        arena_delete(cont->next);

    FREE(cont);
}

// bad recursive design, can fail @fix
void * arena_allocate(arena_t *cont, u64 size) {
    assert(cont != NULL);
    check(size > 0); 

    if (size <= (cont->size - cont->index)) {
        void *current = (u8*)cont->start + cont->index;
        cont->index += size;

        ZERO_CHECK(current, size);
        return current;
    }

    if (cont->next != NULL) {
        return arena_allocate(cont->next, size);
    } 

    cont->next = arena_create(cont->size * 2);

    check(cont->next != NULL);

    return arena_allocate(cont->next, size);
}

void arena_tests(void) {
    arena_t *arena = arena_create(1024);
    void * result;

    assert(arena != NULL);

    assert(arena->size  == 1024);
    assert(arena->index == 0);
    assert(arena->next  == NULL);

    result = arena_allocate(arena, 1024);
    assert(result != NULL);

    assert(arena->next  == NULL);
    assert(arena->index == 1024);

    result = arena_allocate(arena, 1024);
    assert(result != NULL);

    assert(arena->next != NULL);
    assert(arena->next->size  == (arena->size * 2));
    assert(arena->next->index == 1024);

    result = arena_allocate(arena, 1024);
    assert(result != NULL);

    assert(arena->next != NULL);
    assert(arena->next->next  == NULL);
    assert(arena->next->index == 2048);

    arena_delete(arena);
    

    arena_t *arena2 = arena_create(10);

    u8* arr = (u8*)arena_allocate(arena2, 10);

    memcpy(arr, "hello 10", 10);

    log_info(STR("arena: OK"), 0);
}
