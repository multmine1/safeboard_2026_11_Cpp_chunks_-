#include "test_data.hpp"
#include <gtest/gtest.h>
#include <barrier>
#include <thread>

using namespace file_collector;

TEST(Lifecycle, TakeTransfersMutableBufferAndAllowsRegistration) {
    FileCollector collector(2);
    collector.CollectFile(1, 3);
    collector.OnNewChunk(1, 0, FileBytes{4, 5, 6});
    MutableFile bytes = collector.TakeFile(1);
    EXPECT_EQ(*bytes, (FileBytes{4, 5, 6}));
    bytes->at(0) = 99;
    EXPECT_EQ(bytes->front(), 99);
    EXPECT_THROW(static_cast<void>(collector.GetFile(1)), std::out_of_range);
    collector.CollectFile(1, 1);
    collector.OnNewChunk(1, 0, FileBytes{7});
    EXPECT_EQ(*collector.GetFile(1), (FileBytes{7}));
}

TEST(Lifecycle, ClearKeepsRegistrationAndSize) {
    FileCollector collector(2);
    collector.CollectFile(1, 4);
    collector.OnNewChunk(1, 0, FileBytes{1, 2, 3, 4});
    auto previous = collector.GetFile(1);
    collector.ClearFile(1);
    EXPECT_THROW(static_cast<void>(collector.GetFile(1)), std::logic_error);
    EXPECT_EQ(*collector.GetFile(1, false), (FileBytes{0, 0, 0, 0}));
    collector.OnNewChunk(1, 0, FileBytes{5, 6, 7, 8});
    EXPECT_EQ(*collector.GetFile(1), (FileBytes{5, 6, 7, 8}));
    EXPECT_EQ(*previous, (FileBytes{1, 2, 3, 4}));
    EXPECT_THROW(collector.CollectFile(1, 1), std::logic_error);
}

TEST(Lifecycle, RemovalDoesNotInvalidateView) {
    FileCollector collector(2);
    collector.CollectFile(1, 2);
    collector.OnNewChunk(1, 0, FileBytes{3, 4});
    auto previous = collector.GetFile(1);
    collector.RemoveFile(1);
    EXPECT_EQ(*previous, (FileBytes{3, 4}));
    collector.CollectFile(1, 2);
    collector.OnNewChunk(1, 0, FileBytes{5, 6});
    EXPECT_EQ(*collector.GetFile(1), (FileBytes{5, 6}));
}

TEST(Lifecycle, ConcurrentClearAndSubmissionRemainReusable) {
    FileCollector collector(2);
    collector.CollectFile(1, 64);
    std::barrier start(2);
    std::jthread sender([&] {
        start.arrive_and_wait();
        try { collector.OnNewChunk(1, 0, FileBytes(64, 8)); }
        catch (const std::logic_error&) {}  // Clear may close admission first.
    });
    start.arrive_and_wait();
    collector.ClearFile(1);
    sender.join();
    collector.ClearFile(1);
    collector.OnNewChunk(1, 0, FileBytes(64, 9));
    EXPECT_EQ(*collector.GetFile(1), (FileBytes(64, 9)));
}

