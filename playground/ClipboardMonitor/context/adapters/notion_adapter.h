#pragma once

#include "../context_adapter.h"
#include "../utils/ui_automation_helper.h"
#include <memory>

/**
 * @brief Notion Context Adapter
 *
 * Captures page context when copying from Notion.
 * Supports: Notion desktop app (Notion.exe) and web-based Notion in browsers
 *
 * Features:
 * - Extract current page title
 * - Get breadcrumb navigation (page hierarchy)
 * - Determine page type (page/database/board/etc.)
 * - Construct pseudo URL for page reference
 * - Extract workspace name if available
 *
 * Window title format:
 * - "Page Title - Notion"
 * - "Database Name - Notion"
 */
class NotionAdapter : public IContextAdapter {
public:
    /**
     * @brief Constructor
     *
     * @param timeout Timeout in milliseconds (default: 150ms)
     */
    explicit NotionAdapter(int timeout = 150);

    /**
     * @brief Destructor
     */
    ~NotionAdapter() override = default;

    /**
     * @brief Check if this adapter can handle the given process
     *
     * Supports:
     * - Notion.exe (Notion desktop app)
     *
     * Note: For web-based Notion, BrowserAdapter will handle it
     *
     * @param processName Process executable name
     * @param windowTitle Window title (used to distinguish Notion from browser)
     * @return true if this is Notion app
     */
    bool CanHandle(const std::wstring& processName,
                  const std::wstring& windowTitle) override;

    /**
     * @brief Get Notion context
     *
     * Execution flow:
     * 1. Parse window title to extract page title
     * 2. Try to get breadcrumb navigation via UI Automation
     * 3. Determine page type (heuristic based on UI elements)
     * 4. Construct pseudo URL (notion://Workspace/Page/Subpage)
     *
     * @param source Source information (HWND, process name, window title)
     * @return NotionContext with page path, breadcrumbs, etc.
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
     * @return L"NotionAdapter"
     */
    std::wstring GetAdapterName() const override { return L"NotionAdapter"; }

private:
    /**
     * @brief Parse window title to extract page title
     *
     * Window title format: "Page Title - Notion"
     *
     * @param windowTitle Window title
     * @return Page title (without " - Notion" suffix)
     */
    std::wstring ParsePageTitle(const std::wstring& windowTitle);

    /**
     * @brief Get breadcrumb navigation via UI Automation
     *
     * Notion displays breadcrumbs showing the page hierarchy:
     * Workspace > Parent Page > Current Page
     *
     * Strategy:
     * - Look for Hyperlink or Button elements near the top
     * - These typically represent breadcrumb items
     * - Collect them in order to build page path
     *
     * @param hwnd Notion window handle
     * @param uiHelper UI Automation helper
     * @return Vector of breadcrumb items (from workspace to current page)
     */
    std::vector<std::wstring> GetBreadcrumbs(HWND hwnd, UIAutomationHelper& uiHelper);

    /**
     * @brief Determine page type
     *
     * Heuristic approach:
     * - Check if window title or UI elements contain "Database", "Table", "Board", etc.
     * - Default to "page" if cannot determine
     *
     * @param windowTitle Window title
     * @param uiHelper UI Automation helper
     * @param hwnd Window handle
     * @return Page type ("page", "database", "board", "table", etc.)
     */
    std::string DeterminePageType(const std::wstring& windowTitle,
                                  UIAutomationHelper& uiHelper,
                                  HWND hwnd);

    /**
     * @brief Construct pseudo URL from breadcrumbs
     *
     * Format: notion://Workspace/ParentPage/CurrentPage
     *
     * @param breadcrumbs Breadcrumb items
     * @param pageTitle Current page title
     * @return Pseudo URL
     */
    std::wstring ConstructPseudoUrl(const std::vector<std::wstring>& breadcrumbs,
                                   const std::wstring& pageTitle);

    int m_timeout;  // Timeout in milliseconds
};
