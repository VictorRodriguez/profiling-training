@echo off
setlocal

echo Building intentionally bad version...
icx /EHsc /O2 /Zi /Qstd=c++17 /fsycl gpu_matmul_bad.cpp /Fe:gpu_matmul_bad.exe
if errorlevel 1 exit /b 1

echo Building fixed version...
icx /EHsc /O2 /Zi /Qstd=c++17 /fsycl gpu_matmul_good.cpp /Fe:gpu_matmul_good.exe
if errorlevel 1 exit /b 1

echo.
echo Built:
echo   gpu_matmul_bad.exe
echo   gpu_matmul_good.exe
