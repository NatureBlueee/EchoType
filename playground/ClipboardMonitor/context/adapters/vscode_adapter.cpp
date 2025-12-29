#include "vscode_adapter.h"
#include "../../utils.h"
#include "../../debug_log.h"
#include <windows.h>
#include <chrono>
#include <sstream>
#include <regex>

// Language mapping: extension -> language name
const std::map<std::wstring, std::string> VSCodeAdapter::s_languageMap = {
    // Programming languages
    {L"cpp", "C++"},
    {L"cc", "C++"},
    {L"cxx", "C++"},
    {L"h", "C/C++ Header"},
    {L"hpp", "C++ Header"},
    {L"c", "C"},
    {L"py", "Python"},
    {L"js", "JavaScript"},
    {L"ts", "TypeScript"},
    {L"jsx", "React JSX"},
    {L"tsx", "React TSX"},
    {L"java", "Java"},
    {L"cs", "C#"},
    {L"go", "Go"},
    {L"rs", "Rust"},
    {L"php", "PHP"},
    {L"rb", "Ruby"},
    {L"swift", "Swift"},
    {L"kt", "Kotlin"},
    {L"scala", "Scala"},

    // Web
    {L"html", "HTML"},
    {L"htm", "HTML"},
    {L"css", "CSS"},
    {L"scss", "SCSS"},
    {L"sass", "Sass"},
    {L"less", "Less"},
    {L"vue", "Vue"},
    {L"svelte", "Svelte"},

    // Data & Config
    {L"json", "JSON"},
    {L"xml", "XML"},
    {L"yaml", "YAML"},
    {L"yml", "YAML"},
    {L"toml", "TOML"},
    {L"md", "Markdown"},
    {L"txt", "Plain Text"},

    // Shell & Scripts
    {L"sh", "Shell"},
    {L"bash", "Bash"},
    {L"ps1", "PowerShell"},
    {L"bat", "Batch"},
    {L"cmd", "Batch"},
};

VSCodeAdapter::VSCodeAdapter(int timeout)
    : m_timeout(timeout)
{
}

bool VSCodeAdapter::CanHandle(const std::wstring& processName,
                              const std::wstring& windowTitle)
{
    (void)windowTitle;  // Not used currently

    std::wstring lowerProcessName = Utils::ToLower(processName);

    // Support multiple VS Code-based editors
    return lowerProcessName == L"code.exe" ||
           lowerProcessName == L"cursor.exe" ||
           lowerProcessName == L"code-insiders.exe" ||
           lowerProcessName == L"vscodium.exe";
}

std::shared_ptr<ContextData> VSCodeAdapter::GetContext(const SourceInfo& source)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Create context object
    auto context = std::make_shared<VSCodeContext>();
    context->adapterType = "vscode";
    context->success = false;

    try {
        // Parse window title
        std::wstring fileName, projectName;
        bool isModified = false;
        ParseWindowTitle(source.windowTitle, fileName, projectName, isModified);

        if (!fileName.empty()) {
            context->fileName = fileName;
            context->title = fileName;
            DEBUG_LOG("VSCodeAdapter: Got file name: " + Utils::WideToUtf8(fileName));
        }

        if (!projectName.empty()) {
            context->projectName = projectName;
            DEBUG_LOG("VSCodeAdapter: Got project name: " + Utils::WideToUtf8(projectName));
        }

        context->isModified = isModified;

        // Infer language from file extension
        if (!fileName.empty()) {
            context->language = InferLanguage(fileName);
            if (!context->language.empty()) {
                DEBUG_LOG("VSCodeAdapter: Inferred language: " + context->language);
            }
        }

        // Try to get file path and cursor position from status bar via UI Automation
        UIAutomationHelper uiHelper;
        if (uiHelper.Initialize()) {
            // Get file path
            std::wstring filePath = GetFilePathFromStatusBar(source.windowHandle, uiHelper);
            if (!filePath.empty()) {
                context->filePath = filePath;
                context->url = L"vscode://file/" + filePath;  // Construct pseudo URL
                DEBUG_LOG("VSCodeAdapter: Got file path: " + Utils::WideToUtf8(filePath));
            }

            // Get cursor position
            int lineNumber = 0, columnNumber = 0;
            GetCursorPosition(source.windowHandle, uiHelper, lineNumber, columnNumber);
            if (lineNumber > 0) {
                context->lineNumber = lineNumber;
                context->columnNumber = columnNumber;
                DEBUG_LOG("VSCodeAdapter: Cursor at Ln " + std::to_string(lineNumber) +
                         ", Col " + std::to_string(columnNumber));
            }
        } else {
            DEBUG_LOG("VSCodeAdapter: Failed to initialize UI Automation");
        }

        // Mark as successful if we got at least file name
        if (!context->fileName.empty()) {
            context->success = true;

            // Add metadata
            context->metadata[L"editor"] = Utils::Utf8ToWide(Utils::WideToUtf8(source.processName));
            context->metadata[L"is_modified"] = isModified ? L"true" : L"false";
            if (!context->language.empty()) {
                context->metadata[L"language"] = Utils::Utf8ToWide(context->language);
            }
        } else {
            context->error = L"Failed to extract file information from window title";
            DEBUG_LOG("VSCodeAdapter: Failed to get file name from window title");
        }

    } catch (const std::exception& ex) {
        context->success = false;
        context->error = L"Exception: " + Utils::Utf8ToWide(ex.what());
        DEBUG_LOG("VSCodeAdapter: Exception: " + std::string(ex.what()));
    } catch (...) {
        context->success = false;
        context->error = L"Unknown exception";
        DEBUG_LOG("VSCodeAdapter: Unknown exception");
    }

    // Calculate fetch time
    auto endTime = std::chrono::high_resolution_clock::now();
    context->fetchTimeMs = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
    );

    DEBUG_LOG("VSCodeAdapter: Completed in " + std::to_string(context->fetchTimeMs) + "ms, success=" +
              (context->success ? "true" : "false"));

    return context;
}

void VSCodeAdapter::ParseWindowTitle(const std::wstring& windowTitle,
                                    std::wstring& fileName,
                                    std::wstring& projectName,
                                    bool& isModified)
{
    // Window title format:
    // "● filename.ext - Project Name - Visual Studio Code" (modified)
    // "filename.ext - Project Name - Visual Studio Code" (saved)
    // "● filename.ext - Visual Studio Code" (no project)

    fileName.clear();
    projectName.clear();
    isModified = false;

    if (windowTitle.empty()) {
        return;
    }

    std::wstring title = windowTitle;

    // Check for modification indicator (bullet character U+25CF)
    if (title.length() > 0 && title[0] == L'\u25CF') {
        isModified = true;
        // Remove bullet and space prefix
        if (title.length() > 2 && title[1] == L' ') {
            title = title.substr(2);
        } else {
            title = title.substr(1);
        }
    }

    // Find " - Visual Studio Code" or " - Cursor" suffix
    std::vector<std::wstring> suffixes = {
        L" - Visual Studio Code",
        L" - Cursor",
        L" - VSCodium",
        L" - Code - Insiders"
    };

    size_t suffixPos = std::wstring::npos;
    for (const auto& suffix : suffixes) {
        suffixPos = title.rfind(suffix);
        if (suffixPos != std::wstring::npos) {
            title = title.substr(0, suffixPos);
            break;
        }
    }

    // Now title is like: "filename.ext - Project Name" or "filename.ext"
    size_t dashPos = title.find(L" - ");
    if (dashPos != std::wstring::npos) {
        fileName = title.substr(0, dashPos);
        projectName = title.substr(dashPos + 3);  // Skip " - "
    } else {
        fileName = title;
    }
}

std::wstring VSCodeAdapter::GetFilePathFromStatusBar(HWND hwnd, UIAutomationHelper& uiHelper)
{
    if (hwnd == nullptr) {
        return L"";
    }

    try {
        // VS Code's status bar typically contains file path information
        // Strategy: Look for Text elements that might contain a file path

        IUIAutomation* automation = uiHelper.GetAutomation();
        if (!automation) {
            return L"";
        }

        IUIAutomationElement* rootElement = nullptr;
        HRESULT hr = automation->ElementFromHandle(hwnd, &rootElement);
        if (FAILED(hr) || !rootElement) {
            return L"";
        }

        // Look for Text elements
        VARIANT var;
        var.vt = VT_I4;
        var.lVal = UIA_TextControlTypeId;

        IUIAutomationCondition* condition = nullptr;
        hr = automation->CreatePropertyCondition(UIA_ControlTypePropertyId, var, &condition);

        if (SUCCEEDED(hr) && condition) {
            IUIAutomationElementArray* foundElements = nullptr;
            hr = rootElement->FindAll(TreeScope_Descendants, condition, &foundElements);

            if (SUCCEEDED(hr) && foundElements) {
                int length = 0;
                foundElements->get_Length(&length);

                // Look for elements containing file paths (with : or / or \)
                for (int i = 0; i < length; i++) {
                    IUIAutomationElement* element = nullptr;
                    hr = foundElements->GetElement(i, &element);

                    if (SUCCEEDED(hr) && element) {
                        std::wstring text = uiHelper.GetElementText(element);
                        element->Release();

                        // Check if text looks like a file path
                        if (text.find(L":\\") != std::wstring::npos ||  // Windows absolute path
                            text.find(L"/") != std::wstring::npos ||   // Unix path
                            text.find(L"\\") != std::wstring::npos) {  // Windows path

                            // Filter out very long text (probably not a path)
                            if (text.length() < 300) {
                                foundElements->Release();
                                condition->Release();
                                rootElement->Release();
                                return text;
                            }
                        }
                    }
                }

                foundElements->Release();
            }
            condition->Release();
        }

        rootElement->Release();

    } catch (...) {
        DEBUG_LOG("VSCodeAdapter: Exception in GetFilePathFromStatusBar");
    }

    return L"";
}

void VSCodeAdapter::GetCursorPosition(HWND hwnd,
                                     UIAutomationHelper& uiHelper,
                                     int& lineNumber,
                                     int& columnNumber)
{
    lineNumber = 0;
    columnNumber = 0;

    if (hwnd == nullptr) {
        return;
    }

    try {
        // VS Code status bar shows cursor position as "Ln X, Col Y"
        // Look for Text elements containing this pattern

        IUIAutomation* automation = uiHelper.GetAutomation();
        if (!automation) {
            return;
        }

        IUIAutomationElement* rootElement = nullptr;
        HRESULT hr = automation->ElementFromHandle(hwnd, &rootElement);
        if (FAILED(hr) || !rootElement) {
            return;
        }

        // Look for Text elements
        VARIANT var;
        var.vt = VT_I4;
        var.lVal = UIA_TextControlTypeId;

        IUIAutomationCondition* condition = nullptr;
        hr = automation->CreatePropertyCondition(UIA_ControlTypePropertyId, var, &condition);

        if (SUCCEEDED(hr) && condition) {
            IUIAutomationElementArray* foundElements = nullptr;
            hr = rootElement->FindAll(TreeScope_Descendants, condition, &foundElements);

            if (SUCCEEDED(hr) && foundElements) {
                int length = 0;
                foundElements->get_Length(&length);

                // Look for "Ln X, Col Y" pattern
                for (int i = 0; i < length; i++) {
                    IUIAutomationElement* element = nullptr;
                    hr = foundElements->GetElement(i, &element);

                    if (SUCCEEDED(hr) && element) {
                        std::wstring text = uiHelper.GetElementText(element);
                        element->Release();

                        // Check if text matches "Ln X, Col Y" pattern
                        if (text.find(L"Ln ") != std::wstring::npos &&
                            text.find(L"Col ") != std::wstring::npos) {

                            // Parse line and column numbers
                            size_t lnPos = text.find(L"Ln ");
                            size_t colPos = text.find(L"Col ");

                            if (lnPos != std::wstring::npos && colPos != std::wstring::npos) {
                                try {
                                    // Extract line number
                                    size_t lnStart = lnPos + 3;  // Skip "Ln "
                                    size_t lnEnd = text.find(L",", lnStart);
                                    if (lnEnd != std::wstring::npos) {
                                        std::wstring lnStr = text.substr(lnStart, lnEnd - lnStart);
                                        lineNumber = std::stoi(lnStr);
                                    }

                                    // Extract column number
                                    size_t colStart = colPos + 4;  // Skip "Col "
                                    size_t colEnd = text.find_first_not_of(L"0123456789", colStart);
                                    std::wstring colStr = text.substr(colStart, colEnd - colStart);
                                    columnNumber = std::stoi(colStr);

                                } catch (...) {
                                    // Ignore parsing errors
                                }

                                foundElements->Release();
                                condition->Release();
                                rootElement->Release();
                                return;
                            }
                        }
                    }
                }

                foundElements->Release();
            }
            condition->Release();
        }

        rootElement->Release();

    } catch (...) {
        DEBUG_LOG("VSCodeAdapter: Exception in GetCursorPosition");
    }
}

std::string VSCodeAdapter::InferLanguage(const std::wstring& fileName)
{
    std::wstring ext = GetFileExtension(fileName);
    if (ext.empty()) {
        return "";
    }

    // Convert to lowercase for case-insensitive lookup
    std::wstring lowerExt = Utils::ToLower(ext);

    auto it = s_languageMap.find(lowerExt);
    if (it != s_languageMap.end()) {
        return it->second;
    }

    return "";
}

std::wstring VSCodeAdapter::GetFileExtension(const std::wstring& fileName)
{
    size_t dotPos = fileName.rfind(L'.');
    if (dotPos == std::wstring::npos || dotPos == fileName.length() - 1) {
        return L"";
    }

    return fileName.substr(dotPos + 1);
}
