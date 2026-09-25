@echo off
rem Double-click to open AlgoPlayground. Builds it first if it has not been built yet.
set "APP=%~dp0build\Release\AlgorithmVisualizer.exe"
if not exist "%APP%" (
    echo Building AlgoPlayground for the first time, this can take a minute...
    cmake -S "%~dp0." -B "%~dp0build" || goto :fail
    cmake --build "%~dp0build" --config Release || goto :fail
)
start "" "%APP%"
exit /b 0
:fail
echo Build failed - see the messages above.
pause
