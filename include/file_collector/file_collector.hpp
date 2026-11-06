#pragma once

#include "file_collector/i_file_collector.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace file_collector {
namespace detail { class FaultInjector; }
class FileCollectorImpl;

class FileCollector final : public IFileCollector {
public:
    explicit FileCollector(
        std::size_t workers = 0,
        std::shared_ptr<detail::FaultInjector> fault_injector = {});
    ~FileCollector() override;
    FileCollector(const FileCollector&) = delete;
    FileCollector& operator=(const FileCollector&) = delete;
    FileCollector(FileCollector&&) = delete;
    FileCollector& operator=(FileCollector&&) = delete;

    void CollectFile(std::uint32_t file_id, std::size_t file_size) override;
    void OnNewChunk(std::uint32_t file_id, std::size_t pos, FileBytes&& chunk) override;
    [[nodiscard]] FileView GetFile(std::uint32_t file_id, bool check_complete = true) override;
    [[nodiscard]] FileView GetFile(std::uint32_t file_id, std::size_t pos,
                                   std::size_t length, bool check_complete = true) override;
    [[nodiscard]] MutableFile TakeFile(std::uint32_t file_id, bool check_complete = true) override;
    void RemoveFile(std::uint32_t file_id) override;
    void ClearFile(std::uint32_t file_id) override;

private:
    std::unique_ptr<FileCollectorImpl> impl_;
};

}  // namespace file_collector

