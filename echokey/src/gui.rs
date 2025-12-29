//! EchoKey GUI 模块
//! 
//! Apple 风格的用户界面，使用 egui/eframe 实现
//! 
//! 设计原则：
//! - 圆角：12px
//! - 配色：磨砂白背景 #F5F5F7，Apple Blue #007AFF
//! - 字体：系统默认，清晰易读
//! - 动画：流畅的过渡效果

use std::sync::{Arc, Mutex};
use std::path::PathBuf;
use eframe::egui;
use chrono::Local;

use crate::autostart;

/// 当前显示的页面
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Page {
    Status,
    History,
    Settings,
}

/// GUI 应用状态
pub struct EchoKeyApp {
    /// 当前页面
    current_page: Page,
    /// 是否暂停记录
    is_paused: bool,
    /// 今日字符数
    today_chars: usize,
    /// 日志目录
    log_directory: PathBuf,
    /// 是否开机自启动
    autostart_enabled: bool,
    /// 搜索关键词
    search_query: String,
    /// 日志内容（用于历史页面）
    log_content: String,
    /// 状态消息
    status_message: Option<(String, std::time::Instant)>,
    /// 共享状态（与主程序通信）
    shared_state: Option<Arc<Mutex<SharedGuiState>>>,
}

/// 与主程序共享的状态
pub struct SharedGuiState {
    pub paused: bool,
    pub today_chars: usize,
    pub request_new_segment: bool,
    pub request_open_log: bool,
}

impl Default for SharedGuiState {
    fn default() -> Self {
        Self {
            paused: false,
            today_chars: 0,
            request_new_segment: false,
            request_open_log: false,
        }
    }
}

impl Default for EchoKeyApp {
    fn default() -> Self {
        Self {
            current_page: Page::Status,
            is_paused: false,
            today_chars: 0,
            log_directory: PathBuf::new(),
            autostart_enabled: autostart::is_enabled(),
            search_query: String::new(),
            log_content: String::new(),
            status_message: None,
            shared_state: None,
        }
    }
}

impl EchoKeyApp {
    /// 创建新的 GUI 应用
    pub fn new(
        cc: &eframe::CreationContext<'_>,
        log_directory: PathBuf,
        shared_state: Arc<Mutex<SharedGuiState>>,
    ) -> Self {
        // 配置 Apple 风格的视觉效果
        configure_apple_style(&cc.egui_ctx);
        
        Self {
            current_page: Page::Status,
            is_paused: false,
            today_chars: 0,
            log_directory,
            autostart_enabled: autostart::is_enabled(),
            search_query: String::new(),
            log_content: String::new(),
            status_message: None,
            shared_state: Some(shared_state),
        }
    }
    
    /// 显示状态消息
    fn show_message(&mut self, msg: &str) {
        self.status_message = Some((msg.to_string(), std::time::Instant::now()));
    }
    
    /// 渲染状态页面
    fn render_status_page(&mut self, ui: &mut egui::Ui) {
        ui.add_space(20.0);
        
        // 状态卡片
        egui::Frame::none()
            .fill(egui::Color32::WHITE)
            .rounding(egui::Rounding::same(16.0))
            .inner_margin(egui::Margin::same(24.0))
            .shadow(egui::epaint::Shadow {
                offset: egui::vec2(0.0, 2.0),
                blur: 8.0,
                spread: 0.0,
                color: egui::Color32::from_black_alpha(20),
            })
            .show(ui, |ui| {
                ui.vertical_centered(|ui| {
                    // 状态图标
                    let status_color = if self.is_paused {
                        egui::Color32::from_rgb(142, 142, 147) // SF Gray
                    } else {
                        egui::Color32::from_rgb(52, 199, 89) // SF Green
                    };
                    
                    let status_text = if self.is_paused { "已暂停" } else { "记录中" };
                    
                    // 大圆形状态指示器
                    let (rect, _) = ui.allocate_exact_size(egui::vec2(80.0, 80.0), egui::Sense::hover());
                    ui.painter().circle_filled(rect.center(), 40.0, status_color);
                    
                    // 动画效果：记录中时显示脉冲
                    if !self.is_paused {
                        let time = ui.ctx().input(|i| i.time);
                        let alpha = ((time * 2.0).sin() * 0.3 + 0.3) as f32;
                        ui.painter().circle_filled(
                            rect.center(),
                            40.0 + (time * 2.0).sin() as f32 * 5.0,
                            egui::Color32::from_rgba_unmultiplied(52, 199, 89, (alpha * 255.0) as u8),
                        );
                    }
                    
                    ui.add_space(16.0);
                    
                    ui.label(egui::RichText::new(status_text)
                        .size(24.0)
                        .color(status_color)
                        .strong());
                    
                    ui.add_space(24.0);
                    
                    // 今日统计
                    ui.label(egui::RichText::new("今日输入")
                        .size(14.0)
                        .color(egui::Color32::from_rgb(142, 142, 147)));
                    
                    ui.label(egui::RichText::new(format!("{} 字符", self.today_chars))
                        .size(36.0)
                        .strong());
                    
                    ui.add_space(16.0);
                    
                    // 当前时间
                    let now = Local::now();
                    ui.label(egui::RichText::new(now.format("%Y年%m月%d日 %H:%M").to_string())
                        .size(14.0)
                        .color(egui::Color32::from_rgb(142, 142, 147)));
                });
            });
        
        ui.add_space(20.0);
        
        // 操作按钮
        ui.horizontal(|ui| {
            let button_width = (ui.available_width() - 16.0) / 2.0;
            
            // 暂停/恢复按钮
            let pause_text = if self.is_paused { "▶ 恢复" } else { "⏸ 暂停" };
            if ui.add_sized(
                egui::vec2(button_width, 44.0),
                egui::Button::new(egui::RichText::new(pause_text).size(16.0))
                    .fill(if self.is_paused {
                        egui::Color32::from_rgb(52, 199, 89)
                    } else {
                        egui::Color32::from_rgb(255, 149, 0)
                    })
                    .rounding(egui::Rounding::same(10.0))
            ).clicked() {
                self.is_paused = !self.is_paused;
                if let Some(ref state) = self.shared_state {
                    if let Ok(mut s) = state.lock() {
                        s.paused = self.is_paused;
                    }
                }
            }
            
            ui.add_space(16.0);
            
            // 新建日志段按钮
            if ui.add_sized(
                egui::vec2(button_width, 44.0),
                egui::Button::new(egui::RichText::new("📝 新日志段").size(16.0))
                    .fill(egui::Color32::from_rgb(0, 122, 255))
                    .rounding(egui::Rounding::same(10.0))
            ).clicked() {
                if let Some(ref state) = self.shared_state {
                    if let Ok(mut s) = state.lock() {
                        s.request_new_segment = true;
                    }
                }
                self.show_message("已创建新日志段");
            }
        });
        
        ui.add_space(12.0);
        
        // 打开日志目录按钮
        if ui.add_sized(
            egui::vec2(ui.available_width(), 44.0),
            egui::Button::new(egui::RichText::new("📂 打开日志目录").size(16.0))
                .fill(egui::Color32::from_rgb(88, 86, 214))
                .rounding(egui::Rounding::same(10.0))
        ).clicked() {
            open_directory(&self.log_directory);
        }
        
        // 状态消息
        if let Some((msg, time)) = &self.status_message {
            if time.elapsed().as_secs() < 3 {
                ui.add_space(16.0);
                ui.horizontal(|ui| {
                    ui.add_space((ui.available_width() - 200.0) / 2.0);
                    egui::Frame::none()
                        .fill(egui::Color32::from_rgb(52, 199, 89))
                        .rounding(egui::Rounding::same(8.0))
                        .inner_margin(egui::Margin::symmetric(16.0, 8.0))
                        .show(ui, |ui| {
                            ui.label(egui::RichText::new(msg).color(egui::Color32::WHITE));
                        });
                });
            }
        }
    }
    
    /// 渲染历史页面
    fn render_history_page(&mut self, ui: &mut egui::Ui) {
        ui.add_space(12.0);
        
        // 搜索框
        ui.horizontal(|ui| {
            let response = ui.add_sized(
                egui::vec2(ui.available_width() - 80.0, 36.0),
                egui::TextEdit::singleline(&mut self.search_query)
                    .hint_text("🔍 搜索日志内容...")
            );
            
            if ui.add_sized(
                egui::vec2(72.0, 36.0),
                egui::Button::new("搜索")
                    .fill(egui::Color32::from_rgb(0, 122, 255))
            ).clicked() || (response.lost_focus() && ui.input(|i| i.key_pressed(egui::Key::Enter))) {
                self.load_log_content();
            }
        });
        
        ui.add_space(12.0);
        
        // 日志列表/内容区
        egui::Frame::none()
            .fill(egui::Color32::WHITE)
            .rounding(egui::Rounding::same(12.0))
            .inner_margin(egui::Margin::same(16.0))
            .show(ui, |ui| {
                if self.log_content.is_empty() {
                    // 显示日志文件列表
                    ui.label(egui::RichText::new("最近日志")
                        .size(16.0)
                        .strong());
                    ui.add_space(8.0);
                    
                    if let Ok(entries) = std::fs::read_dir(&self.log_directory) {
                        let mut files: Vec<_> = entries
                            .filter_map(|e| e.ok())
                            .filter(|e| e.path().extension().map_or(false, |ext| ext == "log"))
                            .collect();
                        
                        files.sort_by(|a, b| b.path().cmp(&a.path()));
                        
                        for entry in files.iter().take(10) {
                            let path = entry.path();
                            let name = path.file_name()
                                .map(|n| n.to_string_lossy().to_string())
                                .unwrap_or_default();
                            
                            if ui.add(egui::Button::new(&name)
                                .frame(false)
                            ).clicked() {
                                if let Ok(content) = std::fs::read_to_string(&path) {
                                    self.log_content = content;
                                }
                            }
                            ui.add_space(4.0);
                        }
                        
                        if files.is_empty() {
                            ui.label(egui::RichText::new("暂无日志文件")
                                .color(egui::Color32::from_rgb(142, 142, 147)));
                        }
                    }
                } else {
                    // 显示日志内容
                    ui.horizontal(|ui| {
                        if ui.button("← 返回").clicked() {
                            self.log_content.clear();
                        }
                        ui.add_space(8.0);
                        if ui.button("📋 复制全部").clicked() {
                            ui.output_mut(|o| o.copied_text = self.log_content.clone());
                            self.show_message("已复制到剪贴板");
                        }
                    });
                    
                    ui.add_space(8.0);
                    
                    egui::ScrollArea::vertical()
                        .auto_shrink([false; 2])
                        .show(ui, |ui| {
                            let content = if self.search_query.is_empty() {
                                self.log_content.clone()
                            } else {
                                // 高亮搜索结果
                                self.log_content.clone()
                            };
                            ui.add(egui::Label::new(
                                egui::RichText::new(&content)
                                    .monospace()
                                    .size(12.0)
                            ).wrap());
                        });
                }
            });
    }
    
    /// 渲染设置页面
    fn render_settings_page(&mut self, ui: &mut egui::Ui) {
        ui.add_space(12.0);
        
        egui::Frame::none()
            .fill(egui::Color32::WHITE)
            .rounding(egui::Rounding::same(12.0))
            .inner_margin(egui::Margin::same(20.0))
            .show(ui, |ui| {
                ui.label(egui::RichText::new("通用设置").size(18.0).strong());
                ui.add_space(16.0);
                
                // 开机自启动
                ui.horizontal(|ui| {
                    ui.label("开机自启动");
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        let mut enabled = self.autostart_enabled;
                        if ui.add(toggle_switch(&mut enabled)).changed() {
                            if enabled {
                                if autostart::enable().is_ok() {
                                    self.autostart_enabled = true;
                                    self.show_message("已启用开机自启动");
                                }
                            } else {
                                if autostart::disable().is_ok() {
                                    self.autostart_enabled = false;
                                    self.show_message("已禁用开机自启动");
                                }
                            }
                        }
                    });
                });
                
                ui.add_space(12.0);
                ui.separator();
                ui.add_space(12.0);
                
                // 日志目录
                ui.label("日志存储位置");
                ui.add_space(4.0);
                ui.horizontal(|ui| {
                    let path_str = self.log_directory.to_string_lossy();
                    ui.add(egui::TextEdit::singleline(&mut path_str.to_string())
                        .desired_width(ui.available_width() - 80.0)
                        .interactive(false));
                    if ui.button("打开").clicked() {
                        open_directory(&self.log_directory);
                    }
                });
                
                ui.add_space(20.0);
                ui.label(egui::RichText::new("快捷键").size(18.0).strong());
                ui.add_space(16.0);
                
                // 快捷键说明
                let shortcuts = [
                    ("Ctrl+Shift+P", "暂停/恢复记录"),
                    ("Ctrl+Shift+S", "手动保存剪贴板"),
                    ("Ctrl+Shift+N", "新建日志段"),
                ];
                
                for (key, desc) in shortcuts {
                    ui.horizontal(|ui| {
                        egui::Frame::none()
                            .fill(egui::Color32::from_rgb(229, 229, 234))
                            .rounding(egui::Rounding::same(6.0))
                            .inner_margin(egui::Margin::symmetric(8.0, 4.0))
                            .show(ui, |ui| {
                                ui.label(egui::RichText::new(key).monospace().size(12.0));
                            });
                        ui.add_space(12.0);
                        ui.label(desc);
                    });
                    ui.add_space(8.0);
                }
                
                ui.add_space(20.0);
                ui.label(egui::RichText::new("关于").size(18.0).strong());
                ui.add_space(16.0);
                
                ui.label(format!("EchoKey v{}", env!("CARGO_PKG_VERSION")));
                ui.label(egui::RichText::new("你打下的每一个字，都有回声")
                    .size(12.0)
                    .color(egui::Color32::from_rgb(142, 142, 147)));
            });
    }
    
    /// 加载日志内容
    fn load_log_content(&mut self) {
        // 加载今天的日志
        let today = Local::now().format("%Y-%m-%d").to_string();
        let log_path = self.log_directory.join(format!("{}.log", today));
        
        if let Ok(content) = std::fs::read_to_string(&log_path) {
            self.log_content = content;
        }
    }
}

impl eframe::App for EchoKeyApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        // 同步共享状态
        if let Some(ref state) = self.shared_state {
            if let Ok(s) = state.lock() {
                self.today_chars = s.today_chars;
            }
        }
        
        // 清除过期的状态消息
        if let Some((_, time)) = &self.status_message {
            if time.elapsed().as_secs() >= 3 {
                self.status_message = None;
            }
        }
        
        // 主面板
        egui::CentralPanel::default()
            .frame(egui::Frame::none()
                .fill(egui::Color32::from_rgb(242, 242, 247)) // SF Gray 6
                .inner_margin(egui::Margin::same(0.0)))
            .show(ctx, |ui| {
                // 自定义标题栏
                render_title_bar(ui, ctx);
                
                ui.add_space(8.0);
                
                // 导航栏
                ui.horizontal(|ui| {
                    ui.add_space(16.0);
                    
                    let nav_items = [
                        (Page::Status, "状态"),
                        (Page::History, "历史"),
                        (Page::Settings, "设置"),
                    ];
                    
                    for (page, label) in nav_items {
                        let is_selected = self.current_page == page;
                        let text_color = if is_selected {
                            egui::Color32::from_rgb(0, 122, 255)
                        } else {
                            egui::Color32::from_rgb(142, 142, 147)
                        };
                        
                        if ui.add(egui::Button::new(
                            egui::RichText::new(label)
                                .size(15.0)
                                .color(text_color)
                        ).frame(false)).clicked() {
                            self.current_page = page;
                        }
                        
                        ui.add_space(16.0);
                    }
                });
                
                ui.add_space(8.0);
                
                // 页面内容
                egui::Frame::none()
                    .inner_margin(egui::Margin::symmetric(16.0, 0.0))
                    .show(ui, |ui| {
                        match self.current_page {
                            Page::Status => self.render_status_page(ui),
                            Page::History => self.render_history_page(ui),
                            Page::Settings => self.render_settings_page(ui),
                        }
                    });
            });
        
        // 请求持续重绘（为了动画效果）
        ctx.request_repaint_after(std::time::Duration::from_millis(100));
    }
    
    fn on_exit(&mut self, _gl: Option<&eframe::glow::Context>) {
        // 退出时的清理工作
    }
}

/// 配置 Apple 风格的视觉效果
fn configure_apple_style(ctx: &egui::Context) {
    let mut style = (*ctx.style()).clone();
    
    // 圆角设置
    style.visuals.window_rounding = egui::Rounding::same(12.0);
    style.visuals.widgets.noninteractive.rounding = egui::Rounding::same(8.0);
    style.visuals.widgets.inactive.rounding = egui::Rounding::same(8.0);
    style.visuals.widgets.hovered.rounding = egui::Rounding::same(8.0);
    style.visuals.widgets.active.rounding = egui::Rounding::same(8.0);
    
    // 颜色
    style.visuals.widgets.noninteractive.bg_fill = egui::Color32::from_rgb(242, 242, 247);
    style.visuals.selection.bg_fill = egui::Color32::from_rgb(0, 122, 255);
    style.visuals.hyperlink_color = egui::Color32::from_rgb(0, 122, 255);
    
    // 按钮样式
    style.visuals.widgets.inactive.weak_bg_fill = egui::Color32::from_rgb(229, 229, 234);
    style.visuals.widgets.hovered.weak_bg_fill = egui::Color32::from_rgb(209, 209, 214);
    
    // 文本颜色
    style.visuals.widgets.noninteractive.fg_stroke.color = egui::Color32::from_rgb(28, 28, 30);
    
    // 间距
    style.spacing.item_spacing = egui::vec2(8.0, 8.0);
    style.spacing.button_padding = egui::vec2(12.0, 6.0);
    
    ctx.set_style(style);
}

/// 渲染自定义标题栏
fn render_title_bar(ui: &mut egui::Ui, ctx: &egui::Context) {
    egui::Frame::none()
        .fill(egui::Color32::from_rgba_unmultiplied(255, 255, 255, 230))
        .inner_margin(egui::Margin::symmetric(16.0, 12.0))
        .show(ui, |ui| {
            ui.horizontal(|ui| {
                // 窗口控制按钮（macOS 风格）
                let button_size = egui::vec2(12.0, 12.0);
                
                // 关闭按钮（红色）- 隐藏到托盘
                let (close_rect, close_response) = ui.allocate_exact_size(button_size, egui::Sense::click());
                let close_color = if close_response.hovered() {
                    egui::Color32::from_rgb(255, 95, 86)
                } else {
                    egui::Color32::from_rgb(255, 95, 86)
                };
                ui.painter().circle_filled(close_rect.center(), 6.0, close_color);
                if close_response.clicked() {
                    ctx.send_viewport_cmd(egui::ViewportCommand::Visible(false));
                }
                
                ui.add_space(8.0);
                
                // 最小化按钮（黄色）
                let (min_rect, min_response) = ui.allocate_exact_size(button_size, egui::Sense::click());
                let min_color = if min_response.hovered() {
                    egui::Color32::from_rgb(255, 189, 46)
                } else {
                    egui::Color32::from_rgb(255, 189, 46)
                };
                ui.painter().circle_filled(min_rect.center(), 6.0, min_color);
                if min_response.clicked() {
                    ctx.send_viewport_cmd(egui::ViewportCommand::Minimized(true));
                }
                
                ui.add_space(8.0);
                
                // 全屏按钮（绿色）- 暂时不使用
                let (max_rect, _max_response) = ui.allocate_exact_size(button_size, egui::Sense::hover());
                ui.painter().circle_filled(max_rect.center(), 6.0, egui::Color32::from_rgb(39, 201, 63));
                
                // 标题
                ui.with_layout(egui::Layout::centered_and_justified(egui::Direction::LeftToRight), |ui| {
                    ui.label(egui::RichText::new("EchoKey")
                        .size(14.0)
                        .color(egui::Color32::from_rgb(28, 28, 30)));
                });
            });
        });
    
    // 拖动区域
    let title_bar_response = ui.interact(
        ui.min_rect(),
        ui.id().with("title_bar"),
        egui::Sense::drag(),
    );
    
    if title_bar_response.dragged() {
        ctx.send_viewport_cmd(egui::ViewportCommand::StartDrag);
    }
}

/// 创建 iOS 风格的开关
fn toggle_switch(on: &mut bool) -> impl egui::Widget + '_ {
    move |ui: &mut egui::Ui| {
        let desired_size = egui::vec2(51.0, 31.0);
        let (rect, mut response) = ui.allocate_exact_size(desired_size, egui::Sense::click());
        
        if response.clicked() {
            *on = !*on;
            response.mark_changed();
        }
        
        if ui.is_rect_visible(rect) {
            let how_on = ui.ctx().animate_bool_responsive(response.id, *on);
            let bg_color = egui::Color32::from_rgb(
                (142.0 + (52.0 - 142.0) * how_on) as u8,
                (142.0 + (199.0 - 142.0) * how_on) as u8,
                (147.0 + (89.0 - 147.0) * how_on) as u8,
            );
            
            let rounding = rect.height() / 2.0;
            ui.painter().rect_filled(rect, rounding, bg_color);
            
            let circle_x = egui::lerp(rect.left() + 15.5..=rect.right() - 15.5, how_on);
            let circle_center = egui::pos2(circle_x, rect.center().y);
            ui.painter().circle_filled(circle_center, 13.5, egui::Color32::WHITE);
        }
        
        response
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

/// 启动 GUI
pub fn run_gui(log_directory: PathBuf, shared_state: Arc<Mutex<SharedGuiState>>) -> eframe::Result<()> {
    let native_options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([380.0, 600.0])
            .with_min_inner_size([320.0, 480.0])
            .with_decorations(false) // 无边框窗口
            .with_transparent(true)
            .with_resizable(true),
        ..Default::default()
    };
    
    eframe::run_native(
        "EchoKey",
        native_options,
        Box::new(move |cc| Ok(Box::new(EchoKeyApp::new(cc, log_directory, shared_state)))),
    )
}

