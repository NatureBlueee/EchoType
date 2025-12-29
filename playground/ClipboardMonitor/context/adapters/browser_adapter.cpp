#include "browser_adapter.h"
#include "../../utils.h"
#include "../../debug_log.h"
#include <windows.h>
#include <chrono>
#include <sstream>

BrowserAdapter::BrowserAdapter(int timeout)
    : m_timeout(timeout)
{
}

bool BrowserAdapter::CanHandle(const std::wstring& processName,
                               const std::wstring& windowTitle)
{
    (void)windowTitle;  // Not used currently

    std::wstring lowerProcessName = Utils::ToLower(processName);
    return IsSupportedBrowser(lowerProcessName);
}

std::shared_ptr<ContextData> BrowserAdapter::GetContext(const SourceInfo& source)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Create context object
    auto context = std::make_shared<BrowserContext>();
    context->adapterType = "browser";
    context->success = false;

    try {
        // Method 1: Try to get URL from CF_HTML in clipboard (fast, fallback)
        std::wstring sourceUrl = GetUrlFromClipboard();
        if (!sourceUrl.empty()) {
            context->sourceUrl = sourceUrl;
            DEBUG_LOG("BrowserAdapter: Got SourceURL from CF_HTML: " + Utils::WideToUtf8(sourceUrl));
        }

        // Method 2: Try to get URL from address bar via UI Automation (primary, accurate)
        std::wstring addressBarUrl = GetUrlFromAddressBar(source.windowHandle, source.processName);
        if (!addressBarUrl.empty()) {
            context->addressBarUrl = addressBarUrl;
            DEBUG_LOG("BrowserAdapter: Got URL from address bar: " + Utils::WideToUtf8(addressBarUrl));
        }

        // Prioritize address bar URL over CF_HTML URL
        if (!context->addressBarUrl.empty()) {
            context->url = context->addressBarUrl;
        } else if (!context->sourceUrl.empty()) {
            context->url = context->sourceUrl;
            DEBUG_LOG("BrowserAdapter: Using CF_HTML URL as fallback");
        }

        // Extract page title from window title
        if (!source.windowTitle.empty()) {
            context->pageTitle = ExtractPageTitle(source.windowTitle, source.processName);
            context->title = context->pageTitle;
        }

        // Check if we got any URL
        if (!context->url.empty()) {
            context->success = true;

            // Add metadata
            context->metadata[L"browser_type"] = source.processName;
            context->metadata[L"has_address_bar_url"] = context->addressBarUrl.empty() ? L"false" : L"true";
            context->metadata[L"has_source_url"] = context->sourceUrl.empty() ? L"false" : L"true";
        } else {
            context->error = L"Failed to extract URL from both CF_HTML and address bar";
            DEBUG_LOG("BrowserAdapter: Failed to get URL from any source");
        }

    } catch (const std::exception& ex) {
        context->success = false;
        context->error = L"Exception: " + Utils::Utf8ToWide(ex.what());
        DEBUG_LOG("BrowserAdapter: Exception: " + std::string(ex.what()));
    } catch (...) {
        context->success = false;
        context->error = L"Unknown exception";
        DEBUG_LOG("BrowserAdapter: Unknown exception");
    }

    // Calculate fetch time
    auto endTime = std::chrono::high_resolution_clock::now();
    context->fetchTimeMs = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
    );

    DEBUG_LOG("BrowserAdapter: Completed in " + std::to_string(context->fetchTimeMs) + "ms, success=" +
              (context->success ? "true" : "false"));

    return context;
}

std::wstring BrowserAdapter::GetUrlFromClipboard()
{
    std::wstring result;

    // Open clipboard
    if (!OpenClipboard(nullptr)) {
        DEBUG_LOG("BrowserAdapter: Failed to open clipboard for CF_HTML");
        return result;
    }

    // Register CF_HTML format
    UINT htmlFormat = RegisterClipboardFormatW(L"HTML Format");
    if (htmlFormat == 0) {
        CloseClipboard();
        DEBUG_LOG("BrowserAdapter: Failed to register CF_HTML format");
        return result;
    }

    // Check if CF_HTML is available
    if (!IsClipboardFormatAvailable(htmlFormat)) {
        CloseClipboard();
        // This is normal - not all copied content is HTML
        return result;
    }

    // Get CF_HTML data
    HANDLE hData = GetClipboardData(htmlFormat);
    if (hData == nullptr) {
        CloseClipboard();
        DEBUG_LOG("BrowserAdapter: Failed to get CF_HTML data");
        return result;
    }

    // Lock and read data
    const char* data = static_cast<const char*>(GlobalLock(hData));
    if (data != nullptr) {
        try {
            // Parse CF_HTML
            HTMLParser::HTMLClipboardData htmlData;
            if (HTMLParser::ParseCFHTML(data, htmlData)) {
                result = htmlData.sourceUrl;
            }
        } catch (...) {
            DEBUG_LOG("BrowserAdapter: Exception while parsing CF_HTML");
        }

        GlobalUnlock(hData);
    }

    CloseClipboard();
    return result;
}

std::wstring BrowserAdapter::GetUrlFromAddressBar(HWND hwnd, const std::wstring& processName)
{
    std::wstring result;

    if (hwnd == nullptr) {
        DEBUG_LOG("BrowserAdapter: Invalid HWND for UI Automation");
        return result;
    }

    try {
        // Create UI Automation helper (initializes COM in this thread)
        UIAutomationHelper uiHelper;
        if (!uiHelper.Initialize()) {
            DEBUG_LOG("BrowserAdapter: Failed to initialize UI Automation");
            return result;
        }

        // Strategy: Try multiple approaches to find address bar

        // Approach 1: Try browser-specific Automation ID
        std::wstring automationId = GetAddressBarAutomationId(processName);
        if (!automationId.empty()) {
            IUIAutomationElement* element = uiHelper.FindElementByAutomationId(hwnd, automationId);
            if (element) {
                result = uiHelper.GetElementValue(element);
                element->Release();

                if (!result.empty()) {
                    DEBUG_LOG("BrowserAdapter: Found address bar by Automation ID: " + Utils::WideToUtf8(automationId));
                    return result;
                }
            }
        }

        // Approach 2: Search for Edit control (Chrome, Edge use Edit for address bar)
        IUIAutomationElement* element = uiHelper.FindElementByControlType(hwnd, L"Edit");
        if (element) {
            result = uiHelper.GetElementValue(element);
            element->Release();

            if (!result.empty()) {
                // Validate that this looks like a URL (basic heuristic)
                if (result.find(L"://") != std::wstring::npos ||
                    result.find(L"www.") != std::wstring::npos ||
                    result.find(L"http") != std::wstring::npos) {
                    DEBUG_LOG("BrowserAdapter: Found address bar by Edit control type");
                    return result;
                }
            }
        }

        // Approach 3: Search for ComboBox (some browsers use ComboBox for address bar)
        element = uiHelper.FindElementByControlType(hwnd, L"ComboBox");
        if (element) {
            result = uiHelper.GetElementValue(element);
            element->Release();

            if (!result.empty()) {
                // Validate URL pattern
                if (result.find(L"://") != std::wstring::npos ||
                    result.find(L"www.") != std::wstring::npos ||
                    result.find(L"http") != std::wstring::npos) {
                    DEBUG_LOG("BrowserAdapter: Found address bar by ComboBox control type");
                    return result;
                }
            }
        }

        DEBUG_LOG("BrowserAdapter: Could not find address bar using any approach");

    } catch (const std::exception& ex) {
        DEBUG_LOG("BrowserAdapter: Exception in GetUrlFromAddressBar: " + std::string(ex.what()));
    } catch (...) {
        DEBUG_LOG("BrowserAdapter: Unknown exception in GetUrlFromAddressBar");
    }

    return result;
}

std::wstring BrowserAdapter::ExtractPageTitle(const std::wstring& windowTitle,
                                              const std::wstring& processName)
{
    if (windowTitle.empty()) {
        return windowTitle;
    }

    std::wstring lowerProcessName = Utils::ToLower(processName);

    // Browser-specific title patterns:
    // Most browsers use: "Page Title - Browser Name"
    std::vector<std::wstring> suffixes = {
        // Mainstream browsers
        L" - Google Chrome",
        L" - Microsoft Edge",
        L" - Mozilla Firefox",
        L" - Opera",
        L" - Brave",
        L" - Vivaldi",
        L" - Chromium",

        // AI-powered browsers
        L" - Comet",         // Perplexity
        L" - Atlas",         // ChatGPT browser
        L" - Arc",

        // Chinese browsers (English names)
        L" - 360 Secure Browser",
        L" - 360 Chrome",
        L" - QQ Browser",
        L" - Sogou Browser",
        L" - Liebao Browser",
        L" - 2345 Browser",
        L" - Maxthon",

        // Generic patterns
        L" - Browser",
        L" - Web Browser"
    };

    // Try to remove browser suffix
    for (const auto& suffix : suffixes) {
        size_t pos = windowTitle.rfind(suffix);
        if (pos != std::wstring::npos) {
            // Found suffix, extract title before it
            return windowTitle.substr(0, pos);
        }
    }

    // If no pattern matched, return original title
    return windowTitle;
}

bool BrowserAdapter::IsSupportedBrowser(const std::wstring& processName)
{
    // List of explicitly supported browsers (lowercase)
    std::vector<std::wstring> supportedBrowsers = {
        // Mainstream browsers
        L"chrome.exe",
        L"msedge.exe",
        L"firefox.exe",
        L"opera.exe",
        L"brave.exe",
        L"vivaldi.exe",
        L"chromium.exe",
        L"iexplore.exe",     // Internet Explorer (legacy)

        // AI-powered browsers
        L"comet.exe",        // Perplexity browser
        L"atlas.exe",        // ChatGPT browser
        L"arc.exe",          // Arc browser

        // Chinese browsers (Chromium-based)
        L"360se.exe",        // 360 Secure Browser
        L"360chrome.exe",    // 360 Chrome
        L"qqbrowser.exe",    // QQ Browser
        L"sogouexplorer.exe", // Sogou Browser
        L"liebao.exe",       // Liebao Browser
        L"2345explorer.exe", // 2345 Browser
        L"maxthon.exe",      // Maxthon Browser

        // Developer browsers
        L"electron.exe",     // Electron-based apps
        L"browser.exe",      // Generic browser name
        L"webbrowser.exe"    // Generic web browser
    };

    for (const auto& browser : supportedBrowsers) {
        if (processName == browser) {
            return true;
        }
    }

    // Additional heuristic: Check if the process name contains "browser", "chrome", or "web"
    // This helps support new Chromium-based browsers automatically
    if (processName.find(L"browser") != std::wstring::npos ||
        processName.find(L"chrome") != std::wstring::npos ||
        processName.find(L"web") != std::wstring::npos ||
        processName.find(L"edge") != std::wstring::npos) {
        DEBUG_LOG("BrowserAdapter: Heuristic match for potential browser: " + Utils::WideToUtf8(processName));
        return true;
    }

    return false;
}

std::wstring BrowserAdapter::GetAddressBarAutomationId(const std::wstring& processName)
{
    std::wstring lowerProcessName = Utils::ToLower(processName);

    // Browser-specific automation IDs (these may vary by version)
    if (lowerProcessName == L"firefox.exe") {
        return L"urlbar-input";  // Firefox uses this ID
    }

    // Chrome and Edge don't use consistent automation IDs for address bar
    // We'll rely on control type searching instead
    return L"";
}
