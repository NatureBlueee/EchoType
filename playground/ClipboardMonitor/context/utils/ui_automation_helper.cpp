#include "ui_automation_helper.h"
#include "../../debug_log.h"
#include <algorithm>
#include <cctype>

// Link UI Automation library
#pragma comment(lib, "uiautomationcore.lib")

UIAutomationHelper::UIAutomationHelper()
    : m_automation(nullptr)
    , m_comInitialized(false)
{
}

UIAutomationHelper::~UIAutomationHelper() {
    if (m_automation) {
        m_automation->Release();
        m_automation = nullptr;
    }

    if (m_comInitialized) {
        CoUninitialize();
    }
}

bool UIAutomationHelper::Initialize() {
    if (m_automation) {
        return true;  // Already initialized
    }

    // Initialize COM (apartment-threaded for UI Automation)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        m_comInitialized = true;
    } else if (hr == RPC_E_CHANGED_MODE) {
        // COM already initialized in a different mode, continue anyway
        m_comInitialized = false;
    } else {
        DEBUG_LOG("UIAutomationHelper: CoInitializeEx failed");
        return false;
    }

    // Create UI Automation instance
    hr = CoCreateInstance(
        CLSID_CUIAutomation,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IUIAutomation,
        reinterpret_cast<void**>(&m_automation)
    );

    if (FAILED(hr) || !m_automation) {
        DEBUG_LOG("UIAutomationHelper: CoCreateInstance failed");
        if (m_comInitialized) {
            CoUninitialize();
            m_comInitialized = false;
        }
        return false;
    }

    DEBUG_LOG("UIAutomationHelper: Initialized successfully");
    return true;
}

IUIAutomationElement* UIAutomationHelper::FindElementByControlType(
    HWND hwnd,
    const std::wstring& controlTypeName,
    const std::wstring& namePart)
{
    if (!m_automation || !hwnd) {
        return nullptr;
    }

    // Get root element from window handle
    IUIAutomationElement* root = nullptr;
    HRESULT hr = m_automation->ElementFromHandle(hwnd, &root);
    if (FAILED(hr) || !root) {
        return nullptr;
    }

    // Get control type ID
    CONTROLTYPEID controlTypeId = GetControlTypeId(controlTypeName);
    if (controlTypeId == 0) {
        root->Release();
        return nullptr;
    }

    // Create condition for control type
    VARIANT varType;
    varType.vt = VT_I4;
    varType.lVal = controlTypeId;

    IUIAutomationCondition* condition = nullptr;
    hr = m_automation->CreatePropertyCondition(UIA_ControlTypePropertyId, varType, &condition);
    if (FAILED(hr) || !condition) {
        root->Release();
        return nullptr;
    }

    // Search for matching elements
    IUIAutomationElement* result = nullptr;

    if (namePart.empty()) {
        // No name filter - return first match
        hr = root->FindFirst(TreeScope_Descendants, condition, &result);
    } else {
        // Search all matching control types and filter by name
        IUIAutomationElementArray* elements = nullptr;
        hr = root->FindAll(TreeScope_Descendants, condition, &elements);

        if (SUCCEEDED(hr) && elements) {
            int count = 0;
            elements->get_Length(&count);

            std::wstring lowerNamePart = namePart;
            std::transform(lowerNamePart.begin(), lowerNamePart.end(),
                         lowerNamePart.begin(), ::towlower);

            for (int i = 0; i < count; i++) {
                IUIAutomationElement* elem = nullptr;
                if (SUCCEEDED(elements->GetElement(i, &elem)) && elem) {
                    BSTR name = nullptr;
                    if (SUCCEEDED(elem->get_CurrentName(&name)) && name) {
                        std::wstring elemName = BstrToWstring(name);
                        std::transform(elemName.begin(), elemName.end(),
                                     elemName.begin(), ::towlower);

                        if (elemName.find(lowerNamePart) != std::wstring::npos) {
                            result = elem;
                            break;
                        }
                    }
                    if (!result) {
                        elem->Release();
                    }
                }
            }
            elements->Release();
        }
    }

    condition->Release();
    root->Release();

    return result;
}

IUIAutomationElement* UIAutomationHelper::FindElementByName(
    HWND hwnd,
    const std::wstring& namePart)
{
    if (!m_automation || !hwnd || namePart.empty()) {
        return nullptr;
    }

    // Get root element from window handle
    IUIAutomationElement* root = nullptr;
    HRESULT hr = m_automation->ElementFromHandle(hwnd, &root);
    if (FAILED(hr) || !root) {
        return nullptr;
    }

    // Create a "true" condition (matches all elements)
    IUIAutomationCondition* trueCondition = nullptr;
    hr = m_automation->CreateTrueCondition(&trueCondition);
    if (FAILED(hr) || !trueCondition) {
        root->Release();
        return nullptr;
    }

    // Find all elements and filter by name
    IUIAutomationElementArray* elements = nullptr;
    hr = root->FindAll(TreeScope_Descendants, trueCondition, &elements);

    IUIAutomationElement* result = nullptr;

    if (SUCCEEDED(hr) && elements) {
        int count = 0;
        elements->get_Length(&count);

        std::wstring lowerNamePart = namePart;
        std::transform(lowerNamePart.begin(), lowerNamePart.end(),
                     lowerNamePart.begin(), ::towlower);

        for (int i = 0; i < count; i++) {
            IUIAutomationElement* elem = nullptr;
            if (SUCCEEDED(elements->GetElement(i, &elem)) && elem) {
                BSTR name = nullptr;
                if (SUCCEEDED(elem->get_CurrentName(&name)) && name) {
                    std::wstring elemName = BstrToWstring(name);
                    std::transform(elemName.begin(), elemName.end(),
                                 elemName.begin(), ::towlower);

                    if (elemName.find(lowerNamePart) != std::wstring::npos) {
                        result = elem;
                        break;
                    }
                }
                if (!result) {
                    elem->Release();
                }
            }
        }
        elements->Release();
    }

    trueCondition->Release();
    root->Release();

    return result;
}

IUIAutomationElement* UIAutomationHelper::FindElementByAutomationId(
    HWND hwnd,
    const std::wstring& automationId)
{
    if (!m_automation || !hwnd || automationId.empty()) {
        return nullptr;
    }

    // Get root element from window handle
    IUIAutomationElement* root = nullptr;
    HRESULT hr = m_automation->ElementFromHandle(hwnd, &root);
    if (FAILED(hr) || !root) {
        return nullptr;
    }

    // Create condition for automation ID
    VARIANT varId;
    varId.vt = VT_BSTR;
    varId.bstrVal = SysAllocString(automationId.c_str());

    IUIAutomationCondition* condition = nullptr;
    hr = m_automation->CreatePropertyCondition(UIA_AutomationIdPropertyId, varId, &condition);

    SysFreeString(varId.bstrVal);

    IUIAutomationElement* result = nullptr;
    if (SUCCEEDED(hr) && condition) {
        hr = root->FindFirst(TreeScope_Descendants, condition, &result);
        condition->Release();
    }

    root->Release();
    return result;
}

std::wstring UIAutomationHelper::GetElementValue(IUIAutomationElement* element) {
    if (!element) {
        return L"";
    }

    // Try to get ValuePattern
    IUIAutomationValuePattern* valuePattern = nullptr;
    HRESULT hr = element->GetCurrentPatternAs(UIA_ValuePatternId,
                                              IID_IUIAutomationValuePattern,
                                              reinterpret_cast<void**>(&valuePattern));

    if (SUCCEEDED(hr) && valuePattern) {
        BSTR value = nullptr;
        if (SUCCEEDED(valuePattern->get_CurrentValue(&value)) && value) {
            std::wstring result = BstrToWstring(value);
            valuePattern->Release();
            return result;
        }
        valuePattern->Release();
    }

    // Fallback: try to get Name property
    return GetElementText(element);
}

std::wstring UIAutomationHelper::GetElementText(IUIAutomationElement* element) {
    if (!element) {
        return L"";
    }

    BSTR name = nullptr;
    HRESULT hr = element->get_CurrentName(&name);

    if (SUCCEEDED(hr) && name) {
        return BstrToWstring(name);
    }

    return L"";
}

CONTROLTYPEID UIAutomationHelper::GetControlTypeId(const std::wstring& typeName) {
    // Map common control type names to IDs
    std::wstring lower = typeName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    if (lower == L"button") return UIA_ButtonControlTypeId;
    if (lower == L"calendar") return UIA_CalendarControlTypeId;
    if (lower == L"checkbox") return UIA_CheckBoxControlTypeId;
    if (lower == L"combobox") return UIA_ComboBoxControlTypeId;
    if (lower == L"edit") return UIA_EditControlTypeId;
    if (lower == L"hyperlink") return UIA_HyperlinkControlTypeId;
    if (lower == L"image") return UIA_ImageControlTypeId;
    if (lower == L"listitem") return UIA_ListItemControlTypeId;
    if (lower == L"list") return UIA_ListControlTypeId;
    if (lower == L"menu") return UIA_MenuControlTypeId;
    if (lower == L"menubar") return UIA_MenuBarControlTypeId;
    if (lower == L"menuitem") return UIA_MenuItemControlTypeId;
    if (lower == L"progressbar") return UIA_ProgressBarControlTypeId;
    if (lower == L"radiobutton") return UIA_RadioButtonControlTypeId;
    if (lower == L"scrollbar") return UIA_ScrollBarControlTypeId;
    if (lower == L"slider") return UIA_SliderControlTypeId;
    if (lower == L"spinner") return UIA_SpinnerControlTypeId;
    if (lower == L"statusbar") return UIA_StatusBarControlTypeId;
    if (lower == L"tab") return UIA_TabControlTypeId;
    if (lower == L"tabitem") return UIA_TabItemControlTypeId;
    if (lower == L"text") return UIA_TextControlTypeId;
    if (lower == L"toolbar") return UIA_ToolBarControlTypeId;
    if (lower == L"tooltip") return UIA_ToolTipControlTypeId;
    if (lower == L"tree") return UIA_TreeControlTypeId;
    if (lower == L"treeitem") return UIA_TreeItemControlTypeId;
    if (lower == L"custom") return UIA_CustomControlTypeId;
    if (lower == L"group") return UIA_GroupControlTypeId;
    if (lower == L"thumb") return UIA_ThumbControlTypeId;
    if (lower == L"datagrid") return UIA_DataGridControlTypeId;
    if (lower == L"dataitem") return UIA_DataItemControlTypeId;
    if (lower == L"document") return UIA_DocumentControlTypeId;
    if (lower == L"splitbutton") return UIA_SplitButtonControlTypeId;
    if (lower == L"window") return UIA_WindowControlTypeId;
    if (lower == L"pane") return UIA_PaneControlTypeId;
    if (lower == L"header") return UIA_HeaderControlTypeId;
    if (lower == L"headeritem") return UIA_HeaderItemControlTypeId;
    if (lower == L"table") return UIA_TableControlTypeId;
    if (lower == L"titlebar") return UIA_TitleBarControlTypeId;
    if (lower == L"separator") return UIA_SeparatorControlTypeId;

    return 0;  // Unknown type
}

std::wstring UIAutomationHelper::BstrToWstring(BSTR bstr) {
    if (!bstr) {
        return L"";
    }

    std::wstring result(bstr);
    SysFreeString(bstr);
    return result;
}
