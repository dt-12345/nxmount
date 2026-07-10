#include "crypto/crypto.hpp"
#include "formats/hfs0.hpp"
#include "log/logging.hpp"
#include "provider/file_provider.hpp"

#include <stdexcept>

namespace nxmount::formats {

Sha256FileSystem::Sha256FileSystem(provider::UniqueProvider provider, std::string_view name) : PartitionFileSystemBase(std::move(provider), name) {
    if (mProvider == nullptr) {
        LOG_ERROR("Null bytes provider passed to Sha256FileSystem");
        throw std::runtime_error(mName);
    }

    Sha256FileSystemHeader header;
    if (mProvider->read(std::addressof(header), sizeof(header), 0) != sizeof(header)) {
        LOG_ERROR("Failed to read Sha256FileSystem header!");
        throw std::runtime_error(mName);
    }

    if (header.magic != Sha256FileSystemHeader::cMagic) {
        LOG_ERROR("Invalid Sha256FileSystem header magic! {:#010x}", header.magic);
        throw std::runtime_error(mName);
    }

    mEntries.resize(header.entryCount);

    auto stringTable = std::vector<char>(header.stringTableSize + 1);
    stringTable[header.stringTableSize] = '\0';

    const auto entryTableOffset = sizeof(header);
    const auto stringTableOffset = entryTableOffset + header.entryCount * sizeof(FileEntry);

    if (mProvider->read(stringTable.data(), stringTable.size() - 1, stringTableOffset) != stringTable.size() - 1) {
        LOG_ERROR("Failed to read Sha256FileSystem string table");
        throw std::runtime_error(mName);
    }

    mDataStart = stringTableOffset + header.stringTableSize;
    mDataEnd = mProvider->getSize();

    std::size_t offset = 0;
    for (auto& entry : mEntries) {
        FileEntry data;
        if (mProvider->read(std::addressof(data), sizeof(data), entryTableOffset + offset) != sizeof(data)) {
            LOG_ERROR("Failed to read Sha256FileSystem entry!");
            throw std::runtime_error(mName);
        }
        entry.size = data.size;
        entry.offset = mDataStart + data.offset;
        entry.name = stringTable.data() + data.stringOffset;
        
        auto hashRegion = std::vector<std::uint8_t>(data.hashRegionSize);
        if (read(hashRegion.data(), hashRegion.size(), entry.offset) != hashRegion.size()) {
            LOG_ERROR("Failed to read hashed region of file in Sha256FileSystem!");
            throw std::runtime_error(mName);
        }

        if (!crypto::Sha256Verify(hashRegion.data(), hashRegion.size(), data.sha256Digest)) {
            LOG_ERROR("Corrupted block detected in Sha256FileSystem! {}", entry.name);
            throw std::runtime_error(mName);
        }

        offset += sizeof(data);
    }
}

auto RootSha256FileSystem::processEntry(Entry& entry, std::size_t index) -> bool {
    if (Sha256FileSystem::processEntry(entry, index)) {
        return true;
    }

    if (entry.fs != nullptr) {
        return true;
    }

    auto provider = std::make_unique<provider::FileProvider>(std::make_unique<File>(*this, index));
    std::uint32_t magic;
    if (provider->read(std::addressof(magic), sizeof(magic), 0) != sizeof(magic)) {
        entry.fs = nullptr;
        return false;
    }

    if (magic != Sha256FileSystemHeader::cMagic) {
        entry.fs = nullptr;
        return false;
    }

    entry.fs = std::make_unique<Sha256FileSystem>(std::move(provider), entry.name);
    entry.fs->init();
    return true;
}

} // namespace nxmount::formats