#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace file_collector {

using FileBytes = std::vector<std::uint8_t>;
using FileView = std::shared_ptr<const FileBytes>;
using MutableFile = std::shared_ptr<FileBytes>;

class IFileCollector {
public:
    virtual ~IFileCollector() = default;

    virtual void CollectFile(std::uint32_t file_id, std::size_t file_size) = 0;
    virtual void OnNewChunk(std::uint32_t file_id, std::size_t pos, FileBytes&& chunk) = 0;
    [[nodiscard]] virtual FileView GetFile(std::uint32_t file_id, bool check_complete = true) = 0;
    [[nodiscard]] virtual FileView GetFile(
        std::uint32_t file_id, std::size_t pos, std::size_t length,
        bool check_complete = true) = 0;
    [[nodiscard]] virtual MutableFile TakeFile(std::uint32_t file_id, bool check_complete = true) = 0;
    virtual void RemoveFile(std::uint32_t file_id) = 0;
    virtual void ClearFile(std::uint32_t file_id) = 0;
};

}  // namespace file_collector

