#pragma once

#include "fs/file.hpp"

namespace nxmount::fs {

class NullFile final : public ReadOnlyFileBase {
public:
    NullFile() = default;

    [[nodiscard]] auto getName() const -> std::string_view override { return "__NULL_FILE__"; }

    ~NullFile() = default;

    auto read(void*, std::size_t, std::size_t) const -> std::size_t override { return 0; }
    auto getSize() const -> std::size_t override { return 0; }
};

} // namespace nxmount::fs