@echo off
REM Build script for ClipboardMonitor
REM Requires Visual Studio with C++ tools installed

echo Looking for Visual Studio...

REM Try to find Visual Studio Developer Command Prompt
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do (
        set "VSINSTALL=%%i"
    )
)

if defined VSINSTALL (
    echo Found Visual Studio at: %VSINSTALL%
    call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo Visual Studio not found. Please run from Developer Command Prompt.
    pause
    exit /b 1
)

echo.
echo Compiling ClipboardMonitor...

if not exist bin mkdir bin

cl.exe /EHsc /std:c++17 /W4 /O2 /DUNICODE /D_UNICODE ^
    /Fe:bin\ClipboardMonitor.exe ^
    main.cpp clipboard_monitor.cpp storage.cpp ^
    context\async_executor.cpp context\context_manager.cpp ^
    context\adapters\browser_adapter.cpp ^
    context\utils\ui_automation_helper.cpp context\utils\html_parser.cpp ^
    /link user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib shlwapi.lib oleacc.lib uiautomationcore.lib ^
    /SUBSYSTEM:WINDOWS

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful!
    echo Executable: bin\ClipboardMonitor.exe
    del *.obj 2>nul
) else (
    echo.
    echo Build failed with error code: %ERRORLEVEL%
)

pause
