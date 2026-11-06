#include "test_data.hpp"
#include <gtest/gtest.h>

using namespace file_collector;

TEST(Snapshots, PartialAndIncompleteCopiesNeverChange) {
    FileCollector collector(2);
    collector.CollectFile(1, 5);
    collector.OnNewChunk(1, 0, FileBytes{1, 2});
    auto incomplete = collector.GetFile(1, false);
    auto partial = collector.GetFile(1, 0, 4, false);
    collector.OnNewChunk(1, 2, FileBytes{3, 4, 5});
    EXPECT_EQ(*collector.GetFile(1), (FileBytes{1, 2, 3, 4, 5}));
    EXPECT_EQ(*incomplete, (FileBytes{1, 2, 0, 0, 0}));
    EXPECT_EQ(*partial, (FileBytes{1, 2, 0, 0}));
}

TEST(Snapshots, ReadyFileUsesSharedReadOnlyBacking) {
    FileCollector collector(2);
    collector.CollectFile(1, 3);
    collector.OnNewChunk(1, 0, FileBytes{1, 2, 3});
    auto first = collector.GetFile(1);
    auto second = collector.GetFile(1);
    EXPECT_EQ(first.get(), second.get());
    collector.OnNewChunk(1, 0, FileBytes{99});
    EXPECT_EQ(*first, (FileBytes{1, 2, 3}));
}

TEST(Snapshots, TakeCopiesWhenReadOnlyOwnerExists) {
    FileCollector collector(2);
    collector.CollectFile(1, 3);
    collector.OnNewChunk(1, 0, FileBytes{1, 2, 3});
    auto read_only = collector.GetFile(1);
    auto writable = collector.TakeFile(1);
    EXPECT_NE(read_only.get(), writable.get());
    writable->at(1) = 99;
    EXPECT_EQ(*read_only, (FileBytes{1, 2, 3}));
    EXPECT_EQ(*writable, (FileBytes{1, 99, 3}));
}

TEST(Snapshots, ClearAndReregisterPreservePreviouslyIssuedViews) {
    FileCollector collector(2);
    collector.CollectFile(1, 2);
    collector.OnNewChunk(1, 0, FileBytes{1, 2});
    auto old_view = collector.GetFile(1);
    collector.ClearFile(1);
    collector.OnNewChunk(1, 0, FileBytes{3, 4});
    auto mid_view = collector.GetFile(1);
    collector.RemoveFile(1);
    collector.CollectFile(1, 1);
    collector.OnNewChunk(1, 0, FileBytes{5});
    EXPECT_EQ(*old_view, (FileBytes{1, 2}));
    EXPECT_EQ(*mid_view, (FileBytes{3, 4}));
}

