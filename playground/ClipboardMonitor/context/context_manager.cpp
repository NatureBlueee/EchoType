#include "context_manager.h"
#include "../clipboard_monitor.h"
#include "../debug_log.h"
#include "../utils.h"
#include <sstream>
#include <atomic>

ContextManager::ContextManager(size_t threadPoolSize)
    : m_defaultTimeout(100)
    , m_initialized(false)
{
    m_executor = std::make_unique<AsyncExecutor>(threadPoolSize);
}

ContextManager::~ContextManager() {
    if (m_executor) {
        m_executor->Shutdown();
    }
}

bool ContextManager::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Initialize COM for async executor threads
    // Note: Each worker thread will need to initialize COM separately
    // This is handled in the adapters themselves

    m_initialized = true;
    DEBUG_LOG("ContextManager initialized");
    return true;
}

void ContextManager::RegisterAdapter(std::shared_ptr<IContextAdapter> adapter) {
    if (!adapter) {
        return;
    }

    m_adapters.push_back(adapter);

    std::wstring msg = L"Registered adapter: " + adapter->GetAdapterName();
    DEBUG_LOG(Utils::WideToUtf8(msg));
}

void ContextManager::GetContextAsync(const SourceInfo& source, ContextCallback callback) {
    if (!m_initialized) {
        DEBUG_LOG("ContextManager not initialized");
        if (callback) {
            callback(nullptr);
        }
        return;
    }

    // Find matching adapter
    auto adapter = FindAdapter(source.processName, source.windowTitle);
    if (!adapter) {
        // No adapter found - call callback with nullptr
        if (callback) {
            callback(nullptr);
        }
        return;
    }

    // Get timeout for this adapter
    int timeout = adapter->GetTimeout();
    if (timeout <= 0) {
        timeout = m_defaultTimeout;
    }

    // Use shared atomic flag to ensure callback is called only once
    auto callbackCalled = std::make_shared<std::atomic<bool>>(false);

    // Submit async task
    m_executor->SubmitWithTimeout(
        [adapter, source, callback, callbackCalled]() {
            std::shared_ptr<ContextData> contextData = nullptr;

            try {
                contextData = adapter->GetContext(source);
            } catch (const std::exception& e) {
                std::ostringstream oss;
                oss << "Adapter exception: " << e.what();
                DEBUG_LOG(oss.str());

                // Create error context
                contextData = std::make_shared<ContextData>();
                contextData->success = false;
                contextData->error = L"Exception: " +
                    std::wstring(e.what(), e.what() + strlen(e.what()));
            } catch (...) {
                DEBUG_LOG("Adapter unknown exception");

                contextData = std::make_shared<ContextData>();
                contextData->success = false;
                contextData->error = L"Unknown exception";
            }

            // Call callback with result only if not already called
            bool expected = false;
            if (callbackCalled->compare_exchange_strong(expected, true)) {
                if (callback) {
                    callback(contextData);
                }
            }
        },
        timeout,
        [callback, callbackCalled]() {
            // Timeout callback - only call if task hasn't completed yet
            bool expected = false;
            if (callbackCalled->compare_exchange_strong(expected, true)) {
                DEBUG_LOG("Context fetch timeout");

                auto timeoutContext = std::make_shared<ContextData>();
                timeoutContext->success = false;
                timeoutContext->error = L"Timeout";

                if (callback) {
                    callback(timeoutContext);
                }
            }
        }
    );
}

std::shared_ptr<IContextAdapter> ContextManager::FindAdapter(
    const std::wstring& processName,
    const std::wstring& windowTitle)
{
    for (auto& adapter : m_adapters) {
        if (adapter->CanHandle(processName, windowTitle)) {
            return adapter;
        }
    }
    return nullptr;
}
