@echo off
REM Build native_host.exe
REM Run from Developer Command Prompt for VS 2022

cd /d "%~dp0"

echo Building native_host.exe...

cl.exe /EHsc /std:c++17 /W4 /O2 /DUNICODE /D_UNICODE ^
    /Fe:native_host.exe ^
    native_host.cpp ^
    /link shell32.lib

if %ERRORLEVEL% EQU 0 (
    echo Build successful!
    echo Output: native_host.exe
) else (
    echo Build failed!
)

pause
