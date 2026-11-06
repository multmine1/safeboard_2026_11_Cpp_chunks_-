#include "thread_pool.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>

using file_collector::detail::ThreadPool;

TEST(ThreadPool, ZeroReportedWorkersFallsBackToOne) {
    ThreadPool pool(0);
    EXPECT_EQ(pool.WorkerCount(), std::size_t{1});
}

TEST(ThreadPool, ExplicitWorkerCountAndQueueDrain) {
    std::atomic_int calls{0};
    {
        ThreadPool pool(3);
        EXPECT_EQ(pool.WorkerCount(), std::size_t{3});
        for (int i = 0; i < 100; ++i) {
            pool.Submit([&] { calls.fetch_add(1); });
        }
    }
    EXPECT_EQ(calls.load(), 100);
}

TEST(ThreadPool, WorkersSurviveTaskException) {
    ThreadPool pool(2);
    std::promise<void> done;
    auto result = done.get_future();
    pool.Submit([] { throw std::runtime_error("test exception"); });
    pool.Submit([&] { done.set_value(); });
    EXPECT_EQ(result.wait_for(std::chrono::seconds(5)), std::future_status::ready);
}

TEST(ThreadPool, DefaultUsesHardwareConcurrencyOrOne) {
    ThreadPool pool;
    const auto available = std::thread::hardware_concurrency();
    EXPECT_EQ(pool.WorkerCount(), available == 0 ? 1u : available);
}

