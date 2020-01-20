#ifndef GLOBAL_INCLUDES_H

#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<limits.h>
#include<assert.h>
#include<math.h>

// TODO put these kind of defines in a header unless platform-specific
#ifdef CONSOLE_DEBUG
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