#pragma once

#include "../context_adapter.h"
#include "../utils/ui_automation_helper.h"
#include <memory>
#include <map>

/**
 * @brief VS Code Context Adapter
 *
 * Captures code file context when copying from Visual Studio Code.
 * Supports: VS Code (Code.exe), Cursor (Cursor.exe), and other VS Code-based editors
 *
 * Features:
 * - Extract current file path from window title or status bar
 * - Extract project name
 * - Get cursor position (line and column)
 * - Infer programming language from file extension
 * - Detect if file is modified (● indicator)
 *
 * Window title format:
 * - "● filename.ext - Project Name - Visual Studio Code" (modified)
 * - "filename.ext - Project Name - Visual Studio Code" (saved)
 */
class VSCodeAdapter : public IContextAdapter {
public:
    /**
     * @brief Constructor
     *
     * @param timeout Timeout in milliseconds (default: 150ms)
     */
    explicit VSCodeAdapter(int timeout = 150);

    /**
     * @brief Destructor
     */
    ~VSCodeAdapter() override = default;

    /**
     * @brief Check if this adapter can handle the given process
     *
     * Supports:
     * - Code.exe (Visual Studio Code)
     * - Cursor.exe (Cursor AI editor)
     * - code-insiders.exe (VS Code Insiders)
     * - VSCodium.exe (VSCodium)
     *
     * @param processName Process executable name
     * @param windowTitle Window title (not used currently)
     * @return true if this is a VS Code-based editor
     */
    bool CanHandle(const std::wstring& processName,
                  const std::wstring& windowTitle) override;

    /**
     * @brief Get VS Code context
     *
     * Execution flow:
     * 1. Parse window title to extract file name and project name
     * 2. Try to get full file path from status bar (UI Automation)
     * 3. Extract cursor position (Ln X, Col Y) from status bar
     * 4. Infer programming language from file extension
     * 5. Detect if file is modified
     *
     * @param source Source information (HWND, process name, window title)
     * @return VSCodeContext with file path, line number, language, etc.
     */
    std::shared_ptr<ContextData> GetContext(const SourceInfo& source) override;

    /**
     * @brief Get adapter timeout
     *
     * @return Timeout in milliseconds
     */
    int GetTimeout() const override { return m_timeout; }

    /**
     * @brief Get adapter name
     *
     * @return L"VSCodeAdapter"
     */
    std::wstring GetAdapterName() const override { return L"VSCodeAdapter"; }

private:
    /**
     * @brief Parse window title to extract file info
     *
     * Window title format:
     * - "● filename.ext - Project Name - Visual Studio Code"
     * - "filename.ext - Project Name - Visual Studio Code"
     *
     * @param windowTitle Window title
     * @param fileName Output: file name (e.g., "main.cpp")
     * @param projectName Output: project name
     * @param isModified Output: true if file is modified (● present)
     */
    void ParseWindowTitle(const std::wstring& windowTitle,
                         std::wstring& fileName,
                         std::wstring& projectName,
                         bool& isModified);

    /**
     * @brief Get file path from status bar via UI Automation
     *
     * Strategy:
     * - Find status bar element (usually at bottom)
     * - Look for Text elements containing file path
     * - Status bar often shows full path or relative path
     *
     * @param hwnd VS Code window handle
     * @param uiHelper UI Automation helper
     * @return Full or relative file path, or empty if not found
     */
    std::wstring GetFilePathFromStatusBar(HWND hwnd, UIAutomationHelper& uiHelper);

    /**
     * @brief Get cursor position from status bar
     *
     * Status bar typically shows: "Ln 42, Col 15"
     *
     * @param hwnd VS Code window handle
     * @param uiHelper UI Automation helper
     * @param lineNumber Output: line number
     * @param columnNumber Output: column number
     */
    void GetCursorPosition(HWND hwnd,
                          UIAutomationHelper& uiHelper,
                          int& lineNumber,
                          int& columnNumber);

    /**
     * @brief Infer programming language from file extension
     *
     * @param fileName File name with extension (e.g., "main.cpp")
     * @return Language name (e.g., "C++", "Python", "JavaScript")
     */
    std::string InferLanguage(const std::wstring& fileName);

    /**
     * @brief Extract file extension from file name
     *
     * @param fileName File name
     * @return Extension without dot (e.g., "cpp", "py", "js")
     */
    std::wstring GetFileExtension(const std::wstring& fileName);

    int m_timeout;  // Timeout in milliseconds

    // Language mapping: extension -> language name
    static const std::map<std::wstring, std::string> s_languageMap;
};
