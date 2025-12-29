# ClipboardMonitor - Windows 剪贴板监控工具

一个用C++编写的剪贴板监控工具，能够记录复制的内容、来源应用和上下文信息。

## 项目结构

```
ClipboardMonitor/
├── main.cpp              # 主程序入口
├── clipboard_monitor.h   # 剪贴板监控类定义
├── clipboard_monitor.cpp # 剪贴板监控实现
├── storage.h             # 存储类定义
├── storage.cpp           # JSON存储实现
├── utils.h               # 工具函数
├── CMakeLists.txt        # CMake配置
├── build.bat             # 编译脚本
└── README.md             # 本文件
```

## 编译方法

### 方法一：使用 Visual Studio 开发者命令提示符

1. 打开 **Developer Command Prompt for VS 2022** 或 **Developer PowerShell for VS**
2. 进入项目目录：
   ```
   cd d:\Profolio\EchoType\playground\ClipboardMonitor
   ```
3. 创建bin目录并编译：
   ```
   mkdir bin
   cl.exe /EHsc /std:c++17 /W4 /O2 /DUNICODE /D_UNICODE /Fe:bin\ClipboardMonitor.exe main.cpp clipboard_monitor.cpp storage.cpp /link user32.lib gdi32.lib shell32.lib ole32.lib shlwapi.lib oleacc.lib /SUBSYSTEM:WINDOWS
   ```

### 方法二：使用 CMake

1. 打开 Developer Command Prompt 或确保cmake在PATH中
2. 进入项目目录：
   ```
   cd d:\Profolio\EchoType\playground\ClipboardMonitor
   mkdir build
   cd build
   cmake ..
   cmake --build . --config Release
   ```

### 方法三：双击 build.bat

直接双击 `build.bat` 文件运行（需要已安装 Visual Studio）

## 使用方法

1. 运行 `bin\ClipboardMonitor.exe`
2. 程序会在系统托盘显示图标
3. 任何复制操作都会被记录

### 功能

- **实时监控**：自动记录所有剪贴板变化
- **来源追踪**：记录复制来源的程序名、窗口标题
- **托盘控制**：右键托盘图标可以暂停/恢复监控
- **打开历史**：右键托盘选择"Open History File"查看记录
- **退出程序**：按 `Ctrl+Shift+Q` 或右键托盘选择 Exit

### 数据存储位置

剪贴板历史保存在：
```
%APPDATA%\ClipboardMonitor\clipboard_history.json
```

## JSON 输出格式

```json
{
  "timestamp": "2025-12-26T14:50:00.123+08:00",
  "content_type": "text",
  "content": "复制的文本内容",
  "content_preview": "复制的文本内容...",
  "source": {
    "process_name": "chrome.exe",
    "process_path": "C:\\Program Files\\Google\\Chrome\\...",
    "window_title": "Google - Google Chrome",
    "pid": 12345
  }
}
```

## 系统要求

- Windows 10 或更高版本
- Visual Studio 2019/2022 (用于编译)
