#include "test_data.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <type_traits>

using namespace file_collector;

static_assert(std::is_same_v<decltype(std::declval<IFileCollector&>().GetFile(0)), FileView>);
static_assert(std::is_same_v<decltype(std::declval<IFileCollector&>().GetFile(0, 0, 0)), FileView>);
static_assert(std::is_same_v<decltype(std::declval<IFileCollector&>().TakeFile(0)), MutableFile>);

TEST(Collector, PolymorphicInterfaceAndIndependentFiles) {
    std::unique_ptr<IFileCollector> collector = std::make_unique<FileCollector>(2);
    const FileBytes one{1, 2, 3};
    const FileBytes two{4, 5};
    collector->CollectFile(17, one.size());
    collector->CollectFile(18, two.size());
    collector->OnNewChunk(18, 0, FileBytes(two));
    collector->OnNewChunk(17, 0, FileBytes(one));
    EXPECT_EQ(*collector->GetFile(17), one);
    EXPECT_EQ(*collector->GetFile(18), two);
}

TEST(Collector, ZeroLengthReadyImmediately) {
    FileCollector collector(1);
    collector.CollectFile(1, 0);
    EXPECT_TRUE(collector.GetFile(1)->empty());
    EXPECT_TRUE(collector.GetFile(1, 0, 0)->empty());
    collector.OnNewChunk(1, 0, {});
    EXPECT_TRUE(collector.TakeFile(1)->empty());
}

TEST(Collector, IncompleteFileNeedsExplicitOptOut) {
    FileCollector collector(2);
    collector.CollectFile(1, 5);
    collector.OnNewChunk(1, 1, FileBytes{7, 8});
    EXPECT_THROW(static_cast<void>(collector.GetFile(1)), std::logic_error);
    EXPECT_EQ(*collector.GetFile(1, false), (FileBytes{0, 7, 8, 0, 0}));
    EXPECT_EQ(*collector.GetFile(1, 1, 2), (FileBytes{7, 8}));
    EXPECT_THROW(static_cast<void>(collector.GetFile(1, 0, 3)), std::logic_error);
}

TEST(Collector, TakeIncompleteOnlyWhenCheckingDisabled) {
    FileCollector collector(1);
    collector.CollectFile(1, 3);
    collector.OnNewChunk(1, 0, FileBytes{1});
    EXPECT_THROW(static_cast<void>(collector.TakeFile(1)), std::logic_error);
    collector.OnNewChunk(1, 2, FileBytes{3});
    auto bytes = collector.TakeFile(1, false);
    EXPECT_EQ(*bytes, (FileBytes{1, 0, 3}));
    EXPECT_THROW(static_cast<void>(collector.GetFile(1)), std::out_of_range);
}

