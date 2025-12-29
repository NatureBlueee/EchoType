@echo off
REM ClipboardMonitor Browser Extension - Installation Script
REM Run this as Administrator

echo ================================================
echo ClipboardMonitor Browser Extension Installer
echo ================================================
echo.

REM Get the extension ID from user
set /p EXTENSION_ID="Enter Chrome Extension ID: "

if "%EXTENSION_ID%"=="" (
    echo Error: Extension ID is required!
    pause
    exit /b 1
)

REM Set paths
set NATIVE_HOST_NAME=com.clipboardmonitor.context
set NATIVE_HOST_PATH=%APPDATA%\ClipboardMonitor\native_host.exe
set MANIFEST_PATH=%APPDATA%\ClipboardMonitor\%NATIVE_HOST_NAME%.json

REM Create directory if not exists
if not exist "%APPDATA%\ClipboardMonitor" mkdir "%APPDATA%\ClipboardMonitor"

REM Copy native host executable (if exists in current directory)
if exist "native_host.exe" (
    copy /Y "native_host.exe" "%NATIVE_HOST_PATH%"
    echo Copied native_host.exe to %NATIVE_HOST_PATH%
) else (
    echo Warning: native_host.exe not found in current directory
)

REM Create manifest JSON with correct Extension ID
echo Creating manifest file...
(
echo {
echo   "name": "%NATIVE_HOST_NAME%",
echo   "description": "ClipboardMonitor Native Messaging Host",
echo   "path": "%NATIVE_HOST_PATH:\=\\%",
echo   "type": "stdio",
echo   "allowed_origins": [
echo     "chrome-extension://%EXTENSION_ID%/"
echo   ]
echo }
) > "%MANIFEST_PATH%"
echo Created %MANIFEST_PATH%

REM Register in Windows Registry for Chrome
echo Registering native messaging host for Chrome...
reg add "HKCU\Software\Google\Chrome\NativeMessagingHosts\%NATIVE_HOST_NAME%" /ve /d "%MANIFEST_PATH%" /f

REM Register for Edge (uses same format)
echo Registering native messaging host for Edge...
reg add "HKCU\Software\Microsoft\Edge\NativeMessagingHosts\%NATIVE_HOST_NAME%" /ve /d "%MANIFEST_PATH%" /f

echo.
echo ================================================
echo Installation complete!
echo ================================================
echo.
echo Next steps:
echo 1. Load the extension in Chrome: chrome://extensions
echo 2. Enable "Developer mode"
echo 3. Click "Load unpacked" and select the browser_extension folder
echo 4. Copy the Extension ID and re-run this script if needed
echo.
pause
