# ClipboardMonitor 需求文档

> **项目目标**：系统级剪贴板监控与上下文溯源工具
> **技术栈**：C++ (Windows API, UI Automation)
> **当前版本**：v0.1 (基础剪贴板监控已完成)

---

## 一、用户原始需求

### 1.1 核心理念

> "复制粘贴不只是复制粘贴，它是一种**标注**。我们只是现在把它变成了复制件而已，它可以变成比其他的关键件更有意义。"
>
> "我想要**可溯源**——这段代码是在哪个文档里的，哪个代码文件里的，这个代码文件又是在哪个目录下的，这个目录下面又有什么其他的东西。"
>
> "核心需求是要知道用户在选中这段信息的时候，这段信息附近的**上下文**。更大的作用是，为什么他在这里、这个时间、这个地点、这个区域选择了这一块。"

### 1.2 功能需求

1. **基础剪贴板监控**
   - 在Windows系统中监控所有复制操作
   - 不影响正常的复制粘贴功能
   - 将复制的内容同时保存到本地文件（JSON格式）
   - 记录来源应用（进程名、窗口标题、PID）

2. **上下文溯源（Context Tracing）**
   - 浏览器：获取完整URL、标签页标题、整个网页内容
   - 微信：获取群名/联系人名、聊天上下文
   - Notion：获取页面路径、文档结构
   - 代码编辑器：获取文件路径、行号、函数名、项目结构
   - 文件管理器：获取完整文件路径、目录结构

3. **可配置性**
   - 用户可配置每个应用的上下文爬取深度
   - 预设常用场景的默认配置

### 1.3 技术要求

> "用最底层的代码去做，尽量用C++这种底层的代码。"
>
> "我们的语言不能不够底层。理想状况应该是它会同步地发送给我们的，我们会有最高的权限去访问这个剪贴板。"

---

## 二、当前实现进展

### 2.1 已完成功能 ✅

| 功能 | 状态 | 技术实现 |
|------|------|----------|
| 剪贴板变化监控 | ✅ 完成 | `AddClipboardFormatListener()` |
| 多格式内容获取 | ✅ 完成 | `CF_UNICODETEXT`, `CF_TEXT`, `CF_BITMAP`, `CF_HDROP` |
| 来源应用识别 | ✅ 完成 | `GetForegroundWindow()`, `QueryFullProcessImageNameW()` |
| 窗口标题获取 | ✅ 完成 | `GetWindowTextW()` |
| JSON格式存储 | ✅ 完成 | 自定义JSON序列化 |
| 系统托盘控制 | ✅ 完成 | `Shell_NotifyIconW()` |
| 调试日志系统 | ✅ 完成 | 文件日志 |
| 剪贴板锁定问题 | ✅ 完成 | `PostMessage` + 重试机制 (1ms间隔, 最多100次) |

### 2.2 关键技术突破

**问题**：某些应用（微信、Notion、Electron应用）在复制时锁定剪贴板时间较长，导致我们的程序无法读取。

**解决方案**：
```cpp
// 1. 使用 PostMessage 延迟处理
case WM_CLIPBOARDUPDATE:
    PostMessage(hwnd, WM_DEFERRED_CLIPBOARD, 0, 0);
    return 0;

// 2. 快速重试机制
for (int attempt = 0; attempt < 100; attempt++) {
    if (OpenClipboard(m_hwnd)) break;
    Sleep(1);  // 等待1ms
}
```

**效果**：100%成功率，大多数只需等待1ms。

### 2.3 测试验证

| 应用 | 复制成功 | 等待时间 |
|------|----------|----------|
| WeChat.exe | ✅ | 1ms |
| Notion.exe | ✅ | 1ms |
| Antigravity.exe | ✅ | 1ms |
| comet.exe (Perplexity) | ✅ | 0ms |
| explorer.exe | ✅ | 1ms |
| WindowsTerminal | ✅ | 0ms |

### 2.4 项目结构

```
ClipboardMonitor/
├── main.cpp              # 主程序入口，系统托盘
├── clipboard_monitor.h   # 剪贴板监控类
├── clipboard_monitor.cpp # 核心监控实现
├── storage.h             # 存储类
├── storage.cpp           # JSON存储实现
├── utils.h               # 工具函数
├── debug_log.h           # 调试日志
├── CMakeLists.txt        # CMake配置
├── build.bat             # 编译脚本
└── README.md             # 使用说明
```

### 2.5 数据存储格式

```json
{
  "timestamp": "2025-12-29T14:33:29.508+08:00",
  "content_type": "text",
  "content": "复制的文本内容",
  "content_preview": "复制的文本内容...",
  "source": {
    "process_name": "WeChat.exe",
    "process_path": "C:\\...\\WeChat.exe",
    "window_title": "微信",
    "pid": 12345
  },
  "context": {
    // 待实现：上下文信息
  }
}
```

---

## 三、待实现功能

### 3.1 上下文溯源系统

#### 3.1.1 技术方案：Windows UI Automation API

```cpp
#include <UIAutomation.h>

// 获取活动元素及其上下文
IUIAutomation* pAutomation;
IUIAutomationElement* pFocusedElement;

CoCreateInstance(CLSID_CUIAutomation, nullptr, 
    CLSCTX_INPROC_SERVER, IID_IUIAutomation, (void**)&pAutomation);
pAutomation->GetFocusedElement(&pFocusedElement);

// 遍历元素树获取上下文
```

#### 3.1.2 应用适配器架构

```cpp
class IContextAdapter {
public:
    virtual bool CanHandle(const std::wstring& processName) = 0;
    virtual ContextInfo GetContext(HWND hwnd) = 0;
};

class BrowserAdapter : public IContextAdapter { /* URL, 标签页, 页面内容 */ };
class WeChatAdapter : public IContextAdapter { /* 群名, 联系人, 消息上下文 */ };
class VSCodeAdapter : public IContextAdapter { /* 文件路径, 行号, 函数 */ };
class NotionAdapter : public IContextAdapter { /* 页面路径, 文档结构 */ };
class ExplorerAdapter : public IContextAdapter { /* 文件路径, 目录 */ };
```

#### 3.1.3 浏览器上下文获取

| 信息 | 获取方式 |
|------|----------|
| URL | UI Automation 获取地址栏内容 |
| 标签页标题 | 窗口标题解析 |
| 页面全文 | 剪贴板 HTML Format 或浏览器扩展 |
| 选中位置 | 浏览器扩展 (Native Messaging) |

#### 3.1.4 微信上下文获取

| 信息 | 获取方式 |
|------|----------|
| 对话类型 | UI Automation 分析窗口结构 |
| 群名/联系人 | UI Automation 获取标题元素 |
| 消息上下文 | UI Automation 获取聊天列表 (前后N条) |

#### 3.1.5 代码编辑器上下文获取

| 信息 | 获取方式 |
|------|----------|
| 文件路径 | UI Automation 获取标题栏 |
| 行号 | UI Automation 或剪贴板元数据 |
| 函数名 | 代码解析 |
| 项目结构 | 文件系统遍历 |

### 3.2 配置系统

```json
{
  "default_context_depth": 3,
  "apps": {
    "chrome.exe": {
      "enabled": true,
      "capture_full_page": true,
      "capture_url": true
    },
    "WeChat.exe": {
      "enabled": true,
      "context_messages": 5
    },
    "Code.exe": {
      "enabled": true,
      "capture_file_path": true,
      "capture_line_number": true,
      "capture_function": true
    }
  }
}
```

---

## 四、实现路线图

| 阶段 | 内容 | 预估时间 |
|------|------|----------|
| ✅ 阶段1 | 基础剪贴板监控 | 完成 |
| 阶段2 | UI Automation 基础框架 | 2-3天 |
| 阶段3 | 浏览器适配器 (URL, 标签页) | 2天 |
| 阶段4 | 微信适配器 | 2天 |
| 阶段5 | 代码编辑器适配器 | 2天 |
| 阶段6 | Notion/Electron适配器 | 2天 |
| 阶段7 | 配置系统 | 1天 |
| 阶段8 | 浏览器扩展 (可选, 最完整) | 3-5天 |

---

## 五、编译与运行

### 5.1 编译环境

- Windows 10/11
- Visual Studio 2022 (Build Tools)
- C++17

### 5.2 编译命令

```cmd
cd d:\Profolio\EchoType\playground\ClipboardMonitor
cl.exe /EHsc /std:c++17 /W4 /O2 /DUNICODE /D_UNICODE ^
    /Fe:bin\ClipboardMonitor.exe ^
    main.cpp clipboard_monitor.cpp storage.cpp ^
    /link user32.lib gdi32.lib shell32.lib ole32.lib shlwapi.lib oleacc.lib ^
    /SUBSYSTEM:WINDOWS
```

### 5.3 运行

```cmd
bin\ClipboardMonitor.exe
```

### 5.4 数据位置

- 历史记录：`%APPDATA%\ClipboardMonitor\clipboard_history.json`
- 调试日志：`%APPDATA%\ClipboardMonitor\debug.log`

---

## 六、参考资料

- [Windows Clipboard API](https://docs.microsoft.com/en-us/windows/win32/dataxchg/clipboard)
- [UI Automation](https://docs.microsoft.com/en-us/windows/win32/winauto/entry-uiauto-win32)
- [Native Messaging (Chrome Extension)](https://developer.chrome.com/docs/extensions/develop/concepts/native-messaging)
