#pragma once

#include <windows.h>
#include <oleacc.h>      // Must be included before UIAutomation.h
#include <UIAutomation.h>
#include <string>
#include <memory>

/**
 * @brief UI Automation Helper
 *
 * Provides a simplified interface to Windows UI Automation API.
 * This class encapsulates COM initialization and UI element discovery.
 *
 * Key Features:
 * - Automatic COM initialization and cleanup
 * - Element search by role, name, and automation ID
 * - Property extraction (text, value)
 * - RAII pattern for resource management
 *
 * Thread Safety:
 * - Each instance must be created in a COM-initialized thread
 * - Do NOT share instances across threads
 * - Suitable for use in AsyncExecutor worker threads
 */
class UIAutomationHelper {
public:
    /**
     * @brief Constructor
     *
     * Initializes COM (if not already initialized) and creates UI Automation instance.
     * Call Initialize() after construction to check if initialization succeeded.
     */
    UIAutomationHelper();

    /**
     * @brief Destructor
     *
     * Releases all COM resources and uninitializes COM if it was initialized by this instance.
     */
    ~UIAutomationHelper();

    // Disable copy
    UIAutomationHelper(const UIAutomationHelper&) = delete;
    UIAutomationHelper& operator=(const UIAutomationHelper&) = delete;

    /**
     * @brief Initialize the helper
     *
     * @return true if UI Automation is ready to use, false otherwise
     */
    bool Initialize();

    /**
     * @brief Check if helper is initialized
     *
     * @return true if ready to use
     */
    bool IsInitialized() const { return m_automation != nullptr; }

    /**
     * @brief Find element by control type name
     *
     * @param hwnd Window handle to search in
     * @param controlTypeName Control type name (e.g., "Edit", "Button", "ComboBox")
     * @param namePart Optional: part of the element name to match (case-insensitive)
     * @return Element if found, nullptr otherwise
     */
    IUIAutomationElement* FindElementByControlType(HWND hwnd,
                                                    const std::wstring& controlTypeName,
                                                    const std::wstring& namePart = L"");

    /**
     * @brief Find element by name (case-insensitive partial match)
     *
     * @param hwnd Window handle to search in
     * @param namePart Part of the element name to search for
     * @return Element if found, nullptr otherwise
     */
    IUIAutomationElement* FindElementByName(HWND hwnd,
                                            const std::wstring& namePart);

    /**
     * @brief Find element by Automation ID
     *
     * @param hwnd Window handle to search in
     * @param automationId Automation ID to search for
     * @return Element if found, nullptr otherwise
     */
    IUIAutomationElement* FindElementByAutomationId(HWND hwnd,
                                                     const std::wstring& automationId);

    /**
     * @brief Get element's value (for input controls)
     *
     * @param element UI element
     * @return Value string, or empty if not available
     */
    std::wstring GetElementValue(IUIAutomationElement* element);

    /**
     * @brief Get element's text content (Name property)
     *
     * @param element UI element
     * @return Text content, or empty if not available
     */
    std::wstring GetElementText(IUIAutomationElement* element);

    /**
     * @brief Get automation instance (for advanced usage)
     *
     * @return Pointer to IUIAutomation interface (do not release manually)
     */
    IUIAutomation* GetAutomation() { return m_automation; }

private:
    /**
     * @brief Get control type ID from name
     *
     * @param typeName Control type name (e.g., "Edit", "Button")
     * @return Control type ID, or 0 if unknown
     */
    CONTROLTYPEID GetControlTypeId(const std::wstring& typeName);

    /**
     * @brief Convert BSTR to wstring and free BSTR
     *
     * @param bstr BSTR to convert (will be freed)
     * @return Wide string
     */
    std::wstring BstrToWstring(BSTR bstr);

    IUIAutomation* m_automation;
    bool m_comInitialized;
};

/**
 * @brief RAII wrapper for IUIAutomationElement
 *
 * Automatically releases the element when going out of scope.
 */
class AutoElement {
public:
    AutoElement(IUIAutomationElement* elem = nullptr) : m_element(elem) {}

    ~AutoElement() {
        if (m_element) {
            m_element->Release();
        }
    }

    // Disable copy
    AutoElement(const AutoElement&) = delete;
    AutoElement& operator=(const AutoElement&) = delete;

    // Move semantics
    AutoElement(AutoElement&& other) noexcept : m_element(other.m_element) {
        other.m_element = nullptr;
    }

    AutoElement& operator=(AutoElement&& other) noexcept {
        if (m_element) {
            m_element->Release();
        }
        m_element = other.m_element;
        other.m_element = nullptr;
        return *this;
    }

    IUIAutomationElement* Get() { return m_element; }
    IUIAutomationElement* operator->() { return m_element; }
    operator bool() const { return m_element != nullptr; }

    IUIAutomationElement** operator&() {
        if (m_element) {
            m_element->Release();
        }
        m_element = nullptr;
        return &m_element;
    }

private:
    IUIAutomationElement* m_element;
};
