#pragma once

#include "common/utils.hpp"
#include "provider/provider.hpp"

#include <cstdint>
#include <string>

namespace nxmount::formats {

struct ApplicationTitle {
    char name[0x200];
    char publisher[0x100];
};

struct CompressedTitleBlob {
    std::uint16_t dataSize;
    std::uint8_t data[0x2ffe];
};

enum class StartupUserAccount : std::uint8_t {
    None                                        = 0,
    Required                                    = 1,
    RequiredWithNetworkServiceAccountAvailable  = 2,
};

enum class UserAccountSwitchLock : std::uint8_t {
    Disable = 0,
    Enable  = 1,
};

enum class AddOnContentRegistrationType : std::uint8_t {
    AllOnLaunch = 0,
    OnDemand    = 1,
};

enum class Screenshot : std::uint8_t {
    Allow   = 0,
    Deny    = 1,
};

enum class VideoCapture : std::uint8_t {
    Disable = 0,
    Manual  = 1,
    Enable  = 2,
};

enum class DataLossConfirmation : std::uint8_t {
    None        = 0,
    Required    = 1,
};

enum class PlayLogPolicy : std::uint8_t {
    Open    = 0,
    LogOnly = 1,
    None    = 2,
    Closed  = 3,
};

enum class LogoType : std::uint8_t {
    LicensedByNintendo      = 0,
    DistributedByNintendo   = 1,
    Nintendo                = 2,
};

enum class LogoHandling : std::uint8_t {
    Auto    = 0,
    Manual  = 1,
};

enum class RuntimeAddOnContentInstall : std::uint8_t {
    Deny                                        = 0,
    AllowAppend                                 = 1,
    AllowAppendButDontDownloadWhenUsingNetwork  = 2,
};

enum class RuntimeParameterDelivery : std::uint8_t {
    Always                      = 0,
    AlwaysIfUserStateMatched    = 1,
    OnRestart                   = 2,
};

enum class AppropriateAgeForChina : std::uint8_t {
    None    = 0,
    Age8    = 1,
    Age12   = 2,
    Age16   = 3,
};

enum class CrashReport : std::uint8_t {
    Deny    = 0,
    Allow   = 1,
};

enum class Hdcp : std::uint8_t {
    None        = 0,
    Required    = 1,
};

enum class PlayLogQueryCapability : std::uint8_t {
    None        = 0,
    WhiteList   = 1,
    All         = 2,
};

enum class TitleFormat : std::uint8_t {
    Uncompressed    = 0,
    Compressed      = 1,
};

enum class ApparentPlatform : std::uint8_t {
    NX      = 0,
    Ounce   = 1,
};

struct NeighborDetectionGroupConfiguration {
    std::uint64_t id;
    std::uint8_t key[0x10];
};

struct NeighborDetectionClientConfiguration {
    NeighborDetectionGroupConfiguration sendGroupConfiguration;
    NeighborDetectionGroupConfiguration receivableGroupConfigurations[0x10];
};

enum class JitConfigurationFlag : std::uint64_t {
    None    = 0,
    Enabled = 1,
};

struct JitConfiguration {
    JitConfigurationFlag flags;
    std::uint64_t memorySize;
};

struct AccessibleLaunchRequiredVersionValue {
    std::uint64_t applicationIds[8];
};

struct ApplicationControlDataConditionData {
    std::uint8_t priority;
    std::uint8_t reserved0[7];
    std::uint16_t aocIndex;
    std::uint8_t reserved1[6];
};

PACKED_STRUCT(ApplicationControlDataCondition {
    std::uint8_t types[8];
    ApplicationControlDataConditionData data[8];
    std::uint8_t count;
});

struct ApplicationControlProperty {
    union {
        ApplicationTitle titles[0x10];
        CompressedTitleBlob compressedTitles;
    };
    char isbn[0x25];
    StartupUserAccount startupUserAccount;
    UserAccountSwitchLock userAccountSwitchLock;
    AddOnContentRegistrationType aocRegistrationType;
    std::uint32_t attributeFlag;
    std::uint32_t supportedLanguageFlag;
    std::uint32_t parentalControlFlag;
    Screenshot screenshot;
    VideoCapture videoCapture;
    DataLossConfirmation dataLossConfirmation;
    PlayLogPolicy playLogPolicy;
    std::uint64_t presenceGroupId;
    struct {
        std::int8_t cero;
        std::int8_t gracGcrb;
        std::int8_t gsrmr;
        std::int8_t esrb;
        std::int8_t classInd;
        std::int8_t usk;
        std::int8_t pegi;
        std::int8_t pegiPortugal;
        std::int8_t pegiBbfc;
        std::int8_t russian;
        std::int8_t acb;
        std::int8_t oflc;
        std::int8_t iarcGeneric;
        std::uint8_t reserved[0x13];
    } ageRating;
    char displayVersion[0x10];
    std::uint64_t aocBaseId;
    std::uint64_t saveDataOwnerId;
    std::uint64_t userAccountSaveDataSize;
    std::uint64_t userAccountSaveDataJournalSize;
    std::uint64_t deviceSaveDataSize;
    std::uint64_t deviceSaveDataJournalSize;
    std::uint64_t bcatDeliveryCacheStorageSize;
    std::uint64_t applicationErrorCodeCategory;
    std::uint64_t localCommunicationIds[8];
    LogoType logoType;
    LogoHandling logoHandling;
    RuntimeAddOnContentInstall runtimeAddOnContentInstall;
    RuntimeParameterDelivery runtimeParameterDelivery;
    AppropriateAgeForChina appropriateAgeForChina;
    std::uint8_t reserved0;
    CrashReport crashReport;
    Hdcp hdcp;
    std::uint64_t seedForPseudoDeviceId;
    char bcatPassphrase[0x41];
    std::uint8_t startupUserAccountOption;
    std::uint8_t reservedForUserAccountSaveDataOperation[6];
    std::uint64_t userAccountSaveDataSizeMax;
    std::uint64_t userAccountSaveDataJournalSizeMax;
    std::uint64_t deviceSaveDataSizeMax;
    std::uint64_t deviceSaveDataJournalSizeMax;
    std::uint64_t temporaryStorageSize;
    std::uint64_t cacheStorageSize;
    std::uint64_t cacheStorageJournalSize;
    std::uint64_t cacheStorageDataAndJournalSizeMax;
    std::uint16_t cacheStorageIndexMax;
    std::uint8_t reserved1;
    std::uint8_t runtimeUpgrade;
    std::uint32_t supportingLimitedApplicationLicenses;
    std::uint64_t playLogQueryableApplicationId[0x10];
    PlayLogQueryCapability playLogQueryCapability;
    std::uint8_t repairFlag;
    std::uint8_t programIndex;
    std::uint8_t requiredNetworkServiceLicenseOnLaunchFlag;
    std::uint8_t applicationErrorCodePrefix;
    TitleFormat titleFormat;
    std::uint8_t acdIndex;
    ApparentPlatform apparentPlatform;
    NeighborDetectionClientConfiguration neighborDetectionClientConfiguration;
    JitConfiguration jitConfiguration;
    std::uint16_t requiredAddOnContentsSetBinaryDescriptor[0x20];
    std::uint8_t playReportPermission;
    std::uint8_t crashScreenshotForProd;
    std::uint8_t crashScreenshotForDev;
    std::uint8_t contentsAvailabilityTransitionPolicy;
    std::uint32_t supportedLanguageFlagForNxAddon;
    AccessibleLaunchRequiredVersionValue accessibleLaunchRequiredVersionValue;
    ApplicationControlDataCondition applicationControlDataCondition;
    std::uint8_t initialProgramIndex;
    std::uint16_t reserved2;
    std::uint32_t accessibleProgramIndexFlags;
    std::uint8_t albumFileExport;
    std::uint8_t reserved3[7];
    std::uint8_t saveDataCertificateBytes[0x80];
    std::uint8_t hasInGameVoiceChat;
    std::uint8_t reserved4[3];
    std::uint32_t supportedExtraAddOnContentFlag;
    std::uint8_t hasKaraokeFeature;
    std::uint8_t reserved5[0x697];
    std::uint8_t platformSpecificRegion[0x400];
};
static_assert(sizeof(ApplicationControlProperty) == 0x4000);

auto GetDisplayName(provider::UniqueProvider& provider) -> std::string;

} // namespace nxmount::formats