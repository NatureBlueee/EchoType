//! 剪贴板模块
//!
//! 提供剪贴板读取功能，用于：
//! - 检测粘贴时获取粘贴内容
//! - 手动保存时读取剪贴板内容

use arboard::Clipboard;
use std::sync::Mutex;

/// 全局剪贴板实例
/// 
/// 使用 Mutex 保证线程安全
static CLIPBOARD: Mutex<Option<Clipboard>> = Mutex::new(None);

/// 初始化剪贴板
/// 
/// 在程序启动时调用一次
pub fn init() -> Result<(), String> {
    let clipboard = Clipboard::new()
        .map_err(|e| format!("无法初始化剪贴板: {}", e))?;
    
    let mut guard = CLIPBOARD.lock()
        .map_err(|e| format!("无法获取剪贴板锁: {}", e))?;
    *guard = Some(clipboard);
    
    Ok(())
}

/// 读取剪贴板文本内容
/// 
/// 如果剪贴板为空或不包含文本，返回 None
pub fn get_text() -> Option<String> {
    let mut guard = CLIPBOARD.lock().ok()?;
    let clipboard = guard.as_mut()?;
    
    clipboard.get_text().ok()
}

/// 检查剪贴板是否包含文本
#[allow(dead_code)]
pub fn has_text() -> bool {
    get_text().is_some()
}
