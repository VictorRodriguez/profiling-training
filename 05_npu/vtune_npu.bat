@echo off
vtune -collect npu -r vtune_npu_result -- python npu_cnn_stress.py --device NPU --seconds 60 --requests 4
