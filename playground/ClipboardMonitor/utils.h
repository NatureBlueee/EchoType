#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cwctype>
#include <psapi.h>
#include <shlobj.h>

namespace Utils {

// Convert wide string to UTF-8
inline std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], size, nullptr, nullptr);
    return result;
}

// Convert UTF-8 to wide string
inline std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return std::wstring();
    
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size);
    return result;
}

// Get current timestamp in ISO 8601 format
inline std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
    localtime_s(&tm_buf, &time);
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    // Add timezone offset
    char tz[6];
    std::strftime(tz, sizeof(tz), "%z", &tm_buf);
    // Format as +08:00 instead of +0800
    std::string tzStr(tz);
    if (tzStr.length() >= 5) {
        tzStr.insert(3, ":");
    }
    oss << tzStr;
    
    return oss.str();
}

// Escape string for JSON
inline std::string EscapeJson(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

// Get AppData path
inline std::wstring GetAppDataPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return std::wstring(path) + L"\\ClipboardMonitor";
    }
    return L".\\ClipboardMonitor";
}

// Create directory if not exists
inline bool EnsureDirectoryExists(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return CreateDirectoryW(path.c_str(), NULL) != 0;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Truncate string for preview
inline std::string TruncateForPreview(const std::string& str, size_t maxLen = 100) {
    if (str.length() <= maxLen) return str;
    return str.substr(0, maxLen) + "...";
}

// Convert wide string to lowercase
inline std::wstring ToLower(const std::wstring& str) {
    std::wstring result = str;
    for (wchar_t& c : result) {
        c = ::towlower(c);
    }
    return result;
}

} // namespace Utils
