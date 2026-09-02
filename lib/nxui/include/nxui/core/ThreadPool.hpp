#pragma once
// ThreadPool — lightweight, reusable worker pool.
//
// Usage:
//     nxui::ThreadPool pool(2);                       // 2 workers
//     auto fut = pool.submit([]{ heavyWork(); });     // returns std::future<void>
//     // ... poll with fut.wait_for(0s) or block with fut.get() ...
//
// The pool shuts down cleanly in its destructor (waits for in-flight tasks).
#include <functional>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <cstddef>
#ifdef __SWITCH__
#include <switch.h>
#endif

namespace nxui {

class ThreadPool {
public:
    /// Create a pool with `numWorkers` threads.
    explicit ThreadPool(std::size_t numWorkers = 2) {
        m_workers.reserve(numWorkers);
        for (std::size_t i = 0; i < numWorkers; ++i) {
            m_workers.emplace_back([this, i]() { workerLoop(i); });
        }
    }

    ~ThreadPool() {
        shutdown();
    }

    // Stops accepting work, drains already accepted tasks, then joins every
    // worker. Safe to call more than once. Owners should call this before
    // destroying any object captured by a task.
    void shutdown() {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            if (m_stop && m_workers.empty())
                return;
            m_stop = true;
        }
        m_cv.notify_all();
        for (auto& w : m_workers)
            if (w.joinable()) w.join();
        m_workers.clear();
    }

    bool acceptingTasks() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        return !m_stop;
    }

    // Non-copyable, non-movable.
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /// Submit a task. Returns a future that resolves when the task completes.
    std::future<void> submit(std::function<void()> task) {
        auto promise = std::make_shared<std::promise<void>>();
        auto future  = promise->get_future();

        {
            std::lock_guard<std::mutex> lk(m_mutex);
            if (m_stop) {
                promise->set_exception(std::make_exception_ptr(
                    std::runtime_error("ThreadPool is shutting down")));
                return future;
            }
            m_queue.push([t = std::move(task), p = std::move(promise)]() {
                try {
                    t();
                    p->set_value();
                } catch (...) {
                    p->set_exception(std::current_exception());
                }
            });
        }
        m_cv.notify_one();
        return future;
    }

    // Wait until work submitted before this call has completed.  Applet
    // handoff uses this while all services are still valid: destroying a
    // service runtime before one of its worker requests has returned can turn
    // an otherwise orderly menu exit into a process abort.
    //
    // This is intentionally a main-thread operation.  Calling it from a pool
    // worker would wait for that worker itself and deadlock.
    void waitForIdle() {
        std::unique_lock<std::mutex> lk(m_mutex);
        m_idleCv.wait(lk, [this]() {
            return m_queue.empty() && m_activeTasks == 0;
        });
    }

private:
    void workerLoop(std::size_t workerIndex) {
#ifdef __SWITCH__
        // The menu main/render thread starts on core 0. libnx gives a
        // std::thread the whole process CPU mask, so pin background I/O and
        // decode work to cores 1-2 instead of letting it preempt rendering.
        const s32 core = 1 + static_cast<s32>(workerIndex % 2);
        svcSetThreadCoreMask(CUR_THREAD_HANDLE, core, UINT64_C(1) << core);
#else
        (void)workerIndex;
#endif
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lk(m_mutex);
                m_cv.wait(lk, [this]() { return m_stop || !m_queue.empty(); });
                if (m_stop && m_queue.empty()) return;
                task = std::move(m_queue.front());
                m_queue.pop();
                ++m_activeTasks;
            }
            task();
            {
                std::lock_guard<std::mutex> lk(m_mutex);
                --m_activeTasks;
                if (m_queue.empty() && m_activeTasks == 0)
                    m_idleCv.notify_all();
            }
        }
    }

    std::vector<std::thread>            m_workers;
    std::queue<std::function<void()>>   m_queue;
    mutable std::mutex                  m_mutex;
    std::condition_variable             m_cv;
    std::condition_variable             m_idleCv;
    std::size_t                         m_activeTasks = 0;
    bool                                m_stop = false;
};

} // namespace nxui
