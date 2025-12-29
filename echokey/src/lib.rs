//! EchoKey - 你打下的每一个字，都有回声
//!
//! 系统级键盘输入记录工具，确保用户的每一次输入都不会丢失。
//!
//! # 核心功能
//! - 键盘监听：记录所有按键和输入法确认的字符
//! - 剪贴板监听：记录复制和粘贴的内容
//! - 实时写入：每次输入立即保存到本地
//! - 按日期分文件：每天一个日志文件
//!
//! # 模块说明
//! - `config`: 配置项（存储路径、超时时间等）
//! - `logger`: 日志写入（核心模块）
//! - `keyboard_win`: Windows 原生键盘监听（使用 windows-rs）
//! - `clipboard`: 剪贴板操作
//! - `autostart`: 开机自启动
//! - `gui`: 图形用户界面（Apple 风格）

pub mod config;
pub mod logger;
pub mod clipboard;
pub mod autostart;
pub mod gui;
pub mod tray;

// Windows 专用模块
#[cfg(windows)]
pub mod keyboard_win;

// 重新导出常用类型
pub use logger::Logger;
pub use gui::{EchoKeyApp, SharedGuiState};

#[cfg(windows)]
pub use keyboard_win::KeyboardEvent;
