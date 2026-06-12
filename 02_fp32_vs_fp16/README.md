# Case Study 02 – FP32 vs FP16 / Floating-Point Precision & Throughput

## Overview

Modern CPUs and compilers can process **FP32 (single-precision)** data at
roughly **2× the throughput** of **FP64 (double-precision)** because SIMD
registers pack twice as many 32-bit lanes as 64-bit ones.

**FP16 (half-precision)** takes this further:
* Half the memory footprint → half the bandwidth pressure.
* AVX-512 FP16 (Sapphire Rapids+) and ARM NEON support native FP16 arithmetic.
* A common production pattern for ML inference is to *store* weights in FP16
  and *accumulate* in FP32, keeping bandwidth low while preserving precision.

This case study measures a dot-product across 16 M elements using three
representations and three compiler optimisation levels.

## Build

```bash
make        # fp64_scalar, fp32_scalar, fp32_vector
make run    # build and print timings side-by-side
make asm    # print disassembly of the hot loops
```

## Profiling with `perf stat`

```bash
perf stat -e cycles,instructions,fp_arith_inst_retired.128b_packed_single,\
fp_arith_inst_retired.256b_packed_single \
    ./fp64_scalar

perf stat -e cycles,instructions,fp_arith_inst_retired.128b_packed_single,\
fp_arith_inst_retired.256b_packed_single \
    ./fp32_vector
```

Key counters to watch:
* `fp_arith_inst_retired.256b_packed_single` – YMM (AVX2) 256-bit FP32 ops.
* `fp_arith_inst_retired.128b_packed_single` – XMM (SSE) 128-bit FP32 ops.

The vectorised FP32 binary should show a **much higher 256b count** and
significantly more **instructions per cycle (IPC)**.

## Inspecting with `objdump`

```bash
# Look for scalar movsd/addsd (FP64) vs packed vmulps/vaddps (FP32 AVX)
objdump -d fp64_scalar  | grep -E "movsd|addsd|mulsd"
objdump -d fp32_vector  | grep -E "vmulps|vaddps|ymm"
```

With `-O3 -march=native -ffast-math` the compiler should emit `ymm` register
instructions operating on 8 FP32 values at once instead of 1 FP64 value.

## Profiling with Intel VTune

```bash
# Collect vectorisation metrics
vtune -collect hotspots -knob sampling-mode=hw \
      -result-dir vtune_fp64  -- ./fp64_scalar
vtune -collect hotspots -knob sampling-mode=hw \
      -result-dir vtune_fp32v -- ./fp32_vector

vtune -report summary -result-dir vtune_fp64
vtune -report summary -result-dir vtune_fp32v
```

Open the **Vectorization Intensity** column in the Bottom-up view to confirm
that `dot_fp32` in the vectorised binary achieves near-100 % vectorisation.

## Flame Graph

```bash
perf record -F 999 -g ./fp32_vector
perf script > out.perf
/opt/FlameGraph/stackcollapse-perf.pl out.perf > out.folded
/opt/FlameGraph/flamegraph.pl out.folded > flamegraph.svg
```

## Expected Results

| Binary          | Typical time (16 M elements) | Notes |
|-----------------|------------------------------|-------|
| `fp64_scalar`   | ~80–120 ms | FP64, scalar |
| `fp32_scalar`   | ~40–80 ms  | FP32, may auto-vec at -O2 |
| `fp32_vector`   | ~10–30 ms  | FP32, AVX2 256-bit |

*(Highly dependent on CPU, cache, and DRAM bandwidth.)*

## GCC Flags Reference

| Flag | Effect |
|------|--------|
| `-O2` | Standard optimisation; limited vectorisation |
| `-O3` | Aggressive optimisation; enables loop vectorisation |
| `-march=native` | Emit instructions for the host CPU (enables AVX2/AVX-512) |
| `-ffast-math` | Allows reassociation and approximate FP maths (enables better auto-vec) |
| `-ftree-vectorize` | Explicit tree-vectorisation pass (implied by `-O3`) |
| `-funroll-loops` | Unroll inner loops to expose more ILP |

## Fix Summary

| Symptom | Root cause | Fix |
|---------|-----------|-----|
| FP compute-bound, low IPC | Scalar FP64 loop | Switch to FP32 + `-O3 -march=native -ffast-math` |
| Memory-bandwidth bound | Large FP32 arrays | Store tensors/weights in FP16; convert on the fly |
