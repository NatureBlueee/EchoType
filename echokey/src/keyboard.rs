//! 键盘监听模块
//!
//! 使用 rdev 库监听系统级键盘事件。
//!
//! 核心功能：
//! - 捕获所有按键事件
//! - 处理输入法最终确认的字符
//! - 区分 Enter 和 Ctrl+Enter
//! - 检测粘贴操作（Ctrl+V）

use rdev::{listen, Event, EventType, Key};
use std::sync::mpsc::Sender;

/// 键盘事件类型
/// 
/// 这些是我们关心的事件，会发送给主线程处理。
#[derive(Debug, Clone)]
pub enum KeyboardEvent {
    /// 普通字符输入（包括输入法确认的字符）
    Character(char),
    /// Enter 键（换行并添加时间戳）
    Enter,
    /// Ctrl+Enter（只换行，不添加时间戳）
    CtrlEnter,
    /// Backspace 键
    Backspace,
    /// 粘贴操作（Ctrl+V）
    Paste,
    /// 手动保存（Ctrl+Shift+S）
    ManualSave,
    /// 暂停/恢复（Ctrl+Shift+P）
    TogglePause,
    /// 新建日志段（Ctrl+Shift+N）
    NewSegment,
}

/// 修饰键状态
#[derive(Default)]
struct ModifierState {
    ctrl: bool,
    shift: bool,
    alt: bool,
}

/// 启动键盘监听
/// 
/// 这个函数会阻塞当前线程，持续监听键盘事件。
/// 捕获到的事件会通过 channel 发送给调用者。
///
/// # 参数
/// - `sender`: 用于发送事件的 channel 发送端
///
/// # 返回
/// 如果监听失败，返回错误信息
pub fn start_listening(sender: Sender<KeyboardEvent>) -> Result<(), String> {
    let mut modifiers = ModifierState::default();
    
    let callback = move |event: Event| {
        match event.event_type {
            // 按键按下
            EventType::KeyPress(key) => {
                // 更新修饰键状态
                match key {
                    Key::ControlLeft | Key::ControlRight => {
                        modifiers.ctrl = true;
                    }
                    Key::ShiftLeft | Key::ShiftRight => {
                        modifiers.shift = true;
                    }
                    Key::Alt | Key::AltGr => {
                        modifiers.alt = true;
                    }
                    // 处理 Enter 键
                    Key::Return => {
                        let event = if modifiers.ctrl {
                            KeyboardEvent::CtrlEnter
                        } else {
                            KeyboardEvent::Enter
                        };
                        let _ = sender.send(event);
                    }
                    // 处理 Backspace
                    Key::Backspace => {
                        let _ = sender.send(KeyboardEvent::Backspace);
                    }
                    // 检查快捷键
                    Key::KeyV if modifiers.ctrl && !modifiers.shift => {
                        let _ = sender.send(KeyboardEvent::Paste);
                    }
                    Key::KeyS if modifiers.ctrl && modifiers.shift => {
                        let _ = sender.send(KeyboardEvent::ManualSave);
                    }
                    Key::KeyP if modifiers.ctrl && modifiers.shift => {
                        let _ = sender.send(KeyboardEvent::TogglePause);
                    }
                    Key::KeyN if modifiers.ctrl && modifiers.shift => {
                        let _ = sender.send(KeyboardEvent::NewSegment);
                    }
                    // 其他按键暂时忽略，等待字符事件
                    _ => {}
                }
            }
            // 按键释放
            EventType::KeyRelease(key) => {
                match key {
                    Key::ControlLeft | Key::ControlRight => {
                        modifiers.ctrl = false;
                    }
                    Key::ShiftLeft | Key::ShiftRight => {
                        modifiers.shift = false;
                    }
                    Key::Alt | Key::AltGr => {
                        modifiers.alt = false;
                    }
                    _ => {}
                }
            }
            _ => {}
        }
        
        // 处理字符事件（这里会收到输入法确认的字符）
        // rdev 在 Windows 上会通过 name 字段提供字符
        if let EventType::KeyPress(_) = event.event_type {
            if let Some(name) = event.name {
                // 跳过特殊键（单字符且为控制字符）
                if name.len() == 1 {
                    if let Some(c) = name.chars().next() {
                        // 跳过控制字符和不可打印字符
                        if !c.is_control() && c != '\r' && c != '\n' {
                            // 如果是 Ctrl 组合键，不发送字符
                            if !modifiers.ctrl {
                                let _ = sender.send(KeyboardEvent::Character(c));
                            }
                        }
                    }
                } else if !name.is_empty() && !modifiers.ctrl {
                    // 多字符（如中文输入法确认的字）
                    for c in name.chars() {
                        if !c.is_control() {
                            let _ = sender.send(KeyboardEvent::Character(c));
                        }
                    }
                }
            }
        }
    };
    
    // 开始监听
    listen(callback).map_err(|e| format!("键盘监听失败: {:?}", e))
}

/// 将按键转换为可读名称（用于调试）
#[allow(dead_code)]
pub fn key_to_string(key: &Key) -> String {
    format!("{:?}", key)
}
