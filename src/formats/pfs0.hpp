#pragma once

#include "cnmt.hpp"
#include "common/utils.hpp"
#include "fs/directory.hpp"
#include "fs/file.hpp"
#include "fs/filesystem.hpp"
#include "provider/provider.hpp"

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace nxmount::formats {

struct PartitionFileSystemHeader {
    static constexpr const auto cMagic = common::MakeMagic("PFS0");
    
    std::uint32_t magic; // PFS0
    std::uint32_t entryCount;
    std::uint32_t stringTableSize;
    std::uint32_t reserved;
};
static_assert(sizeof(PartitionFileSystemHeader) == 0x10);

struct PartitionEntry {
    std::uint64_t offset;
    std::uint64_t size;
    std::uint32_t stringOffset;
    std::uint32_t reserved;
};
static_assert(sizeof(PartitionEntry) == 0x18);

struct ContentInfo;

class PartitionFileSystemBase : public fs::ReadOnlyFileSystemBase {
public:
    PartitionFileSystemBase(provider::UniqueProvider provider, std::string_view name) :mProvider(std::move(provider)), mName(name), mInitTime(time(nullptr)) {}

    struct Entry {
        std::string name;
        std::size_t offset;
        std::size_t size;
        std::unique_ptr<fs::IFileSystem> fs = nullptr;

        auto isNull() const -> bool {
            return size == 0 && offset == 0xffff'ffff'ffff'ffff;
        }
    };

    ~PartitionFileSystemBase() override { destroy(); }

    [[nodiscard]] auto getRoot() const -> std::unique_ptr<fs::IDirectory> override {
        return std::make_unique<Directory>(*this);
    }

    auto init() -> void override;
    auto destroy() -> void override;

    auto getAttributes(std::string_view path, fs::DirectoryEntry* entry) const -> Result override;

    auto access(std::string_view path, fs::OpenMode mode) const -> Result override;

    auto readLink(std::string_view, char*, std::size_t) const -> Result override { return UNIMPLEMENTED; }

    auto openFile(std::unique_ptr<fs::IFile>* out, std::string_view path, fs::OpenMode mode) const -> Result override;

    auto openDirectory(std::unique_ptr<fs::IDirectory>* dir, std::string_view path) const -> Result override;

    auto applyUpdate(std::unique_ptr<PartitionFileSystemBase> update) -> void;
    auto applyAddOnContent(std::unique_ptr<PartitionFileSystemBase> aoc) -> void;

protected:
    virtual auto processEntry(Entry& entry, std::size_t index) -> bool;

    auto tryParseTicket() const -> void;
    auto rearrangeNCAs() -> void;

    auto getContentMetaReader(std::uint64_t id, fs::IFileSystem** fsOut) const -> ContentMetaReader;

    [[nodiscard]] auto getEntry(std::size_t index) const -> const Entry* {
        if (index >= mEntries.size()) {
            return nullptr;
        }
        return std::addressof(mEntries[index]);
    }

    [[nodiscard]] auto getEntry(std::size_t index) -> Entry* {
        if (index >= mEntries.size()) {
            return nullptr;
        }
        return std::addressof(mEntries[index]);
    }

    [[nodiscard]] auto getEntry(std::string_view name, std::size_t* index = nullptr) const -> const Entry* {
        std::size_t i = 0;
        for (const auto& entry : mEntries) {
            if (entry.name == name) {
                if (index != nullptr) {
                    *index = i;
                }
                return std::addressof(entry);
            }
            ++i;
        }
        return nullptr;
    }

    [[nodiscard]] auto getEntry(std::string_view name, std::size_t* index = nullptr) -> Entry* {
        std::size_t i = 0;
        for (auto& entry : mEntries) {
            if (entry.name == name) {
                if (index != nullptr) {
                    *index = i;
                }
                return std::addressof(entry);
            }
            ++i;
        }
        return nullptr;
    }

    [[nodiscard]] auto read(void* dst, std::size_t size, std::size_t offset) const -> std::size_t {
        if (offset < mDataStart || offset >= mDataEnd) {
            return 0;
        }

        return mProvider->read(dst, size, offset);
    }

    friend class File;
    friend class Directory;

    provider::UniqueProvider mProvider;
    const std::string mName = "";
    std::vector<Entry> mEntries{}; // do not change outside of constructor
    std::size_t mDataStart = 0;
    std::size_t mDataEnd = 0;
    const time_t mInitTime;

    class File final : public fs::ReadOnlyFileBase {
    public:
        File(const PartitionFileSystemBase& fs, std::size_t index) : mParentFileSystem(fs), mIndex(index) {}

        [[nodiscard]] auto getName() const -> std::string_view override { return mParentFileSystem.getEntry(mIndex)->name; }

        ~File() override = default;

        auto read(void* dst, std::size_t size, std::size_t offset) const -> std::size_t override;
        auto getSize() const -> std::size_t override { return mParentFileSystem.getEntry(mIndex)->size; }

    private:
        const PartitionFileSystemBase& mParentFileSystem;
        const std::size_t mIndex;
    };

    class Directory final : public fs::ReadOnlyDirectoryBase {
    public:
        Directory(const PartitionFileSystemBase& fs) : mParentFileSystem(fs), mName(fs.mName) {}

        [[nodiscard]] auto getName() const -> std::string_view override { return mName; }

        ~Directory() override = default;

        auto getCount(std::size_t* count) const -> Result override {
            if (count == nullptr) {
                return INVALID;
            }
            *count = 0;
            for (const auto& entry : mParentFileSystem.mEntries) {
                if (!entry.name.empty()) {
                    (*count)++;
                }
            }
            return SUCCESS;
        }

        auto read(std::size_t* entryCount, fs::DirectoryEntry* entries, std::size_t maxEntries, std::size_t offset) const -> Result override;

    private:
        const PartitionFileSystemBase& mParentFileSystem;
        const std::string mName;
    };
};

class PartitionFileSystem final : public PartitionFileSystemBase {
public:
    PartitionFileSystem(provider::UniqueProvider provider, std::string_view name);
};

} // namespace nxmount::formats