#pragma once

#include "common/string_map.hpp"

#include <list>
#include <mutex>
#include <unordered_map>

namespace nxmount::common {

template <typename Key, typename Value, std::size_t CacheSize>
class Cache {
public:
    Cache() = default;

    template <typename KeyType>
    auto get(KeyType key, Value* value) -> bool {
        const auto _ = std::unique_lock(mMutex);
        if (const auto res = mCacheMap.find(key); res != mCacheMap.end()) {
            setMru(res->second);
            *value = res->second->value;
            return true;
        }
        return false;
    }

    template <typename KeyType>
    auto add(KeyType key, Value value) -> void {
        const auto _ = std::unique_lock(mMutex);
        auto lru = getLru();
        mCacheMap.erase(lru->key);
        lru->key = key;
        lru->value = value;
        setMru(lru);
        mCacheMap.emplace(lru->key, lru);
    }

    auto emplace(Key&& key, Value&& value) -> void {
        const auto _ = std::unique_lock(mMutex);
        auto lru = getLru();
        mCacheMap.erase(lru->key);
        lru->key = std::move(key);
        lru->value = std::move(value);
        setMru(lru);
        mCacheMap.emplace(lru->key, lru);
    }

private:
    struct Node {
        Key key;
        Value value;
    };

    using NodeList = std::list<Node>;
    using CacheMap = std::conditional_t<std::is_same_v<Key, std::string>, StringMap<typename NodeList::iterator>, std::unordered_map<Key, typename NodeList::iterator>>;

    auto setMru(NodeList::iterator it) -> void {
        if (it != mCache.begin()) {
            mCache.splice(mCache.begin(), mCache, it, std::next(it));
        }
    }

    auto getLru() -> NodeList::iterator {
        if (mCache.size() < CacheSize) {
            mCache.emplace_back();
        }
        return std::prev(mCache.end());
    }

    std::mutex mMutex;
    NodeList mCache;
    CacheMap mCacheMap;
};

} // namespace nxmount::common