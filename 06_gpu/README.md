# Case Study 06 — Finding a GPU Launch-Granularity Problem with Intel VTune Profiler

## Overview

This case study teaches a simple GPU performance-debugging workflow:

```text
Bad GPU code
    |
    v
Measure baseline runtime
    |
    v
VTune GPU Offload
    |
    v
Observe inefficient GPU utilization / many small compute tasks
    |
    v
Inspect the source code
    |
    v
Reduce kernel-launch fragmentation
    |
    v
Rebuild and re-profile
    |
    v
Verify the improvement with VTune
```

The example uses **SYCL** matrix multiplication on an Intel GPU.

The mathematical operation is intentionally simple. The performance defect is
also intentional: the bad version submits **one GPU kernel for every matrix
row**, creating thousands of relatively small GPU tasks.

The fixed version submits the complete matrix operation as a **single 2-D
kernel per repetition**.

The objective is not to build the fastest possible matrix multiplication.
The objective is to learn how to use **Intel VTune Profiler** to move from a
performance symptom to evidence, then from evidence to a source-code fix.

---

## Learning Objectives

After completing the exercise, you should be able to:

1. establish a GPU performance baseline;
2. use VTune **GPU Offload** analysis as a first-level GPU investigation;
3. identify excessive kernel submission and poor launch granularity;
4. correlate VTune observations with the source code;
5. change the GPU decomposition without changing the mathematical result;
6. re-profile the optimized version;
7. verify that the performance problem was actually reduced.

A key profiling principle used throughout this training repository is:

> **Measure first. Change the code second. Measure again.**

---

## Files

```text
06_gpu/
├── README.md
├── build_windows.bat
├── run_gpu.bat
├── vtune_offload.bat
├── vtune_hotspots.bat
├── gpu_matmul_bad.cpp
└── gpu_matmul_good.cpp
```

### `gpu_matmul_bad.cpp`

The intentionally inefficient implementation.

For every repetition, the host executes:

```cpp
for (int row = 0; row < n; ++row) {
    q.parallel_for(...);
}
```

With:

```text
N = 1024
REPEATS = 8
```

the program submits:

```text
1024 × 8 = 8192 GPU kernels
```

Each launch performs useful work, but the decomposition is unnecessarily
fragmented.

### `gpu_matmul_good.cpp`

The corrected implementation.

Each repetition launches one 2-D GPU kernel:

```cpp
q.parallel_for(
    sycl::range<2>(n, n),
    ...
);
```

For the same configuration:

```text
1 × 8 = 8 GPU kernels
```

The computation is still matrix multiplication. The main change is the
**granularity of GPU work submission**.

---

## Why This Is a Useful VTune Exercise

GPU performance problems are not always caused by the arithmetic inside a
kernel.

A program may contain perfectly parallel work and still use the GPU
inefficiently because the host presents that work badly.

Common examples include:

- too many small kernel launches;
- synchronization after every small operation;
- repeated host/device transfers;
- insufficient work per GPU submission;
- gaps between GPU tasks;
- CPU-side scheduling overhead;
- poorly balanced compute and memory behavior.

This case study isolates the first problem:

> **Too many small GPU kernel submissions.**

That makes the profiler evidence easier to interpret.

---

# 1. Requirements

The example assumes Windows and an Intel GPU.

Install and configure:

- Intel oneAPI DPC++/C++ Compiler;
- Intel VTune Profiler;
- Intel GPU driver;
- the runtime required for SYCL GPU execution.

Run the commands from an Intel oneAPI command prompt, or otherwise initialize
the oneAPI environment before compiling and profiling.

You should be able to run:

```bat
icx --version
vtune --version
```

and the SYCL application should report an Intel GPU when executed.

---

# 2. Build

Run:

```bat
build_windows.bat
```

This creates:

```text
gpu_matmul_bad.exe
gpu_matmul_good.exe
```

Debug information is retained so that VTune has better source-level context.

---

# 3. Establish the Baseline

Run both applications before opening VTune:

```bat
run_gpu.bat
```

The script uses:

```text
N       = 1024
REPEATS = 8
```

You can also execute them directly:

```bat
gpu_matmul_bad.exe 1024 8
gpu_matmul_good.exe 1024 8
```

Record at least:

| Measurement | Bad | Good |
|---|---:|---:|
| Elapsed kernel region | | |
| Approx. TFLOP/s | | |
| Checksum | | |

The checksum should remain comparable between versions.

Do not assume a specific speedup. The magnitude depends on the GPU,
driver, compiler, clocking, and system state.

The important question is:

> **Does the profiler evidence explain the measured difference?**

---

# 4. Profile the Bad Version with GPU Offload

Start with VTune **GPU Offload** analysis.

Run:

```bat
vtune_offload.bat bad
```

Equivalent command:

```bat
vtune -collect gpu-offload -r vtune_gpu_offload_bad -- gpu_matmul_bad.exe 1024 8
```

Open the result:

```bat
vtune-gui vtune_gpu_offload_bad
```

If your installation launches the VTune GUI differently, open VTune manually
and load the result directory.

---

# 5. What to Inspect in VTune

Start with the **Summary** and then inspect the GPU timeline.

Do not search for one magic metric. Build an argument from several
observations.

## A. GPU Utilization

Ask:

- Is the GPU continuously busy?
- Are there visible idle gaps?
- Does GPU activity look fragmented?
- Is the workload spending substantial time outside sustained GPU execution?

The bad implementation creates many independent GPU tasks.

You should therefore look for evidence consistent with fragmented execution
rather than one long, efficiently submitted compute phase.

## B. Compute Tasks

Inspect the GPU compute tasks.

Ask:

- How many kernel instances are visible?
- Are there many short kernels?
- Is the same kernel invoked repeatedly?
- Does the timeline show repeated submission/execution patterns?

This is the strongest clue in this exercise.

The source code submits one kernel per row.

For the default input:

```text
8192 kernel submissions
```

should be expected from the program structure.

## C. CPU and GPU Timeline

Correlate CPU activity with GPU activity.

Look for a repeating pattern resembling:

```text
CPU submits work
GPU executes a small task
CPU submits more work
GPU executes another task
...
```

The exact visualization varies by platform and VTune version, but the
important question remains:

> Is the GPU receiving work in unnecessarily small pieces?

---

# 6. Form the Performance Hypothesis

After inspecting VTune, formulate a hypothesis before reading the fixed code.

A suitable hypothesis is:

> The workload exposes enough total parallel work for the GPU, but the host
> fragments that work into thousands of kernel submissions. Kernel launch and
> scheduling overhead, together with reduced work available per submission,
> prevents the GPU from being used as efficiently as it could be.

This is stronger than saying only:

> "The GPU is slow."

A profiler should help connect a performance symptom to a mechanism.

---

# 7. Locate the Problem in the Source

Open:

```text
gpu_matmul_bad.cpp
```

The critical structure is:

```cpp
for (int r = 0; r < repeats; ++r) {
    for (int row = 0; row < n; ++row) {
        q.parallel_for(
            sycl::range<1>(n),
            ...
        );
    }
}
```

The decomposition is:

```text
Host repetition
    |
    +-- row 0   -> GPU kernel
    +-- row 1   -> GPU kernel
    +-- row 2   -> GPU kernel
    +-- ...
    +-- row N-1 -> GPU kernel
```

The GPU is capable of expressing both matrix dimensions as parallel work.

There is no need to create a separate kernel launch for each row.

---

# 8. Apply the Fix

The good version changes the decomposition to:

```cpp
q.parallel_for(
    sycl::range<2>(n, n),
    [=](sycl::id<2> idx) {
        int row = idx[0];
        int col = idx[1];

        ...
    });
```

Now the decomposition becomes:

```text
Host repetition
    |
    +-- one 2-D GPU kernel
          |
          +-- all rows
          +-- all columns
```

For the default configuration, the launch count changes approximately from:

```text
BAD  : 8192 launches
GOOD :    8 launches
```

This is a source-level optimization justified by profiler evidence.

---

# 9. Rebuild and Re-run

Build again:

```bat
build_windows.bat
```

Run:

```bat
run_gpu.bat
```

Compare the new runtime with the baseline.

At this point you have a performance measurement, but the optimization
exercise is not finished.

You still need to verify the mechanism with VTune.

---

# 10. Profile the Fixed Version

Collect a second GPU Offload result:

```bat
vtune_offload.bat good
```

Open:

```bat
vtune-gui vtune_gpu_offload_good
```

Compare the bad and good results.

Look specifically for:

- dramatically fewer GPU compute tasks;
- longer useful work per submission;
- less fragmented GPU activity;
- reduced gaps or submission overhead;
- improved overall execution time.

The exact values are platform dependent.

The important result is that the **observed behavior should change in the
direction predicted by the hypothesis**.

---

# 11. Use GPU Compute/Media Hotspots

GPU Offload is useful for understanding CPU/GPU interaction and whether work
is being offloaded effectively.

After finding the launch-granularity issue, use VTune
**GPU Compute/Media Hotspots** to inspect the GPU kernels in more detail.

Profile the bad version:

```bat
vtune_hotspots.bat bad
```

Profile the good version:

```bat
vtune_hotspots.bat good
```

Equivalent commands:

```bat
vtune -collect gpu-hotspots -r vtune_gpu_hotspots_bad -- gpu_matmul_bad.exe 1024 8

vtune -collect gpu-hotspots -r vtune_gpu_hotspots_good -- gpu_matmul_good.exe 1024 8
```

Use this analysis to examine the compute task itself, including the metrics
available for your Intel GPU architecture.

Depending on the platform, useful questions include:

- Are execution units active, stalled, or idle?
- Is the kernel compute-bound or memory-bound?
- What memory hierarchy behavior does VTune report?
- Is bandwidth becoming a limiting factor?
- After launch overhead is reduced, does another bottleneck become visible?

This illustrates an important optimization rule:

> **Removing one bottleneck may expose the next bottleneck.**

---

# 12. Recommended VTune Workflow

For this exercise, use the following sequence:

```text
1. Native execution
       |
       v
2. GPU Offload
       |
       +--> Is the GPU receiving work effectively?
       +--> Are there idle gaps?
       +--> Are there too many small tasks?
       |
       v
3. Source inspection
       |
       v
4. Code change
       |
       v
5. Native execution again
       |
       v
6. GPU Offload again
       |
       v
7. GPU Compute/Media Hotspots
       |
       +--> What limits the remaining kernel performance?
```

This follows a more useful profiling methodology than immediately opening a
large set of low-level GPU counters.

---

# 13. Student Exercise

Before looking at `gpu_matmul_good.cpp`, profile only the bad version.

Answer the following questions.

### Observation

1. How long does the application run?
2. How much of the execution involves GPU activity?
3. How many GPU compute tasks are visible?
4. Is GPU activity continuous or fragmented?
5. Which kernel dominates GPU execution?
6. What pattern is visible in the timeline?

### Hypothesis

Write one sentence explaining the most likely performance problem.

### Evidence

Identify at least two pieces of VTune evidence supporting the hypothesis.

Example structure:

```text
Claim:
The application submits GPU work at inefficient granularity.

Evidence 1:
...

Evidence 2:
...
```

### Fix

Inspect the source code and identify the construct responsible for the
behavior.

Then implement or review the fixed version.

### Verification

Re-profile and answer:

1. Did kernel count decrease?
2. Did GPU execution become less fragmented?
3. Did runtime improve?
4. Did the checksum remain comparable?
5. What is the next bottleneck reported by VTune?

---

# 14. Expected Learning, Not Expected Numbers

This repository should not hard-code claims such as:

```text
"The optimized version is always 4x faster."
```

GPU performance depends on:

- GPU generation;
- driver version;
- compiler version;
- power and thermal state;
- matrix size;
- profiling overhead;
- system activity.

Instead, the exercise expects a qualitative transition:

```text
BAD
many small GPU submissions
        |
        v
VTune evidence
        |
        v
source-level diagnosis
        |
        v
GOOD
fewer, larger GPU submissions
        |
        v
VTune verification
```

The measured speedup is evidence from the local platform, not a universal
constant.

---

# 15. Why the Good Version Is Still Not an Optimized GEMM

The fixed version intentionally solves only one issue.

It is **not** intended to compete with optimized BLAS or oneMKL
implementations.

Further optimizations could include:

- work-group tuning;
- local/shared-memory tiling;
- reducing global-memory traffic;
- improving data reuse;
- vectorization;
- sub-group optimizations;
- specialized matrix instructions where supported;
- using an optimized math library.

Those are excellent follow-up experiments, but adding them to the first fix
would make it harder to determine which change solved the original problem.

For profiling training, one controlled change is more valuable than many
simultaneous optimizations.

---

# 16. Key Takeaway

The purpose of VTune is not merely to produce charts.

The useful workflow is:

```text
measurement
    -> observation
    -> hypothesis
    -> source-code cause
    -> targeted change
    -> re-measurement
```

In this case:

```text
Many small GPU kernels
    -> fragmented GPU execution
    -> excessive launch granularity
    -> collapse row launches into one 2-D launch
    -> fewer GPU tasks
    -> verify with VTune
```

That is the performance-engineering skill this case study is designed to
teach.
