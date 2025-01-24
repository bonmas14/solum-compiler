#include "list.h" 

b32 list_create(list_t *container, u64 init_size, u64 element_size) {
    if (init_size == 0) {
        log_warning(STR("List: init size was 0. defaulting to 1."), 0);
        init_size = 1;
    }

    if (element_size == 0) {
        log_warning(STR("List: element size was 0. defaulting to 1."), 0);
        element_size = 1;
    }

    container->count        = 0;
    container->element_size = element_size;
    container->raw_size     = init_size;
    container->grow_size    = init_size * 2;
    container->data         = ALLOC(init_size * element_size);

    if (container->data == NULL) {
        log_error(STR("List: Couldn't create list."), 0);
        return false; 
    }

    return true;
}

b32 list_delete(list_t *list) {
    if (list == NULL) {
        log_error(STR("List: Reference to list wasn't valid."), 0);
        return false;
    }

    if (list->data == NULL) {
        log_error(STR("List: List was already deleted."), 0);
        return false;
    }

    FREE(list->data); 
    list->data = NULL;

    return true;
}

b32 list_grow(list_t *list) {
    assert(list != 0, "list was null");
    void *data = REALLOC(list->data, list->grow_size * list->element_size);

    if (data == NULL) {
        log_error(STR("List: Couldn't grow list."), 0);
        return false;
    }

    list->data = (u8*)data;

    u64 size   = (list->grow_size - list->raw_size) * list->element_size;
    u64 offset = list->count  *list->element_size;

    // windows copiler need to know what is a type of void
    (void)memset((u8*)list->data + offset, 0, size);
    
    list->raw_size  = list->grow_size;
    list->grow_size = list->raw_size * 2;
    return true;
}

b32 list_grow_fit(list_t *list, u64 fit_elements) {
    assert(fit_elements > 0, "trying to fit 0 elements, it will be always true.");

    if ((list->count + fit_elements - 1) < list->raw_size) {
        return true;
    } else {
        while ((list->count + fit_elements) >= list->raw_size) {
            if (!list_grow(list)) {
                return false;
            }
        }
    }

    return true;
}

// @todo with templates we can just delete this
// and rawdog it on data pointer in list
b32 list_add(list_t *list, void *data) {
    assert(list != 0, "list was null");
    assert(data != 0, "data pointer was null");

    u64 index = 0;
    if (!list_allocate(list, 1, &index)) {
        return false;
    }

    u8 *dest = (u8*)list->data + list->element_size * index;
    memcpy(dest, data, list->element_size);

    return true;
}

b32 list_allocate(list_t *list, u64 elements_amount, u64 *start_index) {
    assert(list != 0, "list was null");
    assert(elements_amount > 0, "allocation amount is zero");

    u64 new_count = list->count + elements_amount;

    if (!list_grow_fit(list, elements_amount)) {
        return false;
    }
    
    *start_index = list->count;
    list->count = new_count;

    return true;
}

void *list_get(list_t *list, u64 index) {
    assert(list != 0, "list was null");
    assert(index < list->count, "index was bigger that count of elements");

    if (index >= list->count) {
        log_error(STR("List: Index is greater than count of elements."), 0);
        return NULL;
    }

    u8* result_data = (u8*)list->data;

    return &result_data[list->element_size * index]; 
}


