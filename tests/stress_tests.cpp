#include "test_data.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <barrier>
#include <cstdint>
#include <exception>
#include <random>
#include <thread>
#include <vector>

using namespace file_collector;

namespace {

struct Chunk {
    std::size_t pos;
    FileBytes bytes;
};

std::vector<Chunk> MakeChunks(const FileBytes& original, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::vector<Chunk> chunks;
    for (std::size_t pos = 0; pos < original.size(); pos += 193) {
        const std::size_t length = std::min<std::size_t>(257, original.size() - pos);
        chunks.push_back({pos, test_data::Slice(original, pos, length)});
        if (rng() % 3 == 0) {
            chunks.push_back({pos, test_data::Slice(original, pos, length)});
        }
    }
    std::shuffle(chunks.begin(), chunks.end(), rng);
    return chunks;
}

std::vector<Chunk> MakeRandomOverlappingChunks(const FileBytes& original, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::vector<Chunk> chunks;
    if (original.empty()) {
        return chunks;
    }

    // A randomized partition guarantees complete coverage independently of overlaps.
    for (std::size_t pos = 0; pos < original.size();) {
        const auto maximum = std::min<std::size_t>(4096, original.size() - pos);
        const auto length = std::uniform_int_distribution<std::size_t>(1, maximum)(rng);
        chunks.push_back({pos, test_data::Slice(original, pos, length)});
        if (rng() % 5 == 0) {
            chunks.push_back(chunks.back());  // Full duplicate.
        }
        pos += length;
    }

    // Independent random windows create additional arbitrary partial overlaps.
    const auto windows = std::min<std::size_t>(4096, original.size() / 31 + 25);
    for (std::size_t i = 0; i < windows; ++i) {
        const auto maximum = std::min<std::size_t>(8192, original.size());
        const auto length = std::uniform_int_distribution<std::size_t>(1, maximum)(rng);
        const auto pos = std::uniform_int_distribution<std::size_t>(0, original.size() - length)(rng);
        chunks.push_back({pos, test_data::Slice(original, pos, length)});
    }

    // Explicitly guarantee 1-byte, weak, medium, heavy and full intersections.
    if (original.size() >= 4096) {
        constexpr std::size_t anchor_length = 1024;
        const auto anchor = std::uniform_int_distribution<std::size_t>(
            0, original.size() - 2 * anchor_length)(rng);
        chunks.push_back({anchor, test_data::Slice(original, anchor, anchor_length)});
        for (const auto overlap : std::array<std::size_t, 8>{1, 102, 256, 512, 768, 922, 1023, 1024}) {
            const auto pos = anchor + anchor_length - overlap;
            chunks.push_back({pos, test_data::Slice(original, pos, anchor_length)});
        }
    }

    std::shuffle(chunks.begin(), chunks.end(), rng);
    return chunks;
}

void SubmitRandomChunks(FileCollector& collector, const std::vector<Chunk>& chunks,
                        std::uint32_t file_id, std::size_t producers_count) {
    std::barrier start(static_cast<std::ptrdiff_t>(producers_count));
    std::vector<std::exception_ptr> errors(producers_count);
    std::vector<std::jthread> producers;
    for (std::size_t producer = 0; producer < producers_count; ++producer) {
        producers.emplace_back([&, producer] {
            start.arrive_and_wait();
            try {
                for (std::size_t index = producer; index < chunks.size(); index += producers_count) {
                    collector.OnNewChunk(file_id, chunks[index].pos, FileBytes(chunks[index].bytes));
                }
            } catch (...) {
                errors[producer] = std::current_exception();
            }
        });
    }
    producers.clear();  // Join all producers before propagating errors.
    for (const auto& error : errors) {
        if (error) {
            std::rethrow_exception(error);
        }
    }
}

}  // namespace

TEST(Stress, DeterministicMultiProducerOverlaps) {
    constexpr std::size_t file_size = 1024 * 1024;
    for (std::uint32_t seed : {101u, 202u, 303u}) {
        SCOPED_TRACE(seed);
        auto original = test_data::Pattern(file_size, seed);
        auto chunks = MakeChunks(original, seed);
        FileCollector collector(4);
        collector.CollectFile(1, file_size);
        std::barrier start(8);
        std::vector<std::jthread> senders;
        for (std::size_t worker = 0; worker < 8; ++worker) {
            senders.emplace_back([&, worker] {
                start.arrive_and_wait();
                for (std::size_t index = worker; index < chunks.size(); index += 8) {
                    collector.OnNewChunk(1, chunks[index].pos, FileBytes(chunks[index].bytes));
                }
            });
        }
        senders.clear();
        EXPECT_EQ(*collector.GetFile(1), original);
    }
}

TEST(Stress, ThousandsOfRepeatedOverlaps) {
    auto original = test_data::Pattern(8193, 77);
    FileCollector collector(4);
    collector.CollectFile(1, original.size());
    for (int repeat = 0; repeat < 2000; ++repeat) {
        const std::size_t pos = static_cast<std::size_t>(repeat * 31) % (original.size() - 128);
        collector.OnNewChunk(1, pos, test_data::Slice(original, pos, 128));
    }
    collector.OnNewChunk(1, 0, FileBytes(original));
    EXPECT_EQ(*collector.GetFile(1), original);
}

TEST(Stress, RandomizedOverlapSpectrumAcrossSizesAndSeeds) {
    constexpr std::array<std::size_t, 8> file_sizes{0, 1, 63, 64, 65, 4096, 65537, 1048576};
    constexpr std::array<std::uint32_t, 4> seeds{7, 101, 2026, 0xC0FFEE};
    for (const auto file_size : file_sizes) {
        for (const auto seed : seeds) {
            SCOPED_TRACE(::testing::Message() << "size=" << file_size << " seed=" << seed);
            const auto original = test_data::Pattern(file_size, seed);
            const auto chunks = MakeRandomOverlappingChunks(original, seed);
            FileCollector collector(4);
            collector.CollectFile(77, file_size);
            SubmitRandomChunks(collector, chunks, 77, 8);
            EXPECT_EQ(*collector.GetFile(77), original);
        }
    }
}

TEST(Stress, RandomizedSnapshotsStayImmutableDuringConcurrentIngestion) {
    constexpr std::size_t file_size = 65537;
    auto original = test_data::Pattern(file_size, 0xBADC0DE);
    auto chunks = MakeRandomOverlappingChunks(original, 0xBADC0DE);
    FileCollector collector(4);
    collector.CollectFile(21, file_size);
    const auto initial_snapshot = collector.GetFile(21, false);
    EXPECT_EQ(*initial_snapshot, (FileBytes(file_size, 0)));

    std::barrier start(5);
    std::array<std::exception_ptr, 5> errors;
    std::vector<FileView> snapshots;
    std::vector<FileBytes> expected_snapshots;
    std::vector<std::jthread> participants;
    for (std::size_t producer = 0; producer < 4; ++producer) {
        participants.emplace_back([&, producer] {
            start.arrive_and_wait();
            try {
                for (std::size_t index = producer; index < chunks.size(); index += 4) {
                    collector.OnNewChunk(21, chunks[index].pos, FileBytes(chunks[index].bytes));
                }
            } catch (...) {
                errors[producer] = std::current_exception();
            }
        });
    }
    participants.emplace_back([&] {
        start.arrive_and_wait();
        try {
            for (int i = 0; i < 32; ++i) {
                auto view = collector.GetFile(21, false);
                expected_snapshots.push_back(*view);
                snapshots.push_back(std::move(view));
            }
        } catch (...) {
            errors[4] = std::current_exception();
        }
    });
    participants.clear();
    for (const auto& error : errors) {
        if (error) {
            std::rethrow_exception(error);
        }
    }

    EXPECT_EQ(*collector.GetFile(21), original);
    EXPECT_EQ(*initial_snapshot, (FileBytes(file_size, 0)));
    for (std::size_t index = 0; index < snapshots.size(); ++index) {
        EXPECT_EQ(*snapshots[index], expected_snapshots[index]);
    }
}

