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

b32 list_add(list_t *list, void *data) {
    if (list->count >= list->raw_size) {
        if (!list_grow(list)) {

            // we dont need that log because it can only happen when using list_grow
            // log_error(STR("List: Couldn't add element to a list."), 0);
            
            return false;
        }
    }
    
    u8 *arr = (u8*)list->data;

    for (u64 i = 0; i < list->element_size; i++) {
        u8 *casted = (u8*)data;
        arr[list->element_size  *list->count + i] = casted[i];
    }

    list->count++;

    return true;
}

void *list_get(list_t *list, u64 index) {
    if (index >= list->count) {
        log_error(STR("List: Index is greater than count of elements."), 0);
        return NULL;
    }

    // windows needs explicit type
    u8* result_data = (u8*)list->data;

    return &result_data[list->element_size * index]; 
}

/*
void list_insert_at(list_t *list, u64 index, void *data) {
    log_error(STR("List: inserting is not supported as I dont need that."), 0);
}

void list_remove_at(list_t *list, u64 index) {
    log_error(STR("List: removing elements is not supported as I dont need that."), 0);
}
*/
