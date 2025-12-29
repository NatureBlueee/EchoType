#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "context_adapter.h"
#include "async_executor.h"
#include <memory>
#include <vector>
#include <functional>

// Forward declaration
struct SourceInfo;
struct ContextData;

// Context manager - coordinates all context adapters
class ContextManager {
public:
    using ContextCallback = std::function<void(std::shared_ptr<ContextData>)>;

    // Constructor
    explicit ContextManager(size_t threadPoolSize = 2);

    // Destructor
    ~ContextManager();

    // Initialize the manager
    // Returns: true if successful
    bool Initialize();

    // Register a context adapter
    // adapter: Shared pointer to adapter instance
    void RegisterAdapter(std::shared_ptr<IContextAdapter> adapter);

    // Get context asynchronously
    // source: Source application information
    // callback: Callback function to receive context data
    void GetContextAsync(const SourceInfo& source, ContextCallback callback);

    // Get default timeout
    int GetDefaultTimeout() const { return m_defaultTimeout; }

    // Set default timeout
    void SetDefaultTimeout(int timeoutMs) { m_defaultTimeout = timeoutMs; }

    // Check if manager is initialized
    bool IsInitialized() const { return m_initialized; }

private:
    // Find matching adapter for a process
    // processName: Process name (e.g., "chrome.exe")
    // windowTitle: Window title for additional context
    // Returns: Pointer to matching adapter, or nullptr if none found
    std::shared_ptr<IContextAdapter> FindAdapter(const std::wstring& processName,
                                                 const std::wstring& windowTitle);

    std::vector<std::shared_ptr<IContextAdapter>> m_adapters;
    std::unique_ptr<AsyncExecutor> m_executor;
    int m_defaultTimeout;
    bool m_initialized;
};
