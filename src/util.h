/*
 * General purpose macros and c library includes
 */
#ifndef GLOBAL_INCLUDES_H

#include<stdlib.h>
#include<stdint.h>
#include<limits.h>
#include<math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static_assert(CHAR_BIT == 8, "Char must be 8 bits");

#define SIZE_OF_ARRAY(arr) (sizeof(arr) / sizeof((arr)[0]))

// distance between two indices 'prev' and 'curr' index in a ring buffer
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


// Debug stuff

#include<stdio.h>
#include<assert.h>
#ifdef STDOUT_DEBUG

#define DEBUG_PRINTF(...) fprintf(stderr, __VA_ARGS__)

#define DEBUG_ASSERT(E) assert(E)

#define FATAL_PRINTF(...)               \
    do                                  \
    {                                   \
        fprintf(stderr, __VA_ARGS__);   \
        exit(1);                        \
    } while(false)

#else

// TODO figure out how to minimize these
// TODO make them do something useful when debug not enabled?
#define DEBUG_PRINTF(...)
#define DEBUG_ASSERT(E)
#define FATAL_PRINTF(...)

#endif


#define GLOBAL_INCLUDES_H
#endif