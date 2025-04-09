#include "stack.h"

void stack_tests(void) {
    stack_t<int> stack = {};

    stack_push(&stack, 1);
    stack_push(&stack, 2);
    stack_push(&stack, 3);
    stack_push(&stack, 4);
    stack_push(&stack, 5);
    stack_push(&stack, 6);

    assert(stack_pop(&stack) == 6);
    assert(stack_pop(&stack) == 5);
    assert(stack_pop(&stack) == 4);
    assert(stack_pop(&stack) == 3);
    assert(stack_pop(&stack) == 2);
    assert(stack_pop(&stack) == 1);

    stack_delete(&stack);

    log_info(STRING("STACK: OK"));
}
