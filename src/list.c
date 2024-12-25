#include "list.h" 

bool list_create(list_t *container, size_t init_size, size_t element_size) {
    if (init_size == 0) {
        log_warning("List: init size was 0. defaulting to 1.");
        init_size = 1;
    }

    if (element_size == 0) {
        log_warning("List: element size was 0. defaulting to 1.");
        element_size = 1;
    }

    container->count        = 0;
    container->element_size = element_size;
    container->raw_size     = init_size;
    container->grow_size    = init_size * 2;
    container->data         = ALLOC(init_size * element_size);

    if (container->data == NULL) {
        log_error("List: Couldn't create list.");
        return false; 
    }

    return true;
}

bool list_delete(list_t *list) {
    if (list == NULL) {
        log_error("List: Reference to list wasn't valid.");
        return false;
    }

    if (list->data == NULL) {
        log_error("List: List was already deleted.");
        return false;
    }

    FREE(list->data); 
    list->data = NULL;

    return true;
}

bool list_grow(list_t *list) {
    void *data = REALLOC(list->data, list->grow_size  *list->element_size);

    if (data == NULL) {
        log_error("List: Couldn't grow list.");
        return false;
    }

    list->data = data;

    size_t size   = (list->grow_size - list->raw_size) * list->element_size;
    size_t offset = list->count  *list->element_size;

    (void)memset(list->data + offset, 0, size);

    list->raw_size  = list->grow_size;
    list->grow_size = list->raw_size * 2;
    return true;
}

bool list_add(list_t *list, void *data) {
    if (list->count >= list->raw_size) {
        if (!list_grow(list)) {

            // we dont need that log because it can only happen when using list_grow
            // log_error("List: Couldn't add element to a list.");
            
            return false;
        }
    }
    
    uint8_t *arr = (uint8_t*)list->data;

    for (size_t i = 0; i < list->element_size; i++) {
        uint8_t *casted = (uint8_t*)data;
        arr[list->element_size  *list->count + i] = casted[i];
    }

    list->count++;

    return true;
}

void *list_get(list_t *list, size_t index) {
    if (index >= list->count) {
        log_error("List: Index is greater than count of elements.");
        return NULL;
    }

    return &list->data[list->element_size * index];
}

/*
void list_insert_at(list_t *list, size_t index, void *data) {
    log_error("List: inserting is not supported as I dont need that.");
}

void list_remove_at(list_t *list, size_t index) {
    log_error("List: removing elements is not supported as I dont need that.");
}
*/
