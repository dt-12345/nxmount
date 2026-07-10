#pragma once

#include "common/bitutils.hpp"
#include "common/utils.hpp"
#include "formats/hfs0.hpp"
#include "fs/filesystem.hpp"
#include "provider/provider.hpp"

#include <memory>

namespace nxmount::formats {

enum class GameCardSize : std::uint8_t {
    Size1GB     = 0xFA,
    Size2GB     = 0xF8,
    Size4GB     = 0xF0,
    Size8GB     = 0xE0,
    Size16GB    = 0xE1,
    Size32GB    = 0xE2,
};

enum class GameCardAttribute : std::uint8_t {
    AutoBoot                            = 1 << 0,
    HistoryErase                        = 1 << 1,
    RepairTool                          = 1 << 2,
    DifferentRegionCupToTerraDevice     = 1 << 3,
    DifferentRegionCupToGlobalDevice    = 1 << 4,
    CardHeaderSignKey                   = 1 << 7,
};

enum class GameCardAttribute2 : std::uint8_t {
    IsSecondCardHeader  = 1 << 0,
    HasSecureContent    = 1 << 1,
};

ENUM_OPERATORS(GameCardAttribute)
ENUM_OPERATORS(GameCardAttribute2)

enum class SelSec : std::uint32_t {
    T1 = 1,
    T2 = 2,
};

enum class FirmwareVersion : std::uint64_t {
    Development_1_0_0   = 0,
    Retail_1_0_0        = 1,
    Retail_4_0_0        = 2,
    Development_11_0_0  = 3,
    Retail_11_0_0       = 4,
    Retail_12_0_0       = 5,
};

enum class AccessControl : std::uint32_t {
    AccCtrl_25MHz   = 0x00a10011,
    AccCtrl_50MHz   = 0x00a10010,
};

union SdkVersion {
    std::uint32_t raw;
    struct {
        std::uint8_t revision;
        std::uint8_t patch;
        std::uint8_t minor;
        std::uint8_t major;
    };
};
static_assert(sizeof(SdkVersion) == 4);

struct UpdateVersion {
    std::uint8_t revisionMinor;
    std::uint8_t revisionMajor;
    union {
        common::BitRange<0, 4, std::uint16_t> patch;
        common::BitRange<4, 6, std::uint16_t> minor;
        common::BitRange<10, 6, std::uint16_t> major;
    };
};
static_assert(sizeof(UpdateVersion) == 4);

enum class CompatibilityType : std::uint8_t {
    Normal  = 0,
    Terra   = 1,
};

struct CardHeaderEncryptedData {
    FirmwareVersion fwVersion;
    AccessControl accessControl;
    std::uint32_t wait1ReadTime;
    std::uint32_t wait2ReadTime;
    std::uint32_t wait1WriteTime;
    std::uint32_t wait2WriteTime;
    SdkVersion fwMode;
    UpdateVersion updatePartitionVersion;
    CompatibilityType compatType;
    std::uint8_t padding[3];
    std::uint8_t updatePartitionHash[8];
    std::uint64_t updatePartitionId;
    std::uint8_t reserved[0x38];
};
static_assert(sizeof(CardHeaderEncryptedData) == 0x70);

struct CardHeaderT2EncryptedData {
    FirmwareVersion fwVersion;
    AccessControl accessControl;
    std::uint32_t wait1ReadTime;
    std::uint32_t wait2ReadTime;
    std::uint32_t wait1WriteTime;
    std::uint32_t wait2WriteTime;
    SdkVersion fwMode;
    UpdateVersion updatePartitionVersion;
    CompatibilityType compatType;
    std::uint8_t padding[3];
    std::uint8_t updatePartitionHash[8];
    std::uint64_t updatePartitionId;
    std::uint64_t reserved0;
    std::uint8_t relatedCardHeaderHash[0x20];
    std::uint8_t reserved1[0x10];
};
static_assert(sizeof(CardHeaderT2EncryptedData) == 0x70);

struct GameCardImageHeader {
    static constexpr const auto cMagic = common::MakeMagic("HEAD");

    std::uint8_t headerSignature[0x100];
    std::uint32_t magic;
    std::uint32_t romAreaStartPageAddress;
    std::uint32_t backupAreaStartPageAddress;
    union {
        common::BitRange<0, 4, std::uint8_t> kekIndex;
        common::BitRange<4, 4, std::uint8_t> titleKeyDecIndex;
    };
    GameCardSize gameCardSize;
    std::uint8_t version;
    GameCardAttribute attributes;
    std::uint64_t packageId;
    std::uint32_t validDataEndAddress;
    std::uint32_t reserved;
    std::uint8_t iv[0x10];
    std::uint64_t partitionFsHeaderAddress;
    std::uint64_t partitionFsHeaderSize;
    std::uint8_t partitionFsHeaderSha256Digest[0x20];
    std::uint8_t initialDataSha256Digest[0x20];
    SelSec selSec;
    std::uint32_t selT1Key;
    std::uint32_t selKey;
    std::uint32_t limArea;
    CardHeaderEncryptedData encryptedData;
};
static_assert(sizeof(GameCardImageHeader) == 0x200);

struct GameCardImageHeaderT2 {
    static constexpr const auto cMagic = common::MakeMagic("HEAD");

    std::uint8_t headerSignature[0x100];
    std::uint32_t magic;
    std::uint32_t romAreaStartPageAddress;
    std::uint32_t backupAreaStartPageAddress;
    union {
        common::BitRange<0, 4, std::uint8_t> kekIndex;
        common::BitRange<4, 4, std::uint8_t> titleKeyDecIndex;
    };
    GameCardSize gameCardSize;
    std::uint8_t version;
    GameCardAttribute attributes;
    std::uint64_t packageId;
    std::uint32_t validDataEndAddress;
    std::uint8_t cardHeaderSignKeyIndex;
    GameCardAttribute2 attributes2;
    std::uint16_t numberOfApplicationIds;
    std::uint8_t iv[0x10];
    std::uint64_t partitionFsHeaderAddress;
    std::uint64_t partitionFsHeaderSize;
    std::uint8_t partitionFsHeaderSha256Digest[0x20];
    std::uint8_t initialDataSha256Digest[0x20];
    SelSec selSec;
    std::uint32_t selT1Key;
    std::uint32_t selKey;
    std::uint32_t limArea;
    CardHeaderT2EncryptedData encryptedData;
};
static_assert(sizeof(GameCardImageHeaderT2) == 0x200);

class GameCardFileSystem : public fs::ReadOnlyFileSystemBase {
public:
    GameCardFileSystem(provider::UniqueProvider provider, std::string_view name);

    struct Entry {
        std::string name;
        std::size_t offset;
        std::size_t size;
        std::unique_ptr<fs::IFileSystem> fs;
    };

    ~GameCardFileSystem() override = default;

    [[nodiscard]] auto getRoot() const -> std::unique_ptr<fs::IDirectory> override {
        return mFileSystem->getRoot();
    }

    auto init() -> void override {
        mFileSystem->init();
    }
    auto destroy() -> void override {
        mFileSystem->destroy();
    }

    auto getAttributes(std::string_view path, fs::DirectoryEntry* entry) const -> Result override {
        return mFileSystem->getAttributes(path, entry);
    }

    auto access(std::string_view path, fs::OpenMode mode) const -> Result override {
        return mFileSystem->access(path, mode);
    }

    auto readLink(std::string_view, char*, std::size_t) const -> Result override { return UNIMPLEMENTED; }

    auto openFile(std::unique_ptr<fs::IFile>* out, std::string_view path, fs::OpenMode mode) const -> Result override {
        return mFileSystem->openFile(out, path, mode);
    }

    auto openDirectory(std::unique_ptr<fs::IDirectory>* dir, std::string_view path) const -> Result override {
        return mFileSystem->openDirectory(dir, path);
    }

    [[nodiscard]] auto getFileSystem() const -> const RootSha256FileSystem& { return *mFileSystem; }

private:
    std::unique_ptr<RootSha256FileSystem> mFileSystem;
};

} // namespace nxmount::formats