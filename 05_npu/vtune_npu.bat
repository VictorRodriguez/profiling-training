@echo off
vtune -collect xpu-offload -knob profile-npu=true -r vtune_npu_result -- python npu_cnn_stress.py --device NPU --seconds 60 --requests 4
