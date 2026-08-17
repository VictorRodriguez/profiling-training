# profiling-training

A curated collection of C performance-bottleneck case studies, each paired with
a Makefile that demonstrates how to fix the issue using GCC flags, and a README
that walks through profiling with **perf**, **Intel VTune**, **objdump**, and
**Flame Graphs**.

## Case Studies

| Directory | Topic | Tools covered |
|-----------|-------|---------------|
| [01_loop_interchange](01_loop_interchange/) | Cache-friendly memory access via loop interchange | `perf stat`, `perf record`/`report`, Flame Graphs |
| [02_fp32_vs_fp16](02_fp32_vs_fp16/) | FP32 vs FP16 / scalar vs vectorised floating-point | `perf stat`, `objdump`, VTune |
| [03_avx_isa](03_avx_isa/) | Scalar vs. AVX/AVX2 SIMD vectorisation | `perf stat`, `objdump`, Flame Graphs |
| [04_docker_profiling](04_docker_profiling/) | Detecting CPU/memory bottlenecks inside containers **without** application access (CSP scenario) | `docker stats`, `nsenter`, `perf`, `bpftrace`, `sysdig` |
| [05_npu](05_npu/) | NPU stress with OpenVINO CNN inference on Intel NPU | VTune `npu` collection, OpenVINO |
| [06_gpu](06_gpu/) | GPU stress with SYCL matrix multiplication on Intel integrated GPU | VTune `gpu-hotspots` collection, SYCL/oneAPI |

## Prerequisites

```bash
# Ubuntu / Debian (for CPU/Linux examples)
sudo apt-get install -y linux-tools-common linux-tools-$(uname -r) \
    linux-tools-generic gcc make

# Flame Graphs (Brendan Gregg)
git clone https://github.com/brendangregg/FlameGraph /opt/FlameGraph

# Intel VTune (optional, requires registration)
# https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html
```

### Additional prerequisites for GPU and NPU examples (Windows)

The `05_npu` and `06_gpu` examples target native Windows with an Intel Core Ultra laptop.
Run them from an **Intel oneAPI command prompt** or initialise the environment first:

```bat
"C:\Program Files (x86)\Intel\oneAPI\setvars.bat"
```

- Windows 11 laptop with Intel Core Ultra / Intel GPU / Intel NPU.
- Intel VTune Profiler 2024.1 or newer.
- Intel oneAPI Base Toolkit (required for the SYCL GPU demo).
- Intel NPU driver installed (Intel AI Boost / NPU visible in Device Manager).
- Python 3.10+ and OpenVINO (required for the NPU demo).

## Future Work

* Additional SIMD case studies (AVX-512, AMX)
