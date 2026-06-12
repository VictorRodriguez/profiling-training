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
/* Simulate FP16 storage bandwidth saving:                             */
/* Store coefficients as uint16_t (raw FP16 bit pattern), load and    */
/* convert to FP32 before multiplying.                                 */
/*                                                                     */
/* On platforms with AVX-512 FP16 or ARM NEON this would be done in   */
/* hardware; here we use a software emulation to illustrate the        */
/* memory-bandwidth difference.                                        */
/* ------------------------------------------------------------------ */

/*
 * IEEE 754 half-precision (FP16) <-> single-precision (FP32) conversion.
 *
 * FP32: 1 sign | 8 exponent (bias 127) | 23 mantissa
 * FP16: 1 sign | 5 exponent (bias  15) | 10 mantissa
 */
static uint16_t fp32_to_fp16(float v)
{
    uint32_t bits;
    memcpy(&bits, &v, sizeof bits);

    uint16_t sign = (uint16_t)((bits >> 16) & 0x8000u);
    int32_t  exp  = (int32_t)((bits >> 23) & 0xFFu) - 127 + 15;
    uint32_t mant = bits & 0x7FFFFFu; /* 23-bit mantissa */

    if (exp <= 0) {
        /* Subnormal or underflow to zero */
        if (exp < -10)
            return sign;                        /* ±0 */
        mant = (mant | 0x800000u) >> (1 - exp); /* denormalise */
        return (uint16_t)(sign | (mant >> 13));
    }
    if (exp >= 31)
        return (uint16_t)(sign | 0x7C00u);      /* ±infinity */

    return (uint16_t)(sign | ((uint16_t)exp << 10) | (uint16_t)(mant >> 13));
}

static float fp16_to_fp32(uint16_t h)
{
    uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    uint32_t exp  = (h >> 10) & 0x1Fu;
    uint32_t mant = h & 0x3FFu;
    uint32_t bits;

    if (exp == 0) {
        if (mant == 0) {
            bits = sign; /* ±0 */
        } else {
            /* Normalise subnormal */
            exp = 1;
            while (!(mant & 0x400u)) { mant <<= 1; exp--; }
            mant &= 0x3FFu;
            bits = sign | ((exp - 15u + 127u) << 23) | (mant << 13);
        }
    } else if (exp == 31u) {
        bits = sign | 0x7F800000u | (mant << 13); /* ±inf / NaN */
    } else {
        bits = sign | ((exp - 15u + 127u) << 23) | (mant << 13);
    }

    float f;
    memcpy(&f, &bits, sizeof f);
    return f;
}

float dot_fp16_storage(const uint16_t * restrict a16,
                       const uint16_t * restrict b16, int n)
{
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        float ai = fp16_to_fp32(a16[i]);
        float bi = fp16_to_fp32(b16[i]);
        sum += ai * bi;
    }
    return sum;
}

/* ------------------------------------------------------------------ */

int main(void)
{
    struct timespec t0, t1;

    /* --- allocate --- */
    double *da = malloc(N * sizeof(double));
    double *db = malloc(N * sizeof(double));
    float  *fa = malloc(N * sizeof(float));
    float  *fb = malloc(N * sizeof(float));
    uint16_t *ha = malloc(N * sizeof(uint16_t));
    uint16_t *hb = malloc(N * sizeof(uint16_t));

    if (!da || !db || !fa || !fb || !ha || !hb) {
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
        ha[i] = fp32_to_fp16((float)v);
        hb[i] = fp32_to_fp16((float)v);
    }

    /* --- FP64 --- */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    volatile double r64 = dot_fp64(da, db, N);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("FP64 dot product : result = %.6f  time = %.2f ms  memory = %zu MB\n",
           r64, elapsed_ms(t0, t1), 2 * N * sizeof(double) / (1024*1024));

    /* --- FP32 --- */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    volatile float r32 = dot_fp32(fa, fb, N);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("FP32 dot product : result = %.6f  time = %.2f ms  memory = %zu MB\n",
           r32, elapsed_ms(t0, t1), 2 * N * sizeof(float) / (1024*1024));

    /* --- FP16 storage --- */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    volatile float r16 = dot_fp16_storage(ha, hb, N);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("FP16 storage     : result = %.6f  time = %.2f ms  memory = %zu MB\n",
           r16, elapsed_ms(t0, t1), 2 * N * sizeof(uint16_t) / (1024*1024));

    free(da); free(db);
    free(fa); free(fb);
    free(ha); free(hb);
    return 0;
}
