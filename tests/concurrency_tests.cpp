#include "test_data.hpp"
#include <gtest/gtest.h>
#include <barrier>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

using namespace file_collector;

TEST(Concurrency, ConcurrentDisjointWritesToOneFile) {
    constexpr std::size_t parts = 8;
    constexpr std::size_t bytes_per_part = 4096;
    FileCollector collector(4);
    auto original = test_data::Pattern(parts * bytes_per_part);
    collector.CollectFile(1, original.size());
    std::barrier start(static_cast<std::ptrdiff_t>(parts));
    std::vector<std::jthread> producers;
    for (std::size_t part = 0; part < parts; ++part) {
        producers.emplace_back([&, part] {
            start.arrive_and_wait();
            const auto pos = part * bytes_per_part;
            collector.OnNewChunk(1, pos, test_data::Slice(original, pos, bytes_per_part));
        });
    }
    producers.clear();  // jthread joins
    EXPECT_EQ(*collector.GetFile(1), original);
}

TEST(Concurrency, ManyProducersOverlappingIdenticalData) {
    constexpr std::size_t producers_count = 8;
    FileCollector collector(4);
    auto original = test_data::Pattern(8192);
    collector.CollectFile(10, original.size());
    std::barrier start(static_cast<std::ptrdiff_t>(producers_count));
    std::vector<std::jthread> producers;
    for (std::size_t i = 0; i < producers_count; ++i) {
        producers.emplace_back([&, i] {
            start.arrive_and_wait();
            for (std::size_t pos = i * 97 % 512; pos < original.size(); pos += 4096) {
                const auto count = std::min<std::size_t>(5000, original.size() - pos);
                collector.OnNewChunk(10, pos, test_data::Slice(original, pos, count));
            }
        });
    }
    producers.clear();
    collector.OnNewChunk(10, 0, FileBytes(original));
    EXPECT_EQ(*collector.GetFile(10), original);
}

TEST(Concurrency, SeveralFilesRemainIndependent) {
    FileCollector collector(4);
    std::vector<FileBytes> originals;
    for (std::uint32_t id = 0; id < 6; ++id) {
        originals.push_back(test_data::Pattern(4096, 400 + id));
        collector.CollectFile(id, 4096);
    }
    std::barrier start(6);
    std::vector<std::jthread> producers;
    for (std::uint32_t id = 0; id < 6; ++id) {
        producers.emplace_back([&, id] {
            start.arrive_and_wait();
            collector.OnNewChunk(id, 2048, test_data::Slice(originals[id], 2048, 2048));
            collector.OnNewChunk(id, 0, test_data::Slice(originals[id], 0, 2048));
        });
    }
    producers.clear();
    for (std::uint32_t id = 0; id < 6; ++id) {
        EXPECT_EQ(*collector.GetFile(id), originals[id]);
    }
}

