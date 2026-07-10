#pragma once

#include "common/errors.hpp"
#include "fs/node.hpp"

#include <cstddef>
#include <iterator>
#include <string>

namespace nxmount::fs {

struct DirectoryEntry {
    std::string name = "";
    Type type = Type::Invalid;
    std::uint32_t attributes = 0;
    std::uint64_t fileSize = 0;
};

class IDirectory : public INode {
public:
    [[nodiscard]] auto getType() const -> Type override { return Type::Directory; }

    virtual ~IDirectory() = default;
    virtual auto getCount(std::size_t* count) const -> Result = 0;
    virtual auto read(std::size_t* entryCount, DirectoryEntry* entries, std::size_t maxEntries, std::size_t offset) const -> Result = 0;

    virtual auto sync(bool flushMetadata) -> Result = 0;

    struct iterator {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = const DirectoryEntry;
        using pointer = const DirectoryEntry*;
        using reference = const DirectoryEntry&;

        iterator() : dir(), offset(0) {}
        iterator(const IDirectory* directory) : dir(directory), offset(0) { readEntry(); }

        constexpr auto operator*() const -> reference { return entry; }
        constexpr auto operator->() const -> pointer { return std::addressof(entry); }

        auto operator++() -> iterator& { ++offset; readEntry(); return *this; }

        constexpr auto operator==(const iterator& rhs) const -> bool { return this->entry.type == rhs.entry.type && this->entry.name == rhs.entry.name; }
        constexpr auto operator!=(const iterator& rhs) const -> bool { return !operator==(rhs); }

    private:
        auto readEntry() -> void {
            std::size_t read = 0;
            if (FAILED(dir->read(std::addressof(read), std::addressof(entry), 1, offset)) || read != 1) {
                entry = {};
            }
        }

        DirectoryEntry entry{};
        const IDirectory* dir;
        std::size_t offset;
    };

    auto begin() const -> iterator { return iterator(this); }
    auto end() const -> iterator { return iterator(); }
};

class ReadOnlyDirectoryBase : public IDirectory {
public:
    auto sync(bool) -> Result override final { return SUCCESS; }
};

} // namespace nxmount::fs