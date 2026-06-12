# Case Study 01 – Loop Interchange & Cache Performance

## Overview

A 2-D array in C is laid out in **row-major order**: `matrix[0][0]`,
`matrix[0][1]`, … are contiguous in memory.  When the inner loop walks
*down a column* (`matrix[0][j]`, `matrix[1][j]`, …) every access skips
an entire row, causing a **cache miss** on virtually every load.
Swapping the loop order so the inner loop walks *along a row* keeps
accesses sequential and dramatically reduces cache misses.

```
Bad  (column-major):  for j → for i  → matrix[i][j]   ← stride N
Good (row-major):     for i → for j  → matrix[i][j]   ← stride 1
```

## Build

```bash
make        # builds both loop_bad and loop_good
make run    # builds and runs both, printing timings
```

## Profiling with `perf`

### 1. Hardware counter overview (`perf stat`)

```bash
# Bad version – expect high cache-miss rate
perf stat -e cache-references,cache-misses,instructions,cycles ./loop_bad

# Good version – expect low cache-miss rate
perf stat -e cache-references,cache-misses,instructions,cycles ./loop_good
```

Key metric to watch: **cache-miss rate** = `cache-misses / cache-references`.
The bad version typically shows > 90 % misses; the good version drops below 5 %.

### 2. Sampling profile (`perf record` / `perf report`)

```bash
perf record -g ./loop_bad
perf report
```

In the report you will see `sum_column_major` consuming the vast majority of
cycles, annotated with `LOAD` instructions that miss the L1/L2 cache.

### 3. Flame Graph

```bash
perf record -F 999 -g ./loop_bad
perf script > out.perf
/opt/FlameGraph/stackcollapse-perf.pl out.perf > out.folded
/opt/FlameGraph/flamegraph.pl out.folded > flamegraph.svg
# Open flamegraph.svg in a browser
```

The flame graph will show a wide tower for `sum_column_major`, confirming it
dominates CPU time.

## Profiling with Intel VTune

```bash
vtune -collect memory-access -result-dir vtune_bad -- ./loop_bad
vtune -report summary -result-dir vtune_bad

vtune -collect memory-access -result-dir vtune_good -- ./loop_good
vtune -report summary -result-dir vtune_good
```

Compare **LLC Miss Count** and **Bound on Memory** in both reports.

## Inspecting with `objdump`

```bash
objdump -d -S loop_bad  | grep -A5 "sum_column_major"
objdump -d -S loop_good | grep -A5 "sum_row_major"
```

With `-O2`, the compiler may auto-vectorise `sum_row_major` and emit `vmovsd`/
`vaddsd` (or wider `ymm` registers), while `loop_bad` at `-O0` stays scalar.

## Expected Results

| Binary      | Approximate time | Cache-miss rate |
|-------------|-----------------|-----------------|
| `loop_bad`  | ~500–800 ms     | > 90 %          |
| `loop_good` | ~80–150 ms      | < 5 %           |

*(Results vary by CPU model and cache sizes.)*

## Fix Summary

| Symptom | Root cause | Fix |
|---------|-----------|-----|
| High cache-miss rate, slow matrix traversal | Column-major access pattern on a row-major array | Swap loop order so the innermost index matches the last array dimension |
