#pragma once

#include <provider/provider.hpp>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace nxmount::formats {

enum class ContentMetaPlatform : std::uint8_t {
    NX      = 0,
    Ounce   = 1,
};

enum class ContentMetaType : std::uint8_t {
    Unknown                 = 0x00,
    SystemProgram           = 0x01,
    SystemData              = 0x02,
    SystemUpdate            = 0x03,
    BootImagePackage        = 0x04,
    BootImagePackageSafe    = 0x05,
    Application             = 0x80,
    Patch                   = 0x81,
    AddOnContent            = 0x82,
    Delta                   = 0x83,
    DataPatch               = 0x84,
};

enum class PackagedContentType : std::uint8_t {
    Meta                    = 0,
    Program                 = 1,
    Data                    = 2,
    Control                 = 3,
    HtmlDocument            = 4,
    LegalInformation        = 5,
    DeltaFragment           = 6,
};

enum class UpdateType : std::uint8_t {
    ApplyAsDelta    = 0,
    Overwrite       = 1,
    Create          = 2,
};

enum class ContentInstallType : std::uint8_t {
    Full            = 0,
    FragmentOnly    = 1,
    Unknown         = 7,
};

using ContentId = std::uint8_t[0x10];

struct PackagedContentInfo {
    std::uint8_t hash[0x20];
    ContentId contentId;
    std::uint32_t sizeLow;
    std::uint8_t sizeHigh;
    std::uint8_t attr;
    PackagedContentType contentType;
    std::uint8_t idOffset;
};
static_assert(sizeof(PackagedContentInfo) == 0x38);

struct PackagedContentMetaHeader {
    std::uint64_t applicationId;
    std::uint32_t version;
    ContentMetaType contentMetaType;
    ContentMetaPlatform platform;
    std::uint16_t extendedHeaderSize;
    std::uint16_t contentCount;
    std::uint16_t contentMetaCount;
    std::uint8_t contentMetaAttribute;
    std::uint8_t storageId;
    std::uint8_t contentInstallType;
    std::uint8_t committed;
    std::uint32_t requiredDownloadSystemVersion;
    std::uint8_t reserved[4];
};
static_assert(sizeof(PackagedContentMetaHeader) == 0x20);

struct SystemUpdateMetaExtendedHeader {
    std::uint32_t extendedDataSize;
};
static_assert(sizeof(SystemUpdateMetaExtendedHeader) == 4);

struct ApplicationMetaExtendedHeader {
    std::uint64_t patchId;
    std::uint32_t requiredSystemVersion;
    std::uint32_t requiredApplicationVersion;
};
static_assert(sizeof(ApplicationMetaExtendedHeader) == 0x10);

struct PatchMetaExtendedHeader {
    std::uint64_t applicationId;
    std::uint32_t requiredSystemVersion;
    std::uint32_t extendedDataSize;
    std::uint64_t reserved;
};
static_assert(sizeof(PatchMetaExtendedHeader) == 0x18);

struct AddOnContentMetaExtendedHeaderOld {
    std::uint64_t applicationId;
    std::uint32_t requiredApplicationVersion;
    std::uint8_t contentAccessibilities;
    std::uint8_t reserved[3];
};
static_assert(sizeof(AddOnContentMetaExtendedHeaderOld) == 0x10);

struct AddOnContentMetaExtendedHeader : public AddOnContentMetaExtendedHeaderOld {
    std::uint64_t dataPatchId;
};
static_assert(sizeof(AddOnContentMetaExtendedHeader) == 0x18);

struct DeltaMetaExtendedHeader {
    std::uint64_t applicationId;
    std::uint32_t extendedDataSize;
    std::uint32_t reserved;
};
static_assert(sizeof(DeltaMetaExtendedHeader) == 0x10);

struct DataPatchMetaExtendedHeader {
    std::uint64_t dataId;
    std::uint64_t applicationId;
    std::uint32_t requiredApplicationVersion;
    std::uint32_t extendedDataSize;
    std::uint64_t reserved;
};
static_assert(sizeof(DataPatchMetaExtendedHeader) == 0x20);

struct PatchMetaExtendedData {
    std::uint32_t patchHistoryHeaderCount;
    std::uint32_t patchDeltaHistoryCount;
    std::uint32_t patchDeltaHeaderCount;
    std::uint32_t fragmentSetCount;
    std::uint32_t patchHistoryContentInfoCount;
    std::uint32_t patchDeltaPackagedContentInfoCount;
    std::uint32_t reserved;
};
static_assert(sizeof(PatchMetaExtendedData) == 0x1c);

struct DeltaMetaExtendedData {
    std::uint64_t sourcePatchId;
    std::uint64_t destinationPatchId;
    std::uint32_t sourceVersion;
    std::uint32_t destinationVersion;
    std::uint16_t fragmentSetCount;
    std::uint8_t reserved[6];
};
static_assert(sizeof(DeltaMetaExtendedData) == 0x20);

struct ContentMetaKey {
    std::uint64_t id;
    std::uint32_t version;
    ContentMetaType metaType;
    ContentInstallType installType;
    std::uint16_t reserved;
};

struct PatchHistoryHeader {
    ContentMetaKey contentMetaKey;
    std::uint8_t digest[0x20];
    std::uint16_t contentInfoCount;
    std::uint8_t reserved[6];
};
static_assert(sizeof(PatchHistoryHeader) == 0x38);

struct PatchDeltaHistory {
    std::uint64_t sourcePatchId;
    std::uint64_t destinationPatchId;
    std::uint32_t sourceVersion;
    std::uint32_t destinationVersion;
    std::uint64_t downloadSize;
    std::uint64_t reserved;
};
static_assert(sizeof(PatchDeltaHistory) == 0x28);

struct PatchDeltaHeader {
    std::uint64_t sourcePatchId;
    std::uint64_t destinationPatchId;
    std::uint32_t sourceVersion;
    std::uint32_t destinationVersion;
    std::uint16_t fragmentSetCount;
    std::uint8_t reserved0[6];
    std::uint16_t contentInfoCount;
    std::uint8_t reserved1[6];
};
static_assert(sizeof(PatchDeltaHeader) == 0x28);

struct FragmentSet {
    ContentId sourceContentId;
    ContentId destinationContentId;
    std::uint32_t sourceSizeLow;
    std::uint16_t sourceSizeHigh;
    std::uint16_t destinationSizeHigh;
    std::uint32_t destinationSizeLow;
    std::uint16_t fragmentIndicatorCount;
    PackagedContentType fragmentTargetContentType;
    UpdateType updateType;
    std::uint32_t reserved;
};
static_assert(sizeof(FragmentSet) == 0x34);

struct PatchHistoryContentInfo {
    ContentId contentId;
    std::uint32_t sizeLow;
    std::uint8_t sizeHigh;
    std::uint8_t attr;
    PackagedContentType contentType;
    std::uint8_t idOffset;
};
static_assert(sizeof(PatchHistoryContentInfo) == 0x18);

struct FragmentIndicator {
    std::uint16_t contentInfoIndex;
    std::uint16_t fragmentIndex;
};
static_assert(sizeof(FragmentIndicator) == 4);

using PatchDeltaPackagedContentInfo = PackagedContentInfo;

template <template <typename> typename T>
struct IsSpan;

template <>
struct IsSpan<std::span> : public std::true_type {};

template <template <typename> typename T>
struct IsSpan : public std::false_type {};

template <template <typename> typename Container>
class ReaderBase {
public:
    using StorageT = std::conditional_t<IsSpan<Container>::value, const std::uint8_t, std::uint8_t>;

    ReaderBase() = default;
    explicit ReaderBase(std::span<StorageT> data) : mData(data) {}

    template <typename T>
    [[nodiscard]] auto getPointer(std::size_t offset) const -> const T* {
        if (mData.size() >= offset + sizeof(T)) {
            return reinterpret_cast<const T*>(mData.data() + offset);
        } else {
            return nullptr;
        }
    }

    template <typename T>
    [[nodiscard]] auto getArray(std::size_t offset, std::size_t count) const -> std::span<const T> {
        if (mData.size() >= offset + sizeof(T) * count) {
            return { reinterpret_cast<const T*>(mData.data() + offset), count };
        } else {
            return {};
        }
    }

protected:
    Container<StorageT> mData;
};

class PatchMetaExtendedDataReader final : public ReaderBase<std::span> {
public:
    PatchMetaExtendedDataReader() = default;
    explicit PatchMetaExtendedDataReader(std::span<const std::uint8_t> data) : ReaderBase(data) {}

    [[nodiscard]] auto isValid() const -> bool {
        return getHeader() != nullptr;
    }

    [[nodiscard]] auto getHeader() const -> const PatchMetaExtendedData* {
        return getPointer<PatchMetaExtendedData>(getHeaderOffset());
    }

    [[nodiscard]] auto getPatchHistoryHeaderCount() const -> std::uint32_t {
        const auto header = getHeader();
        return header != nullptr ? header->patchHistoryHeaderCount : 0;
    }

    [[nodiscard]] auto getPatchDeltaHistoryCount() const -> std::uint32_t {
        const auto header = getHeader();
        return header != nullptr ? header->patchDeltaHistoryCount : 0;
    }

    [[nodiscard]] auto getPatchDeltaHeaderCount() const -> std::uint32_t {
        const auto header = getHeader();
        return header != nullptr ? header->patchDeltaHeaderCount : 0;
    }

    [[nodiscard]] auto getFragmentSetCount() const -> std::uint32_t {
        const auto header = getHeader();
        return header != nullptr ? header->fragmentSetCount : 0;
    }

    [[nodiscard]] auto getPatchHistoryContentInfoCount() const -> std::uint32_t {
        const auto header = getHeader();
        return header != nullptr ? header->patchHistoryContentInfoCount : 0;
    }

    [[nodiscard]] auto getPatchDeltaPackagedContentInfoCount() const -> std::uint32_t {
        const auto header = getHeader();
        return header != nullptr ? header->patchDeltaPackagedContentInfoCount : 0;
    }

    [[nodiscard]] auto getFragmentIndicatorCount() const -> std::uint32_t {
        std::uint32_t count = 0;
        for (const auto& set : getFragmentSet()) {
            count += set.fragmentIndicatorCount;
        }
        return count;
    }

    [[nodiscard]] auto getPatchHistoryHeader() const -> std::span<const PatchHistoryHeader> {
        return getArray<PatchHistoryHeader>(getPatchHistoryHeaderOffset(), getPatchHistoryHeaderCount());
    }

    [[nodiscard]] auto getPatchDeltaHistory() const -> std::span<const PatchDeltaHistory> {
        return getArray<PatchDeltaHistory>(getPatchDeltaHistoryOffset(), getPatchDeltaHistoryCount());
    }

    [[nodiscard]] auto getPatchDeltaHeader() const -> std::span<const PatchDeltaHeader> {
        return getArray<PatchDeltaHeader>(getPatchDeltaHeaderOffset(), getPatchDeltaHeaderCount());
    }

    [[nodiscard]] auto getFragmentSet() const -> std::span<const FragmentSet> {
        return getArray<FragmentSet>(getFragmentSetOffset(), getFragmentSetCount());
    }

    [[nodiscard]] auto getPatchHistoryContentInfo() const -> std::span<const PatchHistoryContentInfo> {
        return getArray<PatchHistoryContentInfo>(getPatchHistoryContentInfoOffset(), getPatchHistoryContentInfoCount());
    }

    [[nodiscard]] auto getPatchDeltaPackagedContentInfo() const -> std::span<const PatchDeltaPackagedContentInfo> {
        return getArray<PatchDeltaPackagedContentInfo>(getPatchDeltaPackagedContentInfoOffset(), getPatchDeltaPackagedContentInfoCount());
    }
    
    [[nodiscard]] auto getFragmentIndicator() const -> std::span<const FragmentIndicator> {
        return getArray<FragmentIndicator>(getFragmentIndicatorOffset(), getFragmentIndicatorCount());
    }

private:
    [[nodiscard]] auto getHeaderOffset() const -> std::size_t {
        return 0;
    }

    [[nodiscard]] auto getPatchHistoryHeaderOffset() const -> std::size_t {
        return getHeaderOffset() + sizeof(PatchMetaExtendedData);
    }

    [[nodiscard]] auto getPatchDeltaHistoryOffset() const -> std::size_t {
        return getPatchHistoryHeaderOffset() + sizeof(PatchHistoryHeader) * getPatchHistoryHeaderCount();
    }

    [[nodiscard]] auto getPatchDeltaHeaderOffset() const -> std::size_t {
        return getPatchDeltaHistoryOffset() + sizeof(PatchDeltaHistory) * getPatchDeltaHistoryCount();
    }

    [[nodiscard]] auto getFragmentSetOffset() const -> std::size_t {
        return getPatchDeltaHeaderOffset() + sizeof(PatchDeltaHeader) * getPatchDeltaHeaderCount();
    }

    [[nodiscard]] auto getPatchHistoryContentInfoOffset() const -> std::size_t {
        return getFragmentSetOffset() + sizeof(FragmentSet) * getFragmentSetCount();
    }

    [[nodiscard]] auto getPatchDeltaPackagedContentInfoOffset() const -> std::size_t {
        return getPatchHistoryContentInfoOffset() + sizeof(PatchHistoryContentInfo) * getPatchHistoryContentInfoCount(); 
    }

    [[nodiscard]] auto getFragmentIndicatorOffset() const -> std::size_t {
        return getPatchDeltaPackagedContentInfoOffset() + sizeof(PatchDeltaPackagedContentInfo) * getPatchDeltaPackagedContentInfoCount();
    }
};

class ContentMetaReader final : public ReaderBase<std::vector> {
public:
    ContentMetaReader() = default;
    explicit ContentMetaReader(provider::UniqueProvider provider);

    [[nodiscard]] auto isValid() const -> bool {
        return getHeader() != nullptr;
    }

    [[nodiscard]] auto getHeader() const -> const PackagedContentMetaHeader* {
        return getPointer<PackagedContentMetaHeader>(getHeaderOffset());
    }

    [[nodiscard]] auto getApplicationId() const -> std::uint64_t {
        const auto header = getHeader();
        return header != nullptr ? header->applicationId : 0;
    }

    [[nodiscard]] auto getVersion() const -> std::uint32_t {
        const auto header = getHeader();
        return header != nullptr ? header->version : 0;
    }

    [[nodiscard]] auto getType() const -> ContentMetaType {
        const auto header = getHeader();
        return header != nullptr ? header->contentMetaType : ContentMetaType::Unknown;
    }

    [[nodiscard]] auto getExtendedHeaderSize() const -> std::uint32_t {
        const auto header = getHeader();
        return header != nullptr ? header->extendedHeaderSize : 0;
    }

    [[nodiscard]] auto getContentCount() const -> std::uint32_t {
        const auto header = getHeader();
        return header != nullptr ? header->contentCount : 0;
    }

    [[nodiscard]] auto getContentMetaCount() const -> std::uint32_t {
        const auto header = getHeader();
        return header != nullptr ? header->contentMetaCount : 0;
    }

    [[nodiscard]] auto isApplication() const -> bool {
        const auto header = getHeader();
        return header != nullptr ? std::to_underlying(header->contentMetaType) >= std::to_underlying(ContentMetaType::Application) : false;
    }

    template <typename T>
    [[nodiscard]] auto getExtendedMetaHeader() const -> const T* {
        const auto header = getHeader();
        if (header == nullptr || header->extendedHeaderSize < sizeof(T)) {
            return nullptr;
        }
        return getPointer<T>(getExtendedMetaHeaderOffset());
    }

    [[nodiscard]] auto getContentInfo() const -> std::span<const PackagedContentInfo> {
        const auto header = getHeader();
        if (header == nullptr) {
            return {};
        }
        return getArray<PackagedContentInfo>(getContentInfoOffset(), header->contentCount);
    }

    [[nodiscard]] auto getContentInfo(std::uint32_t index) const -> const PackagedContentInfo* {
        const auto header = getHeader();
        if (header == nullptr || index >= header->contentCount) {
            return nullptr;
        }
        return getPointer<PackagedContentInfo>(getContentInfoOffset() + sizeof(PackagedContentInfo) * index);
    }

    template <typename T>
    [[nodiscard]] auto getExtendedData() const -> const T* {
        const auto size = getExtendedDataSize();
        if (size < sizeof(T)) {
            return nullptr;
        }
        return getPointer<T>(getExtendedDataOffset());
    }

    template <typename T>
    [[nodiscard]] auto getExtendedDataReader() const -> T {
        return T(getArray<std::uint8_t>(getExtendedDataOffset(), getExtendedDataSize()));
    }

    [[nodiscard]] auto getHash() const -> std::span<const std::uint8_t> {
        const auto header = getHeader();
        if (header == nullptr) {
            return {};
        }
        return getArray<std::uint8_t>(getHashOffset(), 0x20);
    }

    [[nodiscard]] auto findContentInfo(PackagedContentType type) const -> const PackagedContentInfo* {
        for (const auto& info : getContentInfo()) {
            if (info.contentType == type) {
                return std::addressof(info);
            }
        }
        return nullptr;
    }

private:
    [[nodiscard]] auto getHeaderOffset() const -> std::size_t {
        return 0;
    }

    [[nodiscard]] auto getExtendedMetaHeaderOffset() const -> std::size_t {
        return getHeaderOffset() + sizeof(PackagedContentMetaHeader);
    }

    [[nodiscard]] auto getContentInfoOffset() const -> std::size_t {
        return getExtendedMetaHeaderOffset() + getExtendedHeaderSize();
    }

    [[nodiscard]] auto getExtendedDataOffset() const -> std::size_t {
        return getContentInfoOffset() + sizeof(PackagedContentInfo) * getContentCount();
    }

    [[nodiscard]] auto getHashOffset() const -> std::size_t {
        return getExtendedDataOffset() + getExtendedDataSize();
    }

    [[nodiscard]] auto getExtendedDataSize() const -> std::size_t {
        switch (getType()) {
            case ContentMetaType::SystemUpdate: return getExtendedMetaHeader<SystemUpdateMetaExtendedHeader>()->extendedDataSize;
            case ContentMetaType::Patch: return getExtendedMetaHeader<PatchMetaExtendedHeader>()->extendedDataSize;
            case ContentMetaType::Delta: return getExtendedMetaHeader<DeltaMetaExtendedHeader>()->extendedDataSize;
            default: return 0;
        }
    }
};

[[nodiscard]] ALWAYS_INLINE auto ToString(PackagedContentType type) -> std::string_view {
    switch (type) {
        case PackagedContentType::Meta: return "Meta";
        case PackagedContentType::Program: return "Program";
        case PackagedContentType::Data: return "Data";
        case PackagedContentType::Control: return "Control";
        case PackagedContentType::HtmlDocument: return "HtmlDocument";
        case PackagedContentType::LegalInformation: return "LegalInformation";
        case PackagedContentType::DeltaFragment: return "DeltaFragment";
        default: return "Unknown";
    }
}

} // namespace nxmount::formats