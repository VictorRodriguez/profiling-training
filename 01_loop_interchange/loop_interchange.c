/*
 * loop_interchange.c
 *
 * Demonstrates loop interchange and cache locality.
 *
 * Three experiments:
 *
 *   1. COLUMN_MAJOR:
 *        Cache-unfriendly source.
 *
 *   2. COLUMN_MAJOR + -floop-interchange:
 *        Same source code, but GCC may interchange the loops.
 *
 *   3. ROW_MAJOR:
 *        Programmer manually interchanges the loops.
 */

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 2048

/*
 * 2048 x 2048 doubles = 32 MiB per matrix.
 *
 * Two matrices = 64 MiB total.
 */
static double A[N][N];
static double B[N][N];


/*
 * Initialize A using the natural row-major order.
 *
 * Initialization is outside the timed region.
 */
static void initialize(void)
{
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            A[i][j] = (double)(i + j);
        }
    }
}


/*
 * Column-major traversal.
 *
 * Intentionally cache-unfriendly:
 *
 *     A[0][j]
 *     A[1][j]
 *     A[2][j]
 *
 * Each inner-loop iteration jumps:
 *
 *     N * sizeof(double)
 *     = 2048 * 8
 *     = 16384 bytes
 *
 * There are no loop-carried dependencies, making this
 * a good candidate for compiler loop interchange.
 */
#ifdef COLUMN_MAJOR

static void process(void)
{
    for (int j = 0; j < N; ++j) {
        for (int i = 0; i < N; ++i) {
            B[i][j] = A[i][j] * 2.0;
        }
    }
}

#endif


/*
 * Manually interchanged traversal.
 *
 * Inner loop walks through contiguous elements:
 *
 *     A[i][0]
 *     A[i][1]
 *     A[i][2]
 *
 * Consecutive accesses are sizeof(double) = 8 bytes apart.
 */
#ifdef ROW_MAJOR

static void process(void)
{
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            B[i][j] = A[i][j] * 2.0;
        }
    }
}

#endif


static double elapsed_ms(struct timespec start,
                         struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) * 1000.0 +
           (double)(end.tv_nsec - start.tv_nsec) / 1000000.0;
}


int main(void)
{
    struct timespec start;
    struct timespec end;

    double runtime_ms;

#if !defined(COLUMN_MAJOR) && !defined(ROW_MAJOR)
#error "Define either COLUMN_MAJOR or ROW_MAJOR"
#endif

#if defined(COLUMN_MAJOR) && defined(ROW_MAJOR)
#error "Define only one of COLUMN_MAJOR or ROW_MAJOR"
#endif

    printf("Matrix size : %d x %d\n", N, N);

    printf("Matrix data : %.2f MiB each\n",
           ((double)N * N * sizeof(double)) /
           (1024.0 * 1024.0));

    initialize();

#ifdef COLUMN_MAJOR

    printf("Source loop : column-major\n");
    printf("Source stride: %zu bytes\n",
           (size_t)N * sizeof(double));

#endif

#ifdef ROW_MAJOR

    printf("Source loop : row-major\n");
    printf("Source stride: %zu bytes\n",
           sizeof(double));

#endif

    clock_gettime(CLOCK_MONOTONIC, &start);

    process();

    clock_gettime(CLOCK_MONOTONIC, &end);

    runtime_ms = elapsed_ms(start, end);

    /*
     * Make the generated output observable so GCC cannot
     * eliminate process() as dead code.
     */
    printf("Result      : %.2f\n",
           B[N - 1][N - 1]);

    printf("Time        : %.3f ms\n",
           runtime_ms);

    return EXIT_SUCCESS;
}
