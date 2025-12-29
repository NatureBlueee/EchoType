@echo off
REM Build script for ClipboardMonitor
REM Requires Visual Studio with C++ tools installed

echo Looking for Visual Studio...

REM Try to find Visual Studio using vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do (
        set "VSINSTALL=%%i"
    )
)

REM Fallback: check if cl.exe is already in PATH (running from Developer Command Prompt)
if not defined VSINSTALL (
    where cl.exe >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo Using cl.exe from PATH
        goto :compile
    )
)

if defined VSINSTALL (
    echo Found Visual Studio at: %VSINSTALL%
    call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo.
    echo Visual Studio not found automatically.
    echo Please run from "Developer Command Prompt for VS 2022"
    echo.
    echo Or run this in PowerShell:
    echo   cmd /c """C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"" && cd /d d:\Profolio\EchoType\playground\ClipboardMonitor && build.bat"
    pause
    exit /b 1
)

:compile
echo.
echo Compiling ClipboardMonitor...

if not exist bin mkdir bin

cl.exe /EHsc /std:c++17 /W4 /O2 /DUNICODE /D_UNICODE /utf-8 ^
    /Fe:bin\ClipboardMonitor.exe ^
    main.cpp clipboard_monitor.cpp storage.cpp ^
    context\async_executor.cpp context\context_manager.cpp ^
    context\adapters\browser_adapter.cpp context\adapters\wechat_adapter.cpp ^
    context\adapters\vscode_adapter.cpp context\adapters\notion_adapter.cpp ^
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
