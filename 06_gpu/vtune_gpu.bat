@echo off
vtune -collect gpu-hotspots -r vtune_gpu_result -- gpu_matmul.exe 2048 40
