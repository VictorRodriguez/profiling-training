# Case Study 03 – AVX ISA / SIMD Vectorisation

## Overview

**SIMD (Single Instruction, Multiple Data)** allows one CPU instruction to
operate on multiple data elements in parallel using wide registers:

| ISA Extension | Register width | FP32 lanes | FP64 lanes |
|--------------|---------------|-----------|-----------|
| SSE2         | 128-bit (xmm)  | 4         | 2         |
| AVX / AVX2   | 256-bit (ymm)  | 8         | 4         |
| AVX-512      | 512-bit (zmm)  | 16        | 8         |

This example measures a **vector addition** and **dot product** across 64 M
floats using:

1. **Scalar** – plain C, `-O2`, no SIMD.
2. **Auto-vectorised** – same C, but compiled with `-O3 -march=native`; the
   compiler emits `ymm` instructions automatically.
3. **Explicit AVX2** – hand-written `<immintrin.h>` intrinsics using
   `_mm256_add_ps` and `_mm256_fmadd_ps` (fused multiply-add).

## Prerequisites

Check that your CPU supports AVX2:

```bash
grep -m1 avx2 /proc/cpuinfo
# or
lscpu | grep avx2
```

## Build

```bash
make          # builds all three binaries
make run      # builds and runs all three, printing timings
make asm      # prints the disassembly of the hot loops
```

## Profiling with `perf stat`

```bash
# Measure SIMD utilisation for each binary
for bin in avx_scalar avx_autovec avx_avx2; do
    echo "=== $bin ==="
    perf stat -e cycles,instructions,\
fp_arith_inst_retired.256b_packed_single,\
fp_arith_inst_retired.128b_packed_single,\
fp_arith_inst_retired.scalar_single \
        ./$bin 2>&1 | tail -10
done
```

Key counters:
* `fp_arith_inst_retired.scalar_single` – should be near zero for the AVX2 binary.
* `fp_arith_inst_retired.256b_packed_single` – should be high for AVX2 binary.

### GCC vectorisation report

```bash
# Compile to /dev/null (no binary produced) just to view the vectorisation log
gcc -O3 -march=native -ftree-vectorize -fopt-info-vec avx_example.c -o /dev/null
```

Lines marked `vectorized N loops` confirm which loops were auto-vectorised.

## Flame Graph

```bash
perf record -F 999 -g ./avx_scalar
perf script > out.perf
/opt/FlameGraph/stackcollapse-perf.pl out.perf > out.folded
/opt/FlameGraph/flamegraph.pl out.folded > flamegraph.svg
# Open in browser; compare width of vec_add_scalar vs dot_scalar
```

Repeat with `avx_avx2` and notice the towers are much narrower, indicating
faster execution.

## Inspecting with `objdump`

```bash
make asm
```

Expected output differences:

| Binary         | Instruction pattern |
|----------------|---------------------|
| `avx_scalar`   | `addss` (scalar SSE) or `fadd` |
| `avx_autovec`  | `vaddps ymm…` (compiler-generated AVX) |
| `avx_avx2`     | `vaddps ymm…` + `vfmadd231ps ymm…` (FMA) |

```bash
# Grep directly
objdump -d avx_avx2 | grep -E "vfmadd|vaddps|ymm" | head -20
```

## Profiling with Intel VTune

```bash
vtune -collect hotspots -result-dir vtune_scalar  -- ./avx_scalar
vtune -collect hotspots -result-dir vtune_avx2    -- ./avx_avx2
vtune -report summary -result-dir vtune_avx2
```

Check the **Vectorization Intensity** metric in the Bottom-up view.  The
explicit AVX2 binary should show 100 % vectorisation for the hot functions.

## Expected Results

| Binary          | Approximate time (64 M floats) |
|-----------------|-------------------------------|
| `avx_scalar`    | ~200–400 ms |
| `avx_autovec`   | ~40–80 ms   |
| `avx_avx2`      | ~30–60 ms   |

*(Highly dependent on CPU model, cache sizes, and memory bandwidth.)*

## GCC Flags Reference

| Flag | Effect |
|------|--------|
| `-O2` | Standard opt; limited auto-vectorisation |
| `-O3` | Aggressive opt; enables loop vectorisation |
| `-march=native` | Emit ISA-specific instructions for the host CPU |
| `-mavx2` | Enable AVX2 instructions explicitly |
| `-mfma` | Enable Fused Multiply-Add (FMA3) |
| `-ftree-vectorize` | Explicit tree-vectorisation pass |
| `-fopt-info-vec` | Print which loops were vectorised |
| `-fopt-info-vec-missed` | Print which loops could NOT be vectorised and why |

## Fix Summary

| Symptom | Root cause | Fix |
|---------|-----------|-----|
| Low IPC on FP-heavy loops | Scalar FP instructions | Add `-O3 -march=native` or write AVX2 intrinsics |
| Compiler fails to auto-vec | Data dependencies, aliasing | Use `restrict`, remove conditional branches in loop |
| Partial vectorisation | Remainder loop | Pad arrays to multiple of vector width, or use `__builtin_assume_aligned` |
