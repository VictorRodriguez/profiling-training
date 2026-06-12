# Case Study 04 – Profiling Docker Applications Without Source Access (CSP Scenario)

## Overview

In a **Cloud Service Provider (CSP)** or managed-service environment the
operations team often has access to the *host* but not to the application's
source code, build system, or even its container image layers.  This case
study shows how to diagnose CPU and memory bottlenecks **entirely from the
host** using Linux namespaces, eBPF, `perf`, and container-aware tooling.

Two intentionally misbehaving workloads are provided:

| Container | Bottleneck |
|-----------|-----------|
| `cpu_hog` | CPU-bound: tight mathematical loop → 100 % CPU |
| `mem_hog` | Memory-bandwidth-bound: repeated sequential reads of a 512 MB buffer |

## Prerequisites

See the [root README](../README.md) for common prerequisites (`perf`, FlameGraph).
Additional tools for this case study:

```bash
# Host tools
sudo apt-get install -y linux-tools-common linux-tools-$(uname -r) \
    bpftrace sysdig docker.io
```

## Build & Run

```bash
# Build both images and start containers in the background
make build-cpu build-mem
make run-cpu
make run-mem

# Verify containers are running
docker ps
```

## Step 1 – Coarse Metrics with `docker stats`

```bash
# Real-time CPU and memory for all containers
docker stats

# Snapshot (no streaming)
docker stats --no-stream
```

Key columns:
* **CPU %** – percentage of one CPU core.  > 90 % on a single container is
  suspicious.
* **MEM USAGE / LIMIT** – resident set size vs cgroup memory limit.
* **NET I/O** and **BLOCK I/O** – can rule out network/disk as the bottleneck.

Expected output for the two containers:

```
CONTAINER ID   NAME                CPU %   MEM USAGE / LIMIT
xxxxxxxxxxxx   cpu_hog_container   99.x%   10MiB / ...
xxxxxxxxxxxx   mem_hog_container   30-60%  512MiB / 1GiB
```

## Step 2 – Identify the Process Inside the Container

```bash
# Find the PID on the host corresponding to the container process
CPU_PID=$(docker inspect --format '{{.State.Pid}}' cpu_hog_container)
MEM_PID=$(docker inspect --format '{{.State.Pid}}' mem_hog_container)
echo "cpu_hog PID on host: $CPU_PID"
echo "mem_hog PID on host: $MEM_PID"

# Cross-verify with top
top -p $CPU_PID,$MEM_PID
```

## Step 3 – CPU Profiling with `perf` (no source required)

The host `perf` tool can profile a container process using its host PID.

### 3a. Hardware counter overview

```bash
sudo perf stat -p $CPU_PID sleep 5
```

Look for:
* High **cycles** and **instructions** → compute-bound.
* Low **IPC** (< 1) → possible memory stalls.
* `cache-misses / cache-references` ratio → memory-bound if > 20 %.

### 3b. Call-graph sampling

```bash
sudo perf record -F 99 -g -p $CPU_PID -- sleep 10
sudo perf report --stdio | head -60
```

Even without symbols (`[unknown]`) you will see which virtual addresses
consume the most time.  If the binary is unstripped (or if debug packages are
available), full symbol names appear.

### 3c. Flame Graph from container process

```bash
sudo perf record -F 99 -g -p $CPU_PID -- sleep 10
sudo perf script > /tmp/out.perf
/opt/FlameGraph/stackcollapse-perf.pl /tmp/out.perf > /tmp/out.folded
/opt/FlameGraph/flamegraph.pl /tmp/out.folded > /tmp/cpu_hog_flame.svg
# scp or serve the SVG and open in a browser
```

## Step 4 – Memory Profiling with `perf` and `/proc`

### 4a. Memory hardware counters

```bash
sudo perf stat -e cache-misses,cache-references,\
LLC-load-misses,LLC-loads,\
mem-loads,mem-stores \
    -p $MEM_PID sleep 5
```

A high **LLC-load-miss** rate confirms the workload is saturating the
Last-Level Cache and going to DRAM.

### 4b. Memory maps from `/proc`

```bash
# Resident set, virtual size, anonymous mappings
sudo cat /proc/$MEM_PID/status | grep -E "VmRSS|VmSize|VmAnon"

# All memory-mapped regions
sudo cat /proc/$MEM_PID/maps | head -30

# NUMA memory statistics (if NUMA system)
sudo cat /proc/$MEM_PID/numa_maps | head -20
```

### 4c. `smaps` for per-mapping RSS

```bash
sudo cat /proc/$MEM_PID/smaps | awk '/^Size/{s=$2} /^Rss/{print s, $2}' \
    | sort -k2 -n | tail -20
```

This shows which individual mappings (heap, stacks, shared libs) are
consuming the most resident memory.

## Step 5 – Namespace Entry with `nsenter`

If you need to run a profiling tool *inside* the container's namespaces
(PID, network, mount) without modifying the container image:

```bash
# Enter mount and PID namespaces of the cpu_hog container
sudo nsenter -t $CPU_PID --mount --pid -- bash
# Now you are inside the container's view of the filesystem/processes
ps aux
ls /proc/1/exe    # → /usr/local/bin/cpu_hog
exit
```

From inside the namespace you can run `strace`, `ltrace`, or copy a
statically-linked `perf` binary and run it there.

## Step 6 – eBPF / `bpftrace` (kernel-level, no container modification)

`bpftrace` runs on the host and can trace kernel events attributed to a
specific container process.

### CPU scheduler latency

```bash
# Measure on-CPU time per PID (shows how much CPU each task gets)
sudo bpftrace -e '
profile:hz:99 /pid == '"$CPU_PID"'/ {
    @[ustack] = count();
}
interval:s:10 { exit(); }' 2>/dev/null | head -40
```

### Page-fault rate (memory pressure)

```bash
sudo bpftrace -e '
tracepoint:exceptions:page_fault_user /pid == '"$MEM_PID"'/ {
    @faults = count();
}
interval:s:5 {
    print(@faults);
    clear(@faults);
}' 2>/dev/null
```

### DRAM access rate via hardware PMU

```bash
sudo bpftrace -e '
hardware:mem-load-retired:local_pmm:1000 /pid == '"$MEM_PID"'/ {
    @[ustack] = count();
}
interval:s:5 { exit(); }' 2>/dev/null
```

## Step 7 – `sysdig` for System-Call Level Tracing

```bash
# Install
sudo apt-get install -y sysdig

# Capture all syscalls for the container
sudo sysdig container.name=cpu_hog_container > /tmp/cpu_hog.scap &
sleep 10
sudo kill %1

# Inspect: top system calls by count
sudo sysdig -r /tmp/cpu_hog.scap -c topscalls

# CPU time per thread
sudo sysdig -r /tmp/cpu_hog.scap -c topprocs_cpu

# Memory usage over time
sudo sysdig -r /tmp/cpu_hog.scap container.name=mem_hog_container \
    -c fileslower 0
```

## Cleanup

```bash
make stop    # stops and removes containers
make clean   # also removes images
```

## Summary – CSP Profiling Toolkit (No App Access Required)

| Tool | What it answers | Invasiveness |
|------|----------------|--------------|
| `docker stats` | Is the container CPU/memory bound? | None |
| `perf stat -p <pid>` | Which hardware counters are hot? | Very low |
| `perf record -g -p <pid>` | Which functions consume CPU? | Low |
| Flame Graphs | Visual CPU profile | Low |
| `/proc/<pid>/status,maps,smaps` | Memory layout and RSS | None |
| `nsenter` | Run tools inside container namespace | Low |
| `bpftrace` | Kernel-level tracing, page faults, scheduler | Low (eBPF) |
| `sysdig` | System-call level tracing, container-aware | Low–Medium |

> **Tip**: Always correlate two or more tools before drawing conclusions.
> A high CPU % in `docker stats` combined with a low cache-miss rate in
> `perf stat` points to a compute-bound bottleneck; a low CPU % with a
> high LLC-miss rate points to a memory-bandwidth bottleneck.
