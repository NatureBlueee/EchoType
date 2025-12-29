# ClipboardMonitor

> 🔍 **智能剪贴板监控与上下文溯源工具**
> 
> 不只是记录"复制了什么"，更要知道"为什么在这里复制"

[![Windows](https://img.shields.io/badge/Platform-Windows-blue?logo=windows)](https://github.com)
[![C++17](https://img.shields.io/badge/C++-17-00599C?logo=cplusplus)](https://github.com)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)

---

## ✨ 特性

| 功能 | 说明 |
|------|------|
| 📋 **剪贴板监控** | 实时捕获所有复制操作，100%成功率 |
| 🔍 **上下文溯源** | 知道内容来自哪个应用、哪个窗口、哪个URL |
| 🌐 **浏览器集成** | Chrome/Edge扩展，捕获选中文本的上下文 |
| 💬 **微信支持** | 识别群聊/联系人，获取聊天上下文 |
| 📝 **编辑器支持** | VSCode/Notion 文件路径和行号追踪 |
| 🖼️ **多格式** | 支持文本、图片、文件、HTML等 |
| 📊 **JSON存储** | 结构化数据，便于分析和检索 |

---

## 🚀 快速开始

### 系统要求

- Windows 10/11
- Visual Studio 2022 Build Tools

### 编译

```powershell
# 打开 Developer Command Prompt for VS 2022
cd ClipboardMonitor
.\build.bat
```

### 运行

```powershell
bin\ClipboardMonitor.exe
```

程序启动后会在系统托盘显示图标。

---

## 📁 项目结构

```
ClipboardMonitor/
├── 📄 main.cpp              # 主程序入口
├── 📄 clipboard_monitor.*   # 剪贴板监控核心
├── 📄 storage.*             # JSON数据存储
├── 📁 context/              # 上下文溯源系统
│   ├── 📄 context_manager.* # 上下文管理器
│   ├── 📁 adapters/         # 应用适配器
│   │   ├── browser_adapter  # 浏览器适配
│   │   ├── wechat_adapter   # 微信适配
│   │   ├── vscode_adapter   # VSCode适配
│   │   └── notion_adapter   # Notion适配
│   └── 📁 utils/            # 工具类
│       ├── ui_automation    # Windows UI自动化
│       └── html_parser      # HTML格式解析
├── 📁 browser_extension/    # Chrome/Edge扩展
└── 📄 REQUIREMENTS.md       # 需求文档
```

---

## 📊 数据格式

复制的内容会保存为结构化JSON：

```json
{
  "timestamp": "2025-12-29T16:38:30.302+08:00",
  "content_type": "text",
  "content": "复制的文本内容...",
  "source": {
    "process_name": "chrome.exe",
    "window_title": "Example Page - Google Chrome",
    "pid": 12345
  },
  "context": {
    "adapter_type": "browser",
    "url": "https://example.com/article",
    "title": "Example Article",
    "success": true,
    "fetch_time_ms": 45
  }
}
```

---

## 🔧 配置

数据存储位置：
```
%APPDATA%\ClipboardMonitor\
├── clipboard_history.json   # 历史记录
└── debug.log                # 调试日志
```

---

## ⌨️ 快捷键

| 快捷键 | 功能 |
|--------|------|
| `Ctrl+Shift+Q` | 退出程序 |

---

## 🌐 浏览器扩展

扩展位于 `browser_extension/` 目录，提供更丰富的上下文：

- 精确URL（包括hash和query）
- 选中文本前后的内容
- 页面元数据（Open Graph等）

安装方法见 [browser_extension/README.md](browser_extension/README.md)

---

## 🔒 隐私与安全

- ✅ **纯本地运行** - 所有数据存储在本地
- ✅ **不上传数据** - 无网络请求
- ✅ **用户主动触发** - 只记录用户复制的内容
- ✅ **公开API** - 使用Windows官方API，不破解任何应用

---

## 🛠️ 技术栈

- **语言**: C++17
- **平台API**: Windows API, UI Automation
- **浏览器扩展**: Chrome Extension Manifest V3
- **数据格式**: JSON

---

## 📝 开发文档

- [REQUIREMENTS.md](REQUIREMENTS.md) - 完整需求文档
- [IMPLEMENTATION_LOG.md](IMPLEMENTATION_LOG.md) - 实施日志

---

## 📜 License

MIT © 2025
