#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <string>
#include <map>
#include <vector>
#include <memory>

// Base context data structure
struct ContextData {
    std::string adapterType;      // "browser", "wechat", "vscode", "notion"

    // Common fields
    std::wstring url;             // URL, file path, or pseudo-URL
    std::wstring title;           // Page title, document title, etc.

    // Extended fields (for flexibility)
    std::map<std::wstring, std::wstring> metadata;

    // Performance metrics
    int fetchTimeMs = 0;          // Time taken to fetch context
    bool success = false;         // Whether context was successfully retrieved
    std::wstring error;           // Error message if failed

    virtual ~ContextData() = default;

    // Helper methods
    void SetMetadata(const std::wstring& key, const std::wstring& value) {
        metadata[key] = value;
    }

    std::wstring GetMetadata(const std::wstring& key) const {
        auto it = metadata.find(key);
        return it != metadata.end() ? it->second : L"";
    }
};

// Browser-specific context
struct BrowserContext : public ContextData {
    std::wstring sourceUrl;           // URL from CF_HTML format
    std::wstring addressBarUrl;       // URL from address bar (UI Automation)
    std::wstring pageTitle;           // Page title
    std::wstring selectedText;        // Selected text (if available)

    // Phase 2: Full page content (optional)
    // std::wstring fullPageContent;
    // std::wstring domPath;

    BrowserContext() {
        adapterType = "browser";
    }
};

// WeChat-specific context
struct WeChatContext : public ContextData {
    std::wstring contactName;         // Contact/Group name
    std::wstring chatType;            // "private" or "group"
    std::vector<std::wstring> recentMessages;  // Recent N messages
    int messageCount = 5;             // Number of messages captured

    WeChatContext() {
        adapterType = "wechat";
    }
};

// VS Code-specific context
struct VSCodeContext : public ContextData {
    std::wstring fileName;            // File name (e.g., "main.cpp")
    std::wstring filePath;            // Full file path
    std::wstring projectName;         // Project name
    std::wstring projectRoot;         // Project root directory
    int lineNumber = 0;               // Current line number (0 if unknown)
    int columnNumber = 0;             // Current column number (0 if unknown)
    std::string language;             // Programming language (e.g., "C++", "Python")
    bool isModified = false;          // True if file has unsaved changes
    std::vector<std::wstring> openFiles;  // List of open files (optional)

    VSCodeContext() {
        adapterType = "vscode";
    }
};

// Notion-specific context
struct NotionContext : public ContextData {
    std::wstring pagePath;            // Page path
    std::wstring workspace;           // Workspace name
    std::wstring pageType;            // "page" or "database"
    std::vector<std::wstring> breadcrumbs;  // Breadcrumb navigation

    NotionContext() {
        adapterType = "notion";
    }
};
