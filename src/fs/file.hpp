#pragma once

#include "common/errors.hpp"
#include "fs/node.hpp"

#include <cstddef>

namespace nxmount::fs {

class IFile : public INode {
public:
    [[nodiscard]] auto getType() const -> Type override { return Type::File; }

    ~IFile() override = default;

    virtual auto read(void* dst, std::size_t size, std::size_t offset) const -> std::size_t = 0;
    virtual auto write(const void* src, std::size_t size, std::size_t offset) -> std::size_t = 0;

    virtual auto getSize() const -> std::size_t = 0;
    virtual auto setSize(std::size_t size) -> Result = 0;

    virtual auto flush() -> Result = 0;
    virtual auto sync(bool flushMetadata) -> Result = 0;
};

class ReadOnlyFileBase : public IFile {
    auto write(const void*, std::size_t, std::size_t) -> std::size_t override final { return 0; }

    auto setSize(std::size_t) -> Result override final { return READ_ONLY; }

    auto flush() -> Result override final { return SUCCESS; }
    auto sync(bool) -> Result override final { return SUCCESS; }
};

} // namepsace nxmount::fs