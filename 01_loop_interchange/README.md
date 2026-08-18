# Case Study 01 – Loop Interchange & Cache Locality

## Overview

This case study demonstrates how **loop order affects cache locality** and how GCC can automatically apply **loop interchange**.

C stores 2-D arrays in **row-major order**, so elements in the same row are contiguous in memory.

A cache-unfriendly traversal changes the row index in the inner loop:

```c
for (int j = 0; j < N; ++j) {
    for (int i = 0; i < N; ++i) {
        B[i][j] = A[i][j] * 2.0;
    }
}
```

For an `N x N` matrix of `double`, the inner loop advances by:

```text
N * sizeof(double)
```

A cache-friendly traversal changes the column index in the inner loop:

```c
for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
        B[i][j] = A[i][j] * 2.0;
    }
}
```

Now consecutive accesses are adjacent `double` values:

```text
sizeof(double) = 8 bytes
```

This improves spatial locality and cache-line utilization.

---

## Experiment

The example builds three binaries from the same source file:

| Binary | Source traversal | Compiler option | Purpose |
|---|---|---|---|
| `loop_bad` | Column-major | `-fno-loop-interchange` | Preserve the cache-unfriendly loop |
| `loop_compiler` | Column-major | `-floop-interchange` | Allow GCC to interchange the loops |
| `loop_good` | Row-major | Manual interchange | Programmer-optimized reference |

Conceptually:

```text
loop_bad
    column-major source
    -fno-loop-interchange
            |
            v
    cache-unfriendly generated code


loop_compiler
    same column-major source
    -floop-interchange
            |
            v
    compiler may change loop order


loop_good
    manually interchanged source
            |
            v
    row-major traversal
```

---

## Build

Build all binaries:

```bash
make
```

When GCC successfully applies loop interchange, the compiler may report:

```text
optimized: loops interchanged in loop nest
```

Run all three cases:

```bash
make run
```

---

## Profiling with `perf`

Run the complete comparison:

```bash
make perf
```

The Makefile executes commands equivalent to:

```bash
perf stat -e cycles,instructions,cache-references,cache-misses ./loop_bad
perf stat -e cycles,instructions,cache-references,cache-misses ./loop_compiler
perf stat -e cycles,instructions,cache-references,cache-misses ./loop_good
```

Useful metrics include:

- execution time
- cycles
- instructions
- IPC
- cache references
- cache misses

Generic Linux cache events are hardware-dependent, so focus primarily on the **relative behavior between the binaries** rather than fixed cache-miss thresholds.

---

## Expected Behavior

The cache-unfriendly version should generally show:

```text
large memory stride
        |
        v
poor spatial locality
        |
        v
more cache / memory-system pressure
        |
        v
more stalled cycles
        |
        v
lower IPC
        |
        v
longer execution time
```

If GCC applies `-floop-interchange`, `loop_compiler` should behave similarly to the manually optimized `loop_good` version.

---

## Verify the Compiler Transformation

Do not assume the transformation occurred only because the flag was enabled.

Compile with optimization diagnostics:

```bash
gcc -O2 \
    -floop-interchange \
    -fopt-info-loop-optimized \
    source.c \
    -o program
```

When GCC applies the optimization, it may report:

```text
optimized: loops interchanged in loop nest
```

The important distinction is:

```text
optimization enabled != optimization applied
```

GCC still performs legality and profitability analysis before changing the loop nest.

---

## Why Some Loops Are Not Interchanged

A loop such as:

```c
sum += matrix[i][j];
```

contains a reduction dependency.

Interchanging the loops changes the order of floating-point additions, and floating-point addition is not strictly associative.

Therefore, GCC may decide that the transformation cannot be safely applied while preserving program semantics.

A better loop-interchange candidate has independent iterations, for example:

```c
B[i][j] = A[i][j] * 2.0;
```

---

## Key Takeaway

Loop order can significantly affect performance without changing the high-level algorithm.

```text
poor loop order
      |
      v
poor spatial locality
      |
      v
more memory stalls
      |
      v
lower IPC
      |
      v
longer execution time
```

Loop interchange can improve this automatically when the compiler determines that the transformation is legal and profitable.

For deeper details about dependence analysis, failed interchange cases, assembly inspection, vectorization, `perf record`, Flame Graphs, and VTune, see the accompanying Wiki page.

