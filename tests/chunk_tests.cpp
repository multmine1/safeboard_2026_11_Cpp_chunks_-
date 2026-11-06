#include "test_data.hpp"
#include <gtest/gtest.h>

using namespace file_collector;

TEST(Chunks, OutOfOrderAndPartialOverlap) {
    FileCollector collector(3);
    collector.CollectFile(7, 10);
    collector.OnNewChunk(7, 6, FileBytes{'G', 'H', 'I', 'J'});
    collector.OnNewChunk(7, 2, FileBytes{'C', 'D', 'E', 'F'});
    collector.OnNewChunk(7, 0, FileBytes{'A', 'B', 'C', 'D'});
    EXPECT_EQ(*collector.GetFile(7), (FileBytes{'A','B','C','D','E','F','G','H','I','J'}));
}

TEST(Chunks, DuplicatesAndContainedOverlaps) {
    FileCollector collector(4);
    auto original = test_data::Pattern(127);
    collector.CollectFile(1, original.size());
    collector.OnNewChunk(1, 0, test_data::Slice(original, 0, 80));
    collector.OnNewChunk(1, 12, test_data::Slice(original, 12, 30));
    collector.OnNewChunk(1, 63, test_data::Slice(original, 63, 64));
    collector.OnNewChunk(1, 0, FileBytes(original));
    EXPECT_EQ(*collector.GetFile(1), original);
    collector.OnNewChunk(1, 0, FileBytes(original));  // ready: ignore without requeue
    EXPECT_EQ(*collector.GetFile(1), original);
}

TEST(Chunks, WordBoundaries63_64_65) {
    for (std::size_t size : {std::size_t{1}, std::size_t{63}, std::size_t{64},
                             std::size_t{65}, std::size_t{129}}) {
        SCOPED_TRACE(size);
        FileCollector collector(2);
        auto original = test_data::Pattern(size);
        collector.CollectFile(1, size);
        for (std::size_t i = size; i-- > 0;) {
            collector.OnNewChunk(1, i, FileBytes{original[i]});
        }
        EXPECT_EQ(*collector.GetFile(1), original);
    }
}

TEST(Chunks, OriginalZeroBytesAreNotHoles) {
    FileCollector collector(2);
    collector.CollectFile(1, 5);
    collector.OnNewChunk(1, 0, FileBytes{0, 0, 0});
    EXPECT_EQ(*collector.GetFile(1, 0, 3), (FileBytes{0, 0, 0}));
    EXPECT_THROW(static_cast<void>(collector.GetFile(1, 0, 4)), std::logic_error);
    collector.OnNewChunk(1, 3, FileBytes{0, 0});
    EXPECT_EQ(*collector.GetFile(1), (FileBytes(5, 0)));
}

