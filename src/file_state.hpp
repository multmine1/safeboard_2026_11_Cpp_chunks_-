#pragma once

#include "file_collector/i_file_collector.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <vector>

namespace file_collector::detail {

struct WordReservation {
    std::size_t word_index;
    std::uint64_t bits;
};

class FileState final {
public:
    explicit FileState(std::size_t file_size);

    [[nodiscard]] bool RangeWritten(std::size_t pos, std::size_t length) const;
    [[nodiscard]] std::vector<WordReservation> Reserve(std::size_t pos, std::size_t length);
    void Commit(const std::vector<WordReservation>& reservations) noexcept;
    void Rollback(const std::vector<WordReservation>& reservations) noexcept;
    void Reset(MutableFile fresh_data,
               std::vector<std::uint64_t> fresh_written,
               std::vector<std::uint64_t> fresh_in_progress) noexcept;

    std::mutex mutex_;
    std::mutex lifecycle_mutex_;
    MutableFile data_;
    std::vector<std::uint64_t> written_;
    std::vector<std::uint64_t> in_progress_;
    std::size_t file_size_;
    std::size_t received_bytes_ = 0;
    std::atomic_size_t pending_tasks_{0};
    std::exception_ptr error_;
    bool ready_;
    bool closing_ = false;
    bool removed_ = false;
};

void WaitIdle(const FileState& state) noexcept;
void CompleteTask(FileState& state) noexcept;
void ValidateRange(std::size_t file_size, std::size_t pos, std::size_t length);

}  // namespace file_collector::detail

