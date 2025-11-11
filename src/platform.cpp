#include "stddefines.h" 
#include "logger.h" 
#include "strings.h"
#include "allocator.h"
#include "talloc.h"
#include "profiler.h"

#ifdef _WIN32
#include "platform/win32_layer.cpp"
#else
#include "platform/linux_layer.cpp"
#endif
