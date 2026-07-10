#pragma once

#include "common/utils.hpp"
#include "provider/provider.hpp"

#include <memory>
#include <vector>

namespace nxmount::formats {

struct BucketTreeHeader {
    static constexpr const auto cMagic = common::MakeMagic("BKTR");
    static constexpr const auto cVersion = 1u;

    std::uint32_t magic;
    std::uint32_t version;
    std::int32_t entryCount;
    std::uint32_t reserved;
};
static_assert(sizeof(BucketTreeHeader) == 0x10);

struct BucketNodeHeader {
    std::int32_t index;
    std::int32_t count;
    std::int64_t offset;
};
static_assert(sizeof(BucketNodeHeader) == 0x10);

class BucketTree {
    class OffsetNode {
    public:
        explicit OffsetNode(std::size_t size) : mData(size) {}

        [[nodiscard]] auto getHeader() const -> const BucketNodeHeader* { return reinterpret_cast<const BucketNodeHeader*>(mData.data()); }

        [[nodiscard]] auto getCount() const -> std::int32_t { return isInitialized() ? getHeader()->count : 0; }

        [[nodiscard]] auto getStart() const -> const std::int64_t* {
            if (!isInitialized() || mData.size() < sizeof(BucketNodeHeader) + sizeof(std::int64_t) * getCount()) {
                return nullptr;
            }
            return reinterpret_cast<const std::int64_t*>(mData.data() + sizeof(BucketNodeHeader));
        }

        [[nodiscard]] auto getEnd() const -> const std::int64_t* {
            const auto start = getStart();
            return start != nullptr ? start + getCount() : nullptr;
        }

        [[nodiscard]] auto get(std::int32_t index) const -> const std::int64_t* {
            if (!isInitialized() || index < 0 || index >= getCount()) {
                return nullptr;
            }
            const auto start = getStart();
            return start != nullptr ? start + index : nullptr;
        }

        [[nodiscard]] auto isInitialized() const -> bool {
            return mData.size() > sizeof(BucketNodeHeader) && getHeader()->count > 0;
        }

    private:
        friend class BucketTree;
    
        std::vector<std::uint8_t> mData;
    };

public:
    BucketTree(
        std::size_t nodeSize, std::size_t entrySize, std::int32_t entryCount, provider::UniqueProvider nodeProvider, provider::UniqueProvider entryProvider
    );

    friend class Locator;

    class Locator {
    public:
        explicit Locator(const BucketTree& tree) : mBucketTree(tree), mEntryNodeIndex(-1), mEntryIndex(-1), mCurrentNodeEntryCount(0) {}
        Locator(const BucketTree& tree, std::int32_t entryNodeIndex, std::int32_t entryIndex);
        
        [[nodiscard]] auto getOffset() const -> std::int64_t {
            if (mEntryNodeIndex < 0 || mEntryIndex < 0 || mBucketTree.mEntrySize == 0) {
                return -1;
            }
            return static_cast<std::int64_t>(mEntryNodeIndex * mBucketTree.mNodeSize + sizeof(BucketNodeHeader) + mEntryIndex * mBucketTree.mEntrySize);
        }

        [[nodiscard]] auto hasValue() const -> bool {
            return mEntryNodeIndex >= 0 && mEntryIndex >= 0 && mBucketTree.mEntrySize > 0;
        }

        template <typename T>
        [[nodiscard]] auto get(T& out) const -> bool {
            if (!hasValue()) {
                return false;
            }
            return mBucketTree.mEntryProvider->read(std::addressof(out), sizeof(out), getOffset()) == sizeof(out);
        }

        template <typename T>
        [[nodiscard]] auto get() const -> T {
            T out;
            mBucketTree.mEntryProvider->read(std::addressof(out), sizeof(out), getOffset());
            return out;
        }

        auto hasPrev() const -> bool;
        auto hasNext() const -> bool;
        auto prev() -> bool;
        auto next() -> bool;

    private:
        const BucketTree& mBucketTree;
        std::int32_t mEntryNodeIndex;
        std::int32_t mEntryIndex;
        std::int32_t mCurrentNodeEntryCount;
    };

    [[nodiscard]] auto isInitialized() const -> bool { return mEntryCount > 0 && mNodeProvider != nullptr  && mEntryProvider != nullptr; }

    template <typename T>
    [[nodiscard]] auto findEntry(std::int64_t address, T& out) const -> bool {
        if (mEntrySize != sizeof(T)) {
            return false;
        }
        const auto location = findEntry(address);
        if (!location.hasValue()) {
            return false;
        }
        return location.get<T>(out);
    }

    [[nodiscard]] auto findEntry(std::int64_t address) const -> Locator;

    [[nodiscard]] auto getStartAddress() const -> std::int64_t { return mRootStartAddress; }
    [[nodiscard]] auto getEndAddress() const -> std::int64_t { return mRootEndAddress; }

    [[nodiscard]] auto contains(std::int64_t addr) const -> bool {
        return mRootStartAddress <= addr && addr < mRootEndAddress;
    }
    [[nodiscard]] auto contains(std::int64_t offset, std::int64_t size) const -> bool {
        return size > 0 && mRootStartAddress <= offset && offset + size <= mRootEndAddress;
    }

    [[nodiscard]] static auto GetOffsetNodeCount(std::size_t nodeSize, std::size_t entrySize, std::size_t entryCount) -> std::size_t;
    [[nodiscard]] static auto GetEntryNodeCount(std::size_t nodeSize, std::size_t entrySize, std::size_t entryCount) -> std::size_t;

private:
    [[nodiscard]] auto hasLevel2() const -> bool { return mOffsetsPerNode < mEntryNodeCount; }
    [[nodiscard]] auto hasInlineLevel2() const -> bool { return hasLevel2() && mRootOffsetNode.getCount() < mOffsetsPerNode; }

    std::size_t mNodeSize;          // size a node (containing offsets or entries)
    std::size_t mEntrySize;         // size of a single entry
    std::int32_t mEntryCount;       // total number of entries
    std::int32_t mEntryNodeCount;   // number of entry-containing nodes
    std::int32_t mEntriesPerNode;
    std::int32_t mOffsetNodeCount;  // number of offset-containing nodes
    std::int32_t mOffsetsPerNode;   // number of offsets per node
    provider::UniqueProvider mNodeProvider;
    provider::UniqueProvider mEntryProvider;
    OffsetNode mRootOffsetNode;
    std::int64_t mRootStartAddress;
    std::int64_t mRootEndAddress;

    /*
        BucketTreeHeader
        Offset Nodes
            NodeSize [
                NodeHeader
                u64 Offsets
            ]
            if the first node is enough to contain all the entries, it contains offsets for the entries
            otherwise, it contains offsets for the other offset nodes (the first node can contain some of these offsets as well if there's room)
            aka lookup offset in root node to find lvl 2 node index => lookup same offset in that node
        Entry Nodes
            NodeSize [
                NodeHeader
                Entries
            ]
    */
};

} // namespace nxmount::formats