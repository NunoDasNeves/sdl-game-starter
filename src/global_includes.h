#ifndef GLOBAL_INCLUDES_H

#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<limits.h>
#include<assert.h>
#include<math.h>

static_assert(CHAR_BIT == 8, "Char must be 8 bits");

#define sizeofarray(arr) (sizeof(arr) / sizeof((arr)[0]))

#define BITS_PER_BYTE 8

#define KILOBYTES(X) (((uint64_t)X) * 1024)
#define MEGABYTES(X) (KILOBYTES((uint64_t)X) * 1024)
#define GIGABYTES(X) (MEGABYTES((uint64_t)X) * 1024)
#define TERABYTES(X) (GIGABYTES((uint64_t)X) * 1024)

// TODO put these kind of defines in a header unless platform-specific
#ifdef STDOUT_DEBUG
#define DEBUG_PRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DEBUG_ASSERT(E) assert(E)
#else
#define DEBUG_PRINTF(...)
#define DEBUG_ASSERT(E)
#endif

#define FATAL_PRINTF(...)               \
    do                                  \
    {                                   \
        fprintf(stderr, __VA_ARGS__);   \
        exit(1);                        \
    } while(false)

#define GLOBAL_INCLUDES_H
#endif