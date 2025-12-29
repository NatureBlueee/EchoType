#include "clipboard_monitor.h"
#include "utils.h"
#include "debug_log.h"
#include "context/context_manager.h"
#include <psapi.h>
#include <shellapi.h>
#include <oleacc.h>
#include <sstream>

#pragma comment(lib, "oleacc.lib")

const wchar_t* ClipboardMonitor::WINDOW_CLASS_NAME = L"ClipboardMonitorClass";

ClipboardMonitor::ClipboardMonitor()
    : m_hwnd(nullptr)
    , m_hInstance(nullptr)
    , m_running(false)
    , m_callback(nullptr)
    , m_lastSequenceNumber(0)
{
}

ClipboardMonitor::~ClipboardMonitor() {
    Stop();
}

bool ClipboardMonitor::Initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    
    if (!RegisterClassExW(&wc)) {
        // Class may already be registered
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }
    
    // Create hidden message-only window
    m_hwnd = CreateWindowExW(
        0,
        WINDOW_CLASS_NAME,
        L"ClipboardMonitor",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,  // Message-only window
        nullptr,
        hInstance,
        this  // Pass this pointer
    );
    
    if (!m_hwnd) {
        return false;
    }
    
    // Register as clipboard listener
    if (!AddClipboardFormatListener(m_hwnd)) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
        return false;
    }
    
    // Get initial sequence number
    m_lastSequenceNumber = GetClipboardSequenceNumber();
    
    return true;
}

void ClipboardMonitor::Run() {
    m_running = true;
    
    MSG msg;
    while (m_running && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void ClipboardMonitor::Stop() {
    if (m_hwnd) {
        RemoveClipboardFormatListener(m_hwnd);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    m_running = false;
    PostQuitMessage(0);
}

void ClipboardMonitor::SetCallback(ClipboardChangeCallback callback) {
    m_callback = callback;
}

LRESULT CALLBACK ClipboardMonitor::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    ClipboardMonitor* monitor = nullptr;
    
    if (uMsg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        monitor = reinterpret_cast<ClipboardMonitor*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(monitor));
    } else {
        monitor = reinterpret_cast<ClipboardMonitor*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    // Custom message for deferred clipboard processing
    const UINT WM_DEFERRED_CLIPBOARD = WM_USER + 100;
    
    switch (uMsg) {
        case WM_CLIPBOARDUPDATE:
            // Don't process immediately - post a message to ourselves
            // This allows the source app to finish its clipboard operation
            DEBUG_LOG(">>> WM_CLIPBOARDUPDATE - posting deferred message");
            PostMessage(hwnd, WM_DEFERRED_CLIPBOARD, 0, 0);
            return 0;
            
        case WM_USER + 100:  // WM_DEFERRED_CLIPBOARD
            DEBUG_LOG(">>> WM_DEFERRED_CLIPBOARD - now processing");
            if (monitor) {
                monitor->OnClipboardUpdate();
            }
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            
        case WM_HOTKEY:
            // Ctrl+Shift+Q to quit (hotkey ID 1)
            if (wParam == 1 && monitor) {
                monitor->Stop();
            }
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void ClipboardMonitor::OnClipboardUpdate() {
    // Check if clipboard actually changed
    DWORD currentSequence = GetClipboardSequenceNumber();
    if (currentSequence == m_lastSequenceNumber) {
        DEBUG_LOG("Sequence unchanged, skipping");
        return;
    }
    
    std::ostringstream oss;
    oss << "Seq: " << m_lastSequenceNumber << " -> " << currentSequence;
    DEBUG_LOG(oss.str());
    m_lastSequenceNumber = currentSequence;
    
    ClipboardEntry entry;
    entry.timestamp = Utils::GetTimestamp();
    
    // Get source info first (before opening clipboard)
    GetSourceInfo(entry.source);
    DEBUG_LOG("Source: " + Utils::WideToUtf8(entry.source.processName) + " | " + Utils::WideToUtf8(entry.source.windowTitle));
    
    // Get clipboard content
    if (GetClipboardContent(entry)) {
        // Try to get browser URL (deprecated, kept for backward compatibility)
        entry.contextUrl = TryGetBrowserUrl(entry.source.windowHandle, entry.source.processName);

        DEBUG_LOG("OK: " + entry.contentType + " | " + Utils::WideToUtf8(entry.contentPreview.substr(0, 50)));

        // Get context asynchronously if ContextManager is available
        if (m_contextManager) {
            m_contextManager->GetContextAsync(
                entry.source,
                [this, entry](std::shared_ptr<ContextData> contextData) mutable {
                    // Attach context data
                    if (contextData) {
                        entry.contextData = contextData;

                        std::ostringstream oss;
                        oss << "Context: " << contextData->adapterType
                            << ", success=" << (contextData->success ? "true" : "false")
                            << ", time=" << contextData->fetchTimeMs << "ms";
                        DEBUG_LOG(oss.str());
                    }

                    // Call the callback with context data attached
                    if (m_callback) {
                        m_callback(entry);
                    }
                }
            );
        } else {
            // No context manager - call callback directly
            if (m_callback) {
                m_callback(entry);
            }
        }
    } else {
        DEBUG_LOG("FAILED: GetClipboardContent returned false");
    }
}

bool ClipboardMonitor::GetClipboardContent(ClipboardEntry& entry) {
    // Try to open clipboard with retries (some apps hold it longer)
    const int MAX_RETRIES = 100;  // Max 100ms wait
    const int RETRY_DELAY_MS = 1;
    bool clipboardOpened = false;
    
    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        if (OpenClipboard(m_hwnd)) {
            clipboardOpened = true;
            if (attempt > 0) {
                std::ostringstream oss;
                oss << "Clipboard opened after " << attempt << "ms";
                DEBUG_LOG(oss.str());
            }
            break;
        }
        Sleep(RETRY_DELAY_MS);
    }
    
    if (!clipboardOpened) {
        DWORD err = GetLastError();
        std::ostringstream oss;
        oss << "Failed to open clipboard after " << MAX_RETRIES << " retries, error: " << err;
        DEBUG_LOG(oss.str());
        return false;
    }
    
    // First, log all available formats
    {
        std::ostringstream oss;
        oss << "Available formats: ";
        UINT format = 0;
        int count = 0;
        while ((format = EnumClipboardFormats(format)) != 0) {
            wchar_t formatName[256] = {0};
            if (GetClipboardFormatNameW(format, formatName, 256) > 0) {
                oss << Utils::WideToUtf8(formatName);
            } else {
                oss << "CF#" << format;
            }
            oss << " ";
            count++;
        }
        oss << "(total: " << count << ")";
        DEBUG_LOG(oss.str());
    }
    
    bool success = false;
    
    // Check for text content (Unicode first, then ANSI)
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* text = static_cast<wchar_t*>(GlobalLock(hData));
            if (text) {
                entry.contentType = "text";
                entry.content = text;
                
                // Create preview (first 200 chars)
                if (entry.content.length() > 200) {
                    entry.contentPreview = entry.content.substr(0, 200) + L"...";
                } else {
                    entry.contentPreview = entry.content;
                }
                
                GlobalUnlock(hData);
                success = true;
            }
        }
    }
    // Try ANSI text if Unicode not available (some apps like WeChat may use this)
    else if (IsClipboardFormatAvailable(CF_TEXT)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* text = static_cast<char*>(GlobalLock(hData));
            if (text) {
                entry.contentType = "text";
                // Convert ANSI to wide string
                int len = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
                if (len > 0) {
                    std::wstring wtext(len - 1, 0);
                    MultiByteToWideChar(CP_ACP, 0, text, -1, &wtext[0], len);
                    entry.content = wtext;
                    
                    if (entry.content.length() > 200) {
                        entry.contentPreview = entry.content.substr(0, 200) + L"...";
                    } else {
                        entry.contentPreview = entry.content;
                    }
                    success = true;
                }
                GlobalUnlock(hData);
            }
        }
    }
    // Check for bitmap
    else if (IsClipboardFormatAvailable(CF_BITMAP) || IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_DIBV5)) {
        entry.contentType = "image";
        entry.content = L"[Image data]";
        entry.contentPreview = L"[Image copied]";
        success = true;
    }
    // Check for file list
    else if (IsClipboardFormatAvailable(CF_HDROP)) {
        HANDLE hData = GetClipboardData(CF_HDROP);
        if (hData) {
            HDROP hDrop = static_cast<HDROP>(hData);
            UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
            
            entry.contentType = "files";
            std::wstring files;
            
            for (UINT i = 0; i < fileCount && i < 10; i++) {
                wchar_t filePath[MAX_PATH];
                if (DragQueryFileW(hDrop, i, filePath, MAX_PATH)) {
                    if (!files.empty()) files += L"\n";
                    files += filePath;
                }
            }
            
            if (fileCount > 10) {
                files += L"\n... and " + std::to_wstring(fileCount - 10) + L" more files";
            }
            
            entry.content = files;
            entry.contentPreview = std::to_wstring(fileCount) + L" file(s)";
            success = true;
        }
    }
    // Check for HTML
    else {
        UINT htmlFormat = RegisterClipboardFormatW(L"HTML Format");
        if (IsClipboardFormatAvailable(htmlFormat)) {
            entry.contentType = "html";
            entry.content = L"[HTML content]";
            entry.contentPreview = L"[HTML copied]";
            success = true;
        }
    }
    
    // If no known format found, still record the clipboard change with format info
    if (!success) {
        // Enumerate available formats
        std::wstring formats;
        UINT format = 0;
        int count = 0;
        while ((format = EnumClipboardFormats(format)) != 0 && count < 5) {
            wchar_t formatName[256] = {0};
            if (GetClipboardFormatNameW(format, formatName, 256) > 0) {
                if (!formats.empty()) formats += L", ";
                formats += formatName;
            } else {
                // Built-in format, show number
                if (!formats.empty()) formats += L", ";
                formats += L"Format#" + std::to_wstring(format);
            }
            count++;
        }
        
        if (!formats.empty()) {
            entry.contentType = "unknown";
            entry.content = L"[Unknown format: " + formats + L"]";
            entry.contentPreview = L"[Unknown clipboard format]";
            success = true;  // Still record it
        }
    }
    
    CloseClipboard();
    return success;
}

void ClipboardMonitor::GetSourceInfo(SourceInfo& info) {
    // Get foreground window
    info.windowHandle = GetForegroundWindow();
    
    if (!info.windowHandle) {
        info.processName = L"Unknown";
        info.processPath = L"";
        info.windowTitle = L"";
        info.processId = 0;
        return;
    }
    
    // Get window title
    wchar_t title[512];
    if (GetWindowTextW(info.windowHandle, title, 512)) {
        info.windowTitle = title;
    }
    
    // Get process ID
    GetWindowThreadProcessId(info.windowHandle, &info.processId);
    
    // Get process name and path
    info.processName = GetProcessName(info.processId);
    info.processPath = GetProcessPath(info.processId);
}

std::wstring ClipboardMonitor::GetProcessName(DWORD pid) {
    std::wstring name = L"Unknown";
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess) {
        wchar_t path[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
            // Extract filename from path
            wchar_t* lastSlash = wcsrchr(path, L'\\');
            if (lastSlash) {
                name = lastSlash + 1;
            } else {
                name = path;
            }
        }
        CloseHandle(hProcess);
    }
    
    return name;
}

std::wstring ClipboardMonitor::GetProcessPath(DWORD pid) {
    std::wstring path;
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess) {
        wchar_t pathBuf[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, pathBuf, &size)) {
            path = pathBuf;
        }
        CloseHandle(hProcess);
    }
    
    return path;
}

std::wstring ClipboardMonitor::TryGetBrowserUrl(HWND hwnd, const std::wstring& processName) {
    (void)hwnd;  // Reserved for future use (UI Automation)

    // Check if it's a known browser
    std::wstring lowerName = processName;
    for (auto& c : lowerName) c = towlower(c);
    
    bool isBrowser = (lowerName.find(L"chrome") != std::wstring::npos ||
                      lowerName.find(L"firefox") != std::wstring::npos ||
                      lowerName.find(L"edge") != std::wstring::npos ||
                      lowerName.find(L"msedge") != std::wstring::npos ||
                      lowerName.find(L"brave") != std::wstring::npos ||
                      lowerName.find(L"opera") != std::wstring::npos);
    
    if (!isBrowser) {
        return L"";
    }
    
    // For browsers, the window title often contains the page title
    // The URL would require UI Automation which is more complex
    // For now, we note that this is from a browser
    return L"[Browser - see window title for context]";
}
