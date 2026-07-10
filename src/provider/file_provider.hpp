#pragma once

#include "fs/file.hpp"
#include "provider/provider.hpp"

#include <memory>

namespace nxmount::provider {

class FileProvider final : public ReadOnlyBytesProvider {
public:
    explicit FileProvider(std::unique_ptr<fs::IFile> file) : mFile(std::move(file)) {}

    ~FileProvider() override = default;

    auto getSize() const -> std::size_t override { return mFile->getSize(); }

    auto read(void* dst, std::size_t size, std::size_t offset) -> std::size_t override {
        return mFile->read(dst, size, offset);
    }

private:
    std::unique_ptr<fs::IFile> mFile;
};

} // namespace nxmount::provider