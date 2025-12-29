//! 日志写入模块
//!
//! 负责将用户输入实时写入本地日志文件。
//! 
//! 设计原则：
//! - 实时写入：每次写入后立即 flush，确保数据不丢失
//! - 按日期分文件：每天一个新文件
//! - 支持手动分段：用户可以手动创建新的日志段

use std::fs::{self, File, OpenOptions};
use std::io::{self, Write, BufWriter};
use std::path::PathBuf;
use std::time::Instant;
use chrono::{Local, NaiveDate};
use crate::config;

/// 日志写入器
/// 
/// 管理日志文件的创建、写入和分段。
pub struct Logger {
    /// 当前日志文件的写入器
    writer: Option<BufWriter<File>>,
    /// 当前日志文件的日期
    current_date: Option<NaiveDate>,
    /// 当前日志文件的段号（用于手动分段）
    segment_number: u32,
    /// 上次写入时间（用于判断是否需要添加时间戳）
    last_write_time: Option<Instant>,
    /// 当前行是否为空（用于判断是否需要添加时间戳）
    current_line_empty: bool,
    /// 是否暂停记录
    paused: bool,
    /// 文件是否已写入头部（防止重复写入）
    header_written: bool,
}

impl Logger {
    /// 创建新的日志写入器
    pub fn new() -> io::Result<Self> {
        // 确保日志目录存在
        let log_dir = config::get_log_directory();
        fs::create_dir_all(&log_dir)?;
        
        Ok(Self {
            writer: None,
            current_date: None,
            segment_number: 0,
            last_write_time: None,
            current_line_empty: true,
            paused: false,
            header_written: false,
        })
    }

    /// 获取当前日志文件路径
    fn get_log_path(&self, date: NaiveDate) -> PathBuf {
        let log_dir = config::get_log_directory();
        let filename = if self.segment_number == 0 {
            format!("{}.log", date.format("%Y-%m-%d"))
        } else {
            format!("{}_{:02}.log", date.format("%Y-%m-%d"), self.segment_number)
        };
        log_dir.join(filename)
    }

    /// 确保日志文件已打开且日期正确
    fn ensure_file(&mut self) -> io::Result<()> {
        let today = Local::now().date_naive();
        
        // 如果日期变了，需要创建新文件
        if self.current_date != Some(today) {
            self.current_date = Some(today);
            self.segment_number = 0;
            self.header_written = false;
            self.writer = None; // 关闭旧文件
            return self.open_or_create_file();
        }
        
        // 如果文件还没打开，打开它
        if self.writer.is_none() {
            return self.open_or_create_file();
        }
        
        Ok(())
    }

    /// 打开或创建日志文件
    fn open_or_create_file(&mut self) -> io::Result<()> {
        let date = self.current_date.unwrap_or_else(|| Local::now().date_naive());
        let path = self.get_log_path(date);
        
        // 确保目录存在
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)?;
        }
        
        // 检查文件是否已存在且有内容
        let file_exists = path.exists();
        let file_has_content = file_exists && fs::metadata(&path).map(|m| m.len() > 0).unwrap_or(false);
        
        // 打开文件（追加模式）
        let file = OpenOptions::new()
            .create(true)
            .append(true)
            .open(&path)?;
        
        let mut writer = BufWriter::new(file);
        
        // 只在新文件时写入头部
        if !file_has_content && !self.header_written {
            self.write_header_to(&mut writer)?;
            self.header_written = true;
        } else if file_has_content {
            // 文件已存在且有内容，标记头部已写入
            self.header_written = true;
        }
        
        self.writer = Some(writer);
        self.current_line_empty = true;
        
        Ok(())
    }

    /// 写入文件头部
    fn write_header_to(&self, writer: &mut BufWriter<File>) -> io::Result<()> {
        let now = Local::now();
        writeln!(writer, "================== EchoKey 日志 ==================")?;
        writeln!(writer, "日期：{}", now.format("%Y-%m-%d"))?;
        writeln!(writer, "创建时间：{}", now.format("%H:%M:%S"))?;
        writeln!(writer, "==================================================")?;
        writeln!(writer)?;
        writer.flush()?;
        Ok(())
    }

    /// 写入时间戳
    fn write_timestamp(&mut self) -> io::Result<()> {
        if let Some(ref mut writer) = self.writer {
            let now = Local::now();
            write!(writer, "[{}] ", now.format("%H:%M:%S"))?;
            writer.flush()?;
        }
        Ok(())
    }

    /// 检查是否需要添加时间戳（超时或新行）
    fn should_add_timestamp(&self) -> bool {
        // 如果当前行为空，需要添加时间戳
        if self.current_line_empty {
            return true;
        }
        
        // 如果超过 30 秒没有输入，需要添加时间戳
        if let Some(last_time) = self.last_write_time {
            if last_time.elapsed() > config::IDLE_TIMEOUT {
                return true;
            }
        }
        
        false
    }

    /// 写入文本内容
    /// 
    /// 这是最常用的写入方法，用于记录用户输入的字符。
    pub fn write_text(&mut self, text: &str) -> io::Result<()> {
        if self.paused {
            return Ok(());
        }
        
        self.ensure_file()?;
        
        // 检查是否需要换行并添加时间戳
        let need_timestamp = self.should_add_timestamp();
        
        if need_timestamp {
            // 如果不是空行，先换行
            if !self.current_line_empty {
                if let Some(ref mut writer) = self.writer {
                    writeln!(writer)?;
                    writer.flush()?;
                }
            }
            self.write_timestamp()?;
            self.current_line_empty = false;
        }
        
        // 写入内容
        if let Some(ref mut writer) = self.writer {
            write!(writer, "{}", text)?;
            writer.flush()?;
        }
        
        self.last_write_time = Some(Instant::now());
        
        Ok(())
    }

    /// 处理 Enter 键：换行并添加新时间戳
    pub fn handle_enter(&mut self) -> io::Result<()> {
        if self.paused {
            return Ok(());
        }
        
        self.ensure_file()?;
        
        if let Some(ref mut writer) = self.writer {
            writeln!(writer)?;
            writer.flush()?;
        }
        
        self.current_line_empty = true;
        self.last_write_time = Some(Instant::now());
        
        Ok(())
    }

    /// 处理 Ctrl+Enter：只换行，不添加新时间戳
    pub fn handle_ctrl_enter(&mut self) -> io::Result<()> {
        if self.paused {
            return Ok(());
        }
        
        self.ensure_file()?;
        
        if let Some(ref mut writer) = self.writer {
            writeln!(writer)?;
            // 写入缩进对齐时间戳
            write!(writer, "          ")?; // 与时间戳 [HH:MM:SS] 对齐
            writer.flush()?;
        }
        
        // 不设置 current_line_empty = true，这样下次写入不会添加时间戳
        self.last_write_time = Some(Instant::now());
        
        Ok(())
    }

    /// 写入粘贴内容
    pub fn write_paste(&mut self, content: &str) -> io::Result<()> {
        if self.paused {
            return Ok(());
        }
        
        self.ensure_file()?;
        
        // 先换行
        if !self.current_line_empty {
            if let Some(ref mut writer) = self.writer {
                writeln!(writer)?;
            }
        }
        
        // 写入粘贴标记和内容
        if let Some(ref mut writer) = self.writer {
            let now = Local::now();
            writeln!(writer, "[{}] [粘贴] {}", now.format("%H:%M:%S"), content)?;
            writer.flush()?;
        }
        
        self.current_line_empty = true;
        self.last_write_time = Some(Instant::now());
        
        Ok(())
    }

    /// 写入手动保存内容
    pub fn write_manual_save(&mut self, content: &str) -> io::Result<()> {
        if self.paused {
            return Ok(());
        }
        
        self.ensure_file()?;
        
        // 先换行
        if !self.current_line_empty {
            if let Some(ref mut writer) = self.writer {
                writeln!(writer)?;
            }
        }
        
        // 写入手动保存标记和内容
        if let Some(ref mut writer) = self.writer {
            let now = Local::now();
            writeln!(writer, "[{}] [手动保存] {}", now.format("%H:%M:%S"), content)?;
            writer.flush()?;
        }
        
        self.current_line_empty = true;
        self.last_write_time = Some(Instant::now());
        
        Ok(())
    }

    /// 手动创建新的日志段
    pub fn new_segment(&mut self) -> io::Result<()> {
        // 关闭当前文件
        if let Some(ref mut writer) = self.writer {
            writer.flush()?;
        }
        self.writer = None;
        
        // 增加段号
        self.segment_number += 1;
        self.header_written = false;
        
        // 创建新文件
        self.open_or_create_file()?;
        
        Ok(())
    }

    /// 暂停记录
    pub fn pause(&mut self) -> io::Result<()> {
        if !self.paused {
            self.paused = true;
            
            self.ensure_file()?;
            
            // 写入暂停标记
            if !self.current_line_empty {
                if let Some(ref mut writer) = self.writer {
                    writeln!(writer)?;
                }
            }
            
            if let Some(ref mut writer) = self.writer {
                let now = Local::now();
                writeln!(writer, "[{}] --- 暂停记录 ---", now.format("%H:%M:%S"))?;
                writer.flush()?;
            }
            
            self.current_line_empty = true;
        }
        Ok(())
    }

    /// 恢复记录
    pub fn resume(&mut self) -> io::Result<()> {
        if self.paused {
            self.paused = false;
            
            self.ensure_file()?;
            
            // 写入恢复标记
            if let Some(ref mut writer) = self.writer {
                let now = Local::now();
                writeln!(writer, "[{}] --- 恢复记录 ---", now.format("%H:%M:%S"))?;
                writer.flush()?;
            }
            
            self.current_line_empty = true;
        }
        Ok(())
    }

    /// 切换暂停/恢复状态
    pub fn toggle_pause(&mut self) -> io::Result<bool> {
        if self.paused {
            self.resume()?;
        } else {
            self.pause()?;
        }
        Ok(self.paused)
    }

    /// 是否处于暂停状态
    pub fn is_paused(&self) -> bool {
        self.paused
    }

    /// 设置暂停状态
    pub fn set_paused(&mut self, paused: bool) -> io::Result<()> {
        if paused != self.paused {
            if paused {
                self.pause()?;
            } else {
                self.resume()?;
            }
        }
        Ok(())
    }

    /// 获取日志目录路径
    pub fn get_log_directory(&self) -> PathBuf {
        config::get_log_directory()
    }
}

impl Default for Logger {
    fn default() -> Self {
        Self::new().expect("无法创建日志目录")
    }
}

impl Drop for Logger {
    fn drop(&mut self) {
        // 确保文件被正确关闭
        if let Some(ref mut writer) = self.writer {
            let _ = writer.flush();
        }
    }
}
