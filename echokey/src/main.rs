//! EchoKey - 你打下的每一个字，都有回声
//!
//! 主程序入口，负责：
//! 1. 初始化各个模块
//! 2. 启动三线程架构：UI线程、钩子线程、逻辑线程
//! 3. 管理应用生命周期

// Windows: 隐藏控制台窗口
#![cfg_attr(
    all(target_os = "windows", not(debug_assertions)),
    windows_subsystem = "windows"
)]

use std::sync::mpsc;
use std::sync::{Arc, Mutex};
use std::thread;
use std::process;
use std::path::PathBuf;

use echokey::{
    Logger, KeyboardEvent, SharedGuiState,
    keyboard_win, clipboard, autostart, config, gui, tray,
};

/// 应用状态
struct AppState {
    logger: Logger,
    paused: bool,
    char_count: usize,
}

fn main() {
    // 打印启动信息（仅在调试模式下可见）
    eprintln!("EchoKey {} 正在启动...", config::APP_VERSION);
    
    // 初始化剪贴板
    if let Err(e) = clipboard::init() {
        eprintln!("警告: {}", e);
    }
    
    // 初始化日志写入器
    let logger = match Logger::new() {
        Ok(l) => l,
        Err(e) => {
            eprintln!("错误: 无法初始化日志系统: {}", e);
            process::exit(1);
        }
    };
    
    let log_directory = logger.get_log_directory().to_path_buf();
    eprintln!("日志目录: {:?}", log_directory);
    
    // 设置开机自启动（首次运行）
    if !autostart::is_enabled() {
        if let Err(e) = autostart::enable() {
            eprintln!("警告: 无法设置开机自启动: {}", e);
        } else {
            eprintln!("已启用开机自启动");
        }
    }
    
    // 创建应用状态
    let app_state = Arc::new(Mutex::new(AppState {
        logger,
        paused: false,
        char_count: 0,
    }));
    
    // 创建 GUI 共享状态
    let gui_state = Arc::new(Mutex::new(SharedGuiState::default()));
    
    // 创建键盘事件通道
    let (keyboard_tx, keyboard_rx) = mpsc::channel::<KeyboardEvent>();
    
    // 创建托盘事件通道
    let (tray_tx, tray_rx) = mpsc::channel::<tray::TrayEvent>();
    
    // 创建托盘状态
    let tray_state = Arc::new(Mutex::new(tray::TrayState { paused: false }));
    
    // 创建系统托盘（必须在主线程创建）
    let _tray_icon = match tray::create_tray(tray_tx.clone(), Arc::clone(&tray_state)) {
        Ok(tray) => {
            eprintln!("系统托盘已创建");
            Some(tray)
        }
        Err(e) => {
            eprintln!("警告: 无法创建系统托盘: {}", e);
            None
        }
    };
    
    // 线程2: 启动键盘监听线程（带消息循环）
    let keyboard_thread = thread::spawn(move || {
        eprintln!("键盘监听线程启动");
        if let Err(e) = keyboard_win::start_listening(keyboard_tx) {
            eprintln!("键盘监听错误: {}", e);
        }
        eprintln!("键盘监听线程退出");
    });
    
    // 线程3: 业务逻辑线程（处理键盘事件和托盘事件）
    let logic_app_state = Arc::clone(&app_state);
    let logic_gui_state = Arc::clone(&gui_state);
    let logic_log_dir = log_directory.clone();
    let logic_thread = thread::spawn(move || {
        eprintln!("业务逻辑线程启动");
        run_logic_loop(keyboard_rx, tray_rx, logic_app_state, logic_gui_state, logic_log_dir);
        eprintln!("业务逻辑线程退出");
    });
    
    eprintln!("EchoKey 已启动");
    eprintln!("快捷键:");
    eprintln!("  Ctrl+Shift+P: 暂停/恢复");
    eprintln!("  Ctrl+Shift+S: 手动保存剪贴板");
    eprintln!("  Ctrl+Shift+N: 新建日志段");
    
    // 线程1: 主线程 - 运行 GUI
    if let Err(e) = gui::run_gui(log_directory, gui_state) {
        eprintln!("GUI 错误: {}", e);
    }
    
    // GUI 退出后，等待其他线程
    // 注意：键盘线程有自己的消息循环，需要发送退出消息
    let _ = keyboard_thread.join();
    let _ = logic_thread.join();
}

/// 业务逻辑循环
/// 
/// 处理来自键盘钩子和托盘的事件
fn run_logic_loop(
    keyboard_rx: mpsc::Receiver<KeyboardEvent>,
    tray_rx: mpsc::Receiver<tray::TrayEvent>,
    app_state: Arc<Mutex<AppState>>,
    gui_state: Arc<Mutex<SharedGuiState>>,
    log_directory: PathBuf,
) {
    loop {
        // 检查 GUI 请求
        if let Ok(mut gs) = gui_state.lock() {
            // 同步暂停状态
            if let Ok(mut as_) = app_state.lock() {
                if gs.paused != as_.paused {
                    as_.paused = gs.paused;
                    if let Err(e) = as_.logger.set_paused(gs.paused) {
                        eprintln!("设置暂停状态错误: {}", e);
                    }
                }
            }
            
            // 处理新日志段请求
            if gs.request_new_segment {
                gs.request_new_segment = false;
                if let Ok(mut as_) = app_state.lock() {
                    if let Err(e) = as_.logger.new_segment() {
                        eprintln!("创建新日志段错误: {}", e);
                    }
                }
            }
            
            // 处理打开日志目录请求
            if gs.request_open_log {
                gs.request_open_log = false;
                open_directory(&log_directory);
            }
        }
        
        // 处理托盘事件（非阻塞）
        while let Ok(event) = tray_rx.try_recv() {
            match event {
                tray::TrayEvent::ShowWindow => {
                    // TODO: 通知 GUI 显示窗口
                    eprintln!("托盘: 显示窗口");
                }
                tray::TrayEvent::TogglePause => {
                    if let Ok(mut as_) = app_state.lock() {
                        match as_.logger.toggle_pause() {
                            Ok(paused) => {
                                as_.paused = paused;
                                if let Ok(mut gs) = gui_state.lock() {
                                    gs.paused = paused;
                                }
                                eprintln!("{}", if paused { "托盘: 已暂停" } else { "托盘: 已恢复" });
                            }
                            Err(e) => eprintln!("切换暂停状态错误: {}", e),
                        }
                    }
                }
                tray::TrayEvent::NewSegment => {
                    if let Ok(mut as_) = app_state.lock() {
                        if let Err(e) = as_.logger.new_segment() {
                            eprintln!("创建新日志段错误: {}", e);
                        } else {
                            eprintln!("托盘: 已创建新日志段");
                        }
                    }
                }
                tray::TrayEvent::OpenLogDir => {
                    open_directory(&log_directory);
                }
                tray::TrayEvent::Quit => {
                    eprintln!("托盘: 退出程序");
                    process::exit(0);
                }
            }
        }
        
        // 处理键盘事件（带超时）
        match keyboard_rx.recv_timeout(std::time::Duration::from_millis(50)) {
            Ok(event) => {
                handle_keyboard_event(&app_state, &gui_state, event);
            }
            Err(mpsc::RecvTimeoutError::Timeout) => {
                // 超时，继续循环
            }
            Err(mpsc::RecvTimeoutError::Disconnected) => {
                // Channel 关闭，退出循环
                break;
            }
        }
    }
}

/// 处理键盘事件
fn handle_keyboard_event(
    app_state: &Arc<Mutex<AppState>>,
    gui_state: &Arc<Mutex<SharedGuiState>>,
    event: KeyboardEvent,
) {
    let mut state = match app_state.lock() {
        Ok(s) => s,
        Err(e) => {
            eprintln!("无法获取状态锁: {}", e);
            return;
        }
    };
    
    // 检查是否暂停
    if state.paused && !matches!(event, KeyboardEvent::TogglePause) {
        return;
    }
    
    match event {
        KeyboardEvent::Character(c) => {
            if let Err(e) = state.logger.write_text(&c.to_string()) {
                eprintln!("写入错误: {}", e);
            } else {
                state.char_count += 1;
                // 更新 GUI 状态
                if let Ok(mut gs) = gui_state.lock() {
                    gs.today_chars = state.char_count;
                }
            }
        }
        KeyboardEvent::Enter => {
            if let Err(e) = state.logger.handle_enter() {
                eprintln!("写入错误: {}", e);
            }
        }
        KeyboardEvent::CtrlEnter => {
            if let Err(e) = state.logger.handle_ctrl_enter() {
                eprintln!("写入错误: {}", e);
            }
        }
        KeyboardEvent::Backspace => {
            if let Err(e) = state.logger.write_text("⌫") {
                eprintln!("写入错误: {}", e);
            }
        }
        KeyboardEvent::Paste => {
            if let Some(content) = clipboard::get_text() {
                if !content.is_empty() {
                    if let Err(e) = state.logger.write_paste(&content) {
                        eprintln!("写入错误: {}", e);
                    } else {
                        state.char_count += content.chars().count();
                        if let Ok(mut gs) = gui_state.lock() {
                            gs.today_chars = state.char_count;
                        }
                    }
                }
            }
        }
        KeyboardEvent::ManualSave => {
            if let Some(content) = clipboard::get_text() {
                if !content.is_empty() {
                    if let Err(e) = state.logger.write_manual_save(&content) {
                        eprintln!("写入错误: {}", e);
                    } else {
                        eprintln!("已手动保存剪贴板内容");
                    }
                }
            }
        }
        KeyboardEvent::TogglePause => {
            match state.logger.toggle_pause() {
                Ok(paused) => {
                    state.paused = paused;
                    if let Ok(mut gs) = gui_state.lock() {
                        gs.paused = paused;
                    }
                    eprintln!("{}", if paused { "已暂停记录" } else { "已恢复记录" });
                }
                Err(e) => eprintln!("错误: {}", e),
            }
        }
        KeyboardEvent::NewSegment => {
            if let Err(e) = state.logger.new_segment() {
                eprintln!("错误: 无法创建新日志段: {}", e);
            } else {
                eprintln!("已创建新日志段");
            }
        }
    }
}

/// 打开目录
fn open_directory(path: &std::path::Path) {
    #[cfg(windows)]
    {
        let _ = std::process::Command::new("explorer").arg(path).spawn();
    }
    #[cfg(target_os = "macos")]
    {
        let _ = std::process::Command::new("open").arg(path).spawn();
    }
    #[cfg(target_os = "linux")]
    {
        let _ = std::process::Command::new("xdg-open").arg(path).spawn();
    }
}

