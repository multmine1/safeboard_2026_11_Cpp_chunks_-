#include "test_data.hpp"
#include "fault_injector.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <latch>
#include <stdexcept>

using namespace file_collector;

namespace {

class OneShotFault final : public detail::FaultInjector {
public:
    explicit OneShotFault(detail::FaultPoint target) : target_(target) {}

    void Hit(detail::FaultPoint point) override {
        if (point == target_ && !triggered_.exchange(true)) {
            throw std::runtime_error("injected failure");
        }
    }

private:
    detail::FaultPoint target_;
    std::atomic_bool triggered_{false};
};

class GatedFailure final : public detail::FaultInjector {
public:
    void Hit(detail::FaultPoint point) override {
        if (point == detail::FaultPoint::AfterReserve && !hit_.exchange(true)) {
            reached_.count_down();
            resume_.wait();
            throw std::runtime_error("gated injected failure");
        }
    }

    void WaitUntilReserved() { reached_.wait(); }
    void Release() { resume_.count_down(); }

private:
    std::atomic_bool hit_{false};
    std::latch reached_{1};
    std::latch resume_{1};
};

class CountSubmissions final : public detail::FaultInjector {
public:
    void Hit(detail::FaultPoint point) override {
        if (point == detail::FaultPoint::BeforeEnqueue) {
            count_.fetch_add(1);
        }
    }
    [[nodiscard]] int Count() const noexcept { return count_.load(); }

private:
    std::atomic_int count_{0};
};

}  // namespace

TEST(Exceptions, BeforeEnqueueFailureRollsBackPendingCounter) {
    auto injector = std::make_shared<OneShotFault>(detail::FaultPoint::BeforeEnqueue);
    FileCollector collector(2, injector);
    collector.CollectFile(1, 3);
    EXPECT_THROW(collector.OnNewChunk(1, 0, FileBytes{1, 2, 3}), std::runtime_error);
    EXPECT_EQ(*collector.GetFile(1, false), (FileBytes{0, 0, 0}));
    collector.OnNewChunk(1, 0, FileBytes{1, 2, 3});
    EXPECT_EQ(*collector.GetFile(1), (FileBytes{1, 2, 3}));
}

TEST(Exceptions, AllWorkerFaultPointsReportAndRecoverAfterClear) {
    for (auto point : {detail::FaultPoint::AfterReserve,
                       detail::FaultPoint::BeforeCopy,
                       detail::FaultPoint::BeforeCommit}) {
        SCOPED_TRACE(static_cast<int>(point));
        auto injector = std::make_shared<OneShotFault>(point);
        FileCollector collector(2, injector);
        collector.CollectFile(1, 65);
        collector.OnNewChunk(1, 0, FileBytes(65, 7));
        EXPECT_THROW(static_cast<void>(collector.GetFile(1, false)), std::runtime_error);
        EXPECT_THROW(static_cast<void>(collector.TakeFile(1)), std::runtime_error);
        collector.ClearFile(1);
        collector.OnNewChunk(1, 0, FileBytes(65, 9));
        EXPECT_EQ(*collector.GetFile(1), (FileBytes(65, 9)));
    }
}

TEST(Exceptions, FailedReservationCannotLeavePhantomCoverage) {
    auto injector = std::make_shared<GatedFailure>();
    FileCollector collector(2, injector);
    collector.CollectFile(7, 64);
    collector.OnNewChunk(7, 0, FileBytes(64, 42));
    injector->WaitUntilReserved();
    collector.OnNewChunk(7, 0, FileBytes(64, 42));
    injector->Release();
    EXPECT_THROW(static_cast<void>(collector.GetFile(7)), std::runtime_error);
    collector.ClearFile(7);
    collector.OnNewChunk(7, 0, FileBytes(64, 33));
    EXPECT_EQ(*collector.GetFile(7), (FileBytes(64, 33)));
}

TEST(Exceptions, CompletedFileDoesNotEnqueueNewChunks) {
    auto injector = std::make_shared<CountSubmissions>();
    FileCollector collector(2, injector);
    collector.CollectFile(1, 3);
    collector.OnNewChunk(1, 0, FileBytes{1, 2, 3});
    ASSERT_EQ(*collector.GetFile(1), (FileBytes{1, 2, 3}));
    const auto initial_submissions = injector->Count();
    collector.OnNewChunk(1, 0, FileBytes{1, 2, 3});
    EXPECT_EQ(injector->Count(), initial_submissions);
}

