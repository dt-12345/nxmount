#pragma once

#include "provider/provider.hpp"

#include <cstring>
#include <vector>

namespace nxmount::provider {

class MemoryStreamProvider final : public ReadOnlyBytesProvider {
public:
    using Buffer = std::vector<std::uint8_t>;

    explicit MemoryStreamProvider(Buffer&& data) : mData(std::move(data)) {}
    template <typename InputIt>
    MemoryStreamProvider(InputIt start, InputIt end) : mData(start, end) {}
    MemoryStreamProvider(IBytesProvider& provider, std::size_t size, std::size_t offset) : mData(GetDataSize(provider, size, offset)) {
        if (mData.size() > 0) {
            provider.read(mData.data(), size, offset);
        }
    }

    ~MemoryStreamProvider() override = default;

    auto getSize() const -> std::size_t override { return mData.size(); }

    auto read(void* dst, std::size_t size, std::size_t offset) -> std::size_t override {
        size = Clamp(size, offset, getSize());
        if (size == 0) {
            return 0;
        }
        std::memcpy(dst, mData.data() + offset, size);
        return size;
    }

private:
    [[nodiscard]] static auto GetDataSize(IBytesProvider& provider, std::size_t size, std::size_t offset) -> std::size_t {
        if (size == 0) {
            return 0;
        }
        const auto maxSize = provider.getSize();
        if (offset >= maxSize) {
            return 0;
        }
        if (offset + size > maxSize) {
            size = maxSize - offset;
        }
        return size;
    }

    Buffer mData;
};

} // namespace nxmount::provider