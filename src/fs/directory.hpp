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
    time_t createTime = 0;
    std::uint64_t fileSize = 0;
};

class IDirectory : public INode {
public:
    [[nodiscard]] auto getType() const -> Type override { return Type::Directory; }

    ~IDirectory() override = default;
    virtual auto getCount(std::size_t* count) const -> Result = 0;
    virtual auto read(std::size_t* entryCount, DirectoryEntry* entries, std::size_t maxEntries, std::size_t offset) const -> Result = 0;

    virtual auto sync(bool flushMetadata) -> Result = 0;

    using EntryCallback = bool (const DirectoryEntry& entry, void* userdata);
    virtual auto forEachEntry(EntryCallback cb, void* userdata, std::string_view marker) const -> void {
        if (marker.empty()) {
            for (const auto& entry : *this) {
                if (!cb(entry, userdata)) {
                    return;
                }
            }
        } else {
            bool found = false;
            for (const auto& entry : *this) {
                if (found) {
                    if (!cb(entry, userdata)) {
                        return;
                    }
                } else {
                    found = entry.name == marker;
                }
            }
        }
        return;
    }

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
            if (NX_FAILED(dir->read(std::addressof(read), std::addressof(entry), 1, offset)) || read != 1) {
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