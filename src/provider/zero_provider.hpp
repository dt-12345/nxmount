#pragma once

#include "provider/provider.hpp"

#include <cstring>
#include <limits>

namespace nxmount::provider {

class ZeroProvider final : public ReadOnlyBytesProvider {
public:
    ZeroProvider() = default;

    ~ZeroProvider() override = default;

    auto getSize() const -> std::size_t override { return std::numeric_limits<std::size_t>::max(); }

    auto read(void* dst, std::size_t size, std::size_t /* offset */) -> std::size_t override {
        std::memset(dst, 0, size);
        return size;
    }

};

} // namespace nxmount::provider