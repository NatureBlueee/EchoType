#include "clipboard_monitor.h"
#include "storage.h"
#include "utils.h"
#include "debug_log.h"
#include "context/context_manager.h"
#include "context/adapters/browser_adapter.h"
#include "context/adapters/wechat_adapter.h"
#include "context/adapters/vscode_adapter.h"
#include "context/adapters/notion_adapter.h"
#include <shellapi.h>

// Global variables
ClipboardMonitor g_monitor;
Storage g_storage;
std::shared_ptr<ContextManager> g_contextManager;
NOTIFYICONDATAW g_nid = {};
bool g_monitoring = true;
WNDPROC g_originalWndProc = nullptr;  // Save original window procedure

// Tray icon menu IDs
#define ID_TRAY_EXIT    1001
#define ID_TRAY_PAUSE   1002
#define ID_TRAY_OPEN    1003
#define ID_TRAY_ICON    1

// Forward declarations
void CreateTrayIcon(HWND hwnd, HINSTANCE hInstance);
void RemoveTrayIcon();
void ShowTrayMenu(HWND hwnd);
void OpenHistoryFile();
LRESULT CALLBACK TrayWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Windows entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    
    // Initialize COM (needed for some Windows APIs)
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    
    // Initialize storage and debug log
    std::wstring appDataPath = Utils::GetAppDataPath();
    DebugLog::Instance().Initialize(appDataPath);
    DEBUG_LOG("Starting ClipboardMonitor...");
    
    if (!g_storage.Initialize(appDataPath)) {
        DEBUG_LOG("ERROR: Failed to initialize storage");
        MessageBoxW(NULL, L"Failed to initialize storage!", L"ClipboardMonitor Error", MB_ICONERROR);
        return 1;
    }
    DEBUG_LOG("Storage initialized at: " + Utils::WideToUtf8(appDataPath));

    // Initialize context manager
    g_contextManager = std::make_shared<ContextManager>();
    DEBUG_LOG("ContextManager created");

    if (!g_contextManager->Initialize()) {
        DEBUG_LOG("ERROR: Failed to initialize ContextManager");
        MessageBoxW(NULL, L"Failed to initialize context manager!", L"ClipboardMonitor Error", MB_ICONERROR);
        return 1;
    }
    DEBUG_LOG("ContextManager initialized");

    // Register adapters with generous timeout (5 seconds)
    // This ensures UI Automation has enough time to complete
    // Timeout is only for preventing infinite hangs, not for speed
    const int MAX_TIMEOUT = 5000;  // 5 seconds max
    
    auto browserAdapter = std::make_shared<BrowserAdapter>(MAX_TIMEOUT);
    g_contextManager->RegisterAdapter(browserAdapter);
    DEBUG_LOG("BrowserAdapter registered");

    auto wechatAdapter = std::make_shared<WeChatAdapter>(MAX_TIMEOUT, 5);  // 5 recent messages
    g_contextManager->RegisterAdapter(wechatAdapter);
    DEBUG_LOG("WeChatAdapter registered");

    auto vscodeAdapter = std::make_shared<VSCodeAdapter>(MAX_TIMEOUT);
    g_contextManager->RegisterAdapter(vscodeAdapter);
    DEBUG_LOG("VSCodeAdapter registered");

    auto notionAdapter = std::make_shared<NotionAdapter>(MAX_TIMEOUT);
    g_contextManager->RegisterAdapter(notionAdapter);
    DEBUG_LOG("NotionAdapter registered");

    // Initialize clipboard monitor
    if (!g_monitor.Initialize(hInstance)) {
        DEBUG_LOG("ERROR: Failed to initialize clipboard monitor");
        MessageBoxW(NULL, L"Failed to initialize clipboard monitor!", L"ClipboardMonitor Error", MB_ICONERROR);
        return 1;
    }
    DEBUG_LOG("Clipboard monitor initialized successfully");

    // Set context manager on clipboard monitor
    g_monitor.SetContextManager(g_contextManager);
    DEBUG_LOG("ContextManager attached to ClipboardMonitor");

    // Set up the callback
    g_monitor.SetCallback([](const ClipboardEntry& entry) {
        if (g_monitoring) {
            g_storage.SaveEntry(entry);
            
            // Optional: Show notification for debugging
            // OutputDebugStringW((L"Clipboard: " + entry.contentPreview + L"\n").c_str());
        }
    });
    
    // Create tray icon
    CreateTrayIcon(g_monitor.GetWindowHandle(), hInstance);
    
    // Register hotkey: Ctrl+Shift+Q to quit
    RegisterHotKey(g_monitor.GetWindowHandle(), 1, MOD_CONTROL | MOD_SHIFT, 'Q');
    
    // Show startup notification
    g_nid.uFlags = NIF_INFO;
    wcscpy_s(g_nid.szInfoTitle, L"ClipboardMonitor");
    wcscpy_s(g_nid.szInfo, L"Monitoring clipboard. Press Ctrl+Shift+Q to exit.");
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    
    // Run message loop
    g_monitor.Run();
    
    // Cleanup
    UnregisterHotKey(g_monitor.GetWindowHandle(), 1);
    RemoveTrayIcon();
    CoUninitialize();
    
    return 0;
}

void CreateTrayIcon(HWND hwnd, HINSTANCE hInstance) {
    (void)hInstance;
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = ID_TRAY_ICON;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_USER + 1;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"ClipboardMonitor - Running");
    
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    
    // Save original window procedure and subclass
    g_originalWndProc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)TrayWindowProc);
}

LRESULT CALLBACK TrayWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Handle tray icon messages
    if (msg == WM_USER + 1) {
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
            ShowTrayMenu(hwnd);
        }
        return 0;
    }
    
    // Handle hotkey
    if (msg == WM_HOTKEY && wParam == 1) {
        g_monitor.Stop();
        return 0;
    }
    
    // Forward all other messages to original window procedure (including WM_CLIPBOARDUPDATE)
    if (g_originalWndProc) {
        return CallWindowProcW(g_originalWndProc, hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

void ShowTrayMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN, L"Open History File");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING | (g_monitoring ? MF_CHECKED : 0), ID_TRAY_PAUSE, L"Monitoring");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
    
    POINT pt;
    GetCursorPos(&pt);
    
    SetForegroundWindow(hwnd);
    
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
    
    switch (cmd) {
        case ID_TRAY_EXIT:
            g_monitor.Stop();
            break;
        case ID_TRAY_PAUSE:
            g_monitoring = !g_monitoring;
            wcscpy_s(g_nid.szTip, g_monitoring ? L"ClipboardMonitor - Running" : L"ClipboardMonitor - Paused");
            Shell_NotifyIconW(NIM_MODIFY, &g_nid);
            break;
        case ID_TRAY_OPEN:
            OpenHistoryFile();
            break;
    }
    
    DestroyMenu(hMenu);
}

void OpenHistoryFile() {
    std::wstring path = g_storage.GetFilePath();
    ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
}
