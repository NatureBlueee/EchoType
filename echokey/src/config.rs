//! 配置模块
//!
//! 定义 EchoKey 的所有配置项，包括存储路径、快捷键等。

use std::path::PathBuf;
use std::time::Duration;

/// 获取日志存储目录
///
/// Windows: %LOCALAPPDATA%\EchoKey\logs\
/// 例如: C:\Users\用户名\AppData\Local\EchoKey\logs\
pub fn get_log_directory() -> PathBuf {
    let base = dirs::data_local_dir()
        .unwrap_or_else(|| PathBuf::from("."));
    base.join("EchoKey").join("logs")
}

/// 超时时间：超过这个时间没有输入，下次输入时自动添加新时间戳
pub const IDLE_TIMEOUT: Duration = Duration::from_secs(30);

/// 应用名称
pub const APP_NAME: &str = "EchoKey";

/// 应用版本
pub const APP_VERSION: &str = env!("CARGO_PKG_VERSION");

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_log_directory() {
        let dir = get_log_directory();
        assert!(dir.to_string_lossy().contains("EchoKey"));
    }
}
