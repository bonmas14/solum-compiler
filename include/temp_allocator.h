#ifndef TEMP_ALLOC_H
#define TEMP_ALLOC_H

#include "stddefines.h"
#include "logger.h"

void   temp_init(u64 size);
void   temp_deinit(void);
void * temp_allocate(u64 size);
void   temp_reset(void);

void   temp_tests(void);

#endif // TEMP_ALLOC_H
