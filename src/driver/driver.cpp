#include "crypto/key_mgr.hpp"
#include "crypto/key_utils.hpp"
#include "driver/driver.hpp"
#include "formats/hfs0.hpp"
#include "formats/nca.hpp"
#include "formats/pfs0.hpp"
#include "formats/xci.hpp"
#include "fs/filesystem.hpp"
#include "log/logging.hpp"
#include "provider/file_stream_provider.hpp"
#include "provider/provider.hpp"

#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace nxmount::driver::detail {

struct Config {
    std::string mountPoint;
    std::string_view basePath;
    std::string_view keyPath;
    std::string_view titlePath;
    std::string_view intype;
    std::string_view titlekey;
    std::string_view updatePath;
    std::vector<std::string_view> updates;
    std::string_view updateType;
#ifdef ENABLE_LOGGING
    std::string_view logLevel;
#endif
    bool foreground = false;
    bool debug = false;
};

auto RunImpl(const Config& config, std::unique_ptr<fs::IFileSystem> fs) -> std::int32_t;

} // namespace nxmount::driver::detail

#if defined(WIN32) && !defined(USE_WINFUSE)

#include <winfsp/winfsp.h>

namespace nxmount::driver::detail {

static auto SvcStart(FSP_SERVICE* service, ULONG argc, PWSTR* argv) -> NTSTATUS {
    return STATUS_NOT_IMPLEMENTED;
}

static auto SvcStop(FSP_SERVICE* service) -> NTSTATUS {
    return STATUS_NOT_IMPLEMENTED;
}

auto RunImpl(const Config& config, std::unique_ptr<fs::IFileSystem> fs) -> std::int32_t {
    return FspServiceRun(const_cast<wchar_t*>(L"nxmount"), SvcStart, SvcStop, nullptr);
}

} // namespace nxmount::driver::detail

#else

namespace nxmount::driver::detail {

static auto ConfigToArgs(const Config& config) -> std::vector<const char*> {
    auto args = std::vector<const char*>{
        "nxmount", config.mountPoint.c_str(),
#if !defined(WIN32)
        "-o", "allow_other",
#endif
        "-o", "kernel_cache",
    };

    if (config.foreground) {
        args.push_back("-f");
    }

    if (config.debug) {
        args.push_back("-d");
    }

    return args;
}

auto RunImpl(const Config& config, std::unique_ptr<fs::IFileSystem> fs) -> std::int32_t {
    const auto args = ConfigToArgs(config);

    return fuse_main(static_cast<int>(args.size()), const_cast<char**>(args.data()), std::addressof(fs::IFileSystem::cFuseOperations), fs.release());
}
    
} // namespace nxmount::driver::detail

#endif

namespace nxmount::driver {

enum class FileType {
    Auto,
    NSP,
    PFS0,
    XCI,
    HFS0,
    NCA,
    Unknown,
};

[[nodiscard]] static auto GetFileType(provider::IBytesProvider& provider) -> FileType {
    std::uint32_t magic = 0;
    if (provider.read(std::addressof(magic), sizeof(magic), 0) == sizeof(magic)) {
        if (magic == formats::PartitionFileSystemHeader::cMagic) {
            return FileType::NSP;
        }
    
        if (magic == formats::Sha256FileSystemHeader::cMagic) {
            return FileType::HFS0; // idk if there are any files that are just a raw hfs0, but just in case
        }
    }

    if (provider.read(std::addressof(magic), sizeof(magic), offsetof(formats::GameCardImageHeader, magic)) == sizeof(magic)) {
        if (magic == formats::GameCardImageHeader::cMagic) {
            return FileType::XCI;
        }
    }

    return FileType::Unknown;
};

[[nodiscard]] static auto ParseType(std::string_view type) -> FileType {
    if (type == "nsp") {
        return FileType::NSP;
    } else if (type == "pfs0") {
        return FileType::PFS0;
    } else if (type == "xci") {
        return FileType::XCI;
    } else if (type == "hfs0") {
        return FileType::HFS0;
    } else if (type == "nca") {
        return FileType::NCA;
    } else if (type == "auto") {
        return FileType::Auto;
    } else {
        LOG_WARNING("Could not parse file type option: {}", type);
        return FileType::Unknown;
    }
};

#ifdef ENABLE_LOGGING
[[nodiscard]] static auto ParseLogLevel(std::string_view level) -> logging::Logger::Level {
    if (level == "info") {
        return logging::Logger::Info;
    } else if (level == "warning") {
        return logging::Logger::Warning;
    } else if (level == "error") {
        return logging::Logger::Error;
    } else if (level == "fatal") {
        return logging::Logger::Fatal;
    }

    LOG_WARNING("Could not parse log level option: {}", level);
    return logging::Logger::Warning;
}
#endif

[[nodiscard]] static auto CreateFS(
    FileType type, const std::filesystem::path& path, provider::UniqueProvider provider, FileType* outType
) -> std::unique_ptr<fs::IFileSystem> {
    try {
        if (outType != nullptr) {
            *outType = type;
        }
        switch (type) {
            case FileType::NSP:
            case FileType::PFS0:
                return std::make_unique<formats::PartitionFileSystem>(std::move(provider), path.filename().string());
            case FileType::XCI:
                return std::make_unique<formats::GameCardFileSystem>(std::move(provider), path.filename().string());
            case FileType::HFS0:
                return std::make_unique<formats::RootSha256FileSystem>(std::move(provider), path.filename().string());
            case FileType::NCA:
                return std::make_unique<formats::NintendoContentArchiveFileSystem>(std::move(provider), path.filename().string());
            case FileType::Unknown:
                if (path.extension() == ".nca") {
                    if (outType != nullptr) {
                        *outType = FileType::NCA;
                    }
                    return std::make_unique<formats::NintendoContentArchiveFileSystem>(std::move(provider), path.filename().string());
                }
                [[fallthrough]];
            default:
                LOG_FATAL("Unknown input file type!");
        }
    } catch (const std::runtime_error& e) {
        LOG_ERROR("Error while creating filesystem from {}: {}", path.string(), e.what());
        return nullptr;
    }
}

[[nodiscard]] static auto OpenFS(std::string_view path, std::string_view intype, FileType* outType) -> std::unique_ptr<fs::IFileSystem> {
    auto provider = std::make_unique<provider::FileStreamProvider>(path);
    if (!provider->isOpen()) {
        LOG_FATAL("Failed to open input file! {}", path);
    }

    const auto type = intype.empty() ? GetFileType(*provider) : ParseType(intype);
    return CreateFS(type, std::filesystem::path(path), std::move(provider), outType);
}

[[nodiscard]] static auto IsArchiveType(FileType type) -> bool {
    return type == FileType::PFS0 || type == FileType::NSP || type == FileType::HFS0 || type == FileType::XCI;
}

[[nodiscard]] static auto IsUpdateArchiveType(FileType type) -> bool {
    return type == FileType::PFS0 || type == FileType::NSP;
}

[[nodiscard]] static auto GetPartitionFileSystem(std::unique_ptr<fs::IFileSystem>& fs, FileType type) -> formats::PartitionFileSystemBase* {
    switch (type) {
        case FileType::PFS0:
        case FileType::NSP:
            return static_cast<formats::PartitionFileSystemBase*>(fs.get());
        case FileType::HFS0:
            return static_cast<formats::RootSha256FileSystem*>(fs.get())->getSecurePartition();
        case FileType::XCI:
            return static_cast<formats::GameCardFileSystem*>(fs.get())->getFileSystem().getSecurePartition();
        default:
            return nullptr;
    }
}

static auto ApplyUpdate(
    std::unique_ptr<fs::IFileSystem>& base, std::unique_ptr<fs::IFileSystem>& update, FileType baseType, FileType updateType
) -> void {
    if (baseType == FileType::NCA && updateType == FileType::NCA) {
        // nca + nca
        auto baseFs = static_cast<formats::NintendoContentArchiveFileSystem*>(base.get());
        auto updateFs = static_cast<formats::NintendoContentArchiveFileSystem*>(update.get());

        updateFs->setBase(*baseFs);
        base.swap(update);
    } else if (IsArchiveType(baseType) && IsUpdateArchiveType(updateType)) {
        // pfs0/hfs0 + pfs0/hfs0
        formats::PartitionFileSystemBase* baseFs = GetPartitionFileSystem(base, baseType);
        formats::PartitionFileSystemBase* updateFs = GetPartitionFileSystem(update, updateType);

        if (baseFs != nullptr && updateFs != nullptr) {
            baseFs->applyUpdate(std::unique_ptr<formats::PartitionFileSystemBase>(reinterpret_cast<formats::PartitionFileSystemBase*>(update.release())));
        } else {
            LOG_WARNING("Failed to resolve base and update filesystems");
        }
    } else {
        // pfs0/hfs0 + nca
        // nca + pfs0/hfs0
        LOG_WARNING("Unsupported combination of base + update file types");
    }
}

[[noreturn]] auto PrintUsage() -> void {
    fmt::print(
        "nxmount [options]\n"
        "  Options:\n"
        "    --mount, -m        [REQUIRED] mount point (on Windows, this must be a drive that is not currently in use; on Linux, this must be a directory that exists)\n"
        "    --base, -b         [REQUIRED] base file to mount (XCI, NSP, NCA)\n"
        "    --keys, -k         path to a keys file (e.g. prod.keys or dev.keys)\n"
        "    --titlekeys        path to a titlekeys file (e.g. title.keys)\n"
        "    --titlekey         titlekey to use for NCAs (takes priority over other keys)\n"
        "    --type, -t         input file type (nsp, pfs0, xci, hfs0, nca, auto)\n"
        "    --update-dir       path to a directory of update NSPs to apply to the base file\n"
        "    --update, -u       path to an update NSP to apply to the base file\n"
        "    --log, -l          log level (info, warning, error, fatal)\n"
        "    --help, -h         print help message (what you're reading right now)\n"
        "    --foreground, -f   run process in the foreground (i.e. do not daemonize)\n"
        "    --debug, -d        output driver debug logs\n"
    );
    std::exit(0);
}

auto Run(std::int32_t argc, const char* argv[]) -> std::int32_t {
    if (argc < 2) {
        PrintUsage();
    }

    detail::Config config{};

    for (auto argp = argv + 1; argp < argv + argc; ++argp) {
        const auto arg = std::string_view(*argp);
        if (arg.starts_with("--") || arg.starts_with('-')) {
            const bool isShortOpt = !arg.starts_with("--");
            const auto eqPos = arg.find('=');
            
            const auto startPos = isShortOpt ? 1 : 2;
            const auto opt = eqPos == std::string_view::npos ? arg.substr(startPos) : arg.substr(startPos, eqPos - startPos);
            if (opt.empty()) {
                PrintUsage();
            }

            std::string_view value;
            auto getValue = [&]() -> void {
                if (eqPos == std::string_view::npos) {
                    if (argp + 1 < argv + argc) {
                        value = *(++argp);
                    } else {
                        PrintUsage();
                    }
                } else {
                    value = arg.substr(eqPos + 1);
                    if (opt.empty()) {
                        PrintUsage();
                    }
                }
            };
            #define MATCH_OPT(LONG, SHORT) opt == (isShortOpt ? (SHORT) : (LONG))
            #define MATCH_LONG_OPT(LONG) !isShortOpt && opt == LONG
            #define MATCH_SHORT_OPT(SHORT) isShortOpt && opt == SHORT
            #define SET_VALUE(DEST)         \
                getValue();                 \
                if (value.empty()) {        \
                    PrintUsage();           \
                } else {                    \
                    (DEST) = value;         \
                }
            if (MATCH_OPT("mount", "m")) {
                SET_VALUE(config.mountPoint);
            } else if (MATCH_OPT("base", "b")) {
                SET_VALUE(config.basePath);
            } else if (MATCH_OPT("keys", "k")) {
                SET_VALUE(config.keyPath);
            } else if (MATCH_LONG_OPT("titlekeys")) {
                SET_VALUE(config.titlePath);
            } else if (MATCH_LONG_OPT("titlekey")) {
                SET_VALUE(config.titlekey);
            } else if (MATCH_OPT("type", "t")) {
                SET_VALUE(config.intype);
            } else if (MATCH_LONG_OPT("update-dir")) {
                SET_VALUE(config.updatePath);
            } else if (MATCH_OPT("update", "u")) {
                SET_VALUE(config.updates.emplace_back());
#ifdef ENABLE_LOGGING
            } else if (MATCH_OPT("log", "l")) {
                SET_VALUE(config.logLevel);
#endif
            } else if (MATCH_OPT("help", "h")) {
                PrintUsage();
            } else if (MATCH_OPT("foreground", "f")) {
                config.foreground = true;
            } else if (MATCH_OPT("debug", "d")) {
                config.debug = true;
            }
            #undef MATCH_OPT
            #undef SET_VALUE
        } else {
            PrintUsage();
        }
    }

    if (config.mountPoint.empty() || config.basePath.empty()) {
        fmt::println("Please provide a mount point and base file path with --mount and --base");
        PrintUsage();
    }

#ifdef ENABLE_LOGGING
    if (!config.logLevel.empty()) {
        logging::Logger::SetLogLevel(ParseLogLevel(config.logLevel));
    } else {
        logging::Logger::SetLogLevel(logging::Logger::Warning);
    }
#endif

    if (!config.titlekey.empty()) {
        std::uint8_t key[0x10];
        crypto::ParseKey(key, sizeof(key), config.titlekey);
        LOG_INFO("Using titlekey {}", key);
        crypto::KeyManager::instance()->setExternalTitleKey(key);
    }

    crypto::KeyManager::instance()->initialize(config.keyPath, config.titlePath);

    FileType realBaseType;
    auto base = OpenFS(config.basePath, config.intype, std::addressof(realBaseType));
    if (base == nullptr) {
        LOG_FATAL("Failed to create base filesystem! {}", config.basePath);
    }
    base->init();

    for (const auto updatePath : config.updates) {
        if (updatePath.empty()) {
            continue;
        }

        LOG_INFO("Creating update {}", updatePath);

        FileType realUpdateType;
        auto update = OpenFS(updatePath, config.updateType, std::addressof(realUpdateType));
        if (update == nullptr) {
            LOG_ERROR("Failed to create update filesystem! {}", updatePath);
        } else {
            update->init();
            LOG_INFO("Applying update {} to {}", updatePath, config.basePath);
            ApplyUpdate(base, update, realBaseType, realUpdateType);
        }
    }

    if (!config.updatePath.empty()) {
        for (const auto& entry : std::filesystem::directory_iterator(config.updatePath)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            if (entry.path().extension() != ".nsp" && entry.path().extension() != ".nca") {
                continue;
            }

            LOG_INFO("Creating update {}", entry.path().string());

            const auto updatePath = entry.path().string();
            FileType realUpdateType;
            auto update = OpenFS(updatePath, config.updateType, std::addressof(realUpdateType));
            if (update == nullptr) {
                LOG_ERROR("Failed to create update filesystem! {}", updatePath);
            } else {
                update->init();
                LOG_INFO("Applying update {} to {}", updatePath, config.basePath);
                ApplyUpdate(base, update, realBaseType, realUpdateType);
            }
        }
    }

    return detail::RunImpl(config, std::move(base));
}

} // namespace nxmount::driver