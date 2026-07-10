#pragma once

#include "fs/directory.hpp"
#include "fs/filesystem.hpp"
#include "provider/provider.hpp"

namespace nxmount::formats {

struct RomFileSystemInformation {
    std::int64_t size;
    std::int64_t directoryBucketOffset;
    std::int64_t directoryBucketSize;
    std::int64_t directoryEntryOffset;
    std::int64_t directoryEntrySize;
    std::int64_t fileBucketOffset;
    std::int64_t fileBucketSize;
    std::int64_t fileEntryOffset;
    std::int64_t fileEntrySize;
    std::int64_t bodyOffset;
};
static_assert(sizeof(RomFileSystemInformation) == 0x50);

struct RomFileInfo {
    std::int64_t offset;
    std::int64_t size;
};
static_assert(sizeof(RomFileInfo) == 0x10);

struct RomDirectoryInfo {
    std::uint32_t nextDir;
    std::uint32_t nextFile;
};

struct RomDirectoryEntry {
    std::uint32_t next;
    std::uint32_t dir;  // first child directory
    std::uint32_t file; // first child file
};

struct RomFileEntry {
    std::uint32_t next;
    PACKED(RomFileInfo fileInfo);
};

template <typename T>
struct TableEntry {
    std::uint32_t key; // parent
    T value;
    std::uint32_t next;
    std::uint32_t auxSize;
    // std::uint8_t[auxSize];
};
static_assert(sizeof(TableEntry<RomDirectoryEntry>) == 0x18);
static_assert(sizeof(TableEntry<RomFileEntry>) == 0x20);

template <typename ValueType>
class EntryMap {
public:
    using Element = ValueType;
    using Entry = TableEntry<Element>;

    EntryMap(provider::UniqueProvider bucketProvider, provider::UniqueProvider entryProvider) :
        mBucketProvider(std::move(bucketProvider)), mEntryProvider(std::move(entryProvider)), mBucketCount(static_cast<std::uint32_t>(mBucketProvider->getSize() / sizeof(std::uint32_t))) {}

    auto get(std::uint32_t* posOut, Entry* out, std::uint32_t parent, std::string_view name) const -> bool {
        return find(posOut, out, parent, CalcHash(parent, name), name);
    }

    auto get(Entry* out, std::uint32_t pos) const -> bool {
        if (out == nullptr) {
            return false;
        }
        
        if (pos == 0xffff'ffffu || pos >= static_cast<std::uint32_t>(mEntryProvider->getSize())) {
            return false;
        }

        if (mEntryProvider->read(out, sizeof(*out), pos) != sizeof(*out)) {
            return false;
        }

        return true;
    }

    auto get(Entry* out, char* name, std::size_t nameBufSize, std::uint32_t pos) const -> bool {
        if (out == nullptr || name == nullptr) {
            return false;
        }
        
        if (pos == 0xffff'ffffu || pos >= static_cast<std::uint32_t>(mEntryProvider->getSize())) {
            return false;
        }

        if (mEntryProvider->read(out, sizeof(*out), pos) != sizeof(*out)) {
            return false;
        }

        const auto size = std::min(static_cast<std::size_t>(out->auxSize), nameBufSize - 1);
        if (mEntryProvider->read(name, size, pos + sizeof(*out)) != size) {
            return false;
        }

        name[size] = '\0';
        return true;
    }

private:
    [[nodiscard]] static constexpr auto CalcHash(std::uint32_t parent, std::string_view name) -> std::uint32_t {
        std::uint32_t hash = parent ^ 123456789;
        for (const auto c : name) {
            hash = ((hash >> 5) | (hash << 27)) ^ static_cast<std::uint32_t>(static_cast<std::uint8_t>(c));
        }
        return hash;
    }

    auto find(std::uint32_t* posOut, Entry* out, std::uint32_t parent, std::uint32_t hash, std::string_view name) const -> bool;

    provider::UniqueProvider mBucketProvider;
    provider::UniqueProvider mEntryProvider;
    const std::uint32_t mBucketCount;
};

class RomFileTable {
public:
    RomFileTable(
        provider::UniqueProvider dirBucketProvider, provider::UniqueProvider dirEntryProvider,
        provider::UniqueProvider fileBucketProvider, provider::UniqueProvider fileEntryProvider
    );

    auto findFile(RomFileInfo* out, std::string_view path) const -> bool;
    auto findDirectory(RomDirectoryInfo* out, std::string_view path) const -> bool;
    auto getFile(RomFileInfo* out, std::uint32_t pos, std::uint32_t* next = nullptr) const -> bool;
    auto getFile(RomFileInfo* out, char* name, std::size_t maxSize, std::uint32_t pos, std::uint32_t* next = nullptr) const -> bool;
    auto getDirectory(RomDirectoryInfo* out, std::uint32_t pos, std::uint32_t* next = nullptr) const -> bool;
    auto getDirectory(RomDirectoryInfo* out, char* name, std::size_t maxSize, std::uint32_t pos, std::uint32_t* next = nullptr) const -> bool;
    auto getRootDirectory(RomDirectoryInfo* out) const -> bool {
        return getDirectory(out, 0);
    }
    [[nodiscard]] auto getSubDirectoryCount(std::uint32_t pos) -> std::uint32_t;
    [[nodiscard]] auto getSiblingDirectoryCount(std::uint32_t pos) -> std::uint32_t;

private:
    auto findRecursive(std::string_view path, std::uint32_t pos, bool isDir) const -> std::uint32_t;

    EntryMap<RomDirectoryEntry> mDirectoryMap;
    EntryMap<RomFileEntry> mFileMap;
};

class RomFileSystem : public fs::ReadOnlyFileSystemBase {
public:
    explicit RomFileSystem(provider::UniqueProvider provider);

    ~RomFileSystem() override = default;

    [[nodiscard]] auto getRoot() const -> std::unique_ptr<fs::IDirectory> override;

    auto init() -> void override {}
    auto destroy() -> void override {}

    auto getAttributes(std::string_view path, fuse_wrapper::stat* stat) const -> Result override;

    auto access(std::string_view path, fs::OpenMode mode) const -> Result override;

    auto readLink(std::string_view, char*, std::size_t) const -> Result override { return UNIMPLEMENTED; }

    auto openFile(std::unique_ptr<fs::IFile>* out, std::string_view path, fs::OpenMode mode) const -> Result override;

    auto openDirectory(std::unique_ptr<fs::IDirectory>* dir, std::string_view path) const -> Result override;

private:
    [[nodiscard]] auto read(void* dst, std::int64_t offset, std::int64_t size) const -> std::size_t {
        if (offset < 0 || size < 0 || dst == nullptr) {
            return 0;
        }

        return mProvider->read(dst, size, mDataOffset + offset);
    }

    std::unique_ptr<RomFileTable> mFileTable;
    provider::SharedProvider mProvider;
    std::int64_t mDataOffset;
    time_t mInitTime;

    friend class File;
    friend class Directory;

    class File final : public fs::ReadOnlyFileBase {
    public:
        File(const RomFileSystem& fs, const RomFileInfo& info, std::string_view name) : mParentFileSystem(fs), mStart(info.offset), mSize(info.size), mName(name) {}

        [[nodiscard]] auto getName() const -> std::string_view override { return mName; }

        ~File() override = default;

        auto read(void* dst, std::size_t size, std::size_t offset) const -> std::size_t override;
        auto getSize() const -> std::size_t override { return mSize; }

    private:
        const RomFileSystem& mParentFileSystem;
        const std::int64_t mStart;
        const std::int64_t mSize;
        const std::string mName;
    };

    class Directory final : public fs::ReadOnlyDirectoryBase {
    public:
        Directory(const RomFileSystem& fs, const RomDirectoryInfo& info, std::string_view name) : mParentFileSystem(fs), mRootDir(info.nextDir), mRootFile(info.nextFile), mName(name) {}

        [[nodiscard]] auto getName() const -> std::string_view override { return mName; }

        ~Directory() override = default;

        auto getCount(std::size_t* count) const -> Result override {
            return readImpl(count, nullptr, 0, 0);
        }

        auto read(std::size_t* entryCount, fs::DirectoryEntry* entries, std::size_t maxEntries, std::size_t offset) const -> Result override {
            return readImpl(entryCount, entries, maxEntries, offset);
        }

    private:
        [[nodiscard]] auto readImpl(std::size_t* entryCount, fs::DirectoryEntry* entries, std::size_t maxEntries, std::size_t offset) const -> Result;

        const RomFileSystem& mParentFileSystem;
        const std::uint32_t mRootDir;
        const std::uint32_t mRootFile;
        const std::string mName;
    };
};

} // namespace nxmount::formats