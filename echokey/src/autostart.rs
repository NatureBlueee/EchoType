//! 开机自启动模块
//!
//! 在 Windows 上通过注册表实现开机自启动。
//! 
//! 注册表位置：HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run

#[cfg(windows)]
use std::env;
#[cfg(windows)]
use winreg::enums::*;
#[cfg(windows)]
use winreg::RegKey;

#[cfg(windows)]
const APP_NAME: &str = "EchoKey";

/// 启用开机自启动
#[cfg(windows)]
pub fn enable() -> Result<(), String> {
    let exe_path = env::current_exe()
        .map_err(|e| format!("无法获取程序路径: {}", e))?;
    
    let hkcu = RegKey::predef(HKEY_CURRENT_USER);
    let run_key = hkcu
        .open_subkey_with_flags(
            r"Software\Microsoft\Windows\CurrentVersion\Run",
            KEY_WRITE,
        )
        .map_err(|e| format!("无法打开注册表: {}", e))?;
    
    run_key
        .set_value(APP_NAME, &exe_path.to_string_lossy().to_string())
        .map_err(|e| format!("无法写入注册表: {}", e))?;
    
    Ok(())
}

/// 禁用开机自启动
#[cfg(windows)]
pub fn disable() -> Result<(), String> {
    let hkcu = RegKey::predef(HKEY_CURRENT_USER);
    let run_key = hkcu
        .open_subkey_with_flags(
            r"Software\Microsoft\Windows\CurrentVersion\Run",
            KEY_WRITE,
        )
        .map_err(|e| format!("无法打开注册表: {}", e))?;
    
    // 忽略删除失败（可能本来就没有）
    let _ = run_key.delete_value(APP_NAME);
    
    Ok(())
}

/// 检查是否已启用开机自启动
#[cfg(windows)]
pub fn is_enabled() -> bool {
    let hkcu = RegKey::predef(HKEY_CURRENT_USER);
    
    if let Ok(run_key) = hkcu.open_subkey(r"Software\Microsoft\Windows\CurrentVersion\Run") {
        run_key.get_value::<String, _>(APP_NAME).is_ok()
    } else {
        false
    }
}

// 非 Windows 平台的占位实现
#[cfg(not(windows))]
pub fn enable() -> Result<(), String> {
    Err("开机自启动仅支持 Windows".to_string())
}

#[cfg(not(windows))]
pub fn disable() -> Result<(), String> {
    Err("开机自启动仅支持 Windows".to_string())
}

#[cfg(not(windows))]
pub fn is_enabled() -> bool {
    false
}
