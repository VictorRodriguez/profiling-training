/*
 * loop_interchange.c
 *
 * Demonstrates the cache performance difference between
 * column-major (bad) and row-major (good) matrix traversal.
 *
 * A 2-D array in C is stored in row-major order, so accessing
 * consecutive elements along a row is cache-friendly, while
 * jumping between rows on every inner-loop iteration causes
 * cache misses.
 *
 * Build targets (see Makefile):
 *   make bad   - column-major traversal, no optimisation
 *   make good  - row-major traversal, -O2 optimisation
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 4096

/* Allocate on the heap to avoid stack overflow with large N */
static double matrix[N][N];

/* ------------------------------------------------------------------ */
/* Column-major traversal: inner loop walks down a column              */
/* Every access crosses a cache-line boundary                          */
/* ------------------------------------------------------------------ */
double sum_column_major(void)
{
    double sum = 0.0;
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++)
            sum += matrix[i][j];
    return sum;
}

/* ------------------------------------------------------------------ */
/* Row-major traversal: inner loop walks along a row                   */
/* Accesses are sequential in memory → cache-friendly                  */
/* ------------------------------------------------------------------ */
double sum_row_major(void)
{
    double sum = 0.0;
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            sum += matrix[i][j];
    return sum;
}

/* ------------------------------------------------------------------ */

static double elapsed_ms(struct timespec start, struct timespec end)
{
    return (end.tv_sec - start.tv_sec) * 1e3 +
           (end.tv_nsec - start.tv_nsec) / 1e6;
}

int main(void)
{
    /* Initialise matrix with pseudo-random values */
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            matrix[i][j] = (double)(i * N + j) / (N * N);

    struct timespec t0, t1;
    double result;

    /* --- column-major (cache-unfriendly) --- */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    result = sum_column_major();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("column-major (bad)  : sum = %.2f  time = %.2f ms\n",
           result, elapsed_ms(t0, t1));

    /* --- row-major (cache-friendly) --- */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    result = sum_row_major();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("row-major    (good) : sum = %.2f  time = %.2f ms\n",
           result, elapsed_ms(t0, t1));

    return 0;
}
