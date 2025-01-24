#include "area_alloc.h" 

b32 area_create(area_t *container, u64 init_size, u64 element_size) {
    if (init_size == 0) {
        log_warning(STR("Area: init size was 0. defaulting to 1."), 0);
        init_size = 1;
    }

    if (element_size == 0) {
        log_warning(STR("Area: element size was 0. defaulting to 1."), 0);
        element_size = 1;
    }

    container->count        = 0;
    container->element_size = element_size;
    container->raw_size     = init_size;
    container->grow_size    = init_size * 2;
    container->data         = ALLOC(init_size * element_size);

    if (container->data == NULL) {
        log_error(STR("Area: Couldn't create area."), 0);
        return false; 
    }

    return true;
}

b32 area_delete(area_t *area) {
    if (area == NULL) {
        log_error(STR("Area: Reference to area wasn't valid."), 0);
        return false;
    }

    if (area->data == NULL) {
        log_error(STR("Area: Area was already deleted."), 0);
        return false;
    }

    FREE(area->data); 
    area->data = NULL;

    return true;
}

b32 area_grow(area_t *area) {
    assert(area != 0, "area was null");
    void *data = REALLOC(area->data, area->grow_size * area->element_size);

    if (data == NULL) {
        log_error(STR("Area: Couldn't grow area."), 0);
        return false;
    }

    area->data = (u8*)data;

    u64 size   = (area->grow_size - area->raw_size) * area->element_size;
    u64 offset = area->count  *area->element_size;

    // windows copiler need to know what is a type of void
    (void)memset((u8*)area->data + offset, 0, size);
    
    area->raw_size  = area->grow_size;
    area->grow_size = area->raw_size * 2;
    return true;
}

b32 area_grow_fit(area_t *area, u64 fit_elements) {
    assert(fit_elements > 0, "trying to fit 0 elements, it will be always true.");

    if ((area->count + fit_elements - 1) < area->raw_size) {
        return true;
    } else {
        while ((area->count + fit_elements) >= area->raw_size) {
            if (!area_grow(area)) {
                return false;
            }
        }
    }

    return true;
}

// @todo with templates we can just delete this
// and rawdog it on data pointer in area
b32 area_add(area_t *area, void *data) {
    assert(area != 0, "area was null");
    assert(data != 0, "data pointer was null");

    u64 index = 0;
    if (!area_allocate(area, 1, &index)) {
        return false;
    }

    u8 *dest = (u8*)area->data + area->element_size * index;
    memcpy(dest, data, area->element_size);

    return true;
}

b32 area_allocate(area_t *area, u64 elements_amount, u64 *start_index) {
    assert(area != 0, "area was null");
    assert(elements_amount > 0, "allocation amount is zero");

    u64 new_count = area->count + elements_amount;

    if (!area_grow_fit(area, elements_amount)) {
        return false;
    }
    
    *start_index = area->count;
    area->count = new_count;

    return true;
}

void *area_get(area_t *area, u64 index) {
    assert(area != 0, "area was null");
    assert(index < area->count, "index was bigger that count of elements");

    if (index >= area->count) {
        log_error(STR("Area: Index is greater than count of elements."), 0);
        return NULL;
    }

    u8* result_data = (u8*)area->data;

    return &result_data[area->element_size * index]; 
}

void area_tests(void) {
    area_t list = {};

    area_create(&list, 100, sizeof(u64));

    assert(list.data != 0, "pointer to list data is null");
    assert(list.count == 0, "count should be zero");
    assert(list.element_size == sizeof(u64), "element size is not right");
    assert(list.raw_size == 100, "different raw size from specified");
    assert(list.grow_size == 200, "grow size should be multiply of raw size");

    u64 data1 = 404;
    u64 data2 = 804;
    u64 data3 = 104;
    assert(area_add(&list, &data1), "should add element to a list");

    assert(area_add(&list, &data2), "should add element to a list");
    assert(area_add(&list, &data3), "should add element to a list");

    assert(data1 == *(u64*)area_get(&list, 0), "should be equal");
    assert(data2 == *(u64*)area_get(&list, 1), "should be equal");
    assert(data3 == *(u64*)area_get(&list, 2), "should be equal");

    area_delete(&list);
    log_info(STR("Area: OK"), 0);
}
