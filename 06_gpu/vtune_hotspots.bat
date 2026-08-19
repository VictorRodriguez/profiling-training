@echo off
setlocal

if "%~1"=="" goto usage

if /I "%~1"=="bad" (
    set APP=gpu_matmul_bad.exe
    set RESULT=vtune_gpu_hotspots_bad
    goto run
)

if /I "%~1"=="good" (
    set APP=gpu_matmul_good.exe
    set RESULT=vtune_gpu_hotspots_good
    goto run
)

:usage
echo Usage: vtune_hotspots.bat bad^|good
exit /b 1

:run
echo Profiling %APP% with VTune GPU Compute/Media Hotspots...
if exist "%RESULT%" rmdir /S /Q "%RESULT%"
vtune -collect gpu-hotspots -r "%RESULT%" -- %APP% 1024 8
