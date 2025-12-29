#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <string>
#include <functional>
#include <memory>

// Forward declarations
struct ContextData;
class ContextManager;

// Information about the source application
struct SourceInfo {
    std::wstring processName;      // e.g., "chrome.exe"
    std::wstring processPath;      // Full path to executable
    std::wstring windowTitle;      // Window title (context)
    DWORD processId;               // Process ID
    HWND windowHandle;             // Window handle
};

// Clipboard entry data
struct ClipboardEntry {
    std::string timestamp;         // ISO 8601 timestamp
    std::string contentType;       // "text", "image", "files", etc.
    std::wstring content;          // Actual content (for text)
    std::wstring contentPreview;   // Truncated preview
    SourceInfo source;             // Source application info
    std::wstring contextUrl;       // URL if available (deprecated, kept for backward compatibility)
    std::shared_ptr<ContextData> contextData;  // Extended context information
};

// Callback type for clipboard changes
using ClipboardChangeCallback = std::function<void(const ClipboardEntry&)>;

class ClipboardMonitor {
public:
    ClipboardMonitor();
    ~ClipboardMonitor();

    // Initialize the monitor with a hidden window
    bool Initialize(HINSTANCE hInstance);
    
    // Start monitoring (blocking - runs message loop)
    void Run();
    
    // Stop monitoring
    void Stop();
    
    // Set callback for clipboard changes
    void SetCallback(ClipboardChangeCallback callback);
    
    // Get the window handle
    HWND GetWindowHandle() const { return m_hwnd; }

    // Set context manager
    void SetContextManager(std::shared_ptr<ContextManager> manager) {
        m_contextManager = manager;
    }

private:
    // Window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // Handle clipboard update
    void OnClipboardUpdate();
    
    // Get current clipboard content
    bool GetClipboardContent(ClipboardEntry& entry);
    
    // Get source application info
    void GetSourceInfo(SourceInfo& info);
    
    // Get process name from PID
    std::wstring GetProcessName(DWORD pid);
    
    // Get process path from PID
    std::wstring GetProcessPath(DWORD pid);
    
    // Try to extract URL from browser window
    std::wstring TryGetBrowserUrl(HWND hwnd, const std::wstring& processName);

private:
    HWND m_hwnd;
    HINSTANCE m_hInstance;
    bool m_running;
    ClipboardChangeCallback m_callback;
    DWORD m_lastSequenceNumber;  // To detect actual changes
    std::shared_ptr<ContextManager> m_contextManager;  // Context manager for async context retrieval

    static const wchar_t* WINDOW_CLASS_NAME;
};
