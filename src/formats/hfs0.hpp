#pragma once

#include "formats/pfs0.hpp"
#include "provider/provider.hpp"

namespace nxmount::formats {

struct Sha256FileSystemHeader {
    static constexpr const auto cMagic = common::MakeMagic("HFS0");
    
    std::uint32_t magic; // HFS0
    std::uint32_t entryCount;
    std::uint32_t stringTableSize;
    std::uint32_t reserved;
};
static_assert(sizeof(Sha256FileSystemHeader) == 0x10);

struct FileEntry {
    std::uint64_t offset;
    std::uint64_t size;
    std::uint32_t stringOffset;
    std::uint32_t hashRegionSize;
    std::uint64_t reserved;
    std::uint8_t  sha256Digest[0x20];
};
static_assert(sizeof(FileEntry) == 0x40);

class Sha256FileSystem : public PartitionFileSystemBase {
public:
    Sha256FileSystem(provider::UniqueProvider provider, std::string_view name);
};

class RootSha256FileSystem final : public Sha256FileSystem {
public:
    RootSha256FileSystem(provider::UniqueProvider provider, std::string_view name) : Sha256FileSystem(std::move(provider), name) {}
    
    [[nodiscard]] auto getSecurePartition() const -> Sha256FileSystem* {
        for (const auto& entry : mEntries) {
            if (entry.name == "secure") {
                return static_cast<Sha256FileSystem*>(entry.fs.get());
            }
        }
        return nullptr;
    }
private:
    auto processEntry(Entry& entry, std::size_t index) -> bool override;
};

} // namespace nxmount::formats