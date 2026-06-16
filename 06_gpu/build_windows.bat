@echo off
setlocal
icx /EHsc /O2 /Qstd=c++17 /fsycl gpu_matmul.cpp /Fe:gpu_matmul.exe
if errorlevel 1 exit /b 1
echo Built gpu_matmul.exe
