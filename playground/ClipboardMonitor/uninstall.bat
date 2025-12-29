@echo off
REM ================================================
REM ClipboardMonitor - Uninstallation Script
REM ================================================
chcp 65001 >nul

echo.
echo  ╔═══════════════════════════════════════════════════════════════════╗
echo  ║                                                                   ║
echo  ║   🔍 ClipboardMonitor Uninstaller                                ║
echo  ║                                                                   ║
echo  ╚═══════════════════════════════════════════════════════════════════╝
echo.

set INSTALL_DIR=%APPDATA%\ClipboardMonitor

echo 警告: 此操作将删除 ClipboardMonitor 程序和所有数据。
echo.
set /p CONFIRM="确定要卸载吗? (Y/N): "
if /i not "%CONFIRM%"=="Y" (
    echo 取消卸载。
    pause
    exit /b
)

echo.
echo [1/4] 停止运行中的程序...
taskkill /IM ClipboardMonitor.exe /F >nul 2>&1
echo      ✓ 已停止

echo.
echo [2/4] 移除开机自启动...
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "ClipboardMonitor" /f >nul 2>&1
echo      ✓ 已移除

echo.
echo [3/4] 删除开始菜单快捷方式...
del "%APPDATA%\Microsoft\Windows\Start Menu\Programs\ClipboardMonitor.lnk" >nul 2>&1
echo      ✓ 已删除

echo.
set /p KEEP_DATA="是否保留历史数据? (Y/N): "
if /i "%KEEP_DATA%"=="Y" (
    echo [4/4] 删除程序文件 (保留数据)...
    del "%INSTALL_DIR%\ClipboardMonitor.exe" >nul 2>&1
    del "%INSTALL_DIR%\native_host.exe" >nul 2>&1
    echo      ✓ 程序文件已删除
    echo      ○ 历史数据已保留在: %INSTALL_DIR%
) else (
    echo [4/4] 删除所有文件和数据...
    rmdir /S /Q "%INSTALL_DIR%" >nul 2>&1
    echo      ✓ 全部删除
)

REM Remove Native Messaging Host registry
reg delete "HKCU\Software\Google\Chrome\NativeMessagingHosts\com.clipboardmonitor.context" /f >nul 2>&1
reg delete "HKCU\Software\Microsoft\Edge\NativeMessagingHosts\com.clipboardmonitor.context" /f >nul 2>&1

echo.
echo  ╔═══════════════════════════════════════════════════════════════════╗
echo  ║                                                                   ║
echo  ║   ✅ 卸载完成!                                                   ║
echo  ║                                                                   ║
echo  ║   感谢使用 ClipboardMonitor                                      ║
echo  ║                                                                   ║
echo  ╚═══════════════════════════════════════════════════════════════════╝
echo.
pause
