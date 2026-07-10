#pragma once

#include "formats/nca.hpp"
#include "provider/cache_provider.hpp"
#include "provider/provider.hpp"

#include <fmt/base.h>

#include <bit>

namespace nxmount::provider {

inline constexpr const std::size_t DYNAMIC = 0xffff'ffff'ffff'ffff;

template <std::size_t ALIGN, ProviderWrapper ProviderT = UniqueProvider>
class AlignedProvider final : public ReadOnlyBytesProvider {
    [[nodiscard]] static constexpr auto IsDynamicAlign() -> bool { return ALIGN == DYNAMIC; }

public:
    AlignedProvider(ProviderT provider, std::size_t align) requires(IsDynamicAlign()) : mProvider(std::move(provider)), mAlign(align) {}
    explicit AlignedProvider(ProviderT provider) requires(!IsDynamicAlign()) : mProvider(std::move(provider)) {}

    ~AlignedProvider() override = default;

    auto getSize() const -> std::size_t override { return mProvider->getSize(); }

    auto read(void* dst, std::size_t size, std::size_t offset) -> std::size_t override {
        if (offset >= getSize() || size == 0) {
            return 0;
        }
        
        if constexpr (IsDynamicAlign()) {
            auto block = std::vector<std::uint8_t>(getAlign());
            return readImpl(dst, size, offset, block.data());
        } else {
            std::uint8_t block[getAlign()];
            return readImpl(dst, size, offset, block);
        }
    }

private:
    struct Empty {};

    [[nodiscard]] auto getAlign() const -> std::size_t requires (IsDynamicAlign()) {
        return mAlign;
    }

    [[nodiscard]] constexpr static auto getAlign() -> std::size_t requires (!IsDynamicAlign()) {
        return ALIGN;
    }

    auto readImpl(void* dst, std::size_t size, std::size_t offset, std::uint8_t* block) -> std::size_t {
        auto buf = static_cast<std::uint8_t*>(dst);
        const auto aligned = (offset + getAlign() - 1) / getAlign() * getAlign();
        const auto headSize = aligned - offset;
        auto remaining = size;

        if (headSize > 0) {
            if (aligned == 0) {
                return 0;
            }

            const auto headStart = aligned - getAlign();
            if (mProvider->read(block, getAlign(), headStart) != getAlign()) {
                return 0;
            }

            const auto headOffset = getAlign() - headSize;
            const auto readSize = std::min(size, headSize);
            std::memcpy(buf, std::addressof(block[headOffset]), readSize);
            buf += readSize;
            remaining -= readSize;
            if (remaining == 0) {
                return size;
            }
        }

        auto currentOffset = aligned;
        while (remaining > 0) {
            if (remaining >= getAlign()) { // if there's more than enough room, read as many aligned blocks as possible
                const auto readSize = remaining / getAlign() * getAlign();
                if (mProvider->read(buf, readSize, currentOffset) != readSize) {
                    return 0;
                }
                buf += readSize;
                remaining -= readSize;
                currentOffset += readSize;
            } else {
                if (mProvider->read(block, getAlign(), currentOffset) != getAlign()) {
                    return 0;
                }
                std::memcpy(buf, block, std::min(getAlign(), remaining));
                break;
            }
        }

        return size;
    }

    ProviderT mProvider;
#if defined(_MSC_VER)
    [[msvc::no_unique_address]] const std::conditional_t<ALIGN == DYNAMIC, std::size_t, Empty> mAlign{};
#else
    [[no_unique_address]] const std::conditional_t<ALIGN == DYNAMIC, std::size_t, Empty> mAlign{};
#endif
};

ALWAYS_INLINE auto MakeIv(std::uint8_t* iv, const formats::AesCtrUpperIv& upper, std::int64_t offset) -> void {
    *reinterpret_cast<std::uint64_t*>(iv) = std::byteswap(upper.value);
    *reinterpret_cast<std::int64_t*>(iv + sizeof(std::uint64_t)) = std::byteswap(offset);
}

class AesCtrProvider final : public ReadOnlyBytesProvider {
public:
    static constexpr const auto cBlockSize = 0x10;

    AesCtrProvider(UniqueProvider provider, const std::uint8_t* key, const formats::AesCtrUpperIv& upperIv, std::size_t offset) : mProvider(std::move(provider)) {
        std::memcpy(mKey, key, sizeof(mKey));
        MakeIv(mIv, upperIv, offset / cBlockSize);
    }

    ~AesCtrProvider() override = default;

    auto getSize() const -> std::size_t override { return mProvider->getSize(); }

    auto read(void* dst, std::size_t size, std::size_t offset) -> std::size_t override;

private:
    UniqueProvider mProvider;
    std::uint8_t mKey[cBlockSize];
    std::uint8_t mIv[cBlockSize];
};

class AesCtrExProvider final : public ReadOnlyBytesProvider {
public:
    struct Entry {
        enum class Encryption : std::uint8_t {
            Encrypted       = 0,
            NotEncrypted    = 1,
        };

        std::int64_t offset;
        Encryption encrypted;
        std::uint8_t reserved[3];
        std::int32_t generation;
    };

    static constexpr const auto cBlockSize = 0x10;
    static constexpr const std::size_t cNodeSize = 0x4000;
    static constexpr const std::size_t cEntrySize = sizeof(Entry);

    AesCtrExProvider(
        UniqueProvider provider, const std::uint8_t* key, std::uint32_t secureValue, std::size_t offset,
        std::size_t entryCount, UniqueProvider nodeProvider, UniqueProvider entryProvider
    ) : mProvider(std::move(provider)), mSecureValue(secureValue), mOffset(offset), mBucketTree(
        cNodeSize, cEntrySize, static_cast<std::int32_t>(entryCount), std::move(nodeProvider), std::move(entryProvider)
    ) {
        std::memcpy(mKey, key, sizeof(mKey));
    }

    ~AesCtrExProvider() override = default;

    auto getSize() const -> std::size_t override {
        return static_cast<std::size_t>(std::max(mBucketTree.getEndAddress(), std::int64_t(0)));
    }

    auto read(void* dst, std::size_t size, std::size_t offset) -> std::size_t override;

private:
    UniqueProvider mProvider;
    std::uint8_t mKey[0x10];
    const std::uint32_t mSecureValue;
    const std::size_t mOffset;
    formats::BucketTree mBucketTree;
};

class Sha256Provider final : public ReadOnlyBytesProvider {
public:
    static constexpr const auto cLayerCount = 2;
    static constexpr const std::size_t cHashSize = 0x20;

    Sha256Provider(UniqueProvider provider, const formats::HashData::HierarchicalSha256HashData& hashData);

    ~Sha256Provider() override = default;

    auto getSize() const -> std::size_t override { return mProvider->getSize(); }

    auto read(void* dst, std::size_t size, std::size_t offset) -> std::size_t override;

private:
    UniqueProvider mProvider;
    std::vector<std::uint8_t> mHashRegion;
    std::size_t mBlockSize;
    std::size_t mDataToHashRatio;
};

class IntegrityVerificationProvider final : public ReadOnlyBytesProvider {
public:
    static constexpr const std::size_t cHashSize = 0x20;
    static constexpr const std::size_t cMaxHashCount = 0x4000;

    static constexpr const std::size_t cHashCacheSize = 32;
    static constexpr const std::size_t cDataCacheSize = 32;

    using HashProvider = CacheProvider<cHashCacheSize>;
    using DataProvider = CacheProvider<cDataCacheSize>;

    IntegrityVerificationProvider(UniqueProvider hashProvider, UniqueProvider dataProvider, std::size_t blockOrder);

    ~IntegrityVerificationProvider() override = default;

    auto getSize() const -> std::size_t override { return mDataProvider->getSize(); }

    auto read(void* dst, std::size_t size, std::size_t offset) -> std::size_t override;

private:
    UniqueProvider mHashProvider;
    UniqueProvider mDataProvider;
    std::size_t mBlockOrder;
};

class HierarchicalIntegrityVerificationProvider : public ReadOnlyBytesProvider {
public:
    static constexpr const auto cMinLayers = 2u;
    static constexpr const auto cMaxLayers = 7u;

    HierarchicalIntegrityVerificationProvider(SharedProvider provider, const formats::HashData::IntegrityMetaInfo& hashData, std::size_t layerInfoOffset);

    ~HierarchicalIntegrityVerificationProvider() override = default;

    auto getSize() const -> std::size_t override { return mProvider->getSize(); }

    auto read(void* dst, std::size_t size, std::size_t offset) -> std::size_t override;

private:
    using LayerProvider = AlignedProvider<DYNAMIC, std::unique_ptr<IntegrityVerificationProvider>>;

    UniqueProvider mProvider; // last level provider
};

class IndirectProvider : public ReadOnlyBytesProvider {
public:
    PACKED_STRUCT(Entry {
        std::int64_t virtualOffset;
        std::int64_t physicalOffset;
        std::int32_t storageIndex;
    });
    static_assert(sizeof(Entry) == 0x14);

    static constexpr const auto cStorageCount = 2;
    static constexpr const std::size_t cNodeSize = 0x4000;
    static constexpr const std::size_t cEntrySize = sizeof(Entry);

    IndirectProvider(SharedProvider provider1, SharedProvider provider2, std::size_t entryCount, UniqueProvider nodeProvider, UniqueProvider entryProvider);

    ~IndirectProvider() override = default;

    auto getSize() const -> std::size_t override {
        return static_cast<std::size_t>(std::max(mBucketTree.getEndAddress(), std::int64_t(0)));
    }

    auto read(void* dst, std::size_t size, std::size_t offset) -> std::size_t override;

private:
    SharedProvider mProviders[cStorageCount];
    formats::BucketTree mBucketTree;
};

} // namespace nxmount::provider