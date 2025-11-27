//! 系统托盘模块
//!
//! 在系统托盘显示图标和菜单，让用户可以：
//! - 查看运行状态
//! - 暂停/恢复记录
//! - 打开日志目录
//! - 新建日志段
//! - 退出程序

use tray_icon::{
    TrayIconBuilder, TrayIcon, TrayIconEvent,
    menu::{Menu, MenuItem, MenuEvent, PredefinedMenuItem},
    Icon,
};
use crossbeam_channel::Receiver;

/// 托盘菜单事件
#[derive(Debug, Clone)]
pub enum TrayEvent {
    /// 暂停/恢复记录
    TogglePause,
    /// 打开日志目录
    OpenLogDir,
    /// 新建日志段
    NewSegment,
    /// 退出程序
    Quit,
}

/// 菜单项 ID
struct MenuIds {
    toggle_pause: MenuItem,
    open_log_dir: MenuItem,
    new_segment: MenuItem,
    quit: MenuItem,
}

/// 系统托盘
pub struct SystemTray {
    _tray_icon: TrayIcon,
    menu_ids: MenuIds,
    is_paused: bool,
}

impl SystemTray {
    /// 创建系统托盘
    pub fn new() -> Result<Self, String> {
        // 创建菜单
        let menu = Menu::new();
        
        let toggle_pause = MenuItem::new("⏸ 暂停记录", true, None);
        let open_log_dir = MenuItem::new("📂 打开日志目录", true, None);
        let new_segment = MenuItem::new("📄 新建日志段", true, None);
        let separator = PredefinedMenuItem::separator();
        let quit = MenuItem::new("❌ 退出", true, None);
        
        menu.append(&toggle_pause).map_err(|e| format!("菜单错误: {}", e))?;
        menu.append(&open_log_dir).map_err(|e| format!("菜单错误: {}", e))?;
        menu.append(&new_segment).map_err(|e| format!("菜单错误: {}", e))?;
        menu.append(&separator).map_err(|e| format!("菜单错误: {}", e))?;
        menu.append(&quit).map_err(|e| format!("菜单错误: {}", e))?;
        
        // 创建图标（使用内置图标）
        let icon = create_icon()?;
        
        // 创建托盘图标
        let tray_icon = TrayIconBuilder::new()
            .with_menu(Box::new(menu))
            .with_tooltip("EchoKey - 记录中 ✓")
            .with_icon(icon)
            .build()
            .map_err(|e| format!("无法创建托盘图标: {}", e))?;
        
        Ok(Self {
            _tray_icon: tray_icon,
            menu_ids: MenuIds {
                toggle_pause,
                open_log_dir,
                new_segment,
                quit,
            },
            is_paused: false,
        })
    }
    
    /// 处理菜单事件
    pub fn handle_menu_event(&self, event: &MenuEvent) -> Option<TrayEvent> {
        if event.id == self.menu_ids.toggle_pause.id() {
            Some(TrayEvent::TogglePause)
        } else if event.id == self.menu_ids.open_log_dir.id() {
            Some(TrayEvent::OpenLogDir)
        } else if event.id == self.menu_ids.new_segment.id() {
            Some(TrayEvent::NewSegment)
        } else if event.id == self.menu_ids.quit.id() {
            Some(TrayEvent::Quit)
        } else {
            None
        }
    }
    
    /// 更新暂停状态
    pub fn set_paused(&mut self, paused: bool) {
        self.is_paused = paused;
        
        let (text, tooltip) = if paused {
            ("▶ 恢复记录", "EchoKey - 已暂停")
        } else {
            ("⏸ 暂停记录", "EchoKey - 记录中 ✓")
        };
        
        self.menu_ids.toggle_pause.set_text(text);
        // 注意：tray-icon 库目前不支持动态更新 tooltip
        // 如果需要，可以考虑重建托盘图标
        let _ = tooltip; // 暂时忽略
    }
}

/// 创建托盘图标
/// 
/// 创建一个简单的 16x16 图标
fn create_icon() -> Result<Icon, String> {
    // 创建一个简单的 16x16 绿色方块图标
    // RGBA 格式，每个像素 4 字节
    let size = 16;
    let mut rgba = Vec::with_capacity(size * size * 4);
    
    for y in 0..size {
        for x in 0..size {
            // 简单的圆形图标
            let dx = x as f32 - 7.5;
            let dy = y as f32 - 7.5;
            let dist = (dx * dx + dy * dy).sqrt();
            
            if dist < 6.0 {
                // 绿色填充
                rgba.push(76);   // R
                rgba.push(175);  // G
                rgba.push(80);   // B
                rgba.push(255);  // A
            } else if dist < 7.5 {
                // 深绿色边框
                rgba.push(46);   // R
                rgba.push(125);  // G
                rgba.push(50);   // B
                rgba.push(255);  // A
            } else {
                // 透明
                rgba.push(0);
                rgba.push(0);
                rgba.push(0);
                rgba.push(0);
            }
        }
    }
    
    Icon::from_rgba(rgba, size as u32, size as u32)
        .map_err(|e| format!("无法创建图标: {}", e))
}

/// 获取菜单事件接收器
pub fn menu_event_receiver() -> Receiver<MenuEvent> {
    MenuEvent::receiver().clone()
}

/// 获取托盘图标事件接收器
#[allow(dead_code)]
pub fn tray_event_receiver() -> Receiver<TrayIconEvent> {
    TrayIconEvent::receiver().clone()
}
