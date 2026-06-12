/*
 * avx_example.c
 *
 * Shows three implementations of a vector addition and dot product:
 *
 *  1. Scalar baseline  – plain C, no SIMD
 *  2. Auto-vectorised  – let GCC emit SSE/AVX with -O3 -march=native
 *  3. Explicit AVX2    – hand-written intrinsics (requires AVX2 support)
 *
 * The explicit AVX2 path is guarded by __AVX2__ so the file compiles
 * without error on machines that lack AVX2.
 *
 * Build targets (see Makefile):
 *   make scalar   – scalar, -O2
 *   make autovec  – auto-vectorised, -O3 -march=native
 *   make avx2     – explicit AVX2 intrinsics (requires AVX2 CPU)
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#ifdef __AVX2__
#  include <immintrin.h>
#endif

#define N (1 << 26)   /* 64 M floats = 256 MB per array */
#define ALIGN 32      /* AVX2 needs 32-byte alignment    */

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static double elapsed_ms(struct timespec s, struct timespec e)
{
    return (e.tv_sec - s.tv_sec) * 1e3 + (e.tv_nsec - s.tv_nsec) / 1e6;
}

/* ------------------------------------------------------------------ */
/* 1. Scalar vector addition                                            */
/* ------------------------------------------------------------------ */
void vec_add_scalar(const float * restrict a,
                    const float * restrict b,
                    float       * restrict c, int n)
{
    for (int i = 0; i < n; i++)
        c[i] = a[i] + b[i];
}

/* ------------------------------------------------------------------ */
/* 2. Auto-vectorised – same source, compiler handles SIMD             */
/*    (Compiled with -O3 -march=native in the autovec target)          */
/* ------------------------------------------------------------------ */
void vec_add_autovec(const float * restrict a,
                     const float * restrict b,
                     float        * restrict c, int n)
{
    for (int i = 0; i < n; i++)
        c[i] = a[i] + b[i];
}

/* ------------------------------------------------------------------ */
/* 3. Explicit AVX2 intrinsics – 8 floats per instruction              */
/* ------------------------------------------------------------------ */
#ifdef __AVX2__
void vec_add_avx2(const float * restrict a,
                  const float * restrict b,
                  float       * restrict c, int n)
{
    int i = 0;
    /* Process 8 floats per iteration using 256-bit ymm registers */
    for (; i <= n - 8; i += 8) {
        __m256 va = _mm256_load_ps(a + i);
        __m256 vb = _mm256_load_ps(b + i);
        __m256 vc = _mm256_add_ps(va, vb);
        _mm256_store_ps(c + i, vc);
    }
    /* Scalar tail for remainder */
    for (; i < n; i++)
        c[i] = a[i] + b[i];
}

float dot_avx2(const float * restrict a,
               const float * restrict b, int n)
{
    __m256 acc = _mm256_setzero_ps();
    int i = 0;
    for (; i <= n - 8; i += 8) {
        __m256 va = _mm256_load_ps(a + i);
        __m256 vb = _mm256_load_ps(b + i);
        acc = _mm256_fmadd_ps(va, vb, acc);   /* FMA: acc += a*b */
    }
    /* Horizontal reduction of the 8 partial sums */
    __m128 lo  = _mm256_castps256_ps128(acc);
    __m128 hi  = _mm256_extractf128_ps(acc, 1);
    __m128 sum = _mm_add_ps(lo, hi);
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    float result = _mm_cvtss_f32(sum);
    /* Scalar tail */
    for (; i < n; i++)
        result += a[i] * b[i];
    return result;
}
#endif /* __AVX2__ */

/* Scalar dot product for comparison */
float dot_scalar(const float * restrict a,
                 const float * restrict b, int n)
{
    float sum = 0.0f;
    for (int i = 0; i < n; i++)
        sum += a[i] * b[i];
    return sum;
}

/* ------------------------------------------------------------------ */

int main(void)
{
    /* Use aligned allocation for AVX2 loads */
    float *a = aligned_alloc(ALIGN, N * sizeof(float));
    float *b = aligned_alloc(ALIGN, N * sizeof(float));
    float *c = aligned_alloc(ALIGN, N * sizeof(float));

    if (!a || !b || !c) {
        fprintf(stderr, "aligned_alloc failed\n");
        return 1;
    }

    for (int i = 0; i < N; i++) {
        a[i] = (float)i / N;
        b[i] = (float)(N - i) / N;
    }

    struct timespec t0, t1;

    /* --- scalar --- */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    vec_add_scalar(a, b, c, N);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("vec_add scalar   : c[0]=%.4f  time = %.2f ms\n",
           c[0], elapsed_ms(t0, t1));

    /* --- scalar dot --- */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    volatile float sd = dot_scalar(a, b, N);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("dot scalar       : result=%.4f  time = %.2f ms\n",
           sd, elapsed_ms(t0, t1));

#ifdef __AVX2__
    /* --- explicit AVX2 add --- */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    vec_add_avx2(a, b, c, N);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("vec_add AVX2     : c[0]=%.4f  time = %.2f ms\n",
           c[0], elapsed_ms(t0, t1));

    /* --- explicit AVX2 dot --- */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    volatile float ad = dot_avx2(a, b, N);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("dot AVX2 (FMA)   : result=%.4f  time = %.2f ms\n",
           ad, elapsed_ms(t0, t1));
#else
    printf("AVX2 not available on this build (recompile with -mavx2 -mfma)\n");
#endif

    free(a); free(b); free(c);
    return 0;
}
