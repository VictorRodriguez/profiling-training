/*
 * cpu_hog.c
 *
 * Simulates a CPU-bound workload: computes a large number of
 * square-root and modulo operations in an infinite loop.
 * Used to demonstrate how to profile a containerised application
 * from the *outside* without access to its source code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

#define ITERATIONS 100000000UL

int main(void)
{
    printf("cpu_hog starting (PID %d) – press Ctrl-C to stop\n", getpid());
    fflush(stdout);

    unsigned long iteration = 0;
    while (1) {
        volatile double acc = 0.0;
        for (unsigned long i = 1; i <= ITERATIONS; i++) {
            acc += sqrt((double)i) + (double)(i % 7919);
        }
        iteration++;
        printf("iteration %lu done  acc=%.2f\n", iteration, acc);
        fflush(stdout);
    }
    return 0;
}
