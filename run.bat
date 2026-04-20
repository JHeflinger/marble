@echo off
call build.bat %1 %2 %3
if %ERRORLEVEL% NEQ 0 (
    exit /b %ERRORLEVEL%
)
xcopy build\shaders penv\build\shaders /E /I /Q >nul 2>&1
cd penv
"./../build/bin.exe"
cd ..