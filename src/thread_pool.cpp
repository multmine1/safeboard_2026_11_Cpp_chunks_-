#include "thread_pool.hpp"

namespace file_collector::detail {

ThreadPool::ThreadPool(std::size_t worker_count) {
    const auto count = worker_count == 0 ? std::size_t{1} : worker_count;
    workers_.reserve(count);
    try {
        for (std::size_t i = 0; i < count; ++i) {
            workers_.emplace_back([this] { Run(); });
        }
    } catch (...) {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        condition_.notify_all();
        throw;
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    condition_.notify_all();
    // jthread destruction joins every worker, after all accepted tasks drain.
}

std::size_t ThreadPool::WorkerCount() const noexcept {
    return workers_.size();
}

void ThreadPool::Run() {
    for (;;) {
        std::packaged_task<void()> task;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (queue_.empty() && stopping_) {
                return;
            }
            task = std::move(queue_.front());
            queue_.pop_front();
        }
        task();  // packaged_task captures task exceptions, keeping workers alive.
    }
}

}  // namespace file_collector::detail

