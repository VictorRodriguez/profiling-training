/*
 * fp_precision.c
 *
 * Compares the throughput of single-precision (float / FP32) and
 * double-precision (double / FP64) arithmetic on a large array.
 *
 * On modern x86 CPUs both types fit in the same SSE/AVX registers, but
 * - FP32 vectors hold TWICE as many elements as FP64 vectors.
 * - Compilers can auto-vectorise FP32 loops more aggressively when
 *   -ffast-math relaxes strict IEEE-754 rules.
 *
 * Relationship to FP16 (half-precision):
 *   x86 does not execute FP16 arithmetic natively (pre-AVX-512 BF16 /
 *   AVX512-FP16).  The typical production use-case is to *store* weights
 *   in FP16 (halving memory bandwidth) and *compute* in FP32, using
 *   _cvtsh_ss / _cvtss_sh intrinsics or the compiler's -mfp16-format flag
 *   on ARM.  The bandwidth saving is demonstrated in the fp32_bandwidth vs
 *   fp16_bandwidth functions below.
 *
 * Build targets (see Makefile):
 *   make fp64   - FP64 scalar,  -O2
 *   make fp32   - FP32 scalar,  -O2
 *   make fp32v  - FP32 vectorised, -O3 -march=native -ffast-math
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define N (1 << 24)   /* 16 M elements – large enough to be memory-bound */

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static double elapsed_ms(struct timespec s, struct timespec e)
{
    return (e.tv_sec - s.tv_sec) * 1e3 + (e.tv_nsec - s.tv_nsec) / 1e6;
}

/* ------------------------------------------------------------------ */
/* FP64 dot product                                                     */
/* ------------------------------------------------------------------ */
double dot_fp64(const double * restrict a, const double * restrict b, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; i++)
        sum += a[i] * b[i];
    return sum;
}

/* ------------------------------------------------------------------ */
/* FP32 dot product                                                     */
/* ------------------------------------------------------------------ */
float dot_fp32(const float * restrict a, const float * restrict b, int n)
{
    float sum = 0.0f;
    for (int i = 0; i < n; i++)
        sum += a[i] * b[i];
    return sum;
}

/* ------------------------------------------------------------------ */
/* FP16 dot product                                                     */
/* ------------------------------------------------------------------ */
#ifdef ENABLE_FP16

_Float16 dot_fp16(const _Float16 * restrict a,
                  const _Float16 * restrict b, int n)
{
    _Float16 sum = (_Float16)0.0;

    for (int i = 0; i < n; i++)
        sum += a[i] * b[i];

    return sum;
}
#endif

/* ------------------------------------------------------------------ */
int main(void)
{
    struct timespec t0, t1;

    /* --- allocate --- */
    double   *da = malloc(N * sizeof(double));
    double   *db = malloc(N * sizeof(double));
    float    *fa = malloc(N * sizeof(float));
    float    *fb = malloc(N * sizeof(float));

	#ifdef ENABLE_FP16
    _Float16 *ha = malloc(N * sizeof(_Float16));
    _Float16 *hb = malloc(N * sizeof(_Float16));
	#endif

	#ifdef ENABLE_FP16
	if (!da || !db || !fa || !fb || !ha || !hb) {
	#else
	if (!da || !db || !fa || !fb) {
	#endif
		fprintf(stderr, "malloc failed\n");
		return 1;
	}
    /* --- initialise --- */
    for (int i = 0; i < N; i++) {
        double v = (double)i / N;

        da[i] = v;
        db[i] = v;

        fa[i] = (float)v;
        fb[i] = (float)v;
		#ifdef ENABLE_FP16
        ha[i] = (_Float16)v;
        hb[i] = (_Float16)v;
		#endif
    }

    /* --- FP64 --- */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    volatile double r64 = dot_fp64(da, db, N);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    printf("FP64 dot product : result = %.6f  time = %.2f ms  memory = %zu MB\n",
           r64,
           elapsed_ms(t0, t1),
           2 * N * sizeof(double) / (1024 * 1024));

    /* --- FP32 --- */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    volatile float r32 = dot_fp32(fa, fb, N);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    printf("FP32 dot product : result = %.6f  time = %.2f ms  memory = %zu MB\n",
           r32,
           elapsed_ms(t0, t1),
           2 * N * sizeof(float) / (1024 * 1024));

    /* --- FP16 --- */
	#ifdef ENABLE_FP16
	clock_gettime(CLOCK_MONOTONIC, &t0);
	volatile _Float16 r16 = dot_fp16(ha, hb, N);
	clock_gettime(CLOCK_MONOTONIC, &t1);

	printf("FP16 dot product : result = %.6f  time = %.2f ms  memory = %zu MB\n",
       (double)r16,
       elapsed_ms(t0, t1),
       2 * N * sizeof(_Float16) / (1024 * 1024));
	#endif

    free(da);
    free(db);
    free(fa);
    free(fb);
	#ifdef ENABLE_FP16
	free(ha);
	free(hb);
	#endif
    return 0;
}
