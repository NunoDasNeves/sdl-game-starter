#ifndef GLOBAL_INCLUDES_H

#include<stdio.h>
#include<stdint.h>
#include<limits.h>

// TODO: own assert, own math libraries
#include<assert.h>
#include<math.h>

static_assert(CHAR_BIT == 8, "Char must be 8 bits");

#define sizeofarray(arr) (sizeof(arr) / sizeof((arr)[0]))

// distance between a 'prev' and 'curr' index in a ring buffer
#define DIST_IN_RING_BUFFER(prev, curr, size) (((prev) > (curr) ? (curr) + (size) : (curr)) - (prev))

#define MAX(X, Y) ((X) >= (Y) ? (X) : (Y))
#define MIN(X, Y) ((X) <= (Y) ? (X) : (Y))
#define EXP_WEIGHTED_AVG(avg, N, new_sample) (((double)(avg) - (double)(avg)/(double)(N)) + (double)(new_sample)/(double)(N))

#define BITS_PER_BYTE 8

// Technically correct...the best kind of correct!
// Except when it comes to naming things
#define KIBIBYTES(X) (((uint64_t)X) * 1024)
#define MEBIBYTES(X) (KIBIBYTES((uint64_t)X) * 1024)
#define GIBIBYTES(X) (MEBIBYTES((uint64_t)X) * 1024)
#define TEBIBYTES(X) (GIBIBYTES((uint64_t)X) * 1024)

// TODO put these kind of defines in a header unless platform-specific
#ifdef STDOUT_DEBUG
#define DEBUG_PRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DEBUG_ASSERT(E) assert(E)
#define DEBUG_ASSERT_MSG(E, ...)            \
do                                          \
{                                           \
    if (!(E))                               \
    {                                       \
        fprintf(stderr, __VA_ARGS__);       \
    }                                       \
    assert(E);                              \
} while(false)
#else
#define DEBUG_PRINTF(...)
#define DEBUG_ASSERT(E)
#define DEBUG_ASSERT_MSG(E, S, ...)
#endif

#define FATAL_PRINTF(...)               \
    do                                  \
    {                                   \
        fprintf(stderr, __VA_ARGS__);   \
        exit(1);                        \
    } while(false)

#define GLOBAL_INCLUDES_H
#endif