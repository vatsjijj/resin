#ifndef resin_common_h
#define resin_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Disable NAN_BOXING if it somehow
// breaks on your machine.
#define NAN_BOXING
// #define DEBUG_PRINT_CODE
// #define DEBUG_TRACE_EXEC
// #define DEBUG_STRESS_GC
// #define DEBUG_LOG_GC

#define UINT8_COUNT (UINT8_MAX + 1)

#endif