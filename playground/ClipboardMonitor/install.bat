@echo off
REM ================================================
REM ClipboardMonitor - One-Click Installation Script
REM ================================================
chcp 65001 >nul
setlocal EnableDelayedExpansion

echo.
echo  ╔═══════════════════════════════════════════════════════════════════╗
echo  ║                                                                   ║
echo  ║   🔍 ClipboardMonitor Installer                                  ║
echo  ║                                                                   ║
echo  ║   智能剪贴板监控与上下文溯源工具                                 ║
echo  ║                                                                   ║
echo  ╚═══════════════════════════════════════════════════════════════════╝
echo.

REM Check for administrator privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [!] 需要管理员权限，正在请求...
    echo.
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

set INSTALL_DIR=%APPDATA%\ClipboardMonitor
set SOURCE_DIR=%~dp0

echo [1/5] 创建安装目录...
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
echo      ✓ %INSTALL_DIR%
echo.

echo [2/5] 复制程序文件...
if exist "%SOURCE_DIR%bin\ClipboardMonitor.exe" (
    copy /Y "%SOURCE_DIR%bin\ClipboardMonitor.exe" "%INSTALL_DIR%\" >nul
    echo      ✓ ClipboardMonitor.exe
) else (
    echo      ⚠ 未找到 ClipboardMonitor.exe，请先编译项目
    echo      运行: build.bat
    echo.
)

echo.
echo [3/5] 创建开始菜单快捷方式...
set SHORTCUT_PATH=%APPDATA%\Microsoft\Windows\Start Menu\Programs\ClipboardMonitor.lnk
powershell -Command "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut('%SHORTCUT_PATH%'); $s.TargetPath = '%INSTALL_DIR%\ClipboardMonitor.exe'; $s.WorkingDirectory = '%INSTALL_DIR%'; $s.Description = 'ClipboardMonitor - 智能剪贴板监控'; $s.Save()"
echo      ✓ 开始菜单快捷方式已创建
echo.

echo [4/5] 设置开机自启动 (可选)...
set /p AUTOSTART="是否设置开机自动启动? (Y/N): "
if /i "%AUTOSTART%"=="Y" (
    reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "ClipboardMonitor" /d "\"%INSTALL_DIR%\ClipboardMonitor.exe\"" /f >nul
    echo      ✓ 已添加到开机启动项
) else (
    echo      ○ 跳过开机自启动设置
)
echo.

echo [5/5] 安装完成!
echo.
echo  ╔═══════════════════════════════════════════════════════════════════╗
echo  ║                                                                   ║
echo  ║   ✅ 安装成功!                                                   ║
echo  ║                                                                   ║
echo  ║   📍 安装位置: %INSTALL_DIR%                                     ║
echo  ║   📋 数据目录: %INSTALL_DIR%                                     ║
echo  ║                                                                   ║
echo  ║   🚀 使用方法:                                                   ║
echo  ║      - 从开始菜单搜索 "ClipboardMonitor" 启动                    ║
echo  ║      - 或运行: %INSTALL_DIR%\ClipboardMonitor.exe               ║
echo  ║                                                                   ║
echo  ║   ⌨️ 快捷键:                                                      ║
echo  ║      - Ctrl+Shift+Q: 退出程序                                    ║
echo  ║                                                                   ║
echo  ╚═══════════════════════════════════════════════════════════════════╝
echo.

set /p LAUNCH_NOW="是否立即启动 ClipboardMonitor? (Y/N): "
if /i "%LAUNCH_NOW%"=="Y" (
    start "" "%INSTALL_DIR%\ClipboardMonitor.exe"
    echo 程序已启动，查看系统托盘图标。
)

echo.
pause
