#pragma once

#include "provider/provider.hpp"

namespace nxmount::provider {

template <ProviderWrapper ProviderT = std::unique_ptr<IBytesProvider>>
class OffsetProvider final : public ReadOnlyBytesProvider {
public:
    static constexpr const std::size_t cAutoSize = 0xffff'ffff'ffff'ffff;

    explicit OffsetProvider(ProviderT provider, std::size_t offset, std::size_t size = cAutoSize) :
        mProvider(std::move(provider)),
        mOffset(offset >= mProvider->getSize() ? mProvider->getSize() : offset),
        mSize(size == cAutoSize ? mProvider->getSize() - mOffset : std::min(size, mProvider->getSize() - mOffset)) {}

    ~OffsetProvider() override = default;

    auto getSize() const -> std::size_t override { return mSize; }

    auto read(void* dst, std::size_t size, std::size_t offset) -> std::size_t override {
        size = Clamp(size, offset, mSize);
        if (size == 0) {
            return 0;
        }
        return mProvider->read(dst, size, mOffset + offset);
    }

private:
    ProviderT mProvider;
    const std::size_t mOffset;
    const std::size_t mSize;
};

using UniqueOffsetProvider = OffsetProvider<std::unique_ptr<IBytesProvider>>;
using SharedOffsetProvider = OffsetProvider<std::shared_ptr<IBytesProvider>>;

} // namespace nxmount::provider