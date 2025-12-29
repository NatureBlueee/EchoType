#pragma once

#include <functional>
#include <future>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

// Async executor with thread pool
class AsyncExecutor {
public:
    using Task = std::function<void()>;

    // Constructor
    // threadCount: Number of worker threads (default: 2)
    explicit AsyncExecutor(size_t threadCount = 2);

    // Destructor
    ~AsyncExecutor();

    // Submit a task and return a future
    template<typename Func, typename... Args>
    auto Submit(Func&& func, Args&&... args)
        -> std::future<typename std::invoke_result_t<Func, Args...>>;

    // Submit a task with timeout
    // func: Task function
    // timeoutMs: Timeout in milliseconds
    // onTimeout: Callback when timeout occurs
    template<typename Func>
    void SubmitWithTimeout(Func&& func, int timeoutMs,
                          std::function<void()> onTimeout = nullptr);

    // Shutdown the executor
    void Shutdown();

    // Check if executor is running
    bool IsRunning() const { return !m_stop; }

private:
    // Worker thread function
    void WorkerThread();

    std::vector<std::thread> m_workers;
    std::queue<Task> m_tasks;
    std::mutex m_queueMutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_stop;
};

// Template implementations

template<typename Func, typename... Args>
auto AsyncExecutor::Submit(Func&& func, Args&&... args)
    -> std::future<typename std::invoke_result_t<Func, Args...>>
{
    using ReturnType = typename std::invoke_result_t<Func, Args...>;

    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
    );

    std::future<ReturnType> result = task->get_future();

    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        if (m_stop) {
            throw std::runtime_error("Cannot submit task on stopped executor");
        }
        m_tasks.emplace([task]() { (*task)(); });
    }

    m_condition.notify_one();
    return result;
}

template<typename Func>
void AsyncExecutor::SubmitWithTimeout(Func&& func, int timeoutMs,
                                     std::function<void()> onTimeout)
{
    auto future = Submit(std::forward<Func>(func));

    // Wait for future with timeout in a detached thread
    std::thread([future = std::move(future), timeoutMs, onTimeout]() mutable {
        auto status = future.wait_for(std::chrono::milliseconds(timeoutMs));
        if (status == std::future_status::timeout && onTimeout) {
            onTimeout();
        }
    }).detach();
}
