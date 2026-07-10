#pragma once

#include "provider/provider.hpp"

#include <cstring>
#include <list>
#include <mutex>
#include <unordered_map>

namespace nxmount::provider {

template <ProviderWrapper ProviderT = UniqueProvider>
class CacheProvider final : public ReadOnlyBytesProvider {
public:
    static constexpr const std::size_t cMaxBlockSize = 0x10000;

    CacheProvider(ProviderT provider, std::size_t blockSize, std::size_t cacheSize)
     : mProvider(std::move(provider)), mBlockSize(std::min(blockSize, cMaxBlockSize)), mCacheSize(std::max(cacheSize, 1ull)) {}

    ~CacheProvider() override = default;

    auto getSize() const -> std::size_t override { return mProvider->getSize(); }

    auto read(void* dst, std::size_t size, std::size_t offset) -> std::size_t override {
        if (size == 0 || offset >= getSize()) {
            return 0;
        }

        std::unique_ptr<std::uint8_t[]> work = nullptr;
        auto buf = static_cast<std::uint8_t*>(dst);
        const auto readSize = std::min(size, getSize() - offset);
        auto remaining = readSize;
        auto current = offset;

        const auto alignedHead = common::AlignDown(offset, mBlockSize);
        if (alignedHead != current) {
            const auto headOffset = current - alignedHead;
            const auto headSize = std::min(readSize, mBlockSize - headOffset);
            if (tryReadFromCache(buf, headOffset, headSize, alignedHead / mBlockSize)) {
                /* good */
            } else {
                if (work == nullptr) {
                    work.reset(new std::uint8_t[mBlockSize]);
                }

                const auto headReadSize = mProvider->read(work.get(), mBlockSize, alignedHead);
                if (headReadSize == 0) {
                    return 0;
                }

                if (headReadSize < mBlockSize) {
                    const auto remainingHeadSize = mBlockSize - headReadSize;
                    std::memset(work.get() + headReadSize, 0, remainingHeadSize);
                }

                pushBlock(work.get(), alignedHead / mBlockSize);
                std::memcpy(buf, work.get() + headOffset, headSize);
            }

            buf += headSize;
            remaining -= headSize;
            current += headSize;
        }

        while (remaining >= mBlockSize) {
            if (tryReadFromCache(buf, 0, mBlockSize, current / mBlockSize)) {
                /* good */
            } else if (mProvider->read(buf, mBlockSize, current) == mBlockSize) {
                pushBlock(buf, current / mBlockSize);
            } else {
                return 0;
            }

            buf += mBlockSize;
            remaining -= mBlockSize;
            current += mBlockSize;
        }

        if (remaining > 0) {
            // at this point, remaining < mBlockSize
            if (tryReadFromCache(buf, 0, remaining, current / mBlockSize)) {
                /* good */
            } else {
                if (work == nullptr) {
                    work.reset(new std::uint8_t[mBlockSize]);
                }

                const auto tailReadSize = mProvider->read(work.get(), mBlockSize, current);
                if (tailReadSize == 0) {
                    return 0;
                }

                if (tailReadSize < mBlockSize) {
                    const auto remainingTailSize = mBlockSize - tailReadSize;
                    std::memset(work.get() + tailReadSize, 0, remainingTailSize);
                }

                pushBlock(work.get(), current / mBlockSize);
                std::memcpy(buf, work.get(), remaining);
            }

            buf += remaining;
            remaining -= remaining;
            current += remaining;
        }

        if (size > readSize) {
            std::memset(buf, 0, size - readSize);
        }
        
        return size;
    }

private:
    struct Node {
        std::int64_t key;
        std::shared_ptr<std::uint8_t[]> value;
    };

    using NodeList = std::list<Node>;

    [[nodiscard]] auto tryReadFromCache(void* dst, std::size_t offset, std::size_t size, std::size_t blockIndex) -> bool {
        if (static_cast<std::int64_t>(blockIndex) < 0) {
            return false;
        }
        if (offset >= mBlockSize) {
            return false;
        }
        if (size + offset > mBlockSize) {
            size = mBlockSize - offset;
        }
        const auto _ = std::scoped_lock(mCacheMutex);
        if (const auto res = mCacheMap.find(static_cast<std::int64_t>(blockIndex)); res != mCacheMap.end()) {
            std::memcpy(dst, res->second->value.get() + offset, size);
            setMru(res->second);
            return true;
        }
        return false;
    }

    auto pushBlock(const void* data, std::size_t blockIndex) -> bool {
        if (static_cast<std::int64_t>(blockIndex) < 0) {
            return false;
        }

        const auto _ = std::scoped_lock(mCacheMutex);
        if (mCache.empty()) {
            return false;
        }

        auto lru = getLru();
        mCacheMap.erase(lru->key);
        lru->key = static_cast<std::int64_t>(blockIndex);
        std::memcpy(lru->value.get(), data, mBlockSize);
        setMru(lru);
        mCacheMap.emplace(lru->key, lru);
        return true;
    }

    auto setMru(NodeList::iterator it) -> void {
        if (it != mCache.begin()) {
            mCache.splice(mCache.begin(), mCache, it, std::next(it));
        }
    }

    auto getLru() -> NodeList::iterator {
        if (mCache.size() < mCacheSize) {
            mCache.emplace_back(-1, std::make_unique<std::uint8_t[]>(mBlockSize));
        }
        return std::prev(mCache.end());
    }

    ProviderT mProvider;
    const std::size_t mBlockSize;
    const std::size_t mCacheSize;
    mutable std::mutex mCacheMutex;
    NodeList mCache;
    std::unordered_map<std::int64_t, typename NodeList::iterator> mCacheMap;
};

} // namespace nxmount::provider