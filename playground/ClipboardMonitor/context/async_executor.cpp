#include "async_executor.h"

AsyncExecutor::AsyncExecutor(size_t threadCount) : m_stop(false) {
    // Create worker threads
    for (size_t i = 0; i < threadCount; ++i) {
        m_workers.emplace_back([this] { WorkerThread(); });
    }
}

AsyncExecutor::~AsyncExecutor() {
    Shutdown();
}

void AsyncExecutor::WorkerThread() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_condition.wait(lock, [this] {
                return m_stop || !m_tasks.empty();
            });

            if (m_stop && m_tasks.empty()) {
                return;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop();
        }

        // Execute task outside the lock
        task();
    }
}

void AsyncExecutor::Shutdown() {
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_stop = true;
    }

    m_condition.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}
