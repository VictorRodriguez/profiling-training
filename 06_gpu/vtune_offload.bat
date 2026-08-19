@echo off
setlocal

if "%~1"=="" goto usage

if /I "%~1"=="bad" (
    set APP=gpu_matmul_bad.exe
    set RESULT=vtune_gpu_offload_bad
    goto run
)

if /I "%~1"=="good" (
    set APP=gpu_matmul_good.exe
    set RESULT=vtune_gpu_offload_good
    goto run
)

:usage
echo Usage: vtune_offload.bat bad^|good
exit /b 1

:run
echo Profiling %APP% with VTune GPU Offload...
if exist "%RESULT%" rmdir /S /Q "%RESULT%"
vtune -collect gpu-offload -r "%RESULT%" -- %APP% 1024 8
