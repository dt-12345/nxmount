#include "crypto/crypto.hpp"
#include "provider/atmosphere.hpp"
#include "log/logging.hpp"
#include "provider/memory_stream_provider.hpp"
#include "provider/offset_provider.hpp"
#include "provider/provider.hpp"

#include <stdexcept>

namespace nxmount::provider {

auto AesCtrProvider::read(void* dst, std::size_t size, std::size_t offset) -> std::size_t {
    if (offset >= getSize() || size == 0) {
        return 0;
    }

    if (offset % cBlockSize != 0 || size % cBlockSize != 0) {
        return 0;
    }

    auto buffer = std::make_unique<std::uint8_t[]>(size);

    const auto readSize = mProvider->read(buffer.get(), size, offset);
    if (readSize == 0) {
        return 0;
    }

    if (crypto::AesCtrDecrypt(dst, buffer.get(), size, mKey, sizeof(mKey), mIv, offset)) {
        if (size > readSize) {
            std::memset(static_cast<std::uint8_t*>(dst) + readSize, 0, size - readSize);
        }
        return size;
    }

    LOG_WARNING("AES-CTR: Failed to decrypt region from {:#x} to {:#x}", offset, offset + size);
    return 0;
}

auto AesCtrExProvider::read(void* dst, std::size_t size, std::size_t offset) -> std::size_t {
    if (!mBucketTree.isInitialized()) {
        return 0;
    }
    
    if (offset >= getSize() || size == 0) {
        return 0;
    }

    if (offset + size > getSize()) {
        size = getSize() - offset;
    }

    if (offset % cBlockSize != 0 || size % cBlockSize != 0) {
        return 0;
    }

    if (!mBucketTree.contains(static_cast<std::int64_t>(offset), static_cast<std::int64_t>(size))) {
        return 0;
    }

    auto buffer = static_cast<std::uint8_t*>(dst);

    if (mProvider->read(buffer, size, offset) != size) {
        return 0;
    }

    auto loc = mBucketTree.findEntry(static_cast<std::int64_t>(offset));

    auto curDst = static_cast<std::uint8_t*>(dst);
    auto curSrc = buffer;
    auto curOffset = static_cast<std::int64_t>(offset);
    const auto end = static_cast<std::int64_t>(offset + size);
    while (curOffset < end) {
        Entry entry;
        if (!loc.get(entry)) {
            return 0;
        }

        if (entry.offset > curOffset) {
            return 0;
        }

        std::int64_t nextOffset;
        if (loc.hasNext()) {
            if (!loc.next()) {
                return 0;
            }
            nextOffset = loc.get<Entry>().offset;
            if (!mBucketTree.contains(nextOffset)) {
                return 0;
            }
        } else {
            nextOffset = mBucketTree.getEndAddress();
        }

        if (nextOffset % cBlockSize != 0 || nextOffset <= curOffset) {
            return 0;
        }

        const auto offsetInCurrentEntry = curOffset - entry.offset;
        const auto sizeOfCurrentEntry = (nextOffset - entry.offset) - offsetInCurrentEntry;
        const auto sizeToCopy = std::min(sizeOfCurrentEntry, end - curOffset);

        if (entry.encrypted == Entry::Encryption::Encrypted) {
            const auto ctrOffset = (mOffset + entry.offset + offsetInCurrentEntry) / cBlockSize;
            formats::AesCtrUpperIv upperIv { .generation = static_cast<std::uint32_t>(entry.generation), .secureValue = mSecureValue };
            std::uint8_t iv[0x10];
            MakeIv(iv, upperIv, ctrOffset);

            if (!crypto::AesCtrDecrypt(curDst, curSrc, sizeToCopy, mKey, sizeof(mKey), iv, 0)) {
                return 0;
            }
        }

        curDst += sizeToCopy;
        curSrc += sizeToCopy;
        curOffset += sizeToCopy;
    }

    return size;
}

Sha256Provider::Sha256Provider(UniqueProvider provider, const formats::HashData::HierarchicalSha256HashData& hashData) {
    if (hashData.layerCount != cLayerCount) {
        throw std::runtime_error("HierarchicalSha256HashData has an unexpected layer count!");
    }

    if (hashData.blockSize < cHashSize || hashData.blockSize % cHashSize != 0) {
        throw std::runtime_error("Invalid block size!");
    }

    mBlockSize = hashData.blockSize;
    mDataToHashRatio = mBlockSize / cHashSize;

    const auto& hashLayer = hashData.layerRegions[0];
    const auto& dataLayer = hashData.layerRegions[1];

    const auto size = provider->getSize();
    if (hashLayer.size == 0 || hashLayer.offset >= size || hashLayer.offset + hashLayer.size > size) {
        LOG_ERROR("Hash Layer Size: {:#x}, Hash Layer Offset: {:#x}, Total Size: {:#x}", hashLayer.size, hashLayer.offset, size);
        throw std::runtime_error("Invalid hash layer!");
    }
    if (dataLayer.size == 0 || dataLayer.offset >= size || dataLayer.offset + dataLayer.size > size) {
        throw std::runtime_error("Invalid data layer!");
    }

    mHashRegion = std::vector<std::uint8_t>(hashLayer.size);
    if (provider->read(mHashRegion.data(), mHashRegion.size(), hashLayer.offset) != mHashRegion.size()) {
        throw std::runtime_error("Failed to read hash region!");
    }

    if (!crypto::Sha256Verify(mHashRegion.data(), mHashRegion.size(), hashData.masterHash)) {
        throw std::runtime_error("Hash region is corrupted!");
    }

    mProvider = std::make_unique<UniqueOffsetProvider>(std::move(provider), dataLayer.offset, dataLayer.size);
}

template <std::size_t HashSize>
[[nodiscard]] ALWAYS_INLINE auto IsHashNull(const std::uint8_t* hash) -> bool {
    for (std::size_t i = 0; i < HashSize; ++i) {
        if (hash[i] != 0) {
            return false;
        }
    }

    return true;
}

auto Sha256Provider::read(void* dst, std::size_t size, std::size_t offset) -> std::size_t {
    if (offset >= getSize() || size == 0) {
        return 0;
    }

    if (offset % mBlockSize != 0 || size % mBlockSize != 0) {
        return 0;
    }

    const auto readSize = std::min(getSize() - offset, size);
    if (mProvider->read(dst, readSize, offset) != readSize) {
        return 0;
    }

    auto current = offset;
    auto remaining = readSize;
    while (remaining > 0) {
        const auto blockSize = std::min(mBlockSize, remaining);
        const auto hashOffset = current / mDataToHashRatio;
        if (!IsHashNull<cHashSize>(std::addressof(mHashRegion[hashOffset]))) {
            if (!crypto::Sha256Verify(static_cast<const std::uint8_t*>(dst) + current - offset, blockSize, std::addressof(mHashRegion[hashOffset]))) {
                LOG_WARNING("Hierarchical SHA256 validation failed! {:#x}", current);
                return 0;
            } else {
                std::memset(std::addressof(mHashRegion[hashOffset]), 0, cHashSize);
            }
        }

        current += blockSize;
        remaining -= blockSize;
    }

    if (size > readSize) {
        std::memset(static_cast<std::uint8_t*>(dst) + readSize, 0, size - readSize);
    }

    return size;
}

IntegrityVerificationProvider::IntegrityVerificationProvider(
    UniqueProvider hashProvider, UniqueProvider dataProvider, std::size_t blockOrder
) : mHashProvider(std::move(hashProvider)), mDataProvider(std::move(dataProvider)), mBlockOrder(blockOrder) {
    if (mHashProvider->getSize() / cHashSize * (1ull << mBlockOrder) < mDataProvider->getSize()) {
        LOG_ERROR("IntegrityVerification layer's hash region is too small for the provided data region");
        throw std::runtime_error("IntegrityVerificationProvider");
    }
}

auto IntegrityVerificationProvider::read(void* dst, std::size_t size, std::size_t offset) -> std::size_t {
    const auto dataSize = getSize();
    if (size == 0 || offset >= dataSize) {
        return 0;
    }

    const auto blockSize = static_cast<std::size_t>(1ull << mBlockOrder);
    const auto sizeMask = blockSize - 1;

    if ((offset & sizeMask) != 0 || (size & sizeMask) != 0) {
        return 0;
    }

    auto buf = static_cast<std::uint8_t*>(dst);
    auto readSize = size;
    if (offset + size >= dataSize) {
        readSize = dataSize - offset;
        const auto extraSize = size - readSize;
        if (extraSize > blockSize) {
            return 0;
        }

        std::memset(buf + readSize, 0, extraSize);
    }

    if (mDataProvider->read(buf, readSize, offset) != readSize) {
        return 0;
    }

    const auto count = size >> mBlockOrder;
    const auto capacity = std::min(cMaxHashCount, count);
    auto hashBuf = std::vector<std::uint8_t>(cHashSize * capacity);
    std::size_t current = 0, remaining = count;
    while (current < count) {
        const auto hashCount = std::min(capacity, remaining);
        const auto hashOffset = ((offset + (current << mBlockOrder)) >> mBlockOrder) * cHashSize;
        const auto hashSize = hashCount * cHashSize;
        if (mHashProvider->read(hashBuf.data(), hashSize, hashOffset) != hashSize) {
            return 0;
        }

        for (std::size_t i = 0; i < hashCount; ++i) {
            if (!crypto::Sha256Verify(buf + ((current + i) << mBlockOrder), blockSize, hashBuf.data() + i * cHashSize)) {
                LOG_WARNING("Hierarchical integrity verification failed! {:#x} {}", offset + ((current + i) << mBlockOrder), i);
                std::uint8_t hash[cHashSize];
                crypto::Sha256(buf + ((current + i) << mBlockOrder), blockSize, hash);
                LOG_WARNING("Got: {}", hash);
                std::memcpy(hash, hashBuf.data() + i * cHashSize, sizeof(hash));
                LOG_WARNING("Wanted: {}", hash);
                LOG_FATAL("");
                return 0;
            }
        }

        current += hashCount;
        remaining -= hashCount;
    }

    return size;
}

HierarchicalIntegrityVerificationProvider::HierarchicalIntegrityVerificationProvider(
    SharedProvider provider, const formats::HashData::IntegrityMetaInfo& hashData, std::size_t layerInfoOffset
) {
    if (hashData.magic != formats::HashData::IntegrityMetaInfo::cMagic) {
        LOG_ERROR("Invalid IntegrityVerification magic!");
        throw std::runtime_error("HierarchicalIntegrityVerificationProvider");
    }

    if (hashData.levelHashInfo.maxLayers < cMinLayers || hashData.levelHashInfo.maxLayers > cMaxLayers) {
        LOG_ERROR("Invalid layer count!");
        throw std::runtime_error("HierarchicalIntegrityVerificationProvider");
    }

    if (hashData.masterHashSize != IntegrityVerificationProvider::cHashSize) {
        LOG_ERROR("Invalid master hash size!");
        throw std::runtime_error("HierarchicalIntegrityVerificationProvider");
    }

    auto masterHash = std::vector<std::uint8_t>(hashData.masterHashSize);
    std::memcpy(masterHash.data(), hashData.masterHash, masterHash.size());
    auto masterHashProvider = std::make_unique<MemoryStreamProvider>(std::move(masterHash));
    std::unique_ptr<LayerProvider> currentProvider = nullptr;

    std::size_t lastBlockSize = hashData.masterHashSize;
    for (std::size_t i = 0; i < hashData.levelHashInfo.maxLayers - 2; ++i) { // -2 because the master hash and final data layers are unique
        const auto& info = hashData.levelHashInfo.info[i];
        UniqueProvider hashProvider;
        if (currentProvider != nullptr) {
            hashProvider = std::make_unique<IntegrityVerificationProvider::HashProvider>(std::move(currentProvider), lastBlockSize);
        } else if (masterHashProvider != nullptr) {
            hashProvider = std::move(masterHashProvider);
        } else {
            LOG_ERROR("Unknown error, no available hash provider!");
            throw std::runtime_error("HierarchicalIntegrityVerificationProvider");
        }
        if (hashProvider == nullptr) {
            LOG_ERROR("Unknown error occurred! Hash provider is null! {}", i);
            throw std::runtime_error("HierarchicalIntegrityVerificationProvider");
        }

        auto dataProvider = std::make_unique<SharedOffsetProvider>(provider, layerInfoOffset + info.offset, info.size);
        auto cacheDataProvider = std::make_unique<IntegrityVerificationProvider::DataProvider>(std::move(dataProvider), 1 << info.blockOrder);
        auto integrityProvider = std::make_unique<IntegrityVerificationProvider>(std::move(hashProvider), std::move(cacheDataProvider), info.blockOrder);
        currentProvider = std::make_unique<LayerProvider>(std::move(integrityProvider), 1 << info.blockOrder);

        lastBlockSize = 1ull << info.blockOrder;
    }

    const auto& dataInfo = hashData.levelHashInfo.info[hashData.levelHashInfo.maxLayers - 2];
    UniqueProvider hashProvider;
    if (currentProvider != nullptr) {
        hashProvider = std::make_unique<IntegrityVerificationProvider::HashProvider>(std::move(currentProvider), lastBlockSize);
    } else if (masterHashProvider != nullptr) {
        hashProvider = std::move(masterHashProvider);
    } else {
        LOG_ERROR("Unknown error, no available hash provider!");
        throw std::runtime_error("HierarchicalIntegrityVerificationProvider");
    }

    auto dataProvider = std::make_unique<SharedOffsetProvider>(provider, layerInfoOffset > 0 ? 0 : dataInfo.offset, dataInfo.size);
    auto cacheDataProvider = std::make_unique<IntegrityVerificationProvider::DataProvider>(std::move(dataProvider), 1ull << dataInfo.blockOrder);
    auto integrityProvider = std::make_unique<IntegrityVerificationProvider>(std::move(hashProvider), std::move(cacheDataProvider), dataInfo.blockOrder);
    auto layerProvider = std::make_unique<LayerProvider>(std::move(integrityProvider), 1 << dataInfo.blockOrder);
    mProvider = std::make_unique<CacheProvider<32, std::unique_ptr<LayerProvider>>>(std::move(layerProvider), 1ull << dataInfo.blockOrder);
}

auto HierarchicalIntegrityVerificationProvider::read(void* dst, std::size_t size, std::size_t offset) -> std::size_t {
    return mProvider->read(dst, size, offset);
}

IndirectProvider::IndirectProvider(
    SharedProvider provider1, SharedProvider provider2,
    std::size_t entryCount, UniqueProvider nodeProvider, UniqueProvider entryProvider
) : mBucketTree(cNodeSize, cEntrySize, static_cast<std::int32_t>(entryCount), std::move(nodeProvider), std::move(entryProvider)) {
    mProviders[0] = std::move(provider1);
    mProviders[1] = std::move(provider2);
}

auto IndirectProvider::read(void* dst, std::size_t size, std::size_t offset) -> std::size_t {
    if (!mBucketTree.isInitialized()) {
        return 0;
    }
    
    if (size == 0 || offset >= getSize()) {
        return 0;
    }

    auto loc = mBucketTree.findEntry(static_cast<std::int64_t>(offset));

    auto curDst = static_cast<std::uint8_t*>(dst);
    auto curOffset = static_cast<std::int64_t>(offset);
    const auto end = curOffset + static_cast<std::int64_t>(size);
    while (curOffset < end) {
        Entry entry;
        if (!loc.get(entry)) {
            return 0;
        }

        if (entry.virtualOffset > curOffset) {
            return 0;
        }

        if (entry.storageIndex < 0 || entry.storageIndex >= cStorageCount) {
            LOG_WARNING("Indirect storage index out of range! {}", static_cast<std::int32_t>(entry.storageIndex));
            return 0;
        }

        std::int64_t nextOffset;
        if (loc.hasNext()) {
            if (!loc.next()) {
                return 0;
            }
            nextOffset = loc.get<Entry>().virtualOffset;
            if (!mBucketTree.contains(nextOffset)) {
                return 0;
            }
        } else {
            nextOffset = mBucketTree.getEndAddress();
        }

        const auto offsetInCurrentEntry = curOffset - entry.virtualOffset;
        const auto sizeOfCurrentEntry = (nextOffset - entry.virtualOffset) - offsetInCurrentEntry;
        const auto sizeToRead = std::min(sizeOfCurrentEntry, end - curOffset);

        const auto maxSize = mProviders[entry.storageIndex]->getSize();
        if (entry.physicalOffset < 0 || entry.physicalOffset >= static_cast<std::int64_t>(maxSize)) {
            return 0;
        }

        if (mProviders[entry.storageIndex]->read(curDst, sizeToRead, entry.physicalOffset + offsetInCurrentEntry) != static_cast<std::size_t>(sizeToRead)) {
            return 0;
        }

        curDst += sizeToRead;
        curOffset += sizeToRead;
    }

    return size;
}

} // namespace nxmount::provider