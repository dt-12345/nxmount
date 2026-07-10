#include "formats/romfs.hpp"
#include "common/errors.hpp"
#include "fs/path.hpp"
#include "log/logging.hpp"
#include "provider/memory_stream_provider.hpp"
#include "provider/provider.hpp"

#include <stdexcept>

namespace nxmount::formats {

template <typename ValueType>
auto EntryMap<ValueType>::find(std::uint32_t* posOut, Entry* out, std::uint32_t parent, std::uint32_t hash, std::string_view name) const -> bool {
    if (posOut == nullptr || out == nullptr) {
        return false;
    }
    const auto index = hash % mBucketCount;
    std::uint32_t pos;
    if (mBucketProvider->read(std::addressof(pos), sizeof(pos), index * sizeof(std::uint32_t)) != sizeof(pos)) {
        return false;
    }

    const auto maxEntrySize = static_cast<std::uint32_t>(mEntryProvider->getSize());
    if (pos == 0xffff'ffffu || pos >= maxEntrySize) {
        return false;
    }

    // this will loop indefinitely if there is a cyclic reference, but hopefully there's not (wouldn't make any sense anyways)
    char filename[fs::cMaxPath + 1];
    while (true) {
        if (mEntryProvider->read(out, sizeof(*out), pos) != sizeof(*out)) {
            return false;
        }

        if (out->auxSize > fs::cMaxPath || out->auxSize == 0) {
            return false;
        }

        if (mEntryProvider->read(filename, out->auxSize, pos + sizeof(*out)) != out->auxSize) {
            return false;
        }

        filename[out->auxSize] = '\0';
        if (out->key == parent && filename == name) {
            *posOut = pos;
            return true;
        }

        if (pos == out->next) { // infinite loop
            return false;
        }

        pos = out->next;

        if (pos == 0xffff'ffffu || pos >= maxEntrySize) {
            return false;
        }
    }
}

template class EntryMap<RomDirectoryEntry>;
template class EntryMap<RomFileEntry>;

RomFileTable::RomFileTable(
    provider::UniqueProvider dirBucketProvider, provider::UniqueProvider dirEntryProvider,
    provider::UniqueProvider fileBucketProvider, provider::UniqueProvider fileEntryProvider
) : mDirectoryMap(std::move(dirBucketProvider), std::move(dirEntryProvider)),
    mFileMap(std::move(fileBucketProvider), std::move(fileEntryProvider)) {}

auto RomFileTable::findFile(RomFileInfo* out, std::string_view path) const -> bool {
    if (out == nullptr) {
        return false;
    }

    std::uint32_t pos = 0xffff'ffffu;
    if (!mFileCache.get(path, std::addressof(pos))) {
        pos = findRecursive(path, 0, false);
        if (pos == 0xffff'ffffu) {
            return false;
        } else {
            mFileCache.add(path, pos);
        }
    }

    EntryMap<RomFileEntry>::Entry entry;
    if (!mFileMap.get(std::addressof(entry), pos)) {
        return false;
    }

    *out = entry.value.fileInfo;
    return true;
}

auto RomFileTable::findDirectory(RomDirectoryInfo* out, std::string_view path) const -> bool {
    if (out == nullptr) {
        return false;
    }

    std::uint32_t pos = 0xffff'ffffu;
    if (!mDirectoryCache.get(path, std::addressof(pos))) {
        pos = findRecursive(path, 0, true);
        if (pos == 0xffff'ffffu) {
            return false;
        } else {
            mDirectoryCache.add(path, pos);
        }
    }

    EntryMap<RomDirectoryEntry>::Entry entry;
    if (!mDirectoryMap.get(std::addressof(entry), pos)) {
        return false;
    }

    out->nextDir = entry.value.dir;
    out->nextFile = entry.value.file;
    return true;
}

auto RomFileTable::getFile(RomFileInfo* out, std::uint32_t pos, std::uint32_t* next) const -> bool {
    if (out == nullptr) {
        return false;
    }

    EntryMap<RomFileEntry>::Entry entry;
    if (!mFileMap.get(std::addressof(entry), pos)) {
        return false;
    }

    *out = entry.value.fileInfo;
    if (next != nullptr) {
        *next = entry.value.next;
    }
    return true;
}

auto RomFileTable::getFile(RomFileInfo* out, char* name, std::size_t maxSize, std::uint32_t pos, std::uint32_t* next) const -> bool {
    if (out == nullptr) {
        return false;
    }

    EntryMap<RomFileEntry>::Entry entry;
    if (!mFileMap.get(std::addressof(entry), name, maxSize, pos)) {
        return false;
    }

    *out = entry.value.fileInfo;
    if (next != nullptr) {
        *next = entry.value.next;
    }
    return true;
}

auto RomFileTable::getDirectory(RomDirectoryInfo* out, std::uint32_t pos, std::uint32_t* next) const -> bool {
    if (out == nullptr) {
        return false;
    }

    EntryMap<RomDirectoryEntry>::Entry entry;
    if (!mDirectoryMap.get(std::addressof(entry), pos)) {
        return false;
    }

    out->nextDir = entry.value.dir;
    out->nextFile = entry.value.file;
    if (next != nullptr) {
        *next = entry.value.next;
    }
    return true;
}

auto RomFileTable::getDirectory(RomDirectoryInfo* out, char* name, std::size_t maxSize, std::uint32_t pos, std::uint32_t* next) const -> bool {
    if (out == nullptr) {
        return false;
    }

    EntryMap<RomDirectoryEntry>::Entry entry;
    if (!mDirectoryMap.get(std::addressof(entry), name, maxSize, pos)) {
        return false;
    }

    out->nextDir = entry.value.dir;
    out->nextFile = entry.value.file;
    if (next != nullptr) {
        *next = entry.value.next;
    }
    return true;
}

auto RomFileTable::getSubDirectoryCount(std::uint32_t pos) -> std::uint32_t {
    EntryMap<RomDirectoryEntry>::Entry entry;
    if (!mDirectoryMap.get(std::addressof(entry), pos)) {
        return 0;
    }

    std::uint32_t count = 0u;
    auto current = entry.value.dir;
    while (current != 0xffff'ffffu) {
        ++count;
        if (!mDirectoryMap.get(std::addressof(entry), current)) {
            break;
        } else {
            current = entry.value.next;
        }
    }

    return count;
}

auto RomFileTable::getSiblingDirectoryCount(std::uint32_t pos) -> std::uint32_t {
    EntryMap<RomDirectoryEntry>::Entry entry;
    if (!mDirectoryMap.get(std::addressof(entry), pos)) {
        return 0;
    }

    std::uint32_t count = 1u;
    auto current = entry.value.next;
    while (current != 0xffff'ffffu) {
        ++count;
        if (!mDirectoryMap.get(std::addressof(entry), current)) {
            break;
        } else {
            current = entry.value.next;
        }
    }

    return count;
}

auto RomFileTable::findRecursive(std::string_view path, std::uint32_t parentPos, bool isDir) const -> std::uint32_t {
    std::string_view subpath;
    const auto name = fs::FirstComponent(path, std::addressof(subpath));
    if (name.empty()) {
        return 0xffff'ffffu;
    }

    if (isDir) {
        std::uint32_t pos;
        EntryMap<RomDirectoryEntry>::Entry entry;
        if (!mDirectoryMap.get(std::addressof(pos), std::addressof(entry), parentPos, name)) {
            return 0xffff'ffffu;
        }

        if (subpath.empty()) {
            return pos;
        } else {
            return findRecursive(subpath, pos, true);
        }
    } else if (subpath.empty()) { // file
        std::uint32_t pos;
        EntryMap<RomFileEntry>::Entry entry;
        if (!mFileMap.get(std::addressof(pos), std::addressof(entry), parentPos, name)) {
            return 0xffff'ffffu;
        }

        return pos;
    } else { // parent directory of file
        std::uint32_t pos;
        EntryMap<RomDirectoryEntry>::Entry entry;
        if (!mDirectoryMap.get(std::addressof(pos), std::addressof(entry), parentPos, name)) {
            return 0xffff'ffffu;
        }

        return findRecursive(subpath, pos, false);
    }
}

RomFileSystem::RomFileSystem(provider::UniqueProvider provider) : mProvider(std::move(provider)), mInitTime(time(nullptr)) {
    RomFileSystemInformation header;
    if (mProvider->read(std::addressof(header), sizeof(header), 0) != sizeof(header)) {
        LOG_ERROR("Failed to read RomFs header!");
        throw std::runtime_error("RomFileSystem");
    }

    if (header.size != sizeof(RomFileSystemInformation)) {
        LOG_ERROR("RomFs header size is corrupted!");
        throw std::runtime_error("RomFileSystem");
    }

    mDataOffset = header.bodyOffset;
    if (mDataOffset < 0) {
        LOG_ERROR("Invalid RomFs body offset!");
        throw std::runtime_error("RomFileSystem");
    }

    auto dirBucketProvider = std::make_unique<provider::MemoryStreamProvider>(*mProvider, header.directoryBucketSize, header.directoryBucketOffset);
    auto dirEntryProvider = std::make_unique<provider::MemoryStreamProvider>(*mProvider, header.directoryEntrySize, header.directoryEntryOffset);
    auto fileBucketProvider = std::make_unique<provider::MemoryStreamProvider>(*mProvider, header.fileBucketSize, header.fileBucketOffset);
    auto fileEntryProvider = std::make_unique<provider::MemoryStreamProvider>(*mProvider, header.fileEntrySize, header.fileEntryOffset);

    mFileTable = std::make_unique<RomFileTable>(
        std::move(dirBucketProvider), std::move(dirEntryProvider), std::move(fileBucketProvider), std::move(fileEntryProvider)
    );
}

auto RomFileSystem::getRoot() const -> std::unique_ptr<fs::IDirectory> {
    RomDirectoryInfo info{};
    mFileTable->getRootDirectory(std::addressof(info));
    return std::make_unique<Directory>(*this, info, "");
}

auto RomFileSystem::getAttributes(std::string_view path, fs::DirectoryEntry* entry) const -> Result {
    if (entry == nullptr) {
        return INVALID;
    }

    if (path.empty() || path == "/") {
        entry->type = fs::Type::Directory;
        entry->createTime = mInitTime;
        return SUCCESS;
    }

    RomFileInfo fileInfo;
    RomDirectoryInfo dirInfo;
    if (mFileTable->findFile(std::addressof(fileInfo), path)) {
        entry->type = fs::Type::File;
        entry->createTime = mInitTime;
        entry->fileSize = fileInfo.size;
        return SUCCESS;
    } else if (mFileTable->findDirectory(std::addressof(dirInfo), path)) {
        entry->type = fs::Type::Directory;
        entry->createTime = mInitTime;
        return SUCCESS;
    } else {
        LOG_WARNING("Failed to find file {}", path);
        return NO_FILE;
    }
}

auto RomFileSystem::access(std::string_view path, fs::OpenMode mode) const -> Result {
    if (path.empty() || path == "/") {
        if (mode != fs::OpenMode::Read) {
            return PERMISSION_ERROR;
        }

        return SUCCESS;
    }
    
    RomFileInfo fileInfo;
    RomDirectoryInfo dirInfo;

    if (!mFileTable->findFile(std::addressof(fileInfo), path) && !mFileTable->findDirectory(std::addressof(dirInfo), path)) {
        return NO_FILE;
    }

    if (mode != fs::OpenMode::Read) {
        return PERMISSION_ERROR;
    }

    return SUCCESS;
}

auto RomFileSystem::openFile(std::unique_ptr<fs::IFile>* out, std::string_view path, fs::OpenMode mode) const -> Result {
    if (out == nullptr) {
        return INVALID;
    }
    
    RomFileInfo fileInfo;
    if (!mFileTable->findFile(std::addressof(fileInfo), path)) {
        return NO_FILE;
    }

    if (mode != fs::OpenMode::Read) {
        return PERMISSION_ERROR;
    }

    *out = std::make_unique<File>(*this, fileInfo, fs::LastComponent(path));
    return SUCCESS;
}

auto RomFileSystem::openDirectory(std::unique_ptr<fs::IDirectory>* dir, std::string_view path) const -> Result {
    if (dir == nullptr) {
        return INVALID;
    }

    if (path.empty() || path == "/") {
        *dir = getRoot();
        return SUCCESS;
    }

    RomDirectoryInfo dirInfo;
    if (!mFileTable->findDirectory(std::addressof(dirInfo), path)) {
        return NO_FILE;
    }

    *dir = std::make_unique<Directory>(*this, dirInfo, fs::LastComponent(path));
    return SUCCESS;
}

auto RomFileSystem::File::read(void* dst, std::size_t size, std::size_t offset) const -> std::size_t {
    if (size == 0 || static_cast<std::int64_t>(offset) >= mSize) {
        return 0;
    }

    if (static_cast<std::int64_t>(offset + size) >= mSize) {
        size = static_cast<std::size_t>(mSize) - offset;
    }

    return mParentFileSystem.read(dst, mStart + offset, size);
}

auto RomFileSystem::Directory::readImpl(std::size_t* entryCount, fs::DirectoryEntry* entries, std::size_t maxEntries, std::size_t offset) const -> Result {
    std::size_t current = 0;
    std::size_t currentEntry = 0;

    auto dirPos = mRootDir;
    while (dirPos != 0xffff'ffffu) {
        RomDirectoryInfo info{};
        char name[fs::cMaxPath + 1];
        if (!mParentFileSystem.mFileTable->getDirectory(std::addressof(info), name, fs::cMaxPath, dirPos, std::addressof(dirPos))) {
            return NO_FILE;
        }
        if (current++ < offset) {
            continue;
        }
        if (entries != nullptr && currentEntry < maxEntries) {
            name[fs::cMaxPath] = '\0';
            entries[currentEntry].name = name;
            entries[currentEntry].type = fs::Type::Directory;
            entries[currentEntry].createTime = mParentFileSystem.mInitTime;
            entries[currentEntry++].fileSize = 0;
        }
    }

    auto filePos = mRootFile;
    while (filePos != 0xffff'ffffu) {
        RomFileInfo info{};
        char name[fs::cMaxPath + 1];
        if (!mParentFileSystem.mFileTable->getFile(std::addressof(info), name, fs::cMaxPath, filePos, std::addressof(filePos))) {
            return NO_FILE;
        }
        if (current++ < offset) {
            continue;
        }
        if (entries != nullptr && currentEntry < maxEntries) {
            name[fs::cMaxPath] = '\0';
            entries[currentEntry].name = name;
            entries[currentEntry].type = fs::Type::File;
            entries[currentEntry].createTime = mParentFileSystem.mInitTime;
            entries[currentEntry++].fileSize = static_cast<std::size_t>(std::max(info.size, std::int64_t(0)));
        }
    }

    if (entryCount != nullptr) {
        *entryCount = current >= offset ? current - offset : 0;
    }

    return SUCCESS;
}

} // namespace nxmount::formats