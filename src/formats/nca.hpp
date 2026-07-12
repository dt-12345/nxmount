#pragma once

#include "common/utils.hpp"
#include "formats/bktr.hpp"
#include "formats/pfs0.hpp"
#include "fs/directory.hpp"
#include "fs/file.hpp"
#include "fs/filesystem.hpp"
#include "provider/provider.hpp"

#include <array>
#include <ctime>

#if defined(_MSC_VER)
#pragma warning(disable : 4201) // nameless struct/union
#endif

namespace nxmount::formats {

inline constexpr const std::size_t cSectorSize = 0x200;

[[nodiscard]] ALWAYS_INLINE constexpr auto SectorToOffset(std::uint32_t sector) -> std::uint64_t {
    return sector * cSectorSize;
}

[[nodiscard]] ALWAYS_INLINE constexpr auto OffsetToSector(std::uint64_t offset) -> std::uint32_t {
    return offset / cSectorSize;
}

enum class NCAVersion {
    NCA0, NCA0Beta, NCA2, NCA3, Unknown,
};

enum class DistributionType : std::uint8_t {
    Download    = 0,
    GameCard    = 1,
};

enum class ContentType : std::uint8_t {
    Program     = 0,
    Meta        = 1,
    Control     = 2,
    Manual      = 3,
    Data        = 4,
    PublicData  = 5,
};

enum class KeyGeneration : std::uint8_t {
    Generation_1_0_0    = 0,
    // 1
    Generation_3_0_0    = 2,
    Generation_3_0_1    = 3,
    Generation_4_0_0    = 4,
    Generation_5_0_0    = 5,
    Generation_6_0_0    = 6,
    Generation_6_2_0    = 7,
    Generation_7_0_0    = 8,
    Generation_8_1_0    = 9,
    Generation_9_0_0    = 0xa,
    Generation_9_1_0    = 0xb,
    Generation_12_1_0   = 0xc,
    Generation_13_0_0   = 0xd,
    Generation_14_0_0   = 0xe,
    Generation_15_0_0   = 0xf,
    Generation_16_0_0   = 0x10,
    Generation_17_0_0   = 0x11,
    Generation_18_0_0   = 0x12,
    Generation_19_0_0   = 0x13,
    Generation_20_0_0   = 0x14,
    Generation_21_0_0   = 0x15,
    Generation_22_0_0   = 0x16,

    Invalid             = 0xff,
};

enum class KeyAreaIndex : std::uint8_t {
    Application     = 0,
    Ocean           = 1,
    System          = 2,
};

struct FsInfo {
    std::uint32_t startSector;
    std::uint32_t endSector;
    std::uint32_t hashSectors;
    std::uint32_t reserved;
};
static_assert(sizeof(FsInfo) == 0x10);

struct NintendoContentArchiveHeader {
    static constexpr const auto cMagic0 = common::MakeMagic("NCA0");
    static constexpr const auto cMagic1 = common::MakeMagic("NCA1");
    static constexpr const auto cMagic2 = common::MakeMagic("NCA2");
    static constexpr const auto cMagic3 = common::MakeMagic("NCA3");

    static constexpr const auto cFsCount = 4u;

    std::uint8_t headerSignature[0x100];
    std::uint8_t headerSignatureNpdm[0x100];
    std::uint32_t magic;
    DistributionType distributionType;
    ContentType contentType;
    KeyGeneration keyGenerationOld;
    KeyAreaIndex keyAreaEncryptionKeyIndex;
    std::uint64_t contentSize;
    std::uint64_t programId;
    std::uint32_t contentIndex;
    union {
        std::uint32_t sdkVersion;
        struct {
            std::uint8_t revision;
            std::uint8_t patch;
            std::uint8_t minor;
            std::uint8_t major;
        };
    };
    KeyGeneration keyGeneration;
    std::uint8_t signatureKeyGeneration;
    std::uint8_t reserved0[0xe];
    std::uint8_t rightsId[0x10];
    FsInfo fsInfo[cFsCount];
    std::uint8_t fsHeaderSha256Digests[cFsCount][0x20];
    std::uint8_t encryptedKeyArea[0x100];
};
static_assert(sizeof(NintendoContentArchiveHeader) == 0x400);

enum class FsType : std::uint8_t {
    RomFs       = 0,
    PartitionFs = 1,
};

enum class HashType : std::uint8_t {
    Auto                            = 0,
    None                            = 1,
    HierarchicalSha256Hash          = 2,
    HierarchicalIntegrityHash       = 3,
    AutoSha3                        = 4,
    HierarchicalSha3256Hash         = 5,
    HierarchicalIntegritySha3Hash   = 6,
};

enum class EncryptionType : std::uint8_t {
    Auto                    = 0,
    None                    = 1,
    AesXts                  = 2,
    AesCtr                  = 3,
    AesCtrEx                = 4,
    AesCtrSkipLayerHash     = 5,
    AesCtrExSkipLayerHash   = 6,
};

enum DecryptionKey {
    DecryptionKey_AesXts   = 0,
    DecryptionKey_AesXts1  = DecryptionKey_AesXts,
    DecryptionKey_AesXts2  = 1,
    DecryptionKey_AesCtr   = 2,
    DecryptionKey_AesCtrEx = 3,
    DecryptionKey_AesCtrHw = 4,
    DecryptionKey_Count,
};

enum class MetaDataHashType : std::uint8_t {
    None                    = 0,
    HierarchicalIntegrity   = 1,
};

struct LayerRegion {
    std::uint64_t offset;
    std::uint64_t size;
};

union HashData {
    struct HierarchicalSha256HashData {
        std::uint8_t masterHash[0x20];
        std::uint32_t blockSize;
        std::uint32_t layerCount;
        LayerRegion layerRegions[5];
        std::uint8_t reserved[0x80];
    } hierarchicalSha256HashData;

    struct IntegrityMetaInfo {
        static constexpr const auto cMagic = common::MakeMagic("IVFC");

        std::uint32_t magic;
        std::uint32_t version;
        std::uint32_t masterHashSize;

        struct LevelHashInfo {
            std::uint32_t maxLayers;

            struct HierarchicalIntegrityVerificationLevelInformation {
                PACKED(std::int64_t offset);
                PACKED(std::int64_t size);
                std::int32_t blockOrder;
                std::uint32_t reserved;
            } info[6];

            struct SignatureSalt {
                std::uint8_t value[0x20];
            } seed;
        } levelHashInfo;

        std::uint8_t masterHash[0x20];
    } integrityMetaInfo;
};
static_assert(sizeof(HashData) == 0xf8);

struct BucketInfo {
    std::int64_t offset;
    std::int64_t size;
    BucketTreeHeader header;
};
static_assert(sizeof(BucketInfo) == 0x20);

struct PatchInfo {
    std::int64_t indirectOffset;
    std::int64_t indirectSize;
    BucketTreeHeader indirectHeader;
    std::int64_t aesCtrExOffset;
    std::int64_t aesCtrExSize;
    BucketTreeHeader aesCtrExHeader;
};
static_assert(sizeof(PatchInfo) == 0x40);

struct SparseInfo {
    BucketInfo bucket;
    std::uint64_t physicalOffset;
    std::uint16_t generation;
    std::uint8_t reserved[6];
};
static_assert(sizeof(SparseInfo) == 0x30);

union AesCtrUpperIv {
    std::uint64_t value;
    struct {
        std::uint32_t generation;
        std::uint32_t secureValue;
    };
};
static_assert(sizeof(AesCtrUpperIv) == 8);

struct CompressionInfo {
    BucketInfo bucket;
    std::uint64_t reserved;
};
static_assert(sizeof(CompressionInfo) == 0x28);

struct MetaDataHashDataInfo {
    std::int64_t offset;
    std::int64_t size;
    std::uint8_t hash[0x20];
};
static_assert(sizeof(MetaDataHashDataInfo) == 0x30);

struct FsHeader {
    static constexpr const std::uint16_t cVersion = 2;

    std::uint16_t version;
    FsType fsType;
    HashType hashType;
    EncryptionType encryptionType;
    MetaDataHashType metaDataHashType;
    std::uint16_t reserved0;
    HashData hashData;
    PatchInfo patchInfo;
    AesCtrUpperIv aesCtrUpperIv;
    SparseInfo sparseInfo;
    CompressionInfo compressionInfo;
    MetaDataHashDataInfo metaDataHashDataInfo;
    std::uint8_t reserved1[0x30];
};
static_assert(sizeof(FsHeader) == 0x200);

struct EncryptedHeaderRegion {
    NintendoContentArchiveHeader header;
    FsHeader fsHeaders[NintendoContentArchiveHeader::cFsCount];
};
static_assert(sizeof(EncryptedHeaderRegion) == 0xc00);

struct MetaDataHashData {
    std::int64_t layerInfoOffset;
    HashData::IntegrityMetaInfo info;
};

[[nodiscard]] auto ToString(ContentType type) -> std::string_view;

class NintendoContentArchiveFileSystem : public fs::ReadOnlyFileSystemBase {
public:
    NintendoContentArchiveFileSystem(provider::SharedProvider provider, std::string_view name);

    ~NintendoContentArchiveFileSystem() override = default;

    [[nodiscard]] auto getRoot() const -> std::unique_ptr<fs::IDirectory> override { return std::make_unique<Directory>(*this); }

    auto init() -> void override {}
    auto destroy() -> void override {}

    auto getAttributes(std::string_view path, fs::DirectoryEntry* entry) const -> Result override;

    auto access(std::string_view path, fs::OpenMode mode) const -> Result override;

    auto readLink(std::string_view, char*, std::size_t) const -> Result override { return UNIMPLEMENTED; }

    auto openFile(std::unique_ptr<fs::IFile>* out, std::string_view path, fs::OpenMode mode) const -> Result override;

    auto openDirectory(std::unique_ptr<fs::IDirectory>* dir, std::string_view path) const -> Result override;

    [[nodiscard]] auto getContentType() const -> ContentType { return mHeader.header.contentType; }
    [[nodiscard]] auto getRightsId() const -> const std::uint8_t(&)[0x10] { return mHeader.header.rightsId; }

    auto setBase(NintendoContentArchiveFileSystem& baseNCA) -> void;

private:
    [[nodiscard]] auto decryptNCAHeader(EncryptedHeaderRegion& outHeader) -> NCAVersion;
    auto initializeFileSystems(NintendoContentArchiveFileSystem* baseNCA = nullptr) -> void;

    [[nodiscard]] auto createProvider(std::size_t fsIndex, NintendoContentArchiveFileSystem* baseNCA = nullptr) const -> provider::UniqueProvider;
    [[nodiscard]] auto createAesCtrExProvider(
        provider::UniqueProvider dataProvider, provider::SharedProvider metaProvider, const FsHeader& header, const FsInfo& info
    ) const -> provider::UniqueProvider;
    [[nodiscard]] auto createAesCtrProvider(provider::UniqueProvider provider, const AesCtrUpperIv& upperIv, std::size_t offset) const -> provider::UniqueProvider;
    [[nodiscard]] auto createDecryptedProvider(provider::UniqueProvider provider, const FsHeader& header, const FsInfo& info) const -> provider::UniqueProvider;
    [[nodiscard]] auto createIndirectProvider(
        provider::SharedProvider dataProvider, provider::SharedProvider metaProvider, NintendoContentArchiveFileSystem* baseNCA, std::size_t fsIndex, const PatchInfo& patchInfo
    ) const -> provider::UniqueProvider;
    [[nodiscard]] auto createSha256Provider(provider::UniqueProvider provider, const HashData::HierarchicalSha256HashData& hashData) const -> provider::UniqueProvider;

    [[nodiscard]] auto getDecryptionKey(DecryptionKey type) const -> const std::uint8_t(&)[0x10];

    auto printFsInfo(std::size_t index) const -> void;

    struct NCAFileSystem {
        std::unique_ptr<fs::IFileSystem> fs = nullptr;
        std::string name = "";
    };

    auto verifyNPDMSignature(NCAFileSystem& fs) -> void;

    friend class Directory;

    provider::SharedProvider mProvider;
    std::uint8_t mDecryptedKeys[DecryptionKey_Count][0x10] = { { 0 }, { 0 }, { 0 }, { 0 }, { 0 } };
    std::uint8_t mTitleKey[0x10] = { 0 };
    std::string mName;
    std::array<NCAFileSystem, NintendoContentArchiveHeader::cFsCount> mFileSystems;
    time_t mInitTime;
    EncryptedHeaderRegion mHeader;
    NCAVersion mVersion;

    // file representing the raw encrypted NCA (TODO: should this give you a view of the unencrypted NCA?)
    class NCAFile final : public fs::ReadOnlyFileBase {
    public:
        NCAFile(const NintendoContentArchiveFileSystem& fs) : mParentFileSystem(fs) {}

        [[nodiscard]] auto getName() const -> std::string_view override { return mParentFileSystem.mName; }

        ~NCAFile() override = default;

        auto getSize() const -> std::size_t override { return mParentFileSystem.mProvider->getSize(); }

        auto read(void* dst, std::size_t size, std::size_t offset) const -> std::size_t override;

    private:
        const NintendoContentArchiveFileSystem& mParentFileSystem;
    };

    class Directory final : public fs::ReadOnlyDirectoryBase {
    public:
        Directory(const NintendoContentArchiveFileSystem& fs) : mParentFileSystem(fs) {}

        [[nodiscard]] auto getName() const -> std::string_view override { return mParentFileSystem.mName; }

        ~Directory() override = default;

        auto getCount(std::size_t* count) const -> Result override {
            if (count == nullptr) {
                return INVALID;
            }

            std::size_t nonNull = 0;
            for (const auto& fs : mParentFileSystem.mFileSystems) {
                if (fs.fs != nullptr) {
                    ++nonNull;
                }
            }
            *count = nonNull + 1;
            return SUCCESS;
        }

        auto read(std::size_t* entryCount, fs::DirectoryEntry* entries, std::size_t maxEntries, std::size_t offset) const -> Result override;

    private:
        const NintendoContentArchiveFileSystem& mParentFileSystem;
    };
};

} // namespace nxmount::formats

#if defined(_MSC_VER)
#pragma warning(default : 4201) // nameless struct/union
#endif