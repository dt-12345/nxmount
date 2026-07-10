#include "formats/bktr.hpp"
#include "log/logging.hpp"
#include "provider/provider.hpp"

#include <algorithm>
#include <stdexcept>

namespace nxmount::formats {

BucketTree::BucketTree(
    std::size_t nodeSize, std::size_t entrySize, std::int32_t entryCount,
    provider::UniqueProvider nodeProvider, provider::UniqueProvider entryProvider
) : mNodeSize(nodeSize), mEntrySize(entrySize), mEntryCount(entryCount), mNodeProvider(std::move(nodeProvider)), mEntryProvider(std::move(entryProvider)), mRootOffsetNode(nodeSize) {
    if (mNodeSize <= sizeof(BucketNodeHeader)) {
        LOG_ERROR("Bucket tree node size is too small!");
        throw std::runtime_error("BucketTree");
    }

    if (mEntrySize == 0) {
        LOG_ERROR("Entry size must be non-zero!");
        throw std::runtime_error("BucketTree");
    }

    if (mEntryCount == 0) {
        LOG_INFO("Empty bucket tree");
        mEntryNodeCount = 0;
        mOffsetNodeCount = 0;
        mRootStartAddress = 0;
        mRootEndAddress = 0;
        return;
    }
    
    mEntriesPerNode = static_cast<std::int32_t>((mNodeSize - sizeof(BucketNodeHeader)) / mEntrySize);
    mEntryNodeCount = static_cast<std::int32_t>(GetEntryNodeCount(mNodeSize, mEntrySize, mEntryCount));
    mOffsetsPerNode = static_cast<std::int32_t>((mNodeSize - sizeof(BucketNodeHeader)) / sizeof(std::int64_t));
    mOffsetNodeCount = static_cast<std::int32_t>(GetOffsetNodeCount(mNodeSize, mEntrySize, mEntryCount));

    if (mNodeProvider->read(mRootOffsetNode.mData.data(), mNodeSize, 0) != mNodeSize) {
        LOG_ERROR("Failed to read root bucket tree offset node!");
        throw std::runtime_error("BucketTree");
    }

    const auto start = mRootOffsetNode.getStart();
    if (start == nullptr) {
        LOG_ERROR("Failed to get bucket tree address range!");
        throw std::runtime_error("BucketTree");
    }
    mRootStartAddress = *start;
    mRootEndAddress = mRootOffsetNode.getHeader()->offset;
}

auto BucketTree::GetOffsetNodeCount(std::size_t nodeSize, std::size_t entrySize, std::size_t entryCount) -> std::size_t {
    const auto entryNodeCount = GetEntryNodeCount(nodeSize, entrySize, entryCount);
    if (entryNodeCount == 0) {
        return 1;
    }

    const auto offsetsPerNode = (nodeSize - sizeof(BucketNodeHeader)) / sizeof(std::int64_t);
    if (offsetsPerNode >= entryNodeCount) {
        return 1;
    }

    const auto requiredOffsetNodes = (entryNodeCount + offsetsPerNode - 1) / offsetsPerNode;
    if (requiredOffsetNodes > offsetsPerNode) {
        LOG_ERROR("The number of required offset nodes exceeds the maximum number of offsets in a node!");
        throw std::runtime_error("BucketTree");
    }
    const auto offsetsNotInFirstNode = entryNodeCount - (offsetsPerNode - (requiredOffsetNodes - 1));
    const auto numberOfLevel2OffsetNodes = (offsetsNotInFirstNode + offsetsPerNode - 1) / offsetsPerNode;
    return 1 + numberOfLevel2OffsetNodes;
}

auto BucketTree::GetEntryNodeCount(std::size_t nodeSize, std::size_t entrySize, std::size_t entryCount) -> std::size_t {
    if (nodeSize == 0 || nodeSize <= sizeof(BucketNodeHeader)) {
        return 0;
    }

    const auto entriesPerNode = (nodeSize - sizeof(BucketNodeHeader)) / entrySize;
    if (entriesPerNode == 0) {
        return 0;
    }

    return (entryCount + entriesPerNode - 1) / entriesPerNode;
}

[[nodiscard]] static auto FindEntry(provider::IBytesProvider& provider, std::int64_t address, std::size_t baseOffset, std::size_t entrySize) -> std::int32_t {
    BucketNodeHeader header;
    if (provider.read(std::addressof(header), sizeof(header), baseOffset) != sizeof(header)) {
        return -1;
    }
    if (header.count == 0) {
        return -1;
    }
    
    const auto start = baseOffset + sizeof(header);
    auto pos = start;
    auto len = header.count;

    while (len > 0) {
        const auto half = len / 2;
        auto middle = pos + half * entrySize;

        std::int64_t offset;
        if (provider.read(std::addressof(offset), sizeof(offset), middle) != sizeof(offset)) {
            return -1;
        }

        if (offset <= address) {
            pos = middle + entrySize;
            len -= half + 1;
        } else {
            len = half;
        }
    }

    return static_cast<std::int32_t>((pos - start) / entrySize) - 1;
}

auto BucketTree::findEntry(std::int64_t address) const -> Locator {
    if (!isInitialized()) {
        LOG_ERROR("Tried to look up address before BucketTree initialization");
        return Locator(*this);
    }

    if (address >= mRootEndAddress) {
        LOG_ERROR("Tried to look up out-of-range address");
        return Locator(*this);
    }

    std::int32_t entryNodeIndex = -1;
    if (hasInlineLevel2() && address < mRootStartAddress) {
        const auto start = mRootOffsetNode.getEnd();
        const auto end = mRootOffsetNode.getStart() + mOffsetsPerNode;

        auto pos = std::upper_bound(start, end, address);
        if (pos-- == start) {
            return Locator(*this);
        }

        entryNodeIndex = static_cast<std::int32_t>(std::distance(start, pos));
    } else {
        const auto start = mRootOffsetNode.getStart();
        const auto end = mRootOffsetNode.getEnd();

        auto pos = std::upper_bound(start, end, address);
        if (pos-- == start) {
            return Locator(*this);
        }

        entryNodeIndex = static_cast<std::int32_t>(std::distance(start, pos));
        if (hasLevel2()) {
            const auto offsetNodeOffset = mNodeSize * (entryNodeIndex + 1); // +1 bc of the root node
            entryNodeIndex = FindEntry(*mNodeProvider, address, offsetNodeOffset, sizeof(std::int64_t));
        }
    }

    if (entryNodeIndex < 0 || entryNodeIndex >= mEntryNodeCount) {
        return Locator(*this);
    }

    const auto entryNodeOffset = mNodeSize * entryNodeIndex;
    const auto entryIndex = FindEntry(*mEntryProvider, address, entryNodeOffset, mEntrySize);
    if (entryIndex < 0) {
        return Locator(*this);
    }

    return Locator(*this, entryNodeIndex, entryIndex);
}

BucketTree::Locator::Locator(const BucketTree& tree, std::int32_t entryNodeIndex, std::int32_t entryIndex)
    : mBucketTree(tree), mEntryNodeIndex(entryNodeIndex), mEntryIndex(entryIndex), mCurrentNodeEntryCount(0) {
    BucketNodeHeader header;
    if (mBucketTree.mEntryProvider->read(std::addressof(header), sizeof(header), mEntryNodeIndex * mBucketTree.mNodeSize) != sizeof(header)) {
        mEntryNodeIndex = -1;
        mEntryIndex = -1;
    } else {
        mCurrentNodeEntryCount = header.count;
    }
}

auto BucketTree::Locator::hasPrev() const -> bool {
    if (!hasValue()) {
        return false;
    }

    return mEntryIndex > 0 || mEntryNodeIndex > 0;
}

auto BucketTree::Locator::hasNext() const -> bool {
    if (!hasValue()) {
        return false;
    }

    return mEntryIndex + 1 < mCurrentNodeEntryCount || mEntryNodeIndex + 1 < mBucketTree.mEntryNodeCount;
}

auto BucketTree::Locator::prev() -> bool {
    if (!hasValue()) {
        return false;
    }

    if (mEntryIndex <= 0) {
        if (mEntryNodeIndex <= 0) {
            return false;
        }

        --mEntryNodeIndex;

        BucketNodeHeader header;
        if (mBucketTree.mEntryProvider->read(std::addressof(header), sizeof(header), mEntryNodeIndex * mBucketTree.mNodeSize) != sizeof(header)) {
            return false;
        }

        mCurrentNodeEntryCount = header.count;
        if (mCurrentNodeEntryCount == 0) {
            return false;
        }
        mEntryIndex = mCurrentNodeEntryCount - 1;
    } else {
        --mEntryIndex;
    }

    return true;
}

auto BucketTree::Locator::next() -> bool {
    if (!hasValue()) {
        return false;
    }

    if (mEntryIndex + 1 >= mCurrentNodeEntryCount) {
        if (mEntryNodeIndex + 1 >= mBucketTree.mEntryNodeCount) {
            return false;
        }

        ++mEntryNodeIndex;

        BucketNodeHeader header;
        if (mBucketTree.mEntryProvider->read(std::addressof(header), sizeof(header), mEntryNodeIndex * mBucketTree.mNodeSize) != sizeof(header)) {
            return false;
        }

        mCurrentNodeEntryCount = header.count;
        if (mCurrentNodeEntryCount == 0) {
            return false;
        }
        mEntryIndex = 0;
    } else {
        ++mEntryIndex;
    }

    return true;
}

}