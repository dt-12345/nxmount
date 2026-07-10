#include "formats/titles.hpp"

#include <array>
#include <algorithm>

namespace nxmount::formats {

struct TitleInfo {
    const std::uint64_t id;
    const char* name;
};

// https://github.com/DarkMatterCore/nxdumptool/blob/bfbc5e511f3b358a1fe41158bf6d8a2581232c3c/source/core/title.c#L133
static constexpr const std::array<TitleInfo, 396> sSystemTitles = {{
    /* System modules. */
    /* Meta + Program NCAs. */
    { 0x0100000000000000, "fs" },                               ///< Unused, bundled with kernel.
    { 0x0100000000000001, "ldr" },                              ///< Unused, bundled with kernel.
    { 0x0100000000000002, "ncm" },                              ///< Unused, bundled with kernel.
    { 0x0100000000000003, "pm" },                               ///< Unused, bundled with kernel.
    { 0x0100000000000004, "sm" },                               ///< Unused, bundled with kernel.
    { 0x0100000000000005, "boot" },                             ///< Unused, bundled with kernel.
    { 0x0100000000000006, "usb" },
    { 0x0100000000000007, "htc" },
    { 0x0100000000000008, "boot2" },
    { 0x0100000000000009, "settings" },
    { 0x010000000000000A, "Bus" },
    { 0x010000000000000B, "bluetooth" },
    { 0x010000000000000C, "bcat" },
    { 0x010000000000000D, "dmnt" },
    { 0x010000000000000E, "friends" },
    { 0x010000000000000F, "nifm" },
    { 0x0100000000000010, "ptm" },
    { 0x0100000000000011, "shell" },
    { 0x0100000000000012, "bsdsockets" },
    { 0x0100000000000013, "hid" },
    { 0x0100000000000014, "audio" },
    { 0x0100000000000015, "LogManager" },
    { 0x0100000000000016, "wlan" },
    { 0x0100000000000017, "cs" },
    { 0x0100000000000018, "ldn" },
    { 0x0100000000000019, "nvservices" },
    { 0x010000000000001A, "pcv" },
    { 0x010000000000001B, "capmtp" },
    { 0x010000000000001C, "nvnflinger" },
    { 0x010000000000001D, "pcie" },
    { 0x010000000000001E, "account" },
    { 0x010000000000001F, "ns" },
    { 0x0100000000000020, "nfc" },
    { 0x0100000000000021, "psc" },
    { 0x0100000000000022, "capsrv" },
    { 0x0100000000000023, "am" },
    { 0x0100000000000024, "ssl" },
    { 0x0100000000000025, "nim" },
    { 0x0100000000000026, "cec" },
    { 0x0100000000000027, "tspm" },
    { 0x0100000000000028, "spl" },                              ///< Unused, bundled with kernel.
    { 0x0100000000000029, "lbl" },
    { 0x010000000000002A, "btm" },
    { 0x010000000000002B, "erpt" },
    { 0x010000000000002C, "time" },
    { 0x010000000000002D, "vi" },
    { 0x010000000000002E, "pctl" },
    { 0x010000000000002F, "npns" },
    { 0x0100000000000030, "eupld" },
    { 0x0100000000000031, "glue" },
    { 0x0100000000000032, "eclct" },
    { 0x0100000000000033, "es" },
    { 0x0100000000000034, "fatal" },
    { 0x0100000000000035, "grc" },
    { 0x0100000000000036, "creport" },
    { 0x0100000000000037, "ro" },
    { 0x0100000000000038, "profiler" },
    { 0x0100000000000039, "sdb" },
    { 0x010000000000003A, "migration" },
    { 0x010000000000003B, "jit" },
    { 0x010000000000003C, "jpegdec" },
    { 0x010000000000003D, "safemode" },
    { 0x010000000000003E, "olsc" },
    { 0x010000000000003F, "dt" },
    { 0x0100000000000040, "nd" },
    { 0x0100000000000041, "ngct" },
    { 0x0100000000000042, "pgl" },
    { 0x0100000000000043, "sysmod_unknown_00" },                ///< Placeholder.
    { 0x0100000000000044, "sysmod_unknown_01" },                ///< Placeholder.
    { 0x0100000000000045, "omm" },
    { 0x0100000000000046, "eth" },
    { 0x0100000000000047, "sysmod_unknown_02" },                ///< Placeholder.
    { 0x0100000000000048, "sysmod_unknown_03" },                ///< Placeholder.
    { 0x0100000000000049, "sysmod_unknown_04" },                ///< Placeholder.
    { 0x010000000000004A, "sysmod_unknown_05" },                ///< Placeholder.
    { 0x010000000000004B, "sysmod_unknown_06" },                ///< Placeholder.
    { 0x010000000000004C, "netTc" },
    { 0x010000000000004D, "sysmod_unknown_07" },                ///< Placeholder.
    { 0x010000000000004E, "sysmod_unknown_08" },                ///< Placeholder.
    { 0x010000000000004F, "sysmod_unknown_09" },                ///< Placeholder.
    { 0x0100000000000050, "ngc" },
    { 0x0100000000000051, "dmgr" },

    /* System data archives. */
    /* Meta + Data NCAs. */
    { 0x0100000000000800, "CertStore" },
    { 0x0100000000000801, "ErrorMessage" },
    { 0x0100000000000802, "MiiModel" },
    { 0x0100000000000803, "BrowserDll" },
    { 0x0100000000000804, "Help" },
    { 0x0100000000000805, "SharedFont" },
    { 0x0100000000000806, "NgWord" },
    { 0x0100000000000807, "SsidList" },
    { 0x0100000000000808, "Dictionary" },
    { 0x0100000000000809, "SystemVersion" },
    { 0x010000000000080A, "AvatarImage" },
    { 0x010000000000080B, "LocalNews" },
    { 0x010000000000080C, "Eula" },
    { 0x010000000000080D, "UrlBlackList" },
    { 0x010000000000080E, "TimeZoneBinary" },
    { 0x010000000000080F, "CertStoreCruiser" },
    { 0x0100000000000810, "FontNintendoExtension" },
    { 0x0100000000000811, "FontStandard" },
    { 0x0100000000000812, "FontKorean" },
    { 0x0100000000000813, "FontChineseTraditional" },
    { 0x0100000000000814, "FontChineseSimple" },
    { 0x0100000000000815, "FontBfcpx" },
    { 0x0100000000000816, "SystemUpdate" },
    { 0x0100000000000818, "FirmwareDebugSettings" },
    { 0x0100000000000819, "BootImagePackage" },
    { 0x010000000000081A, "BootImagePackageSafe" },
    { 0x010000000000081B, "BootImagePackageExFat" },
    { 0x010000000000081C, "BootImagePackageExFatSafe" },
    { 0x010000000000081D, "FatalMessage" },
    { 0x010000000000081E, "ControllerIcon" },
    { 0x010000000000081F, "PlatformConfigIcosa" },
    { 0x0100000000000820, "PlatformConfigCopper" },
    { 0x0100000000000821, "PlatformConfigHoag" },
    { 0x0100000000000822, "ControllerFirmware" },
    { 0x0100000000000823, "NgWord2" },
    { 0x0100000000000824, "PlatformConfigIcosaMariko" },
    { 0x0100000000000825, "ApplicationBlackList" },
    { 0x0100000000000826, "RebootlessSystemUpdateVersion" },
    { 0x0100000000000827, "ContentActionTable" },
    { 0x0100000000000828, "FunctionBlackList" },
    { 0x0100000000000829, "PlatformConfigCalcio" },
    { 0x0100000000000830, "NgWordT" },
    { 0x0100000000000831, "PlatformConfigAula" },
    { 0x0100000000000832, "CradleFirmware" },
    { 0x0100000000000835, "ErrorMessageUtf8" },
    { 0x0100000000000859, "sysdata_unknown_00" },               ///< Placeholder.
    { 0x010000000000085C, "sysdata_unknown_01" },               ///< Placeholder.

    /* System applets. */
    /* Meta + Program NCAs. */
    { 0x0100000000001000, "qlaunch" },
    { 0x0100000000001001, "auth" },
    { 0x0100000000001002, "cabinet" },
    { 0x0100000000001003, "controller" },
    { 0x0100000000001004, "dataErase" },
    { 0x0100000000001005, "error" },
    { 0x0100000000001006, "netConnect" },
    { 0x0100000000001007, "playerSelect" },
    { 0x0100000000001008, "swkbd" },
    { 0x0100000000001009, "miiEdit" },
    { 0x010000000000100A, "LibAppletWeb" },
    { 0x010000000000100B, "LibAppletShop" },
    { 0x010000000000100C, "overlayDisp" },
    { 0x010000000000100D, "photoViewer" },
    { 0x010000000000100E, "set" },
    { 0x010000000000100F, "LibAppletOff" },
    { 0x0100000000001010, "LibAppletLns" },
    { 0x0100000000001011, "LibAppletAuth" },
    { 0x0100000000001012, "starter" },
    { 0x0100000000001013, "myPage" },
    { 0x0100000000001014, "PlayReport" },
    { 0x0100000000001015, "maintenance" },
    { 0x0100000000001016, "application_install" },              ///< Placeholder.
    { 0x0100000000001017, "nn.am.SystemReportTask" },           ///< Placeholder.
    { 0x0100000000001018, "systemupdate_dl_throughput" },       ///< Placeholder.
    { 0x0100000000001019, "volume_update"},                     ///< Placeholder.
    { 0x010000000000101A, "gift" },
    { 0x010000000000101B, "DummyECApplet" },
    { 0x010000000000101C, "userMigration" },
    { 0x010000000000101D, "EncounterSys" },
    { 0x010000000000101E, "sysapplet_unknown_00" },             ///< Placeholder.
    { 0x010000000000101F, "sysapplet_unknown_01" },             ///< Placeholder.
    { 0x0100000000001020, "story" },
    { 0x0100000000001021, "systemupdate_pass" },                ///< Placeholder.
    { 0x0100000000001023, "statistics" },                       ///< Placeholder.
    { 0x0100000000001024, "syslog" },                           ///< Placeholder.
    { 0x0100000000001025, "sysapplet_unknown_02" },             ///< Placeholder.
    { 0x0100000000001026, "sysapplet_unknown_03" },             ///< Placeholder.
    { 0x0100000000001027, "sysapplet_unknown_04" },             ///< Placeholder.
    { 0x0100000000001028, "sysapplet_unknown_05" },             ///< Placeholder.
    { 0x0100000000001029, "request_count" },                    ///< Placeholder.
    { 0x010000000000102A, "sysapplet_unknown_06" },             ///< Placeholder.
    { 0x010000000000102B, "sysapplet_unknown_07" },             ///< Placeholder.
    { 0x010000000000102C, "sysapplet_unknown_08" },             ///< Placeholder.
    { 0x010000000000102E, "blacklist" },                        ///< Placeholder.
    { 0x010000000000102F, "content_delivery" },                 ///< Placeholder.
    { 0x0100000000001030, "npns_create_token" },                ///< Placeholder.
    { 0x0100000000001031, "sysapplet_unknown_09" },             ///< Placeholder.
    { 0x0100000000001032, "sysapplet_unknown_0a" },             ///< Placeholder.
    { 0x0100000000001033, "promotion" },                        ///< Placeholder.
    { 0x0100000000001034, "sysapplet_unknown_0b" },             ///< Placeholder.
    { 0x0100000000001037, "sysapplet_unknown_0c" },             ///< Placeholder.
    { 0x0100000000001038, "sample" },
    { 0x010000000000103C, "mnpp" },                             ///< Placeholder.
    { 0x010000000000103D, "bsdsocket_setting" },                ///< Placeholder.
    { 0x010000000000103E, "ntf_mission_completed" },            ///< Placeholder.
    { 0x0100000000001042, "systemWeb" },
    { 0x0100000000001043, "openWeb" },
    { 0x0100000000001048, "splay" },
    { 0x0100000000001FFF, "EndOceanProgramId" },                ///< Placeholder.

    /* Development system applets. */
    { 0x0100000000002000, "A2BoardFunction" },
    { 0x0100000000002001, "A3Wireless" },
    { 0x0100000000002002, "C1LcdAndKey" },
    { 0x0100000000002003, "C2UsbHpmic" },
    { 0x0100000000002004, "C3Aging" },
    { 0x0100000000002005, "C4SixAxis" },
    { 0x0100000000002006, "C5Wireless" },
    { 0x0100000000002007, "C7FinalCheck" },
    { 0x010000000000203F, "AutoCapture" },
    { 0x0100000000002040, "DevMenuCommandSystem" },
    { 0x0100000000002041, "recovery" },
    { 0x0100000000002042, "DevMenuSystem" },
    { 0x0100000000002044, "HB-TBIntegrationTest" },
    { 0x010000000000204D, "BackupSaveData" },
    { 0x010000000000204E, "A4BoardCalWriti" },
    { 0x0100000000002054, "RepairSslCertificate" },
    { 0x0100000000002055, "GameCardWriter" },
    { 0x0100000000002056, "UsbPdTestTool" },
    { 0x0100000000002057, "RepairDeletePctl" },
    { 0x0100000000002058, "RepairBackup" },
    { 0x0100000000002059, "RepairRestore" },
    { 0x010000000000205A, "RepairAccountTransfer" },
    { 0x010000000000205B, "RepairAutoNetworkUpdater" },
    { 0x010000000000205C, "RefurbishReset" },
    { 0x010000000000205D, "RepairAssistCup" },
    { 0x010000000000205E, "RepairPairingCutter" },
    { 0x0100000000002064, "DevMenu" },
    { 0x0100000000002065, "DevMenuApp" },
    { 0x0100000000002066, "GetGameCardAsicInfo" },
    { 0x0100000000002068, "NfpDebugToolSystem" },
    { 0x0100000000002069, "AlbumSynchronizer" },
    { 0x0100000000002071, "SnapShotDumper" },
    { 0x0100000000002073, "DevMenuSystemApp" },
    { 0x0100000000002099, "DevOverlayDisp" },
    { 0x010000000000209A, "NandVerifier" },
    { 0x010000000000209B, "GpuCoreDumper" },
    { 0x010000000000209C, "TestApplication" },
    { 0x010000000000209E, "HelloWorld" },
    { 0x01000000000020A0, "XcieWriter" },
    { 0x01000000000020A1, "GpuOverrunNotifier" },
    { 0x01000000000020C8, "NfpDebugTool" },
    { 0x01000000000020CA, "NoftWriter" },
    { 0x01000000000020D0, "BcatSystemDebugTool" },
    { 0x01000000000020D1, "DevSafeModeUpdater" },
    { 0x01000000000020D3, "ControllerConnectionAnalyzer" },
    { 0x01000000000020D4, "DevKitUpdater" },
    { 0x01000000000020D6, "RepairTimeReviser" },
    { 0x01000000000020D7, "RepairReinitializeFuelGauge" },
    { 0x01000000000020DA, "RepairAbortMigration" },
    { 0x01000000000020DC, "RepairShowDeviceId" },
    { 0x01000000000020DD, "RepairSetCycleCountReliability" },
    { 0x01000000000020E0, "Interface" },
    { 0x01000000000020E1, "AlbumDownloader" },
    { 0x01000000000020E3, "FuelGaugeDumper" },
    { 0x01000000000020E4, "UnsafeExtract" },
    { 0x01000000000020E5, "UnsafeEngrave" },
    { 0x01000000000020EE, "BluetoothSettingTool" },
    { 0x01000000000020F0, "ApplicationInstallerRomfs" },
    { 0x0100000000002100, "DevMenuLotcheckDownloader" },
    { 0x0100000000002101, "DevMenuCommand" },
    { 0x0100000000002102, "ExportPartition" },
    { 0x0100000000002103, "SystemInitializer" },
    { 0x0100000000002104, "SystemUpdaterHostFs" },
    { 0x0100000000002105, "WriteToStorage" },
    { 0x0100000000002106, "CalWriter" },
    { 0x0100000000002107, "SettingsManager" },
    { 0x0100000000002109, "testBuildSystemIris" },
    { 0x010000000000210A, "SystemUpdater" },
    { 0x010000000000210B, "nvnflinger_util" },
    { 0x010000000000210C, "ControllerFirmwareUpdater" },
    { 0x010000000000210D, "testBuildSystemNintendoWare" },
    { 0x0100000000002110, "TestSaveDataCreator" },
    { 0x0100000000002111, "C9LcdSpker" },
    { 0x0100000000002114, "RankTurn" },
    { 0x0100000000002116, "BleTestTool" },
    { 0x010000000000211A, "PreinstallAppWriter" },
    { 0x010000000000211C, "ControllerSerialFlashTool" },
    { 0x010000000000211D, "ControllerFlashWriter" },
    { 0x010000000000211E, "C13Handling" },
    { 0x010000000000211F, "HidTest" },
    { 0x0100000000002120, "ControllerTestApp" },
    { 0x0100000000002121, "HidInspectionTool" },
    { 0x0100000000002124, "BatteryCyclesEditor" },
    { 0x0100000000002125, "UsbFirmwareUpdater" },
    { 0x0100000000002126, "PalmaSerialCodeTool" },
    { 0x0100000000002127, "renderdoccmd" },
    { 0x0100000000002128, "HidInspectionToolProd" },
    { 0x010000000000212C, "ExhibitionMenu" },
    { 0x010000000000212F, "ExhibitionSaveData" },
    { 0x0100000000002130, "LuciaConverter" },
    { 0x0100000000002133, "CalDumper" },
    { 0x0100000000002134, "AnalogStickEvaluationTool" },
    { 0x010000000000216A, "ButtonTest" },
    { 0x010000000000216D, "ExhibitionSaveDataSnapshot" },       ///< Placeholder.
    { 0x010000000000216E, "HandlingA" },
    { 0x0100000000002178, "SecureStartupSettings" },            ///< Placeholder.
    { 0x010000000000217A, "WirelessInterference" },
    { 0x010000000000217D, "CradleFirmwareUpdater" },
    { 0x0100000000002184, "HttpInstallSettings" },              ///< Placeholder.
    { 0x0100000000002187, "ExhibitionMovieAssetData" },         ///< Placeholder.
    { 0x0100000000002191, "ExhibitionPlayData" },               ///< Placeholder.

    /* Debug system modules. */
    { 0x0100000000003002, "DummyProcess" },
    { 0x0100000000003003, "DebugMonitor0" },
    { 0x0100000000003004, "SystemHelloWorld" },

    /* Development system modules. */
    { 0x010000000000B120, "nvdbgsvc" },
    { 0x010000000000B123, "acc:CORNX" },
    { 0x010000000000B14A, "manu" },
    { 0x010000000000B14B, "ManuUsbLoopBack" },
    { 0x010000000000B1B8, "DevFwdbgHbPackage" },
    { 0x010000000000B1B9, "DevFwdbgUsbPackage" },
    { 0x010000000000B1BA, "ProdFwdbgPackage" },
    { 0x010000000000B22A, "scs" },
    { 0x010000000000B22B, "ControllerFirmwareDebug" },
    { 0x010000000000B23D, "dt0" },
    { 0x010000000000B240, "htc" },

    /* Bdk system modules. */
    { 0x010000000000C600, "BdkSample01" },
    { 0x010000000000C601, "BdkSample02" },
    { 0x010000000000C602, "BdkSample03" },
    { 0x010000000000C603, "BdkSample04" },

    /* New development system modules. */
    { 0x010000000000D609, "dmnt.gen2" },
    { 0x010000000000D60A, "dev_sysmod_unknown_00" },            ///< Placeholder.
    { 0x010000000000D60B, "dev_sysmod_unknown_01" },            ///< Placeholder.
    { 0x010000000000D60C, "dev_sysmod_unknown_02" },            ///< Placeholder.
    { 0x010000000000D60D, "dev_sysmod_unknown_03" },            ///< Placeholder.
    { 0x010000000000D60E, "dev_sysmod_unknown_04" },            ///< Placeholder.
    { 0x010000000000D610, "dev_sysmod_unknown_05" },            ///< Placeholder.
    { 0x010000000000D611, "dev_sysmod_unknown_06" },            ///< Placeholder.
    { 0x010000000000D612, "dev_sysmod_unknown_07" },            ///< Placeholder.
    { 0x010000000000D613, "dev_sysmod_unknown_08" },            ///< Placeholder.
    { 0x010000000000D614, "dev_sysmod_unknown_09" },            ///< Placeholder.
    { 0x010000000000D615, "dev_sysmod_unknown_0a" },            ///< Placeholder.
    { 0x010000000000D616, "dev_sysmod_unknown_0b" },            ///< Placeholder.
    { 0x010000000000D617, "dev_sysmod_unknown_0c" },            ///< Placeholder.
    { 0x010000000000D619, "dev_sysmod_unknown_0d" },            ///< Placeholder.
    { 0x010000000000D621, "dev_sysmod_unknown_0e" },            ///< Placeholder.
    { 0x010000000000D623, "DevServer" },
    { 0x010000000000D62F, "WlanControlDaemon" },
    { 0x010000000000D633, "dev_sysmod_unknown_0f" },            ///< Placeholder.
    { 0x010000000000D640, "htcnet" },
    { 0x010000000000D65A, "netTcDev" },
    { 0x010000000000D65B, "dev_sysmod_unknown_10" },            ///< Placeholder.
    { 0x010000000000D65C, "dev_sysmod_unknown_11" },            ///< Placeholder.

    /* System applications. */
    { 0x01008BB00013C000, "flog" },
    { 0x0100069000078000, "RetailInteractiveDisplayMenu" },
    { 0x010000B003486000, "AudioUsbMicDebugTool" },
    { 0x0100458001E04000, "BcatTestApp01" },
    { 0x0100F910020F8000, "BcatTestApp02" },
    { 0x0100B7D0020FC000, "BcatTestApp03" },
    { 0x0100132002100000, "BcatTestApp04" },
    { 0x0100935002116000, "BcatTestApp05" },
    { 0x0100DA4002130000, "BcatTestApp06" },
    { 0x0100B0F002104000, "BcatTestApp07" },
    { 0x010051E002132000, "BcatTestApp08" },
    { 0x01004CB0015C8000, "BcatTestApp09" },
    { 0x01009720015CA000, "BcatTestApp10" },
    { 0x01002F20015C6000, "BcatTestApp11" },
    { 0x0100204001F90000, "BcatTestApp12" },
    { 0x0100060001F92000, "BcatTestApp13" },
    { 0x0100C26001F94000, "BcatTestApp14" },
    { 0x0100462001F96000, "BcatTestApp15" },
    { 0x01005C6001F98000, "BcatTestApp16" },
    { 0x010070000E3C0000, "EncounterUsr" },
    { 0x010086000E49C000, "EncounterUsrDummy" },
    { 0x0100810002D5A000, "ShopMonitaringTool" },
    { 0x010023D002B98000, "DeltaStress" },
    { 0x010099F00D810000, "sysapp_unknown_00" },                ///< Placeholder.
    { 0x0100E6C01163C000, "sysapp_unknown_01" },                ///< Placeholder.

    /* Pre-release system applets. */
    { 0x1000000000000001, "SystemInitializer" },
    { 0x1000000000000004, "CalWriter" },
    { 0x1000000000000005, "DevMenuCommand" },
    { 0x1000000000000006, "SettingsManager" },
    { 0x1000000000000007, "DevMenu" },
    { 0x100000000000000B, "SnapShotDumper" },
    { 0x100000000000000C, "SystemUpdater" },
    { 0x100000000000000E, "ControllerFirmwareUpdater" },

    /* Pre-release system modules. */
    { 0x1000000000000201, "usb" },
    { 0x1000000000000202, "tma" },
    { 0x1000000000000203, "boot2" },
    { 0x1000000000000204, "settings" },
    { 0x1000000000000205, "bus" },
    { 0x1000000000000206, "bluetooth" },
    { 0x1000000000000208, "DebugMonitor0" },
    { 0x1000000000000209, "dmnt" },
    { 0x100000000000020B, "nifm" },
    { 0x100000000000020C, "ptm" },
    { 0x100000000000020D, "shell" },
    { 0x100000000000020E, "bsdsocket" },
    { 0x100000000000020F, "hid" },
    { 0x1000000000000210, "audio" },
    { 0x1000000000000212, "LogManager" },
    { 0x1000000000000213, "wlan" },
    { 0x1000000000000214, "cs" },
    { 0x1000000000000215, "ldn" },
    { 0x1000000000000216, "nvservices" },
    { 0x1000000000000217, "pcv" },
    { 0x1000000000000218, "ppc" },
    { 0x100000000000021A, "lbl0" },
    { 0x100000000000021B, "nvnflinger" },
    { 0x100000000000021C, "pcie" },
    { 0x100000000000021D, "account" },
    { 0x100000000000021E, "ns" },
    { 0x100000000000021F, "nfc" },
    { 0x1000000000000220, "psc" },
    { 0x1000000000000221, "capsrv" },
    { 0x1000000000000222, "am" },
    { 0x1000000000000223, "ssl" },
    { 0x1000000000000224, "nim" },
}};

auto TryGetTitleName(std::uint64_t id) -> std::string_view {
    const auto res = std::find_if(sSystemTitles.begin(), sSystemTitles.end(), [=](const auto& title) -> bool { return title.id == id; });
    if (res == sSystemTitles.end()) {
        return "";
    } else {
        return res->name;
    }
}

} // namespace nxmount::formats