# CLAUDE.md

此文档为 Claude Code 提供项目指导，帮助在此代码库中高效工作。

## 项目概述

EchoType 是一个 Windows 输入监控系统，核心理念是：**"你打下的每一个字，都有回声"**。确保用户的每一次输入都不会丢失——无论是浏览器崩溃、语音输入出错，还是表单刷新。

### 仓库结构

```
EchoType/
├── echokey/           # 主项目：Rust 系统级键盘记录工具（带 GUI）
└── playground/        # 实验性项目
    ├── ClipboardMonitor/   # C++ 剪贴板监控工具
    └── FloatingTool/       # C# 悬浮反馈窗口
```

## 构建命令

### EchoKey (Rust) - 主项目

```bash
cd echokey

# 构建
cargo build --release    # 生产构建（优化体积）
cargo build              # 调试构建

# 运行
cargo run                # 调试模式运行

# 测试
cargo test               # 运行所有测试
cargo test <test_name>   # 运行单个测试
```

输出位置：`echokey/target/release/echokey.exe` 或 `target/debug/echokey.exe`

### ClipboardMonitor (C++)

```bash
cd playground/ClipboardMonitor

# 使用 CMake
mkdir build && cd build
cmake .. && cmake --build . --config Release

# 或使用 build.bat（MSVC 直接编译）
./build.bat
```

### FloatingTool (C#)

```bash
cd playground

dotnet build -c Release  # 生产构建
dotnet run               # 运行
```

## 代码架构

### EchoKey 多线程模型

EchoKey 使用基于 channel 的多线程架构：

```
主线程 (GUI - egui)
     ↓
键盘线程 (Windows hook) → 逻辑线程 ← 托盘线程
     ↓                            ↓
   mpsc::channel<KeyboardEvent>   mpsc::channel<TrayEvent>
```

### 核心模块（echokey/src/）

| 文件 | 职责 |
|------|------|
| `main.rs` | 程序入口，线程初始化 |
| `keyboard_win.rs` | Windows 键盘钩子，30ms 去重窗口 |
| `logger.rs` | 缓冲 I/O + 立即刷新，按日期分文件 |
| `gui.rs` | egui 界面，日志查看和统计 |
| `tray.rs` | 系统托盘（tray-icon/muda） |
| `clipboard.rs` | 剪贴板操作 |
| `config.rs` | 配置管理（serde 序列化） |
| `autostart.rs` | 开机自启动（注册表） |

### 数据存储

- **EchoKey 日志**: `%LOCALAPPDATA%\EchoKey\logs\` - 按日期自动分文件
- **EchoKey 配置**: `%LOCALAPPDATA%\EchoKey\config.toml`
- **ClipboardMonitor**: `%APPDATA%\ClipboardMonitor\clipboard_history.json`

## 关键设计模式

### 线程安全
- 使用 `Arc<Mutex<State>>` 在 GUI、逻辑、托盘线程间共享状态
- channel 操作使用 50ms 超时，避免阻塞 UI

### 数据完整性
- Logger 每次写入后立即 flush，保证崩溃时不丢数据
- 键盘钩子使用 30ms 去重窗口，过滤重复事件

### 二进制体积优化
`Cargo.toml` 中的 release profile：
- `opt-level = "z"` 优化体积
- LTO + 单 codegen unit + strip symbols

## 快捷键

### EchoKey
| 快捷键 | 功能 |
|--------|------|
| `Ctrl+Shift+P` | 暂停/恢复记录 |
| `Ctrl+Shift+S` | 保存当前剪贴板 |
| `Ctrl+Shift+N` | 新建日志段 |

### FloatingTool
- `Alt+Q` - 在光标位置显示反馈窗口

## 开发注意事项

### 修改键盘钩子 (keyboard_win.rs)
30ms 去重窗口经过实测调优。修改时需测试：快速按键、按键重复、多输入设备场景。

### 修改日志模块 (logger.rs)
立即 flush 是刻意设计。任何缓冲改动必须保持此保证。日期轮换发生在本地午夜。

### 修改 GUI (gui.rs)
GUI 运行在主线程，使用 egui 即时模式。长耗时操作不能阻塞此线程，需使用 channel + 超时模式。

## 主要依赖

| 依赖 | 用途 |
|------|------|
| `windows-rs` | Windows 原生 API |
| `eframe/egui` | GUI 框架 |
| `tray-icon/muda` | 系统托盘 |
| `arboard` | 跨平台剪贴板 |
| `chrono` | 时间处理 |
| `winreg` | Windows 注册表操作 |
