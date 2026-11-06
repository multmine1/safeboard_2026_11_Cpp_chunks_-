#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace file_collector::detail {

class ThreadPool final {
public:
    explicit ThreadPool(std::size_t worker_count = std::thread::hardware_concurrency());
    ~ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    [[nodiscard]] std::size_t WorkerCount() const noexcept;

    template <class Task>
    void Submit(Task&& task) {
        std::packaged_task<void()> packaged(std::forward<Task>(task));
        {
            std::lock_guard lock(mutex_);
            if (stopping_) {
                throw std::runtime_error("thread pool is stopping");
            }
            queue_.push_back(std::move(packaged));
        }
        condition_.notify_one();
    }

private:
    void Run();

    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<std::packaged_task<void()>> queue_;
    std::vector<std::jthread> workers_;
    bool stopping_ = false;
};

}  // namespace file_collector::detail

