#ifndef TEMP_ALLOC_H
#define TEMP_ALLOC_H

#include "stddefines.h"
#include "logger.h"
#include "allocator.h"

#ifndef TEMP_MEM_SIZE
#define TEMP_MEM_SIZE MB(10)
#endif

void * temp_allocate(u64 size);
void   temp_reset(void);

void   temp_tests(void);

allocator_t *get_temporary_allocator(void);

#endif // TEMP_ALLOC_H
