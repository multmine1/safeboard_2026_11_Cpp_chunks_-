#include "file_state.hpp"

#include <algorithm>
#include <bit>
#include <stdexcept>
#include <utility>

namespace file_collector::detail {
namespace {

constexpr std::size_t kWordBits = 64;

std::uint64_t RangeMask(std::size_t word_index, std::size_t pos, std::size_t end) noexcept {
    const auto start = word_index * kWordBits;
    const auto low = pos > start ? pos - start : 0;
    const auto high = std::min(kWordBits, end - start);
    const auto low_mask = ~std::uint64_t{0} << low;
    const auto high_mask = high == kWordBits
                               ? ~std::uint64_t{0}
                               : (std::uint64_t{1} << high) - 1;
    return low_mask & high_mask;
}

std::size_t WordCount(std::size_t file_size) noexcept {
    return file_size / kWordBits + (file_size % kWordBits != 0);
}

}  // namespace

FileState::FileState(std::size_t file_size)
    : data_(std::make_shared<FileBytes>(file_size, 0)),
      written_(WordCount(file_size), 0),
      in_progress_(WordCount(file_size), 0),
      file_size_(file_size),
      ready_(file_size == 0) {}

void ValidateRange(std::size_t file_size, std::size_t pos, std::size_t length) {
    if (pos > file_size || length > file_size - pos) {
        throw std::out_of_range("chunk or snapshot exceeds registered file size");
    }
}

bool FileState::RangeWritten(std::size_t pos, std::size_t length) const {
    if (length == 0) {
        return true;
    }
    const auto end = pos + length;
    for (auto word = pos / kWordBits; word <= (end - 1) / kWordBits; ++word) {
        const auto required = RangeMask(word, pos, end);
        if ((written_[word] & required) != required) {
            return false;
        }
    }
    return true;
}

std::vector<WordReservation> FileState::Reserve(std::size_t pos, std::size_t length) {
    if (length == 0) {
        return {};
    }
    const auto first = pos / kWordBits;
    const auto last = (pos + length - 1) / kWordBits;
    std::vector<WordReservation> reservations;
    reservations.reserve(last - first + 1);  // Allocation before changing any mask.
    for (auto word = first; word <= last; ++word) {
        const auto incoming = RangeMask(word, pos, pos + length);
        const auto available = incoming & ~(written_[word] | in_progress_[word]);
        if (available == 0) {
            continue;
        }
        reservations.push_back({word, available});
        in_progress_[word] |= available;
    }
    return reservations;
}

void FileState::Commit(const std::vector<WordReservation>& reservations) noexcept {
    for (const auto& reservation : reservations) {
        written_[reservation.word_index] |= reservation.bits;
        in_progress_[reservation.word_index] &= ~reservation.bits;
        received_bytes_ += std::popcount(reservation.bits);
    }
    if (received_bytes_ == file_size_) {
        ready_ = true;
    }
}

void FileState::Rollback(const std::vector<WordReservation>& reservations) noexcept {
    for (const auto& reservation : reservations) {
        in_progress_[reservation.word_index] &= ~reservation.bits;
    }
}

void FileState::Reset(
    MutableFile fresh_data,
    std::vector<std::uint64_t> fresh_written,
    std::vector<std::uint64_t> fresh_in_progress) noexcept {
    data_ = std::move(fresh_data);
    written_ = std::move(fresh_written);
    in_progress_ = std::move(fresh_in_progress);
    received_bytes_ = 0;
    error_ = nullptr;
    ready_ = file_size_ == 0;
    closing_ = false;
}

void WaitIdle(const FileState& state) noexcept {
    auto pending = state.pending_tasks_.load(std::memory_order_acquire);
    while (pending != 0) {
        state.pending_tasks_.wait(pending, std::memory_order_acquire);
        pending = state.pending_tasks_.load(std::memory_order_acquire);
    }
}

void CompleteTask(FileState& state) noexcept {
    state.pending_tasks_.fetch_sub(1, std::memory_order_release);
    state.pending_tasks_.notify_all();
}

}  // namespace file_collector::detail

