#include "file_collector/file_collector.hpp"

#include "fault_injector.hpp"
#include "file_state.hpp"
#include "thread_pool.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <exception>
#include <mutex>
#include <ranges>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>

namespace file_collector {
namespace {

using detail::FileState;
using detail::WordReservation;

void Hit(const std::shared_ptr<detail::FaultInjector>& injector, detail::FaultPoint point) {
    if (injector) {
        injector->Hit(point);
    }
}

void CopyReserved(const FileState& state, std::size_t pos, const FileBytes& chunk,
                  const std::vector<WordReservation>& reservations) {
    for (const auto& reservation : reservations) {
        auto remaining = reservation.bits;
        while (remaining != 0) {
            const auto first_bit = std::countr_zero(remaining);
            const auto count = std::countr_one(remaining >> first_bit);
            const auto dst_pos = reservation.word_index * 64 + first_bit;
            std::ranges::copy_n(chunk.data() + (dst_pos - pos), count,
                                state.data_->data() + dst_pos);
            const auto run = count == 64
                                 ? ~std::uint64_t{0}
                                 : ((std::uint64_t{1} << count) - 1) << first_bit;
            remaining &= ~run;
        }
    }
}

void ProcessChunk(std::shared_ptr<FileState> state, std::size_t pos, FileBytes chunk,
                  std::shared_ptr<detail::FaultInjector> injector) noexcept {
    std::vector<WordReservation> reservations;
    try {
        {
            std::lock_guard lock(state->mutex_);
            reservations = state->Reserve(pos, chunk.size());
        }
        if (!reservations.empty()) {
            Hit(injector, detail::FaultPoint::AfterReserve);
            Hit(injector, detail::FaultPoint::BeforeCopy);
            CopyReserved(*state, pos, chunk, reservations);
            Hit(injector, detail::FaultPoint::BeforeCommit);
            std::lock_guard lock(state->mutex_);
            state->Commit(reservations);
        }
    } catch (...) {
        std::lock_guard lock(state->mutex_);
        state->Rollback(reservations);
        if (!state->error_) {
            state->error_ = std::current_exception();
        }
    }
    detail::CompleteTask(*state);
}

void CheckUsable(const FileState& state) {
    if (state.removed_ || state.closing_) {
        throw std::logic_error("file is being removed, taken, or cleared");
    }
    if (state.error_) {
        std::rethrow_exception(state.error_);
    }
}

template <class Action>
auto WithIdleFile(const std::shared_ptr<FileState>& state, Action action) {
    for (;;) {
        detail::WaitIdle(*state);
        std::unique_lock lock(state->mutex_);
        if (state->pending_tasks_.load(std::memory_order_acquire) != 0) {
            continue;
        }
        CheckUsable(*state);
        return action(*state);
    }
}

}  // namespace

class FileCollectorImpl final {
public:
    explicit FileCollectorImpl(std::size_t workers, std::shared_ptr<detail::FaultInjector> injector)
        : injector_(std::move(injector)),
          pool_(workers == 0 ? std::thread::hardware_concurrency() : workers) {}

    void CollectFile(std::uint32_t file_id, std::size_t file_size) {
        auto state = std::make_shared<FileState>(file_size);
        std::lock_guard lock(registry_mutex_);
        if (!files_.emplace(file_id, std::move(state)).second) {
            throw std::logic_error("file identifier is already registered");
        }
    }

    void OnNewChunk(std::uint32_t file_id, std::size_t pos, FileBytes&& chunk) {
        auto state = Find(file_id);
        detail::ValidateRange(state->file_size_, pos, chunk.size());
        {
            std::lock_guard lock(state->mutex_);
            CheckUsable(*state);
            if (state->ready_ || chunk.empty()) {
                return;
            }
            state->pending_tasks_.fetch_add(1, std::memory_order_relaxed);
        }
        try {
            Hit(injector_, detail::FaultPoint::BeforeEnqueue);
            pool_.Submit([state, pos, chunk = std::move(chunk), injector = injector_]() mutable {
                ProcessChunk(std::move(state), pos, std::move(chunk), std::move(injector));
            });
        } catch (...) {
            detail::CompleteTask(*state);
            throw;
        }
    }

    [[nodiscard]] FileView GetFile(std::uint32_t file_id, bool check_complete) {
        return WithIdleFile(Find(file_id), [check_complete](const FileState& state) -> FileView {
            if (check_complete && !state.ready_) {
                throw std::logic_error("file is incomplete");
            }
            if (state.ready_) {
                return state.data_;  // No byte copy for finalized files.
            }
            return std::make_shared<const FileBytes>(*state.data_);
        });
    }

    [[nodiscard]] FileView GetFile(std::uint32_t file_id, std::size_t pos,
                                   std::size_t length, bool check_complete) {
        auto state = Find(file_id);
        detail::ValidateRange(state->file_size_, pos, length);
        return WithIdleFile(state, [=](const FileState& locked) -> FileView {
            if (check_complete && !locked.RangeWritten(pos, length)) {
                throw std::logic_error("requested range is incomplete");
            }
            const auto first = locked.data_->begin() + static_cast<std::ptrdiff_t>(pos);
            return std::make_shared<const FileBytes>(first, first + static_cast<std::ptrdiff_t>(length));
        });
    }

    [[nodiscard]] MutableFile TakeFile(std::uint32_t file_id, bool check_complete) {
        auto state = Find(file_id);
        std::unique_lock lifecycle(state->lifecycle_mutex_);
        BeginClosing(*state);
        detail::WaitIdle(*state);
        MutableFile result;
        {
            std::lock_guard lock(state->mutex_);
            try {
                if (state->error_) {
                    std::rethrow_exception(state->error_);
                }
                if (check_complete && !state->ready_) {
                    throw std::logic_error("file is incomplete");
                }
                result = state->data_.use_count() == 1
                             ? std::move(state->data_)
                             : std::make_shared<FileBytes>(*state->data_);
                state->removed_ = true;
            } catch (...) {
                state->closing_ = false;
                throw;
            }
        }
        EraseSame(file_id, state);
        return result;
    }

    void RemoveFile(std::uint32_t file_id) {
        auto state = Find(file_id);
        std::unique_lock lifecycle(state->lifecycle_mutex_);
        BeginClosing(*state);
        detail::WaitIdle(*state);
        {
            std::lock_guard lock(state->mutex_);
            state->removed_ = true;
        }
        EraseSame(file_id, state);
    }

    void ClearFile(std::uint32_t file_id) {
        auto state = Find(file_id);
        std::unique_lock lifecycle(state->lifecycle_mutex_);
        BeginClosing(*state);
        detail::WaitIdle(*state);
        try {
            auto fresh = std::make_shared<FileBytes>(state->file_size_, 0);
            const auto words = state->file_size_ / 64 + (state->file_size_ % 64 != 0);
            std::vector<std::uint64_t> written(words, 0);
            std::vector<std::uint64_t> in_progress(words, 0);
            std::lock_guard lock(state->mutex_);
            state->Reset(std::move(fresh), std::move(written), std::move(in_progress));
        } catch (...) {
            std::lock_guard lock(state->mutex_);
            state->closing_ = false;
            throw;
        }
    }

private:
    [[nodiscard]] std::shared_ptr<FileState> Find(std::uint32_t file_id) const {
        std::lock_guard lock(registry_mutex_);
        const auto it = files_.find(file_id);
        if (it == files_.end()) {
            throw std::out_of_range("unknown file identifier");
        }
        return it->second;
    }

    void EraseSame(std::uint32_t file_id, const std::shared_ptr<FileState>& state) {
        std::lock_guard lock(registry_mutex_);
        const auto it = files_.find(file_id);
        if (it != files_.end() && it->second == state) {
            files_.erase(it);
        }
    }

    static void BeginClosing(FileState& state) {
        std::lock_guard lock(state.mutex_);
        if (state.removed_ || state.closing_) {
            throw std::logic_error("file is no longer registered or is closing");
        }
        state.closing_ = true;
    }

    mutable std::mutex registry_mutex_;
    std::unordered_map<std::uint32_t, std::shared_ptr<FileState>> files_;
    std::shared_ptr<detail::FaultInjector> injector_;
    detail::ThreadPool pool_;  // Destroyed first; tasks drain before registry destruction.
};

FileCollector::FileCollector(std::size_t workers, std::shared_ptr<detail::FaultInjector> fault_injector)
    : impl_(std::make_unique<FileCollectorImpl>(workers, std::move(fault_injector))) {}
FileCollector::~FileCollector() = default;
void FileCollector::CollectFile(std::uint32_t file_id, std::size_t file_size) {
    impl_->CollectFile(file_id, file_size);
}
void FileCollector::OnNewChunk(std::uint32_t file_id, std::size_t pos, FileBytes&& chunk) {
    impl_->OnNewChunk(file_id, pos, std::move(chunk));
}
FileView FileCollector::GetFile(std::uint32_t file_id, bool check_complete) {
    return impl_->GetFile(file_id, check_complete);
}
FileView FileCollector::GetFile(std::uint32_t file_id, std::size_t pos,
                                std::size_t length, bool check_complete) {
    return impl_->GetFile(file_id, pos, length, check_complete);
}
MutableFile FileCollector::TakeFile(std::uint32_t file_id, bool check_complete) {
    return impl_->TakeFile(file_id, check_complete);
}
void FileCollector::RemoveFile(std::uint32_t file_id) { impl_->RemoveFile(file_id); }
void FileCollector::ClearFile(std::uint32_t file_id) { impl_->ClearFile(file_id); }

}  // namespace file_collector

