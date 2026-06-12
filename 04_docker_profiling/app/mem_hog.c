/*
 * mem_hog.c
 *
 * Simulates a memory-bandwidth-bound workload: repeatedly allocates and
 * reads large arrays to saturate the memory subsystem.
 * Used to demonstrate how to detect memory bottlenecks inside a container
 * from the outside without source-code access.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define ARRAY_SIZE_MB  512
#define ARRAY_BYTES    ((size_t)ARRAY_SIZE_MB * 1024 * 1024)
#define ELEMENT_COUNT  (ARRAY_BYTES / sizeof(double))

static double elapsed_ms(struct timespec s, struct timespec e)
{
    return (e.tv_sec - s.tv_sec) * 1e3 + (e.tv_nsec - s.tv_nsec) / 1e6;
}

int main(void)
{
    printf("mem_hog starting (PID %d) – allocating %d MB\n",
           getpid(), ARRAY_SIZE_MB);
    fflush(stdout);

    double *buf = malloc(ARRAY_BYTES);
    if (!buf) {
        fprintf(stderr, "malloc failed for %zu bytes\n", ARRAY_BYTES);
        return 1;
    }

    /* Touch all pages to commit physical memory */
    memset(buf, 0, ARRAY_BYTES);

    unsigned long iteration = 0;
    while (1) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        /* Sequential read – maximise DRAM bandwidth */
        volatile double sum = 0.0;
        for (size_t i = 0; i < ELEMENT_COUNT; i++)
            sum += buf[i];

        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ms = elapsed_ms(t0, t1);
        double gb_s = ((double)ARRAY_BYTES / (1024.0*1024.0*1024.0)) /
                      (ms / 1000.0);

        iteration++;
        printf("iteration %lu  time=%.1f ms  bandwidth=%.2f GB/s  sum=%.0f\n",
               iteration, ms, gb_s, sum);
        fflush(stdout);
    }

    free(buf);
    return 0;
}
