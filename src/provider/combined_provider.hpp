#pragma once

#include "provider/provider.hpp"

#include <vector>

namespace nxmount::provider {

class CombinedProvider : public ReadOnlyBytesProvider {
public:
    CombinedProvider() = default;

    template <typename... Ts>
    CombinedProvider(Ts... args) {
        static_assert((... && std::is_convertible_v<Ts, UniqueProvider>));
        addProvider(std::move(args)...);
    }

    ~CombinedProvider() override = default;

    auto getSize() const -> std::size_t override { return mSize; }

    auto read(void* dst, std::size_t size, std::size_t offset) -> std::size_t override {
        if (size == 0 || offset >= mSize) {
            return 0;
        }

        auto buf = static_cast<std::uint8_t*>(dst);
        std::size_t current = 0;
        auto remaining = size;
        for (const auto& provider : mProviders) {
            if (current >= offset) {
                const auto segmentSize = provider->getSize();

                const auto readOffset = current - offset;
                if (readOffset > segmentSize) {
                    return 0;
                }

                const auto readSize = std::min(remaining, segmentSize - readOffset);

                if (provider->read(buf, readSize, readOffset) != readSize) {
                    return 0;
                }

                buf += readSize;
                current += readSize;
                offset += readSize;
                remaining -= readSize;
            } else {
                current += provider->getSize();
                continue;
            }
        }

        return size;
    }

    auto addProvider(UniqueProvider provider) -> void {
        mSize += provider->getSize();
        mProviders.emplace_back(std::move(provider));
    }

private:
    std::vector<UniqueProvider> mProviders;
    std::size_t mSize;
};

} // namespace nxmount::provider