//! EchoKey - 你打下的每一个字，都有回声
//!
//! 主程序入口，负责：
//! 1. 初始化各个模块
//! 2. 启动键盘监听线程
//! 3. 运行事件循环
//! 4. 处理托盘菜单事件

// Windows: 隐藏控制台窗口
#![cfg_attr(
    all(target_os = "windows", not(debug_assertions)),
    windows_subsystem = "windows"
)]

use std::sync::mpsc;
use std::thread;
use std::process;

use echokey::{
    Logger, KeyboardEvent, SystemTray, TrayEvent,
    keyboard, clipboard, autostart, config,
    tray::menu_event_receiver,
};

fn main() {
    // 打印启动信息（仅在调试模式下可见）
    eprintln!("EchoKey {} 正在启动...", config::APP_VERSION);
    
    // 初始化剪贴板
    if let Err(e) = clipboard::init() {
        eprintln!("警告: {}", e);
    }
    
    // 初始化日志写入器
    let mut logger = match Logger::new() {
        Ok(l) => l,
        Err(e) => {
            eprintln!("错误: 无法初始化日志系统: {}", e);
            process::exit(1);
        }
    };
    
    eprintln!("日志目录: {:?}", logger.get_log_directory());
    
    // 设置开机自启动（首次运行）
    if !autostart::is_enabled() {
        if let Err(e) = autostart::enable() {
            eprintln!("警告: 无法设置开机自启动: {}", e);
        } else {
            eprintln!("已启用开机自启动");
        }
    }
    
    // 创建系统托盘
    let mut tray = match SystemTray::new() {
        Ok(t) => t,
        Err(e) => {
            eprintln!("错误: 无法创建系统托盘: {}", e);
            process::exit(1);
        }
    };
    
    // 创建事件通道
    let (keyboard_tx, keyboard_rx) = mpsc::channel::<KeyboardEvent>();
    
    // 启动键盘监听线程
    thread::spawn(move || {
        if let Err(e) = keyboard::start_listening(keyboard_tx) {
            eprintln!("键盘监听错误: {}", e);
        }
    });
    
    // 获取菜单事件接收器
    let menu_rx = menu_event_receiver();
    
    eprintln!("EchoKey 已启动，按 Ctrl+C 退出（或使用托盘菜单）");
    
    // 主事件循环
    loop {
        // 处理键盘事件（非阻塞）
        while let Ok(event) = keyboard_rx.try_recv() {
            handle_keyboard_event(&mut logger, event);
        }
        
        // 处理托盘菜单事件（非阻塞）
        while let Ok(event) = menu_rx.try_recv() {
            if let Some(tray_event) = tray.handle_menu_event(&event) {
                match tray_event {
                    TrayEvent::TogglePause => {
                        match logger.toggle_pause() {
                            Ok(paused) => {
                                tray.set_paused(paused);
                                eprintln!("{}", if paused { "已暂停记录" } else { "已恢复记录" });
                            }
                            Err(e) => eprintln!("错误: {}", e),
                        }
                    }
                    TrayEvent::OpenLogDir => {
                        let log_dir = logger.get_log_directory();
                        open_directory(&log_dir);
                    }
                    TrayEvent::NewSegment => {
                        if let Err(e) = logger.new_segment() {
                            eprintln!("错误: 无法创建新日志段: {}", e);
                        } else {
                            eprintln!("已创建新日志段");
                        }
                    }
                    TrayEvent::Quit => {
                        eprintln!("正在退出...");
                        process::exit(0);
                    }
                }
            }
        }
        
        // 短暂休眠，避免 CPU 空转
        thread::sleep(std::time::Duration::from_millis(10));
    }
}

/// 处理键盘事件
fn handle_keyboard_event(logger: &mut Logger, event: KeyboardEvent) {
    match event {
        KeyboardEvent::Character(c) => {
            if let Err(e) = logger.write_text(&c.to_string()) {
                eprintln!("写入错误: {}", e);
            }
        }
        KeyboardEvent::Enter => {
            if let Err(e) = logger.handle_enter() {
                eprintln!("写入错误: {}", e);
            }
        }
        KeyboardEvent::CtrlEnter => {
            if let Err(e) = logger.handle_ctrl_enter() {
                eprintln!("写入错误: {}", e);
            }
        }
        KeyboardEvent::Backspace => {
            // Backspace 我们记录为特殊字符，但用户看到的是原始输入
            // 这意味着如果用户打错字然后删除，我们会同时记录错的和删除操作
            // 这正是我们想要的：原始输入都在
            if let Err(e) = logger.write_text("⌫") {
                eprintln!("写入错误: {}", e);
            }
        }
        KeyboardEvent::Paste => {
            // 获取剪贴板内容并记录
            if let Some(content) = clipboard::get_text() {
                if !content.is_empty() {
                    if let Err(e) = logger.write_paste(&content) {
                        eprintln!("写入错误: {}", e);
                    }
                }
            }
        }
        KeyboardEvent::ManualSave => {
            // 手动保存剪贴板内容
            if let Some(content) = clipboard::get_text() {
                if !content.is_empty() {
                    if let Err(e) = logger.write_manual_save(&content) {
                        eprintln!("写入错误: {}", e);
                    } else {
                        eprintln!("已手动保存剪贴板内容");
                    }
                }
            }
        }
        KeyboardEvent::TogglePause => {
            match logger.toggle_pause() {
                Ok(paused) => {
                    eprintln!("{}", if paused { "已暂停记录" } else { "已恢复记录" });
                }
                Err(e) => eprintln!("错误: {}", e),
            }
        }
        KeyboardEvent::NewSegment => {
            if let Err(e) = logger.new_segment() {
                eprintln!("错误: 无法创建新日志段: {}", e);
            } else {
                eprintln!("已创建新日志段");
            }
        }
    }
}

/// 打开目录（Windows 资源管理器）
fn open_directory(path: &std::path::Path) {
    #[cfg(windows)]
    {
        let _ = std::process::Command::new("explorer")
            .arg(path)
            .spawn();
    }
    
    #[cfg(not(windows))]
    {
        // Linux/macOS 使用 xdg-open 或 open
        #[cfg(target_os = "macos")]
        let _ = std::process::Command::new("open")
            .arg(path)
            .spawn();
        
        #[cfg(target_os = "linux")]
        let _ = std::process::Command::new("xdg-open")
            .arg(path)
            .spawn();
    }
}
