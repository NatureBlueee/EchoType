//! 系统托盘模块
//!
//! 实现系统托盘图标和右键菜单功能。
//! 使用 tray-icon crate，在专门的线程中运行 Windows 消息循环。

use std::sync::{Arc, Mutex, mpsc};
use tray_icon::{
    TrayIcon, TrayIconBuilder,
    menu::{Menu, MenuEvent, MenuItem, PredefinedMenuItem},
    Icon,
};

/// 托盘事件
#[derive(Debug, Clone)]
pub enum TrayEvent {
    /// 显示主窗口
    ShowWindow,
    /// 暂停/恢复记录
    TogglePause,
    /// 新建日志段
    NewSegment,
    /// 打开日志目录
    OpenLogDir,
    /// 退出程序
    Quit,
}

/// 托盘状态
pub struct TrayState {
    pub paused: bool,
}

/// 创建托盘图标
/// 
/// 返回一个 TrayIcon 实例和菜单项 ID 映射
pub fn create_tray(
    event_tx: mpsc::Sender<TrayEvent>,
    _state: Arc<Mutex<TrayState>>,
) -> Result<TrayIcon, String> {
    // 创建菜单
    let menu = Menu::new();
    
    let show_item = MenuItem::new("显示窗口", true, None);
    let pause_item = MenuItem::new("暂停记录", true, None);
    let new_segment_item = MenuItem::new("新建日志段", true, None);
    let open_log_item = MenuItem::new("打开日志目录", true, None);
    let quit_item = MenuItem::new("退出", true, None);
    
    // 保存菜单项 ID
    let show_id = show_item.id().clone();
    let pause_id = pause_item.id().clone();
    let new_segment_id = new_segment_item.id().clone();
    let open_log_id = open_log_item.id().clone();
    let quit_id = quit_item.id().clone();
    
    menu.append(&show_item).map_err(|e| format!("添加菜单项失败: {}", e))?;
    menu.append(&PredefinedMenuItem::separator()).map_err(|e| format!("添加分隔符失败: {}", e))?;
    menu.append(&pause_item).map_err(|e| format!("添加菜单项失败: {}", e))?;
    menu.append(&new_segment_item).map_err(|e| format!("添加菜单项失败: {}", e))?;
    menu.append(&open_log_item).map_err(|e| format!("添加菜单项失败: {}", e))?;
    menu.append(&PredefinedMenuItem::separator()).map_err(|e| format!("添加分隔符失败: {}", e))?;
    menu.append(&quit_item).map_err(|e| format!("添加菜单项失败: {}", e))?;
    
    // 创建图标（使用内嵌的简单图标）
    let icon = create_default_icon()?;
    
    // 创建托盘图标
    let tray = TrayIconBuilder::new()
        .with_menu(Box::new(menu))
        .with_tooltip("EchoKey - 记录中")
        .with_icon(icon)
        .build()
        .map_err(|e| format!("创建托盘图标失败: {}", e))?;
    
    // 启动菜单事件监听线程
    std::thread::spawn(move || {
        let menu_channel = MenuEvent::receiver();
        
        loop {
            if let Ok(event) = menu_channel.recv() {
                let tray_event = if event.id == show_id {
                    TrayEvent::ShowWindow
                } else if event.id == pause_id {
                    TrayEvent::TogglePause
                } else if event.id == new_segment_id {
                    TrayEvent::NewSegment
                } else if event.id == open_log_id {
                    TrayEvent::OpenLogDir
                } else if event.id == quit_id {
                    TrayEvent::Quit
                } else {
                    continue;
                };
                
                if event_tx.send(tray_event).is_err() {
                    break;
                }
            }
        }
    });
    
    Ok(tray)
}

/// 创建默认图标（蓝色圆形，带 E 字母）
fn create_default_icon() -> Result<Icon, String> {
    // 创建一个简单的 32x32 RGBA 图标
    let size = 32u32;
    let mut rgba = vec![0u8; (size * size * 4) as usize];
    
    let center = size as f32 / 2.0;
    let radius = center - 2.0;
    
    for y in 0..size {
        for x in 0..size {
            let idx = ((y * size + x) * 4) as usize;
            let dx = x as f32 - center;
            let dy = y as f32 - center;
            let dist = (dx * dx + dy * dy).sqrt();
            
            if dist <= radius {
                // Apple Blue: #007AFF
                rgba[idx] = 0;      // R
                rgba[idx + 1] = 122; // G
                rgba[idx + 2] = 255; // B
                rgba[idx + 3] = 255; // A
                
                // 绘制简单的 E 字母（白色）
                let in_e = is_in_letter_e(x, y, size);
                if in_e {
                    rgba[idx] = 255;     // R
                    rgba[idx + 1] = 255; // G
                    rgba[idx + 2] = 255; // B
                }
            } else if dist <= radius + 1.0 {
                // 抗锯齿边缘
                let alpha = ((radius + 1.0 - dist) * 255.0) as u8;
                rgba[idx] = 0;
                rgba[idx + 1] = 122;
                rgba[idx + 2] = 255;
                rgba[idx + 3] = alpha;
            }
        }
    }
    
    Icon::from_rgba(rgba, size, size)
        .map_err(|e| format!("创建图标失败: {}", e))
}

/// 判断点是否在字母 E 内
fn is_in_letter_e(x: u32, y: u32, size: u32) -> bool {
    let cx = size / 2;
    let cy = size / 2;
    
    // E 字母的边界
    let left = cx - 5;
    let right = cx + 5;
    let top = cy - 7;
    let bottom = cy + 7;
    let mid = cy;
    
    // 竖线
    if x >= left && x <= left + 2 && y >= top && y <= bottom {
        return true;
    }
    
    // 上横线
    if x >= left && x <= right && y >= top && y <= top + 2 {
        return true;
    }
    
    // 中横线
    if x >= left && x <= right - 2 && y >= mid - 1 && y <= mid + 1 {
        return true;
    }
    
    // 下横线
    if x >= left && x <= right && y >= bottom - 2 && y <= bottom {
        return true;
    }
    
    false
}

/// 更新托盘图标的暂停状态
pub fn update_pause_state(tray: &TrayIcon, paused: bool) {
    let tooltip = if paused {
        "EchoKey - 已暂停"
    } else {
        "EchoKey - 记录中"
    };
    
    let _ = tray.set_tooltip(Some(tooltip));
    
    // 可以根据状态更换图标颜色
    if let Ok(icon) = create_status_icon(paused) {
        let _ = tray.set_icon(Some(icon));
    }
}

/// 创建状态图标
fn create_status_icon(paused: bool) -> Result<Icon, String> {
    let size = 32u32;
    let mut rgba = vec![0u8; (size * size * 4) as usize];
    
    let center = size as f32 / 2.0;
    let radius = center - 2.0;
    
    // 颜色选择
    let (r, g, b) = if paused {
        (142, 142, 147) // SF Gray
    } else {
        (0, 122, 255)   // Apple Blue
    };
    
    for y in 0..size {
        for x in 0..size {
            let idx = ((y * size + x) * 4) as usize;
            let dx = x as f32 - center;
            let dy = y as f32 - center;
            let dist = (dx * dx + dy * dy).sqrt();
            
            if dist <= radius {
                rgba[idx] = r;
                rgba[idx + 1] = g;
                rgba[idx + 2] = b;
                rgba[idx + 3] = 255;
                
                // E 字母
                if is_in_letter_e(x, y, size) {
                    rgba[idx] = 255;
                    rgba[idx + 1] = 255;
                    rgba[idx + 2] = 255;
                }
            } else if dist <= radius + 1.0 {
                let alpha = ((radius + 1.0 - dist) * 255.0) as u8;
                rgba[idx] = r;
                rgba[idx + 1] = g;
                rgba[idx + 2] = b;
                rgba[idx + 3] = alpha;
            }
        }
    }
    
    Icon::from_rgba(rgba, size, size)
        .map_err(|e| format!("创建图标失败: {}", e))
}

