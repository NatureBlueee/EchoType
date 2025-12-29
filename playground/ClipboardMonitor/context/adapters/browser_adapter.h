#pragma once

#include "../context_adapter.h"
#include "../utils/ui_automation_helper.h"
#include "../utils/html_parser.h"
#include <memory>

/**
 * @brief Browser Context Adapter (Phase 1)
 *
 * Captures context when copying from web browsers.
 * Supports: Chrome, Edge, Firefox, Opera, Brave, Vivaldi
 *
 * Two-method approach:
 * 1. Primary: UI Automation to get address bar URL (accurate, current page)
 * 2. Fallback: Parse CF_HTML from clipboard to get SourceURL (may be stale)
 *
 * Phase 1 Features (Current):
 * - URL extraction (address bar or CF_HTML)
 * - Page title extraction (window title or CF_HTML)
 * - Performance metrics (fetch time)
 *
 * Phase 2 Features (Future - requires browser extension):
 * - Full page content
 * - Selected text context
 * - DOM path of selection
 */
class BrowserAdapter : public IContextAdapter {
public:
    /**
     * @brief Constructor
     *
     * @param timeout Timeout in milliseconds (default: 150ms)
     */
    explicit BrowserAdapter(int timeout = 150);

    /**
     * @brief Destructor
     */
    ~BrowserAdapter() override = default;

    /**
     * @brief Check if this adapter can handle the given process
     *
     * Supports:
     * - chrome.exe (Google Chrome)
     * - msedge.exe (Microsoft Edge)
     * - firefox.exe (Mozilla Firefox)
     * - opera.exe (Opera)
     * - brave.exe (Brave)
     * - vivaldi.exe (Vivaldi)
     *
     * @param processName Process executable name (e.g., "chrome.exe")
     * @param windowTitle Window title (not used currently)
     * @return true if this is a supported browser
     */
    bool CanHandle(const std::wstring& processName,
                  const std::wstring& windowTitle) override;

    /**
     * @brief Get browser context
     *
     * Execution flow:
     * 1. Try to get URL from CF_HTML in clipboard (fast, may be stale)
     * 2. Try to get URL from address bar via UI Automation (accurate, current)
     * 3. Prioritize address bar URL over CF_HTML URL
     * 4. Extract page title from window title or CF_HTML
     * 5. Record fetch time for performance monitoring
     *
     * @param source Source information (HWND, process name, window title)
     * @return BrowserContext with URL and title, or error on failure
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
     * @return L"BrowserAdapter"
     */
    std::wstring GetAdapterName() const override { return L"BrowserAdapter"; }

private:
    /**
     * @brief Get URL from clipboard CF_HTML format
     *
     * This is a fast fallback method. It parses the CF_HTML data that
     * was copied to the clipboard. The SourceURL field indicates where
     * the HTML was copied from.
     *
     * Advantages:
     * - Fast (no UI automation needed)
     * - Works even if window is not active
     *
     * Disadvantages:
     * - May be stale (user might have navigated away)
     * - Only works if HTML was copied (not plain text)
     *
     * @return SourceURL from CF_HTML, or empty if not available
     */
    std::wstring GetUrlFromClipboard();

    /**
     * @brief Get URL from browser address bar via UI Automation
     *
     * This is the primary method. It uses Windows UI Automation to
     * find the address bar control and extract its current value.
     *
     * Advantages:
     * - Accurate (reflects current page)
     * - Works for all browsers with standard address bars
     *
     * Disadvantages:
     * - Slower (50-150ms)
     * - May fail if window is minimized or address bar is hidden
     *
     * @param hwnd Window handle of the browser
     * @param processName Process name (for browser-specific logic)
     * @return Current URL from address bar, or empty if not found
     */
    std::wstring GetUrlFromAddressBar(HWND hwnd, const std::wstring& processName);

    /**
     * @brief Extract page title from window title
     *
     * Browsers typically format window titles as:
     * - Chrome/Edge: "Page Title - Google Chrome"
     * - Firefox: "Page Title - Mozilla Firefox"
     *
     * This function extracts the page title by removing the browser suffix.
     *
     * @param windowTitle Full window title
     * @param processName Process name (to identify browser type)
     * @return Page title, or full window title if pattern not recognized
     */
    std::wstring ExtractPageTitle(const std::wstring& windowTitle,
                                  const std::wstring& processName);

    /**
     * @brief Check if process name is a supported browser
     *
     * @param processName Process name in lowercase
     * @return true if supported
     */
    bool IsSupportedBrowser(const std::wstring& processName);

    /**
     * @brief Get browser-specific address bar automation ID
     *
     * Different browsers use different automation IDs for their address bars:
     * - Chrome: No specific ID, search by control type (Edit/ComboBox)
     * - Edge: No specific ID, search by control type
     * - Firefox: "urlbar-input" or similar
     *
     * @param processName Process name
     * @return Automation ID hint, or empty if none
     */
    std::wstring GetAddressBarAutomationId(const std::wstring& processName);

    int m_timeout;  // Timeout in milliseconds
};
