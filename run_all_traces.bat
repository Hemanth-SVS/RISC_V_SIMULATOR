@echo off
setlocal enabledelayedexpansion

echo Running all 10 trace files...
echo Results will be saved to phase3_results.txt
echo. > phase3_results.txt

for /L %%i in (1,1,10) do (
    set "num=0%%i"
    set "num=!num:~-2!"
    echo === Trace !num! === >> phase3_results.txt
    echo Running trace!num!...
    simulator.exe phase3_traces\trace!num!.trace phase3_config.cfg >> phase3_results.txt 2>&1
    echo. >> phase3_results.txt
    echo. >> phase3_results.txt
)

echo All traces completed. Results saved to phase3_results.txt
