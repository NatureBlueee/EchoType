# ClipboardMonitor 开发者指南

## 📖 项目概述

**核心理念：将"复制"视为"标注"**

ClipboardMonitor 不仅仅是一个剪贴板监控工具，它的核心价值在于**上下文溯源（Context Tracing）**。当用户复制内容时，系统会捕获：
- ✅ 复制的内容是什么（What）
- ✅ 在哪里复制的（Where）
- ✅ 什么时候复制的（When）
- 🔄 **为什么复制/上下文是什么（Why/Context）** ← 核心价值

这个系统的设计目标是让每一次复制操作都携带丰富的上下文信息，方便后续检索、分析和知识管理。

---

## 🏗️ 系统架构设计

### 整体架构图

```
┌─────────────────────────────────────────────────────────────┐
│                    ClipboardMonitor (主程序)                  │
│  ┌──────────────┐         ┌──────────────────────────────┐  │
│  │ Clipboard    │         │     ContextManager            │  │
│  │ Monitor      │────────▶│  (异步上下文获取协调器)        │  │
│  └──────────────┘         └──────────────┬───────────────┘  │
│         │                                 │                  │
│         │                   ┌─────────────┴──────────────┐   │
│         │                   │     AsyncExecutor          │   │
│         │                   │  (2-thread pool)           │   │
│         │                   └─────────────┬──────────────┘   │
│         │                                 │                  │
│         │                   ┌─────────────┴──────────────┐   │
│         │                   │   Adapter Pattern          │   │
│         │                   ├────────────────────────────┤   │
│         │                   │ • BrowserAdapter           │   │
│         │                   │ • WeChatAdapter            │   │
│         │                   │ • VSCodeAdapter            │   │
│         │                   │ • NotionAdapter            │   │
│         │                   └─────────────┬──────────────┘   │
│         │                                 │                  │
│         │                   ┌─────────────┴──────────────┐   │
│         │                   │  UI Automation Helper      │   │
│         │                   │  (Windows API封装)         │   │
│         │                   └────────────────────────────┘   │
│         ▼                                                    │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              Storage (JSON持久化)                     │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘

数据流向：
Clipboard Change → Trigger Context Fetch (async) → Adapter获取上下文
→ 回调返回数据 → 与剪贴板内容合并 → 保存到JSON
```

### 为什么选择这个架构？

#### 1. **异步架构 - 不阻塞主线程**

**问题背景：**
- 剪贴板监控必须快速响应，不能让用户感觉到卡顿
- UI Automation API调用可能需要50-300ms（查找元素、遍历DOM树）
- 如果同步执行，用户每次复制都会感觉到明显延迟

**解决方案：**
```cpp
// AsyncExecutor：2个工作线程的线程池
class AsyncExecutor {
    std::vector<std::thread> m_workers;  // 2 worker threads
    std::queue<std::function<void()>> m_tasks;

    // 提交任务立即返回，不阻塞调用方
    void SubmitTask(std::function<void()> task);
};
```

**设计巧思：**
- ✅ 主线程立即返回，用户无感知
- ✅ 工作线程并发处理上下文获取
- ✅ 通过回调函数返回结果，解耦数据流
- ✅ 即使上下文获取失败/超时，也不影响剪贴板内容保存

#### 2. **Adapter模式 - 可扩展的应用支持**

**问题背景：**
- 不同应用的上下文结构完全不同（浏览器有URL，微信有聊天记录，VSCode有文件路径）
- 未来需要支持更多应用（Slack、Feishu、Obsidian等）
- 不能让每次新增应用都修改核心代码

**解决方案：**
```cpp
// 抽象基类：统一接口
class IContextAdapter {
public:
    virtual bool CanHandle(const std::wstring& processName,
                          const std::wstring& windowTitle) = 0;
    virtual std::shared_ptr<ContextData> GetContext(const SourceInfo& source) = 0;
};

// ContextManager：责任链模式分发
class ContextManager {
    std::vector<std::shared_ptr<IContextAdapter>> m_adapters;

    void FetchContext(const SourceInfo& source,
                     std::function<void(std::shared_ptr<ContextData>)> callback) {
        // 遍历所有adapter，找到第一个能处理的
        for (auto& adapter : m_adapters) {
            if (adapter->CanHandle(source.processName, source.windowTitle)) {
                // 异步执行，结果通过callback返回
                m_executor->SubmitTask([adapter, source, callback]() {
                    auto context = adapter->GetContext(source);
                    callback(context);
                });
                return;
            }
        }
    }
};
```

**设计巧思：**
- ✅ 新增应用支持：只需实现新的Adapter，注册到ContextManager
- ✅ 松耦合：Adapter之间完全独立，互不影响
- ✅ 责任链模式：按顺序匹配，第一个匹配成功的处理
- ✅ 多态：所有Adapter统一接口，ContextManager无需关心细节

#### 3. **超时与回调机制 - 数据完整性保证**

**问题背景：**
- UI Automation可能因为应用UI结构变化而失败
- 某些应用响应慢，可能超过预期时间
- 不能因为上下文获取失败就丢失剪贴板内容

**解决方案：**
```cpp
// 双回调机制
void ContextManager::FetchContext(const SourceInfo& source,
                                 std::function<void(std::shared_ptr<ContextData>)> callback)
{
    std::atomic<bool> completed(false);

    // 1. 启动异步任务
    m_executor->SubmitTask([adapter, source, callback, &completed]() {
        auto context = adapter->GetContext(source);
        if (!completed.exchange(true)) {  // 原子操作，确保只调用一次
            callback(context);
        }
    });

    // 2. 启动超时计时器
    m_executor->SubmitTask([timeout, callback, &completed]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
        if (!completed.exchange(true)) {  // 如果任务还没完成
            auto emptyContext = std::make_shared<ContextData>();
            emptyContext->success = false;
            emptyContext->error = L"Timeout";
            callback(emptyContext);  // 返回超时结果
        }
    });
}
```

**设计巧思：**
- ✅ `std::atomic<bool>` 确保回调只执行一次（要么超时，要么成功）
- ✅ `compare_exchange_strong` 原子操作，线程安全
- ✅ 超时后仍然保存记录，只是标记 `success: false`
- ✅ 用户可以在JSON中看到哪些操作失败了，便于调试

**遇到的问题与修复：**
- ❌ **Bug**: 最初设计导致每次复制产生2条记录（超时记录 + 成功记录）
- ✅ **Fix**: 增加timeout从150ms → 300ms，确保大部分操作在超时前完成
- 📊 **数据**: 实测浏览器上下文获取耗时 84-238ms，300ms超时合理

---

## 🔍 各Adapter实现详解

### 1. BrowserAdapter - 浏览器上下文

**目标：** 获取用户复制内容时的网页URL和标题

**技术方案：**

#### 方案A：CF_HTML格式（当前实现）
```cpp
// Windows剪贴板HTML格式包含元数据
// 格式示例：
// Version:0.9
// StartHTML:000000
// EndHTML:000000
// StartFragment:000000
// EndFragment:000000
// SourceURL:https://example.com
// <html>...</html>

std::wstring BrowserAdapter::ExtractSourceUrl(const std::string& htmlData) {
    size_t pos = htmlData.find("SourceURL:");
    if (pos != std::string::npos) {
        // 提取URL
    }
}
```

**优点：**
- ✅ 完全可靠，浏览器自动提供
- ✅ 无需UI Automation，性能好
- ✅ 支持所有主流浏览器（Chrome, Edge, Firefox等）

**缺点：**
- ❌ 只能获取URL，无法获取选中文本的上下文（前后文本）
- ❌ 只适用于网页，不适用于本地文件

#### 方案B：浏览器扩展（Phase 2规划）
```javascript
// Chrome Extension + Native Messaging
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    if (request.action === "getContext") {
        const selection = window.getSelection();
        const range = selection.getRangeAt(0);

        // 获取选中文本的前后文本
        const beforeText = getPreviousText(range, 200);
        const afterText = getNextText(range, 200);

        return {
            selectedText: selection.toString(),
            beforeText: beforeText,
            afterText: afterText,
            url: window.location.href,
            title: document.title
        };
    }
});
```

**为什么暂缓Phase 2？**
- 需要用户安装浏览器扩展，增加使用门槛
- Phase 1已能满足基本需求（URL溯源）
- 先完成所有基础Adapter，再考虑深度功能

**支持的浏览器列表（20+）：**
```cpp
// 显式支持
{"chrome.exe", "msedge.exe", "firefox.exe", "brave.exe", "opera.exe",
 "vivaldi.exe", "arc.exe", "comet.exe", "sogouexplorer.exe", ...}

// 启发式匹配（进程名包含关键词）
if (contains(lowerName, L"browser") || contains(lowerName, L"chrome") ||
    contains(lowerName, L"web") || contains(lowerName, L"edge")) {
    return true;
}
```

### 2. WeChatAdapter - 微信聊天上下文

**目标：** 获取聊天对象名称、聊天类型、最近N条消息

**技术方案：UI Automation**

```cpp
std::wstring WeChatAdapter::GetChatName(HWND hwnd, UIAutomationHelper& uiHelper) {
    // 策略1：查找Text/Static控件
    // 微信聊天窗口顶部通常有Text元素显示联系人名称

    // 策略2：从窗口标题提取
    // "联系人名称 - WeChat" 格式

    // 策略3：多重过滤启发式
    if (!text.empty() &&
        text.length() > 1 &&
        text.length() < 100 &&  // 聊天名称通常不会太长
        text.find(L"WeChat") == std::wstring::npos &&  // 排除UI字符串
        text.find(L"微信") == std::wstring::npos) {
        return text;
    }
}
```

**聊天类型判断（启发式）：**
```cpp
std::wstring WeChatAdapter::DetermineChatType(const std::wstring& chatName) {
    // 规则1：包含"群"字
    if (chatName.find(L"群") != std::wstring::npos) return L"group";

    // 规则2：括号内有数字（如"工作群(123)"）
    if (has_parentheses_with_digits(chatName)) return L"group";

    // 默认：私聊
    return L"private";
}
```

**消息获取策略：**
```cpp
std::vector<std::wstring> WeChatAdapter::GetRecentMessages(HWND hwnd, int count) {
    // 1. 查找List控件（消息列表容器）
    // 2. 查找ListItem子元素（每条消息）
    // 3. 提取最后N条消息（最近的）

    int startIndex = (messageCount - count > 0 ? messageCount - count : 0);
    for (int i = startIndex; i < messageCount; i++) {
        // 提取消息文本
    }
}
```

**设计巧思：**
- ✅ 配置化消息数量（默认5条，可调整）
- ✅ 多重fallback策略（UI Automation → 窗口标题）
- ✅ 启发式判断聊天类型（无需API，纯文本分析）

**已知限制：**
- ⚠️ 微信UI结构可能因版本不同而变化
- ⚠️ 聊天类型判断非100%准确（启发式）

### 3. VSCodeAdapter - 代码编辑器上下文

**目标：** 获取文件名、项目名、编程语言、光标位置

**窗口标题解析：**
```cpp
// 标题格式："● filename.ext - Project Name - Visual Studio Code"
// "●" = 未保存修改标记（U+25CF）

void VSCodeAdapter::ParseWindowTitle(const std::wstring& windowTitle,
                                     std::wstring& fileName,
                                     std::wstring& projectName,
                                     bool& isModified) {
    // 1. 检测修改标记
    if (title[0] == L'\u25CF') {  // 使用Unicode转义避免编码问题
        isModified = true;
        title = title.substr(2);  // 移除 "● "
    }

    // 2. 查找后缀并移除
    std::vector<std::wstring> suffixes = {
        L" - Visual Studio Code",
        L" - Cursor",
        L" - Code - Insiders",
        L" - VSCodium"
    };

    // 3. 分割 "文件名 - 项目名"
    size_t dashPos = title.find(L" - ");
    fileName = title.substr(0, dashPos);
    projectName = title.substr(dashPos + 3);
}
```

**编程语言推断：**
```cpp
// 40+ 文件扩展名映射
const std::map<std::wstring, std::string> VSCodeAdapter::s_languageMap = {
    {L"cpp", "C++"}, {L"h", "C/C++ Header"}, {L"c", "C"},
    {L"py", "Python"}, {L"js", "JavaScript"}, {L"ts", "TypeScript"},
    {L"rs", "Rust"}, {L"go", "Go"}, {L"java", "Java"},
    {L"cs", "C#"}, {L"rb", "Ruby"}, {L"php", "PHP"},
    {L"swift", "Swift"}, {L"kt", "Kotlin"}, {L"dart", "Dart"},
    {L"md", "Markdown"}, {L"json", "JSON"}, {L"xml", "XML"},
    // ... 40+ mappings
};

std::string VSCodeAdapter::InferLanguage(const std::wstring& fileName) {
    size_t dotPos = fileName.find_last_of(L'.');
    if (dotPos != std::wstring::npos) {
        std::wstring ext = fileName.substr(dotPos + 1);
        if (s_languageMap.count(ext)) {
            return s_languageMap.at(ext);
        }
    }
    return "Unknown";
}
```

**光标位置提取（UI Automation）：**
```cpp
void VSCodeAdapter::GetCursorPosition(HWND hwnd, UIAutomationHelper& uiHelper,
                                     int& lineNumber, int& columnNumber) {
    // 查找状态栏中的 "Ln 42, Col 15" 文本
    // VSCode状态栏是StatusBar控件

    // 正则匹配：Ln (\d+), Col (\d+)
    std::wregex pattern(L"Ln\\s+(\\d+),\\s+Col\\s+(\\d+)");
    std::wsmatch match;
    if (std::regex_search(statusText, match, pattern)) {
        lineNumber = std::stoi(match[1].str());
        columnNumber = std::stoi(match[2].str());
    }
}
```

**支持的编辑器：**
- Visual Studio Code (`Code.exe`)
- Cursor (`Cursor.exe`)
- VS Code Insiders (`code-insiders.exe`)
- VSCodium (`VSCodium.exe`)

**设计巧思：**
- ✅ 窗口标题解析优先（快速、可靠）
- ✅ UI Automation作为补充（光标位置、文件路径）
- ✅ 修改标记检测（U+25CF Unicode字符）
- ✅ 支持多种VSCode变体（Cursor等）

**编码问题修复：**
```cpp
// ❌ 错误写法（导致C4819编码警告）
if (title[0] == L'●') { ... }

// ✅ 正确写法（使用Unicode转义）
if (title[0] == L'\u25CF') { ... }
```

### 4. NotionAdapter - 笔记应用上下文

**目标：** 获取页面标题、面包屑导航、页面类型

**窗口标题解析：**
```cpp
std::wstring NotionAdapter::ParsePageTitle(const std::wstring& windowTitle) {
    // 格式："Page Title - Notion"

    size_t suffixPos = title.rfind(L" - Notion");
    if (suffixPos != std::wstring::npos) {
        return title.substr(0, suffixPos);
    }
    return title;
}
```

**面包屑导航提取：**
```cpp
std::vector<std::wstring> NotionAdapter::GetBreadcrumbs(HWND hwnd,
                                                        UIAutomationHelper& uiHelper) {
    // Notion在顶部显示面包屑：Workspace > Parent Page > Current Page

    // 策略1：查找Hyperlink控件（面包屑通常是链接）
    // 策略2：查找Button控件（某些版本用按钮）

    // 过滤条件：
    // - 文本长度 < 100（面包屑不会太长）
    // - 排除常见UI元素（"Back", "Forward", "Share"）
    // - 排除URL（contains "http"）
}
```

**页面类型推断：**
```cpp
std::string NotionAdapter::DeterminePageType(const std::wstring& windowTitle,
                                             UIAutomationHelper& uiHelper,
                                             HWND hwnd) {
    // 从标题中提取关键词
    std::wstring lowerTitle = Utils::ToLower(windowTitle);

    if (lowerTitle.find(L"database") != std::wstring::npos) return "database";
    if (lowerTitle.find(L"table") != std::wstring::npos) return "table";
    if (lowerTitle.find(L"board") != std::wstring::npos) return "board";
    if (lowerTitle.find(L"calendar") != std::wstring::npos) return "calendar";
    if (lowerTitle.find(L"timeline") != std::wstring::npos) return "timeline";
    if (lowerTitle.find(L"gallery") != std::wstring::npos) return "gallery";

    return "page";  // 默认普通页面
}
```

**伪URL构造：**
```cpp
std::wstring NotionAdapter::ConstructPseudoUrl(
    const std::vector<std::wstring>& breadcrumbs,
    const std::wstring& pageTitle) {

    // 构造格式：notion://Workspace/ParentPage/CurrentPage
    std::wostringstream url;
    url << L"notion://";

    for (const auto& crumb : breadcrumbs) {
        url << crumb << L"/";
    }
    url << pageTitle;

    return url.str();
}
```

**设计巧思：**
- ✅ 伪URL方案（Notion没有直接URL，构造层级路径）
- ✅ 面包屑提供完整导航路径
- ✅ 页面类型识别（Database vs Page）
- ✅ 启发式匹配（基于窗口标题关键词）

---

## 🛠️ 关键技术点

### 1. Windows UI Automation封装

**UIAutomationHelper类设计：**

```cpp
class UIAutomationHelper {
    IUIAutomation* m_automation;  // COM对象

public:
    bool Initialize() {
        CoInitializeEx(NULL, COINIT_MULTITHREADED);  // 初始化COM
        CoCreateInstance(CLSID_CUIAutomation, ...);   // 创建Automation实例
    }

    std::wstring GetElementText(IUIAutomationElement* element) {
        // 1. 尝试 CurrentName 属性
        BSTR name;
        element->get_CurrentName(&name);

        // 2. 尝试 Value pattern
        IUIAutomationValuePattern* valuePattern;
        element->GetCurrentPattern(UIA_ValuePatternId, ...);

        // 3. 遍历子元素递归提取
    }

    ~UIAutomationHelper() {
        if (m_automation) m_automation->Release();
        CoUninitialize();
    }
};
```

**RAII模式：**
```cpp
// 资源自动管理
{
    UIAutomationHelper uiHelper;
    if (uiHelper.Initialize()) {
        // 使用uiHelper...
    }
}  // 离开作用域自动调用析构函数，释放COM资源
```

### 2. 字符串编码处理

**Windows宽字符 vs UTF-8：**

```cpp
// Windows API使用宽字符（UTF-16）
std::wstring processName;  // L"chrome.exe"

// JSON存储使用UTF-8
std::string jsonText;      // "chrome.exe"

// 转换工具函数
class Utils {
    static std::string WideToUtf8(const std::wstring& wstr);
    static std::wstring Utf8ToWide(const std::string& str);
    static std::string EscapeJson(const std::string& str);  // 转义特殊字符
};
```

**编码问题修复历程：**

```cpp
// ❌ 问题1：chatType类型不匹配
std::string chatType;  // 定义为string
context->chatType = DetermineChatType(chatName);  // 返回wstring
// 修复：统一为wstring

// ❌ 问题2：JSON序列化时未转换
json << context->chatType;  // wstring直接输出到char流
// 修复：json << Utils::WideToUtf8(context->chatType);

// ❌ 问题3：DEBUG_LOG字符串拼接
DEBUG_LOG("Chat type: " + context->chatType);  // wstring + const char*
// 修复：DEBUG_LOG("Chat type: " + Utils::WideToUtf8(context->chatType));
```

### 3. Windows API宏冲突处理

**问题：**
```cpp
// Windows.h 定义了min/max宏
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))

// 导致 std::min/max 无法编译
for (int i = 0; i < std::min(length, 10); i++) {  // ❌ 语法错误
```

**解决方案：**
```cpp
// 方案1：使用三元运算符替代
for (int i = 0; i < (length < 10 ? length : 10); i++) {  // ✅

// 方案2：#define NOMINMAX（未采用，避免影响其他代码）
```

### 4. JSON手动序列化

**为什么不用JSON库？**
- ✅ 减少依赖（无需第三方库）
- ✅ 控制输出格式（可读性好）
- ✅ 简单场景下手动序列化足够

**序列化实现：**
```cpp
void Storage::SaveRecord(const ClipboardRecord& record) {
    std::ostringstream json;
    json << "  {\n";
    json << "    \"timestamp\": \"" << record.timestamp << "\",\n";
    json << "    \"content\": \"" << EscapeJson(record.content) << "\",\n";

    // Adapter特定字段
    if (ctx->adapterType == "browser") {
        const BrowserContext* browserCtx = static_cast<const BrowserContext*>(ctx.get());
        json << "    \"source_url\": \"" << EscapeJson(browserCtx->sourceUrl) << "\",\n";
    }
    else if (ctx->adapterType == "wechat") {
        // ...
    }

    json << "  }";
}
```

**转义处理：**
```cpp
std::string Utils::EscapeJson(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '\"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}
```

---

## 🐛 问题与修复记录

### Bug #1: 重复记录问题

**现象：**
```json
// 每次复制产生2条记录
{"timestamp": "...", "success": false, "error": "Timeout"},
{"timestamp": "...", "success": true, "url": "https://..."}
```

**根本原因：**
```cpp
std::atomic<bool> completed(false);

// 超时任务
if (!completed.exchange(true)) {
    callback(emptyContext);  // 第1次调用callback
}

// 实际任务（假设耗时200ms）
if (!completed.exchange(true)) {
    callback(context);  // 第2次调用callback（被阻止）
}

// 问题：虽然第2次调用被阻止，但超时任务已经保存了空记录
```

**修复方案A（已采用）：**
```cpp
// 增加timeout：150ms → 300ms
auto browserAdapter = std::make_shared<BrowserAdapter>(300);

// 原理：给予足够时间让任务在超时前完成
// 实测数据：84-238ms，300ms足够
```

**修复方案B（未采用）：**
```cpp
// 超时时不保存记录，只在任务完成时保存
// 缺点：无法知道哪些操作超时失败
```

**修复方案C（未采用）：**
```cpp
// 使用更复杂的状态机管理
enum class TaskState { Pending, Timeout, Success };
std::atomic<TaskState> state;
// 缺点：过度设计，增加复杂度
```

**用户反馈：**
> "所以实际上是那个操作成功了吗？... 不管你超时还是怎么样，我要的就是你记录了内容"

**决策：** 方案A最简单且满足需求，耗时分析证明合理。

### Bug #2: 字符编码显示问题

**现象：**
```powershell
# PowerShell显示
"title": "杈圭紭榛戝鏉?"

# 文件实际内容（UTF-8）
"title": "边缘黑客杂志"
```

**分析：**
- ✅ 文件编码正确（UTF-8）
- ❌ PowerShell默认使用GBK显示

**解决：**
```powershell
# 方法1：设置PowerShell编码
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# 方法2：使用编辑器打开（推荐）
notepad $env:APPDATA\ClipboardMonitor\clipboard_history.json
```

**特殊情况：飞书零宽字符**
```json
// 飞书标题包含零宽字符（用于追踪）
"title": "文档标题\u200B\u200C"
```
- 这是飞书的行为，不是我们的bug
- 不需要处理，如实记录

### Bug #3: 类型不匹配编译错误

**错误示例：**
```cpp
// storage.cpp:101
json << ",\n \"chat_type\": \"" << EscapeJson(wechatCtx->chatType) << "\"";
//                                            ^^^^^^^^^^^^^^^^^^^^^^
// error C2664: 无法将 const std::wstring 转换为 const std::string &

// wechat_adapter.cpp:52
context->chatType = DetermineChatType(chatName);
// error C2679: 二元"=": 没有找到接受"std::string"类型的右操作数

// wechat_adapter.cpp:137
for (int i = 0; i < std::min(length, 10); i++) {
// error C2589: "(":"::"右边的非法标记
```

**修复过程：**

1. **统一chatType类型为wstring**
   ```cpp
   // context_data.h
   struct WeChatContext {
       std::wstring chatType;  // 改为wstring
   };

   // wechat_adapter.h & .cpp
   std::wstring DetermineChatType(const std::wstring& chatName);
   return L"group";  // 返回wstring字面量
   ```

2. **添加缺失的类型转换**
   ```cpp
   // storage.cpp
   json << EscapeJson(Utils::WideToUtf8(wechatCtx->chatType));

   // wechat_adapter.cpp
   DEBUG_LOG("Chat type: " + Utils::WideToUtf8(context->chatType));
   ```

3. **修复Windows宏冲突**
   ```cpp
   // 替换 std::min/max
   for (int i = 0; i < (length < 10 ? length : 10); i++) {
   int startIndex = (messageCount - count > 0 ? messageCount - count : 0);
   ```

4. **修复Unicode字符编码**
   ```cpp
   // vscode_adapter.cpp
   if (title[0] == L'\u25CF') {  // 使用Unicode转义
   ```

---

## 📊 数据结构设计

### ContextData继承体系

```cpp
// 基类：所有上下文的公共字段
struct ContextData {
    std::string adapterType;        // "browser", "wechat", "vscode", "notion"
    bool success = false;            // 是否成功获取
    std::wstring error;              // 错误信息（如果失败）
    int fetchTimeMs = 0;             // 获取耗时（ms）
    std::wstring title;              // 通用标题字段
    std::wstring url;                // 通用URL字段
    std::map<std::wstring, std::wstring> metadata;  // 额外元数据
};

// 浏览器上下文
struct BrowserContext : public ContextData {
    std::wstring sourceUrl;          // CF_HTML中的SourceURL
    std::wstring addressBarUrl;      // 地址栏URL（未来）
    std::wstring pageTitle;          // 网页标题
};

// 微信上下文
struct WeChatContext : public ContextData {
    std::wstring contactName;        // 联系人/群名称
    std::wstring chatType;           // "private" 或 "group"
    std::vector<std::wstring> recentMessages;  // 最近N条消息
    int messageCount = 5;            // 实际获取的消息数
};

// VSCode上下文
struct VSCodeContext : public ContextData {
    std::wstring fileName;           // 文件名 "main.cpp"
    std::wstring filePath;           // 完整路径
    std::wstring projectName;        // 项目名
    std::wstring projectRoot;        // 项目根目录（未来）
    int lineNumber = 0;              // 光标行号
    int columnNumber = 0;            // 光标列号
    std::string language;            // 编程语言 "C++"
    bool isModified = false;         // 是否有未保存修改
    std::vector<std::wstring> openFiles;  // 打开的文件列表（未来）
};

// Notion上下文
struct NotionContext : public ContextData {
    std::wstring pagePath;           // 伪URL "notion://Workspace/Page"
    std::wstring workspace;          // 工作区名称
    std::wstring pageType;           // "page", "database", "table"等
    std::vector<std::wstring> breadcrumbs;  // 面包屑导航
};
```

**设计原则：**
- ✅ 继承复用公共字段
- ✅ 每个Adapter有特定字段
- ✅ `metadata` 字段用于存放临时/实验性数据
- ✅ 所有时间戳、成功标记统一在基类

### JSON输出格式

```json
{
  "records": [
    {
      "timestamp": "2025-12-29T16:45:32.123+08:00",
      "content": "复制的文本内容",
      "source": {
        "process_name": "chrome.exe",
        "window_title": "GitHub - Anthropics/claude-code",
        "adapter_type": "browser",
        "success": true,
        "fetch_time_ms": 127,

        // Browser特定字段
        "source_url": "https://github.com/Anthropics/claude-code",
        "page_title": "GitHub - Anthropics/claude-code",

        // 元数据
        "metadata": {
          "app": "Chrome",
          "version": "120.0"
        }
      }
    },
    {
      "timestamp": "2025-12-29T16:46:15.456+08:00",
      "content": "微信消息内容",
      "source": {
        "process_name": "WeChat.exe",
        "window_title": "张三 - 微信",
        "adapter_type": "wechat",
        "success": true,
        "fetch_time_ms": 189,

        // WeChat特定字段
        "contact_name": "张三",
        "chat_type": "private",
        "recent_messages": [
          "你好",
          "在吗？",
          "看到请回复"
        ],

        "metadata": {
          "message_count": "3",
          "chat_type": "private"
        }
      }
    }
  ]
}
```

**设计巧思：**
- ✅ 人类可读（格式化缩进）
- ✅ 机器可解析（标准JSON）
- ✅ 扩展性好（adapter特定字段灵活添加）
- ✅ 时间戳包含时区（ISO 8601格式）

---

## 🚀 未来规划

### Phase 2: 浏览器扩展深度集成

**目标：** 获取选中文本的上下文（前后文本）

**技术方案：**

1. **Chrome Extension**
   ```javascript
   // content.js
   document.addEventListener('copy', (e) => {
       const selection = window.getSelection();
       const range = selection.getRangeAt(0);

       // 获取选中文本前200字符
       const beforeText = extractTextBefore(range, 200);

       // 获取选中文本后200字符
       const afterText = extractTextAfter(range, 200);

       // 发送到Native Messaging Host
       chrome.runtime.sendNativeMessage('com.echotype.clipboard', {
           selectedText: selection.toString(),
           beforeContext: beforeText,
           afterContext: afterText,
           url: window.location.href,
           title: document.title
       });
   });
   ```

2. **Native Messaging Host（C++）**
   ```cpp
   // 接收来自扩展的消息
   class NativeMessagingHost {
       void ProcessMessage(const std::string& jsonMessage) {
           // 解析JSON
           // 存储到剪贴板记录
           // 与ClipboardMonitor通信（Named Pipe/共享内存）
       }
   };
   ```

3. **ClipboardMonitor集成**
   ```cpp
   // 优先使用扩展数据，fallback到CF_HTML
   if (extensionDataAvailable) {
       context->selectedText = extensionData.selectedText;
       context->beforeContext = extensionData.beforeContext;
       context->afterContext = extensionData.afterContext;
   } else {
       context->sourceUrl = ExtractFromCF_HTML();
   }
   ```

**挑战：**
- 🔴 需要用户安装扩展（使用门槛）
- 🔴 Firefox/Safari需要单独实现
- 🔴 跨进程通信复杂度

### Phase 3: 更多Adapter支持

**规划中的Adapter：**

1. **SlackAdapter**
   - 频道名称
   - 消息线程
   - 提及用户

2. **FeishuAdapter**（飞书）
   - 文档类型（Doc/Sheet/Bitable）
   - 文档URL
   - 协作者信息

3. **ObsidianAdapter**
   - 笔记路径
   - 反向链接
   - 标签列表

4. **ExcelAdapter**
   - 工作簿名称
   - 工作表名称
   - 单元格范围

5. **PDFReaderAdapter**（Adobe/Foxit）
   - PDF文件路径
   - 页码
   - 高亮/注释

**实现优先级：** 根据用户需求反馈决定

### Phase 4: 配置化与UI

**配置文件（config.json）：**
```json
{
  "adapters": {
    "browser": {
      "enabled": true,
      "timeout": 300,
      "phase2_extension": false
    },
    "wechat": {
      "enabled": true,
      "timeout": 200,
      "message_count": 5
    },
    "vscode": {
      "enabled": true,
      "timeout": 150
    },
    "notion": {
      "enabled": true,
      "timeout": 150
    }
  },
  "storage": {
    "max_records": 10000,
    "auto_backup": true
  }
}
```

**管理界面：**
- 查看剪贴板历史
- 搜索/过滤记录
- 启用/禁用Adapter
- 调整超时和参数

---

## 🎯 核心设计哲学

### 1. **非侵入式设计**
- ✅ 后台运行，不干扰用户工作流
- ✅ 异步获取上下文，无感知延迟
- ✅ 失败静默降级（不弹窗报错）

### 2. **渐进增强策略**
- ✅ Phase 1先实现基础功能（URL、标题）
- ✅ Phase 2再追求深度功能（选中文本上下文）
- ✅ 每个Adapter独立演进，互不影响

### 3. **启发式 > 完美主义**
- ✅ 聊天类型判断：启发式规则足够好
- ✅ 页面类型推断：基于标题关键词
- ✅ 追求80%场景下可用，而非100%完美

### 4. **数据完整性优先**
- ✅ 宁可记录失败，也不丢失剪贴板内容
- ✅ 超时机制确保不会无限等待
- ✅ 所有记录带时间戳和成功标记

### 5. **可扩展性**
- ✅ Adapter模式：新增应用支持只需新建类
- ✅ ContextData继承：每个应用有独特字段
- ✅ metadata字段：预留扩展空间

---

## 🔧 开发环境与工具链

### 编译器
- **MSVC 19.41** (Visual Studio 2022 Build Tools)
- C++17标准
- `/EHsc /W4 /O2 /DUNICODE /D_UNICODE`

### 依赖库
- **Windows SDK**: UI Automation, COM
- **系统库**: user32, gdi32, shell32, ole32, oleaut32, shlwapi, oleacc, uiautomationcore

### 构建方式
```bash
# 方式1：CMake（跨平台）
mkdir build && cd build
cmake .. && cmake --build . --config Release

# 方式2：build.bat（Windows快速编译）
.\build.bat

# 方式3：手动编译
cl.exe /EHsc /std:c++17 /W4 /O2 /DUNICODE /D_UNICODE ...
```

### 调试技巧
```cpp
// debug_log.h：文件日志
#define DEBUG_LOG(msg) WriteDebugLog(msg)

// 输出位置：%APPDATA%\ClipboardMonitor\debug.log
// 实时监控：
Get-Content $env:APPDATA\ClipboardMonitor\debug.log -Wait -Tail 50
```

---

## 📚 代码组织结构

```
ClipboardMonitor/
├── main.cpp                          # 主程序入口
├── clipboard_monitor.h/cpp           # 剪贴板监控核心
├── storage.h/cpp                     # JSON持久化
├── utils.h                           # 工具函数（字符串转换等）
├── debug_log.h                       # 调试日志
│
├── context/
│   ├── context_data.h                # 上下文数据结构定义
│   ├── context_adapter.h             # IContextAdapter接口
│   ├── context_manager.h/cpp         # 上下文管理器（责任链）
│   ├── async_executor.h/cpp          # 异步任务执行器（线程池）
│   │
│   ├── adapters/
│   │   ├── browser_adapter.h/cpp     # 浏览器适配器
│   │   ├── wechat_adapter.h/cpp      # 微信适配器
│   │   ├── vscode_adapter.h/cpp      # VSCode适配器
│   │   └── notion_adapter.h/cpp      # Notion适配器
│   │
│   └── utils/
│       ├── ui_automation_helper.h/cpp  # UI Automation封装
│       └── html_parser.h/cpp           # HTML解析器
│
├── CMakeLists.txt                    # CMake构建配置
├── build.bat                         # Windows快速编译脚本
├── install.bat                       # 安装脚本（注册表）
├── uninstall.bat                     # 卸载脚本
│
└── browser_extension/                # Phase 2浏览器扩展（规划中）
    ├── manifest.json
    ├── content.js
    └── native_host/
```

**设计原则：**
- ✅ 清晰的模块划分（context/adapters/utils）
- ✅ 头文件与实现分离
- ✅ 每个Adapter独立文件
- ✅ 工具类集中管理

---

## 🤝 交接建议

### 给下一个AI开发者的建议

1. **先理解架构再改代码**
   - 阅读本文档的"系统架构设计"章节
   - 理解异步模型和Adapter模式
   - 看懂 ContextManager → AsyncExecutor → Adapter 的调用链

2. **新增Adapter的步骤**
   ```cpp
   // 1. 在 context/adapters/ 创建 xxx_adapter.h/cpp
   // 2. 继承 IContextAdapter，实现 CanHandle 和 GetContext
   // 3. 在 context_data.h 定义 XXXContext 结构
   // 4. 在 storage.cpp 添加序列化逻辑
   // 5. 在 main.cpp 注册Adapter
   // 6. 更新 CMakeLists.txt 和 build.bat
   ```

3. **调试技巧**
   - 使用 DEBUG_LOG 记录关键步骤
   - 检查 `%APPDATA%\ClipboardMonitor\debug.log`
   - 用Spy++查看窗口结构（UI Automation调试）

4. **性能优化建议**
   - 监控 `fetchTimeMs` 字段，识别慢Adapter
   - 调整各Adapter的timeout参数
   - 考虑缓存UI Automation查询结果

5. **用户反馈处理**
   - 优先修复导致记录丢失的bug
   - 新增Adapter根据用户需求排优先级
   - 启发式规则可以不完美，但要记录失败原因

---

## 📝 总结

这个系统的核心价值在于：

**将"复制"从简单的文本传输，升级为带有完整上下文的知识标注。**

通过异步架构、Adapter模式、启发式算法，我们在不影响用户体验的前提下，自动捕获了丰富的上下文信息。这些数据可以用于：

- 📚 个人知识库构建（记录你从哪些网站/文档获取知识）
- 🔍 历史检索（回溯某个想法的来源）
- 📊 工作流分析（统计你最常用的应用和内容类型）
- 🤖 AI训练数据（理解用户的信息获取模式）

**设计哲学：**
- 🎯 **实用主义** - 80%可用 > 100%完美
- 🚀 **渐进增强** - 先MVP，再迭代
- 🛡️ **数据优先** - 宁可记录失败，不能丢数据
- 🧩 **模块化** - 松耦合，易扩展

希望这份文档能帮助下一个AI（或人类）开发者快速理解整个系统！

---

**文档版本：** v1.0
**最后更新：** 2025-12-29
**作者：** Claude (Sonnet 4.5)
**项目状态：** Phase 1完成（4个Adapter），待编译测试
