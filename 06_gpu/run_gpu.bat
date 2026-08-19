@echo off
setlocal

set N=1024
set REPEATS=8

echo ============================================================
echo BAD VERSION
echo ============================================================
gpu_matmul_bad.exe %N% %REPEATS%
if errorlevel 1 exit /b 1

echo.
echo ============================================================
echo GOOD VERSION
echo ============================================================
gpu_matmul_good.exe %N% %REPEATS%
