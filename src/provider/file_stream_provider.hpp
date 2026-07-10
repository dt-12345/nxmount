#pragma once

#include "common/mem_mapped_file.hpp"
#include "provider/provider.hpp"

namespace nxmount::provider {

class FileStreamProvider final : public ReadOnlyBytesProvider {
public:
    explicit FileStreamProvider(std::string_view path) : mFile(path) {}

    ~FileStreamProvider() override = default;

    auto getSize() const -> std::size_t override { return mFile.getSize(); }

    auto read(void* dst, std::size_t size, std::size_t offset) -> std::size_t override {
        return mFile.read(dst, size, offset);
    }

    [[nodiscard]] auto isOpen() const -> bool { return mFile.isOpen(); }

private:
    common::MemMappedFile mFile;
};

} // namespace nxmount::provider