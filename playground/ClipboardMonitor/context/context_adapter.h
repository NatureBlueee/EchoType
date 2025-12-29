#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "context_data.h"
#include "../clipboard_monitor.h"
#include <memory>
#include <string>
#include <algorithm>  // for std::transform
#include <cwctype>    // for std::towlower

// Abstract base class for context adapters
class IContextAdapter {
public:
    virtual ~IContextAdapter() = default;

    // Check if this adapter can handle the given process
    // processName: e.g., "chrome.exe", "WeChat.exe"
    // windowTitle: Window title for additional context
    virtual bool CanHandle(const std::wstring& processName,
                          const std::wstring& windowTitle = L"") = 0;

    // Get context information
    // source: Source application information
    // Returns: Pointer to context data (nullptr if failed)
    virtual std::shared_ptr<ContextData> GetContext(const SourceInfo& source) = 0;

    // Get timeout in milliseconds for this adapter
    // Returns: Timeout value (default 100ms)
    virtual int GetTimeout() const { return 100; }

    // Get adapter name for logging
    virtual std::wstring GetAdapterName() const = 0;

protected:
    // Helper: Convert string to lowercase for case-insensitive comparison
    static std::wstring ToLower(const std::wstring& str) {
        std::wstring lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                      [](wchar_t c) { return ::towlower(c); });
        return lower;
    }

    // Helper: Check if processName contains a substring (case-insensitive)
    static bool ProcessNameContains(const std::wstring& processName,
                                   const std::wstring& substring) {
        std::wstring lowerName = ToLower(processName);
        std::wstring lowerSub = ToLower(substring);
        return lowerName.find(lowerSub) != std::wstring::npos;
    }
};
