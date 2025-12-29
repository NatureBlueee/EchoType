# build.ps1 - PowerShell build script for ClipboardMonitor
# Run this in any PowerShell window

$ErrorActionPreference = "Stop"

Write-Host "Looking for Visual Studio..." -ForegroundColor Cyan

# Find vswhere
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path $vswhere)) {
    Write-Host "ERROR: Visual Studio not found. Please install Visual Studio with C++ tools." -ForegroundColor Red
    exit 1
}

# Get VS installation path
$vsPath = & $vswhere -latest -property installationPath

if (-not $vsPath) {
    Write-Host "ERROR: Could not find Visual Studio installation." -ForegroundColor Red
    exit 1
}

Write-Host "Found Visual Studio at: $vsPath" -ForegroundColor Green

# Find vcvarsall.bat
$vcvarsall = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"

if (-not (Test-Path $vcvarsall)) {
    Write-Host "ERROR: vcvarsall.bat not found. Please install C++ tools in Visual Studio." -ForegroundColor Red
    exit 1
}

Write-Host "Setting up build environment..." -ForegroundColor Cyan

# Create bin directory if not exists
$binDir = Join-Path $PSScriptRoot "bin"
if (-not (Test-Path $binDir)) {
    New-Item -ItemType Directory -Path $binDir | Out-Null
}

# Use cmd to set environment and compile
$compileCmd = @"
call "$vcvarsall" x64
cd /d "$PSScriptRoot"
cl.exe /EHsc /std:c++17 /W4 /O2 /DUNICODE /D_UNICODE /Fe:bin\ClipboardMonitor.exe main.cpp clipboard_monitor.cpp storage.cpp /link user32.lib gdi32.lib shell32.lib ole32.lib shlwapi.lib oleacc.lib /SUBSYSTEM:WINDOWS
"@

$result = cmd /c "$compileCmd" 2>&1
Write-Host $result

# Clean up obj files
Remove-Item -Path "*.obj" -ErrorAction SilentlyContinue

if (Test-Path (Join-Path $binDir "ClipboardMonitor.exe")) {
    Write-Host "`nBuild successful!" -ForegroundColor Green
    Write-Host "Executable: $binDir\ClipboardMonitor.exe" -ForegroundColor Yellow
} else {
    Write-Host "`nBuild may have failed. Check output above." -ForegroundColor Red
}
