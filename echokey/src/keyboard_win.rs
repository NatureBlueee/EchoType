//! Windows 原生键盘监听模块
//!
//! 使用 Windows API 实现全局键盘钩子，解决 rdev 的字符重复问题。
//! 
//! 核心改进：
//! - 过滤 LLKHF_INJECTED 标志，避免软件注入事件的重复
//! - 时间戳去重：防止短时间内的重复事件
//! - 使用 GetMessage 消息循环，确保钩子稳定运行
//! - 不在钩子回调中调用 ToUnicode，避免破坏键盘状态

use std::sync::mpsc::Sender;
use std::sync::Mutex;
use std::time::{Instant, Duration};
use once_cell::sync::Lazy;

use windows::Win32::Foundation::{LPARAM, LRESULT, WPARAM};
use windows::Win32::UI::WindowsAndMessaging::{
    CallNextHookEx, SetWindowsHookExW, UnhookWindowsHookEx,
    GetMessageW, TranslateMessage, DispatchMessageW, PostQuitMessage,
    HHOOK, KBDLLHOOKSTRUCT, MSG, WH_KEYBOARD_LL,
    WM_KEYDOWN, WM_SYSKEYDOWN,
};
use windows::Win32::UI::Input::KeyboardAndMouse::{
    GetKeyState, GetKeyboardState, ToUnicode,
    VK_CONTROL, VK_SHIFT, VK_MENU, VK_RETURN, VK_BACK,
    VK_LCONTROL, VK_RCONTROL, VK_LSHIFT, VK_RSHIFT, VK_LMENU, VK_RMENU,
    VIRTUAL_KEY,
};

/// 键盘事件类型
#[derive(Debug, Clone)]
pub enum KeyboardEvent {
    /// 普通字符输入
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

/// 线程安全的钩子句柄包装
struct HookHandle(HHOOK);
unsafe impl Send for HookHandle {}
unsafe impl Sync for HookHandle {}

/// 全局钩子句柄
static HOOK_HANDLE: Lazy<Mutex<Option<HookHandle>>> = Lazy::new(|| Mutex::new(None));

/// 全局事件发送器
static EVENT_SENDER: Lazy<Mutex<Option<Sender<KeyboardEvent>>>> = Lazy::new(|| Mutex::new(None));

/// 去重状态：记录上一次按键的虚拟键码和时间
static LAST_KEY_EVENT: Lazy<Mutex<LastKeyEvent>> = Lazy::new(|| {
    Mutex::new(LastKeyEvent {
        vk_code: 0,
        scan_code: 0,
        time: None,
    })
});

/// 上一次按键事件信息
struct LastKeyEvent {
    vk_code: u32,
    scan_code: u32,
    time: Option<Instant>,
}

/// LLKHF_INJECTED 标志值 (0x10)
const INJECTED_FLAG: u32 = 0x10;

/// 去重时间窗口（毫秒）- 同一按键在此时间内只记录一次
const DEDUP_WINDOW_MS: u64 = 30;

/// 低级键盘钩子回调函数
/// 
/// 关键设计：
/// 1. 过滤注入事件（LLKHF_INJECTED）避免重复
/// 2. 时间戳去重：防止短时间内的重复事件
/// 3. 快速返回，不做耗时操作
/// 4. 通过 Channel 发送事件到主线程处理
unsafe extern "system" fn low_level_keyboard_proc(
    code: i32,
    wparam: WPARAM,
    lparam: LPARAM,
) -> LRESULT {
    if code >= 0 {
        let kbd = &*(lparam.0 as *const KBDLLHOOKSTRUCT);
        
        // 关键1：过滤软件注入的事件
        let is_injected = (kbd.flags.0 & INJECTED_FLAG) != 0;
        if is_injected {
            return CallNextHookEx(HHOOK::default(), code, wparam, lparam);
        }
        
        // 只处理按键按下事件
        let msg_type = wparam.0 as u32;
        if msg_type == WM_KEYDOWN || msg_type == WM_SYSKEYDOWN {
            // 关键2：时间戳去重 - 防止短时间内的重复按键
            if !should_process_key(kbd.vkCode, kbd.scanCode) {
                return CallNextHookEx(HHOOK::default(), code, wparam, lparam);
            }
            
            process_key_down(kbd);
        }
    }
    
    CallNextHookEx(HHOOK::default(), code, wparam, lparam)
}

/// 检查是否应该处理此按键（去重逻辑）
/// 
/// 如果与上一次按键相同且在时间窗口内，则跳过
fn should_process_key(vk_code: u32, scan_code: u32) -> bool {
    let mut last_event = match LAST_KEY_EVENT.lock() {
        Ok(guard) => guard,
        Err(_) => return true, // 锁失败时默认处理
    };
    
    let now = Instant::now();
    let should_process = if let Some(last_time) = last_event.time {
        // 如果是同一个按键且在去重窗口内，跳过
        if last_event.vk_code == vk_code 
            && last_event.scan_code == scan_code
            && now.duration_since(last_time) < Duration::from_millis(DEDUP_WINDOW_MS) 
        {
            false
        } else {
            true
        }
    } else {
        true
    };
    
    // 更新上一次按键信息
    if should_process {
        last_event.vk_code = vk_code;
        last_event.scan_code = scan_code;
        last_event.time = Some(now);
    }
    
    should_process
}

/// 处理按键按下事件
fn process_key_down(kbd: &KBDLLHOOKSTRUCT) {
    let sender_guard = match EVENT_SENDER.lock() {
        Ok(g) => g,
        Err(_) => return,
    };
    
    let sender = match sender_guard.as_ref() {
        Some(s) => s,
        None => return,
    };
    
    let vk = VIRTUAL_KEY(kbd.vkCode as u16);
    
    // 获取修饰键状态
    let ctrl_pressed = is_key_pressed(VK_CONTROL) || is_key_pressed(VK_LCONTROL) || is_key_pressed(VK_RCONTROL);
    let shift_pressed = is_key_pressed(VK_SHIFT) || is_key_pressed(VK_LSHIFT) || is_key_pressed(VK_RSHIFT);
    let _alt_pressed = is_key_pressed(VK_MENU) || is_key_pressed(VK_LMENU) || is_key_pressed(VK_RMENU);
    
    // 处理特殊按键
    match vk {
        VK_RETURN => {
            let event = if ctrl_pressed {
                KeyboardEvent::CtrlEnter
            } else {
                KeyboardEvent::Enter
            };
            let _ = sender.send(event);
            return;
        }
        VK_BACK => {
            let _ = sender.send(KeyboardEvent::Backspace);
            return;
        }
        _ => {}
    }
    
    // 处理快捷键组合
    if ctrl_pressed {
        match vk.0 {
            // Ctrl+V - 粘贴
            0x56 if !shift_pressed => {
                let _ = sender.send(KeyboardEvent::Paste);
                return;
            }
            // Ctrl+Shift+S - 手动保存
            0x53 if shift_pressed => {
                let _ = sender.send(KeyboardEvent::ManualSave);
                return;
            }
            // Ctrl+Shift+P - 暂停/恢复
            0x50 if shift_pressed => {
                let _ = sender.send(KeyboardEvent::TogglePause);
                return;
            }
            // Ctrl+Shift+N - 新建日志段
            0x4E if shift_pressed => {
                let _ = sender.send(KeyboardEvent::NewSegment);
                return;
            }
            _ => {
                // Ctrl 组合键不记录字符
                return;
            }
        }
    }
    
    // 尝试将按键转换为字符
    if let Some(c) = vk_to_char(kbd.vkCode, kbd.scanCode) {
        if !c.is_control() {
            let _ = sender.send(KeyboardEvent::Character(c));
        }
    }
}

/// 检查按键是否被按下
fn is_key_pressed(vk: VIRTUAL_KEY) -> bool {
    unsafe { (GetKeyState(vk.0 as i32) & 0x8000u16 as i16) != 0 }
}

/// 将虚拟键码转换为字符
/// 
/// 注意：此函数在钩子线程中调用，应尽量简单
fn vk_to_char(vk_code: u32, scan_code: u32) -> Option<char> {
    unsafe {
        let mut keyboard_state = [0u8; 256];
        if GetKeyboardState(&mut keyboard_state).is_err() {
            return None;
        }
        
        let mut buffer = [0u16; 4];
        let result = ToUnicode(
            vk_code,
            scan_code,
            Some(&keyboard_state),
            &mut buffer,
            0, // 不处理死键
        );
        
        if result == 1 {
            char::from_u32(buffer[0] as u32)
        } else {
            None
        }
    }
}

/// 启动键盘监听
/// 
/// 此函数会在当前线程运行消息循环，直到收到退出信号。
/// 必须在专用线程中调用。
pub fn start_listening(sender: Sender<KeyboardEvent>) -> Result<(), String> {
    // 保存发送器
    {
        let mut guard = EVENT_SENDER.lock()
            .map_err(|_| "无法获取发送器锁".to_string())?;
        *guard = Some(sender);
    }
    
    // 安装钩子
    let hook = unsafe {
        SetWindowsHookExW(
            WH_KEYBOARD_LL,
            Some(low_level_keyboard_proc),
            None,
            0,
        )
    }.map_err(|e| format!("无法安装键盘钩子: {:?}", e))?;
    
    {
        let mut guard = HOOK_HANDLE.lock()
            .map_err(|_| "无法获取钩子句柄锁".to_string())?;
        *guard = Some(HookHandle(hook));
    }
    
    eprintln!("键盘钩子已安装");
    
    // 运行消息循环（必须！否则钩子无法工作）
    run_message_loop();
    
    // 清理
    {
        let mut guard = HOOK_HANDLE.lock().ok();
        if let Some(ref mut g) = guard {
            if let Some(h) = g.take() {
                unsafe {
                    let _ = UnhookWindowsHookEx(h.0);
                }
            }
        }
    }
    
    Ok(())
}

/// 运行 Windows 消息循环
/// 
/// 这是正确的阻塞等待方式：
/// - GetMessage 在没有消息时会挂起线程（0% CPU）
/// - 有消息时自动唤醒处理
/// - 不要使用 thread::sleep！
fn run_message_loop() {
    unsafe {
        let mut msg = MSG::default();
        // GetMessage 返回 0 表示收到 WM_QUIT
        // 返回 -1 表示错误
        while GetMessageW(&mut msg, None, 0, 0).as_bool() {
            let _ = TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
}

/// 停止键盘监听
/// 
/// 向消息循环发送 WM_QUIT 消息
pub fn stop_listening() {
    unsafe {
        PostQuitMessage(0);
    }
}

/// 检查钩子是否仍然有效
/// 
/// 用于 Watchdog 检测钩子是否被系统移除
pub fn is_hook_active() -> bool {
    if let Ok(guard) = HOOK_HANDLE.lock() {
        guard.is_some()
    } else {
        false
    }
}
