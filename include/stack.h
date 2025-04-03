
#ifndef STACK_H
#define STACK_H

#include "stddefines.h"
#include "logger.h"
#include "memctl.h"

#ifndef CUSTIOM_MEM_CTRL
#define ALLOC(x)      calloc(1, x) 
#define FREE(x)       free(x) 
#endif

#define STANDART_STACK_SIZE 64

template<typename DataType>
struct stack_t {
    u64 index;
    DataType *data;

    u64 current_size;
    u64 grow_size;
};

// ----------- Initialization 

template<typename DataType>
b32 stack_create(stack_t<DataType> *stack, u64 init_size);
template<typename DataType>
b32 stack_delete(stack_t<DataType> *stack);

// --------- Control

template<typename DataType>
void stack_push(stack_t<DataType> *stack, DataType data);

template<typename DataType>
DataType stack_pop(stack_t<DataType> *stack);

template<typename DataType>
DataType stack_peek(stack_t<DataType> *stack);


// ----------- Helpers

void stack_tests(void);

template<typename DataType>
b32 stack_grow(stack_t<DataType> *stack);

template<typename DataType>
void stack_create_if_needed(stack_t<DataType> *stack);


// ----------- Implementation

template<typename DataType>
b32 stack_create(stack_t<DataType> *stack, u64 init_size) {
    stack->index        = 0;
    stack->current_size = init_size;

    stack->grow_size = init_size * 2;
    stack->data      = (DataType*)ALLOC(init_size * sizeof(DataType));

    if (stack->data == NULL) {
        log_error(STR("Couldn't create stack."));
        return false; 
    }

    return true;
}

template<typename DataType>
b32 stack_delete(stack_t<DataType> *stack) {
    if (stack == NULL) {
        log_error(STR("Reference to stack wasn't valid."));
        return false;
    }

    if (stack->data == NULL) {
        log_error(STR("Stack was already deleted."));
        return false;
    }

    FREE(stack->data); 
    stack->data = NULL;

    return true;
}

template<typename DataType>
void stack_push(stack_t<DataType> *stack, DataType data) {
    stack_create_if_needed(stack);

    if (stack->index >= stack->current_size) {
        if (!stack_grow(stack)) return;
    }

    mem_copy((u8*)(stack->data + stack->index), (u8*)&data, sizeof(DataType));
    stack->index++;
}

template<typename DataType>
DataType stack_pop(stack_t<DataType> *stack) {
    stack_create_if_needed(stack);

    if (stack->index == 0) {
        log_error(STR("Stack doesn't have elements in it to pop."));
        return {};
    }

    return stack->data[--stack->index]; 
}

template<typename DataType>
DataType stack_peek(stack_t<DataType> *stack) {
    stack_create_if_needed(stack);
    
    if (stack->index == 0) {
        log_error(STR("Stack doesn't have elements in it to peek at."));
        return {};
    }

    return stack->data[stack->index - 1]; 
}
// -------------- local functions

template<typename DataType>
void stack_create_if_needed(stack_t<DataType> *stack) {
    if (stack->data == NULL && !stack_create(stack, STANDART_STACK_SIZE)) {
        log_error(STR("Tried to create stack but failed."));
    }
}

template<typename DataType>
b32 stack_grow(stack_t<DataType> *stack) {
    assert(stack != 0);
    DataType *data = (DataType*)ALLOC(stack->grow_size * sizeof(DataType));

    if (data == NULL) {
        log_error(STR("Couldn't grow stack."));
        return false;
    }

    (void)mem_set((u8*)data, 0, stack->grow_size * sizeof(DataType));

    (void)mem_copy((u8*)data, (u8*)stack->data, stack->current_size * sizeof(DataType));

    FREE(stack->data);
    stack->data = data;
    stack->current_size  = stack->grow_size;
    stack->grow_size = stack->current_size * 2;

    return true;
}

#endif // STACK_H
