#include "notion_adapter.h"
#include "../../utils.h"
#include "../../debug_log.h"
#include <windows.h>
#include <chrono>
#include <sstream>

NotionAdapter::NotionAdapter(int timeout)
    : m_timeout(timeout)
{
}

bool NotionAdapter::CanHandle(const std::wstring& processName,
                              const std::wstring& windowTitle)
{
    (void)windowTitle;  // Not used currently

    std::wstring lowerProcessName = Utils::ToLower(processName);

    // Support Notion desktop app
    return lowerProcessName == L"notion.exe";
}

std::shared_ptr<ContextData> NotionAdapter::GetContext(const SourceInfo& source)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Create context object
    auto context = std::make_shared<NotionContext>();
    context->adapterType = "notion";
    context->success = false;

    try {
        // Parse page title from window title
        std::wstring pageTitle = ParsePageTitle(source.windowTitle);
        if (!pageTitle.empty()) {
            context->title = pageTitle;
            DEBUG_LOG("NotionAdapter: Got page title: " + Utils::WideToUtf8(pageTitle));
        }

        // Initialize UI Automation
        UIAutomationHelper uiHelper;
        if (uiHelper.Initialize()) {
            // Get breadcrumbs
            std::vector<std::wstring> breadcrumbs = GetBreadcrumbs(source.windowHandle, uiHelper);
            if (!breadcrumbs.empty()) {
                context->breadcrumbs = breadcrumbs;
                DEBUG_LOG("NotionAdapter: Got " + std::to_string(breadcrumbs.size()) + " breadcrumb(s)");

                // Extract workspace from first breadcrumb if available
                if (!breadcrumbs.empty()) {
                    context->workspace = breadcrumbs[0];
                }
            }

            // Determine page type
            std::string pageType = DeterminePageType(source.windowTitle, uiHelper, source.windowHandle);
            if (!pageType.empty()) {
                context->pageType = Utils::Utf8ToWide(pageType);
                DEBUG_LOG("NotionAdapter: Page type: " + pageType);
            }

            // Construct pseudo URL
            std::wstring pseudoUrl = ConstructPseudoUrl(breadcrumbs, pageTitle);
            if (!pseudoUrl.empty()) {
                context->url = pseudoUrl;
                context->pagePath = pseudoUrl;
                DEBUG_LOG("NotionAdapter: Constructed URL: " + Utils::WideToUtf8(pseudoUrl));
            }

        } else {
            DEBUG_LOG("NotionAdapter: Failed to initialize UI Automation");
        }

        // Mark as successful if we got at least page title
        if (!context->title.empty()) {
            context->success = true;

            // Add metadata
            context->metadata[L"app"] = L"Notion";
            if (!context->pageType.empty()) {
                context->metadata[L"page_type"] = context->pageType;
            }
        } else {
            context->error = L"Failed to extract page information from window title";
            DEBUG_LOG("NotionAdapter: Failed to get page title from window title");
        }

    } catch (const std::exception& ex) {
        context->success = false;
        context->error = L"Exception: " + Utils::Utf8ToWide(ex.what());
        DEBUG_LOG("NotionAdapter: Exception: " + std::string(ex.what()));
    } catch (...) {
        context->success = false;
        context->error = L"Unknown exception";
        DEBUG_LOG("NotionAdapter: Unknown exception");
    }

    // Calculate fetch time
    auto endTime = std::chrono::high_resolution_clock::now();
    context->fetchTimeMs = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
    );

    DEBUG_LOG("NotionAdapter: Completed in " + std::to_string(context->fetchTimeMs) + "ms, success=" +
              (context->success ? "true" : "false"));

    return context;
}

std::wstring NotionAdapter::ParsePageTitle(const std::wstring& windowTitle)
{
    // Window title format: "Page Title - Notion"
    // Also handles: "Database Name - Notion"

    if (windowTitle.empty()) {
        return L"";
    }

    std::wstring title = windowTitle;

    // Find " - Notion" suffix
    size_t suffixPos = title.rfind(L" - Notion");
    if (suffixPos != std::wstring::npos) {
        title = title.substr(0, suffixPos);
    }

    // Trim whitespace
    size_t start = title.find_first_not_of(L" \t\r\n");
    size_t end = title.find_last_not_of(L" \t\r\n");

    if (start != std::wstring::npos && end != std::wstring::npos) {
        return title.substr(start, end - start + 1);
    }

    return title;
}

std::vector<std::wstring> NotionAdapter::GetBreadcrumbs(HWND hwnd, UIAutomationHelper& uiHelper)
{
    std::vector<std::wstring> breadcrumbs;

    if (hwnd == nullptr) {
        return breadcrumbs;
    }

    try {
        // Notion displays breadcrumbs as Hyperlink or Button elements near the top
        // Strategy: Find all Hyperlink and Button elements, filter by position

        IUIAutomation* automation = uiHelper.GetAutomation();
        if (!automation) {
            return breadcrumbs;
        }

        IUIAutomationElement* rootElement = nullptr;
        HRESULT hr = automation->ElementFromHandle(hwnd, &rootElement);
        if (FAILED(hr) || !rootElement) {
            return breadcrumbs;
        }

        // Look for Hyperlink elements (common for breadcrumbs)
        VARIANT var;
        var.vt = VT_I4;
        var.lVal = UIA_HyperlinkControlTypeId;

        IUIAutomationCondition* condition = nullptr;
        hr = automation->CreatePropertyCondition(UIA_ControlTypePropertyId, var, &condition);

        if (SUCCEEDED(hr) && condition) {
            IUIAutomationElementArray* foundElements = nullptr;
            hr = rootElement->FindAll(TreeScope_Descendants, condition, &foundElements);

            if (SUCCEEDED(hr) && foundElements) {
                int length = 0;
                foundElements->get_Length(&length);

                // Collect hyperlinks (likely breadcrumbs)
                for (int i = 0; i < length && breadcrumbs.size() < 10; i++) {
                    IUIAutomationElement* element = nullptr;
                    hr = foundElements->GetElement(i, &element);

                    if (SUCCEEDED(hr) && element) {
                        std::wstring text = uiHelper.GetElementText(element);
                        element->Release();

                        // Filter out empty or very long text (probably not a breadcrumb)
                        if (!text.empty() && text.length() < 100) {
                            // Filter out common UI elements
                            if (text != L"Back" && text != L"Forward" &&
                                text != L"Share" && text != L"Updates" &&
                                text.find(L"http") == std::wstring::npos) {
                                breadcrumbs.push_back(text);
                            }
                        }
                    }
                }

                foundElements->Release();
            }
            condition->Release();
        }

        // Also try Button elements if we didn't find enough breadcrumbs
        if (breadcrumbs.empty()) {
            var.lVal = UIA_ButtonControlTypeId;
            hr = automation->CreatePropertyCondition(UIA_ControlTypePropertyId, var, &condition);

            if (SUCCEEDED(hr) && condition) {
                IUIAutomationElementArray* foundElements = nullptr;
                hr = rootElement->FindAll(TreeScope_Descendants, condition, &foundElements);

                if (SUCCEEDED(hr) && foundElements) {
                    int length = 0;
                    foundElements->get_Length(&length);

                    for (int i = 0; i < length && breadcrumbs.size() < 10; i++) {
                        IUIAutomationElement* element = nullptr;
                        hr = foundElements->GetElement(i, &element);

                        if (SUCCEEDED(hr) && element) {
                            std::wstring text = uiHelper.GetElementText(element);
                            element->Release();

                            if (!text.empty() && text.length() < 100) {
                                // Look for breadcrumb-like buttons
                                if (text.find(L">") != std::wstring::npos ||
                                    text.find(L"/") != std::wstring::npos) {
                                    breadcrumbs.push_back(text);
                                }
                            }
                        }
                    }

                    foundElements->Release();
                }
                condition->Release();
            }
        }

        rootElement->Release();

    } catch (...) {
        DEBUG_LOG("NotionAdapter: Exception in GetBreadcrumbs");
    }

    return breadcrumbs;
}

std::string NotionAdapter::DeterminePageType(const std::wstring& windowTitle,
                                             UIAutomationHelper& uiHelper,
                                             HWND hwnd)
{
    (void)uiHelper;  // Not used currently
    (void)hwnd;      // Not used currently

    // Heuristic approach: check window title for keywords
    std::wstring lowerTitle = Utils::ToLower(windowTitle);

    if (lowerTitle.find(L"database") != std::wstring::npos) {
        return "database";
    }
    if (lowerTitle.find(L"table") != std::wstring::npos) {
        return "table";
    }
    if (lowerTitle.find(L"board") != std::wstring::npos) {
        return "board";
    }
    if (lowerTitle.find(L"calendar") != std::wstring::npos) {
        return "calendar";
    }
    if (lowerTitle.find(L"timeline") != std::wstring::npos) {
        return "timeline";
    }
    if (lowerTitle.find(L"gallery") != std::wstring::npos) {
        return "gallery";
    }
    if (lowerTitle.find(L"list") != std::wstring::npos) {
        return "list";
    }

    // Default to "page"
    return "page";
}

std::wstring NotionAdapter::ConstructPseudoUrl(const std::vector<std::wstring>& breadcrumbs,
                                               const std::wstring& pageTitle)
{
    std::wostringstream url;
    url << L"notion://";

    // Add breadcrumbs (workspace > parent pages)
    for (const auto& crumb : breadcrumbs) {
        url << crumb << L"/";
    }

    // Add current page title
    if (!pageTitle.empty()) {
        url << pageTitle;
    } else if (!breadcrumbs.empty()) {
        // If no page title, use last breadcrumb
        // Remove trailing slash
        std::wstring result = url.str();
        if (!result.empty() && result.back() == L'/') {
            result.pop_back();
        }
        return result;
    }

    return url.str();
}
