#include "test_data.hpp"
#include <gtest/gtest.h>
#include <limits>

using namespace file_collector;

TEST(Boundary, UnknownIdRejectedEverywhere) {
    FileCollector collector(1);
    EXPECT_THROW(collector.OnNewChunk(99, 0, {}), std::out_of_range);
    EXPECT_THROW(static_cast<void>(collector.GetFile(99)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(collector.GetFile(99, 0, 0)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(collector.TakeFile(99)), std::out_of_range);
    EXPECT_THROW(collector.RemoveFile(99), std::out_of_range);
    EXPECT_THROW(collector.ClearFile(99), std::out_of_range);
}

TEST(Boundary, OffsetsCheckedWithoutOverflow) {
    FileCollector collector(1);
    collector.CollectFile(1, 4);
    EXPECT_THROW(collector.OnNewChunk(1, 5, {}), std::out_of_range);
    EXPECT_THROW(collector.OnNewChunk(1, 3, FileBytes{1, 2}), std::out_of_range);
    EXPECT_THROW(collector.OnNewChunk(1, std::numeric_limits<std::size_t>::max(), {}), std::out_of_range);
    EXPECT_THROW(static_cast<void>(collector.GetFile(1, 3, 2, false)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(collector.GetFile(1, 5, 0, false)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(collector.GetFile(1, std::numeric_limits<std::size_t>::max(), 1, false)), std::out_of_range);
    EXPECT_EQ(collector.GetFile(1, 4, 0)->size(), std::size_t{0});
}

TEST(Boundary, DuplicateRegistrationRejectedUntilRemoved) {
    FileCollector collector(1);
    collector.CollectFile(1, 1);
    EXPECT_THROW(collector.CollectFile(1, 9), std::logic_error);
    collector.RemoveFile(1);
    EXPECT_NO_THROW(collector.CollectFile(1, 9));
}

TEST(Boundary, EmptyChunkIsNoopExceptReady) {
    FileCollector collector(1);
    collector.CollectFile(1, 2);
    collector.OnNewChunk(1, 2, {});
    EXPECT_THROW(static_cast<void>(collector.GetFile(1)), std::logic_error);
    collector.OnNewChunk(1, 0, FileBytes{3, 4});
    EXPECT_EQ(*collector.GetFile(1), (FileBytes{3, 4}));
}

