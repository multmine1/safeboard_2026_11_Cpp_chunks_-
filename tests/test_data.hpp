#pragma once

#include "file_collector/file_collector.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>

namespace test_data {

using namespace file_collector;

inline FileBytes Pattern(std::size_t size, std::uint32_t seed = 0xC0FFEE) {
    std::mt19937 rng(seed);
    FileBytes bytes(size);
    std::generate(bytes.begin(), bytes.end(), [&] { return static_cast<std::uint8_t>(rng()); });
    return bytes;
}

inline FileBytes Slice(const FileBytes& data, std::size_t pos, std::size_t length) {
    return FileBytes(data.begin() + static_cast<std::ptrdiff_t>(pos),
                     data.begin() + static_cast<std::ptrdiff_t>(pos + length));
}

}  // namespace test_data

