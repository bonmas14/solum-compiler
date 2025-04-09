#include "arena.h"

arena_t *arena_create(u64 init_size) {
    arena_t * arena = (arena_t*)ALLOC(init_size + sizeof(arena_t));

    if (arena == NULL) {
        log_error(STRING("Arena't create arena."));
        return NULL;
    }

    arena->size  = init_size;
    arena->start = arena + 1;

    return arena;
}

void arena_delete(arena_t *cont) {
    assert(cont != NULL);

    if (cont->next != NULL)
        arena_delete(cont->next);

	FREE(cont);
}

ALLOCATOR_PROC(arena_allocator_proc) {
    UNUSED(p);

    switch (message) {
        case ALLOC_ALLOCATE:
            assert(data != NULL);
            return arena_allocate((arena_t*)data, size);

        case ALLOC_REALLOCATE:
            log_warning(STRING("arena allocator doesn't reallocate."));
            return NULL;

        case ALLOC_DEALLOCATE:
            log_warning(STRING("arena allocator doesn't deallocate allocated memory the whole arena"));
            return NULL;

        default:
            log_error(STRING("unexpected allocator message."));
            assert(false);
            break;
    }
}


allocator_t create_arena_allocator(u64 init_size) {
    allocator_t allocator = {};

    allocator.proc = arena_allocator_proc;
    allocator.data = arena_create(init_size);

    return allocator;
}

void delete_arena_allocator(allocator_t allocator) {
    assert(allocator.data != NULL);

    arena_delete((arena_t*)allocator.data);
}

// bad recursive design, can fail @fix
void * arena_allocate(arena_t *cont, u64 size) {
    assert(cont != NULL);
    check_value(size > 0); 

    if (size <= (cont->size - cont->index)) {
        void *current = (u8*)cont->start + cont->index;
        cont->index += size;

        ZERO_CHECK(current, size);
        return current;
    }

    if (cont->next != NULL) {
        return arena_allocate(cont->next, size);
    } 

    cont->next = (arena_t*)arena_create(cont->size * 2);

    check_value(cont->next != NULL);

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
    
    log_info(STRING("ARENA: OK"));
}
