#include "test_data.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>
#if defined(__APPLE__) || defined(__linux__)
#include <sys/resource.h>
#endif

using namespace file_collector;

namespace {

double PeakMiB() {
#if defined(__APPLE__) || defined(__linux__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
#if defined(__APPLE__)
        return static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
#else
        return static_cast<double>(usage.ru_maxrss) / 1024.0;
#endif
    }
#endif
    return -1.;
}

}  // namespace

TEST(Performance, BaselineMiBPerSecondAndSubmissionLatency) {
    using Clock = std::chrono::steady_clock;
    constexpr std::size_t size = 4 * 1024 * 1024;
    constexpr std::size_t chunk_size = 16 * 1024;
    auto original = test_data::Pattern(size, 1001);
    FileCollector collector(4);
    collector.CollectFile(1, size);
    std::vector<double> submit_microseconds;
    submit_microseconds.reserve(size / chunk_size);
    const auto start = Clock::now();
    for (std::size_t pos = 0; pos < size; pos += chunk_size) {
        auto chunk = test_data::Slice(original, pos, chunk_size);
        const auto t0 = Clock::now();
        collector.OnNewChunk(1, pos, std::move(chunk));
        submit_microseconds.push_back(std::chrono::duration<double, std::micro>(Clock::now() - t0).count());
    }
    auto restored = collector.GetFile(1);
    const auto elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    ASSERT_EQ(*restored, original);
    std::sort(submit_microseconds.begin(), submit_microseconds.end());
    const auto percentile = [&](double fraction) {
        const auto index = static_cast<std::size_t>((submit_microseconds.size() - 1) * fraction);
        return submit_microseconds[index];
    };
    std::cout << std::fixed << std::setprecision(2)
              << "profile: size_mib=4 chunk_kib=16 workers=4 seed=1001"
              << " throughput_mib_s=" << 4.0 / elapsed
              << " submission_p50_us=" << percentile(0.50)
              << " submission_p95_us=" << percentile(0.95)
              << " peak_process_rss_mib=" << PeakMiB()
              << " duplicate_fraction=0" << '\n';
    // Metrics are informational, with no machine-specific timing thresholds.
}

