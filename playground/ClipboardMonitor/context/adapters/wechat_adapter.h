#pragma once

#include "../context_adapter.h"
#include "../utils/ui_automation_helper.h"
#include <memory>

/**
 * @brief WeChat Context Adapter
 *
 * Captures chat context when copying from WeChat.
 * Supports: Windows WeChat client (WeChat.exe)
 *
 * Features:
 * - Extract current chat contact/group name
 * - Capture recent N messages (configurable, default: 5)
 * - Detect chat type (private/group)
 * - Extract sender names for each message
 *
 * Implementation:
 * - Uses UI Automation to traverse WeChat window structure
 * - Main window class: "WeChatMainWndForPC"
 * - Chat name: Top Static Text element
 * - Message list: Custom control requiring tree traversal
 */
class WeChatAdapter : public IContextAdapter {
public:
    /**
     * @brief Constructor
     *
     * @param timeout Timeout in milliseconds (default: 200ms)
     * @param messageCount Number of recent messages to capture (default: 5)
     */
    explicit WeChatAdapter(int timeout = 200, int messageCount = 5);

    /**
     * @brief Destructor
     */
    ~WeChatAdapter() override = default;

    /**
     * @brief Check if this adapter can handle the given process
     *
     * Supports:
     * - WeChat.exe (Windows WeChat client)
     *
     * @param processName Process executable name
     * @param windowTitle Window title (not used currently)
     * @return true if this is WeChat
     */
    bool CanHandle(const std::wstring& processName,
                  const std::wstring& windowTitle) override;

    /**
     * @brief Get WeChat chat context
     *
     * Execution flow:
     * 1. Find WeChat main window by class name
     * 2. Extract current chat contact/group name
     * 3. Determine chat type (private chat or group chat)
     * 4. Traverse message list to get recent N messages
     * 5. Extract sender and content for each message
     *
     * @param source Source information (HWND, process name, window title)
     * @return WeChatContext with chat info and recent messages
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
     * @return L"WeChatAdapter"
     */
    std::wstring GetAdapterName() const override { return L"WeChatAdapter"; }

private:
    /**
     * @brief Get current chat contact/group name
     *
     * Strategy:
     * - Find top-level Static Text elements in main window
     * - The chat name is usually the first significant text element
     * - Filter out system UI text (buttons, tabs, etc.)
     *
     * @param hwnd WeChat main window handle
     * @param uiHelper UI Automation helper
     * @return Chat contact/group name, or empty if not found
     */
    std::wstring GetChatName(HWND hwnd, UIAutomationHelper& uiHelper);

    /**
     * @brief Determine chat type from chat name
     *
     * Heuristic rules:
     * - Contains "群" or has parentheses with numbers like "(123)" → Group chat
     * - Otherwise → Private chat
     *
     * Note: This is a heuristic approach and may not be 100% accurate
     *
     * @param chatName Chat name to analyze
     * @return L"group" or L"private"
     */
    std::wstring DetermineChatType(const std::wstring& chatName);

    /**
     * @brief Get recent messages from message list
     *
     * Strategy:
     * 1. Find message list container (scrollable area)
     * 2. Traverse child elements to find message items
     * 3. Extract sender name and message content
     * 4. Return most recent N messages
     *
     * Note: WeChat's message list structure is complex and may vary
     * between versions. This implementation tries common patterns.
     *
     * @param hwnd WeChat main window handle
     * @param uiHelper UI Automation helper
     * @param count Number of recent messages to capture
     * @return Vector of message strings (may be less than count)
     */
    std::vector<std::wstring> GetRecentMessages(HWND hwnd,
                                                UIAutomationHelper& uiHelper,
                                                int count);

    /**
     * @brief Extract message text from a message element
     *
     * @param element Message UI element
     * @param uiHelper UI Automation helper
     * @return Message text, or empty if extraction failed
     */
    std::wstring ExtractMessageText(IUIAutomationElement* element,
                                    UIAutomationHelper& uiHelper);

    int m_timeout;       // Timeout in milliseconds
    int m_messageCount;  // Number of recent messages to capture
};
