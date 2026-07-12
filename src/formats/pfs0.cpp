#include "crypto/key_mgr.hpp"
#include "formats/cnmt.hpp"
#include "formats/nacp.hpp"
#include "formats/nca.hpp"
#include "formats/pfs0.hpp"
#include "formats/titles.hpp"
#include "fs/directory.hpp"
#include "fs/filesystem.hpp"
#include "fs/null_file.hpp"
#include "fs/path.hpp"
#include "fs/wrapper_filesystem.hpp"
#include "common/errors.hpp"
#include "log/logging.hpp"
#include "provider/file_provider.hpp"
#include "provider/provider.hpp"

#include <memory>
#include <stdexcept>

namespace nxmount::formats {

PartitionFileSystem::PartitionFileSystem(provider::UniqueProvider provider, std::string_view name) : PartitionFileSystemBase(std::move(provider), name) {
    if (mProvider == nullptr) {
        LOG_ERROR("Null bytes provider passed to PartitionFileSystem");
        throw std::runtime_error(mName);
    }

    PartitionFileSystemHeader header;
    if (mProvider->read(std::addressof(header), sizeof(header), 0) != sizeof(header)) {
        LOG_ERROR("Failed to read PartitionFileSystem header!");
        throw std::runtime_error(mName);
    }

    if (header.magic != PartitionFileSystemHeader::cMagic) {
        LOG_ERROR("Invalid PartitionFileSystem header magic! {:#010x}", header.magic);
        throw std::runtime_error(mName);
    }

    mEntries.resize(header.entryCount);

    auto stringTable = std::vector<char>(header.stringTableSize + 1);
    stringTable[header.stringTableSize] = '\0';

    const auto entryTableOffset = sizeof(header);
    const auto stringTableOffset = entryTableOffset + header.entryCount * sizeof(PartitionEntry);

    if (mProvider->read(stringTable.data(), stringTable.size() - 1, stringTableOffset) != stringTable.size() - 1) {
        LOG_ERROR("Failed to read PartitionFileSystem string table");
        throw std::runtime_error(mName);
    }

    mDataStart = stringTableOffset + header.stringTableSize;
    mDataEnd = mProvider->getSize();

    std::size_t offset = 0;
    for (auto& entry : mEntries) {
        PartitionEntry data;
        if (mProvider->read(std::addressof(data), sizeof(data), entryTableOffset + offset) != sizeof(data)) {
            LOG_ERROR("Failed to read PartitionFileSystem entry!");
            throw std::runtime_error(mName);
        }
        entry.size = data.size;
        entry.offset = data.offset + mDataStart;
        entry.name = stringTable.data() + data.stringOffset;
        offset += sizeof(data);
    }
}

auto PartitionFileSystemBase::tryParseTicket() const -> void {
    std::size_t index = 0;
    for (auto& entry : mEntries) {
        if (entry.name.ends_with(".tik")) {
            // I'm not gonna bother verifying the entire ticket, just assume it's valid and extract the title key
            auto file = std::make_unique<provider::FileProvider>(std::make_unique<File>(*this, index));
            std::uint32_t sigType;
            if (file->read(std::addressof(sigType), sizeof(sigType), 0) != sizeof(sigType)) {
                continue;
            }
            std::size_t signatureSizeWithPadding = sizeof(sigType);
            switch (sigType) {
                case 0x010000: signatureSizeWithPadding += 0x200 + 0x3c; break;
                case 0x010001: signatureSizeWithPadding += 0x100 + 0x3c; break;
                case 0x010002: signatureSizeWithPadding += 0x3c + 0x40; break;
                case 0x010003: signatureSizeWithPadding += 0x200 + 0x3c; break;
                case 0x010004: signatureSizeWithPadding += 0x100 + 0x3c; break;
                case 0x010005: signatureSizeWithPadding += 0x3c + 0x40; break;
                case 0x010006: signatureSizeWithPadding += 0x14 + 0x28; break;
                default: continue;
            }
            std::uint8_t titleKeyBlock[0x100];
            if (file->read(titleKeyBlock, sizeof(titleKeyBlock), signatureSizeWithPadding + 0x40) != sizeof(titleKeyBlock)) {
                continue;
            }
            std::uint8_t version, type;
            if (file->read(std::addressof(version), sizeof(version), signatureSizeWithPadding + 0x140) != sizeof(version)
                || file->read(std::addressof(type), sizeof(type), signatureSizeWithPadding + 0x141) != sizeof(type)) {
                continue;
            }
            if (version != 2) {
                continue;
            }
            std::uint8_t rightsId[0x10];
            if (file->read(rightsId, sizeof(rightsId), signatureSizeWithPadding + 0x160) != sizeof(rightsId)) {
                continue;
            }
            switch (type) {
                case 0:
                    LOG_INFO("Adding title key from ticket");
                    crypto::KeyManager::instance()->addTitleKey(rightsId, titleKeyBlock);
                    break;
                case 1:
                    LOG_WARNING("Unimplemented ticket title key encryption type");
                    break;
                default:
                    continue;
            }
        }
        ++index;
    }
}

[[nodiscard]] static auto SearchContentMeta(const std::unique_ptr<fs::IFileSystem>& fs) -> provider::UniqueProvider {
    auto dir = fs->getRoot();
    fs::DirectoryEntry firstDir;
    if (NX_FAILED(dir->read(nullptr, std::addressof(firstDir), 1, 0))) {
        return nullptr;
    }

    if (NX_FAILED(fs->openDirectory(std::addressof(dir), firstDir.name))) {
        return nullptr;
    }

    for (const auto& entry : *dir) {
        if (entry.name.ends_with(".cnmt")) {
            std::string name;
            fmt::format_to(std::back_inserter(name), "{}/{}", firstDir.name, entry.name);
            std::unique_ptr<fs::IFile> file;
            if (NX_FAILED(fs->openFile(std::addressof(file), name, fs::OpenMode::Read))) {
                return nullptr;
            }
            return std::make_unique<provider::FileProvider>(std::move(file));
        }
    }

    return nullptr;
}

auto PartitionFileSystemBase::rearrangeNCAs() -> void {
    struct ContentPackage {
        ContentMetaReader reader;
        std::size_t metaIndex;
    };
    auto packages = std::vector<ContentPackage>{};
    for (std::size_t i = 0; i < mEntries.size(); ++i) {
        const auto& entry = mEntries[i];
        if (entry.fs == nullptr || !entry.name.ends_with(".nca")) {
            continue;
        }

        auto fs = static_cast<NintendoContentArchiveFileSystem*>(entry.fs.get());

        provider::UniqueProvider metaProvider = nullptr;
        if (fs->getContentType() == ContentType::Meta) {
            metaProvider = SearchContentMeta(entry.fs);
        }

        if (metaProvider) {
            packages.emplace_back(ContentMetaReader(std::move(metaProvider)), i);
        }
    }
    
    for (const auto& package : packages) {
        if (!package.reader.isValid()) {
            continue;
        }
        
        std::string name;
        for (const auto& info : package.reader.getContentInfo()) {
            if (info.contentType == PackagedContentType::Control) {
                std::string path;
                fmt::format_to(std::back_inserter(path), "{}.nca", info.contentId);
                std::size_t index;
                const auto entry = getEntry(path, std::addressof(index));
                if (entry == nullptr || entry->fs == nullptr) {
                    continue;
                }
                std::unique_ptr<fs::IFile> file;
                if (NX_FAILED(entry->fs->openFile(std::addressof(file), "0/control.nacp", fs::OpenMode::Read))) {
                    continue;
                }
                provider::UniqueProvider provider = std::make_unique<provider::FileProvider>(std::move(file));
                name = GetDisplayName(provider);
                if (!name.empty()) {
                    fmt::format_to(std::back_inserter(name), " [{:016x}]", package.reader.getApplicationId());
                }
                break;
            }
        }

        if (name.empty()) {
            if (const auto titleName = TryGetTitleName(package.reader.getApplicationId()); !titleName.empty()) {
                fmt::format_to(std::back_inserter(name), "{} [{:016x}]", titleName, package.reader.getApplicationId());
            } else {
                fmt::format_to(std::back_inserter(name), "{:016x}", package.reader.getApplicationId());
            }
        }

        if (package.reader.isApplication()) {
            const auto version = package.reader.getVersion();
            fmt::format_to(std::back_inserter(name), "[{}.{}]", version >> 0x10 & 0xffff, version & 0xffff);
        } else {
            const auto version = package.reader.getVersion();
            fmt::format_to(
                std::back_inserter(name), "[{}.{}.{}-{}.{}]",
                version >> 0x1a & 0x3f,
                version >> 0x14 & 0x3f,
                version >> 0x10 & 0xf,
                version >> 8 & 0xff,
                version & 0xff
            );
        }

        auto wrapper = std::make_unique<fs::WrapperFs>(name);

        {
            const auto entry = getEntry(package.metaIndex);
            entry->name.clear();
            if (wrapper->contains("Meta")) {
                std::string collision = "";
                fmt::format_to(std::back_inserter(collision), "Meta{}", package.metaIndex);
                wrapper->addFileSystem(collision, std::move(entry->fs));
            } else {
                wrapper->addFileSystem("Meta", std::move(entry->fs));
            }
        }

        for (const auto& info : package.reader.getContentInfo()) {
            std::string path;
            fmt::format_to(std::back_inserter(path), "{}.nca", info.contentId);
            std::size_t index;
            const auto entry = getEntry(path, std::addressof(index));
            if (entry == nullptr) {
                continue;
            }
            entry->name.clear();
            const auto typeName = ToString(info.contentType);
            if (wrapper->contains(typeName)) {
                auto collision = std::string(typeName);
                fmt::format_to(std::back_inserter(collision), "{}", index);
                wrapper->addFileSystem(collision, std::move(entry->fs));
            } else {
                wrapper->addFileSystem(typeName, std::move(entry->fs));
            }
        }

        auto& e = mEntries.emplace_back();
        e.name = name;
        e.fs = std::move(wrapper);
        e.offset = 0;
        e.size = 0;
    }

    for (auto& entry : mEntries) {
        if (entry.name.ends_with(".nca")) {
            const auto filename = entry.name.substr(0, entry.name.size() - 4);
            auto fs = static_cast<const NintendoContentArchiveFileSystem*>(entry.fs.get());
            entry.name.clear();
            fmt::format_to(std::back_inserter(entry.name), "{}_{}", ToString(fs->getContentType()), filename);
        }
    }
}

auto PartitionFileSystemBase::init() -> void {
    tryParseTicket();

    std::size_t i = 0;
    for (auto& entry : mEntries) {
        processEntry(entry, i++);
    }

    rearrangeNCAs();

#if defined(WIN32) // windows doesn't allow mounting empty directories
    if (mEntries.empty()) {
        mEntries.emplace_back("__NULL_FILE__", 0xffff'ffff'ffff'ffff, 0, nullptr);
    }
#endif
}

auto PartitionFileSystemBase::destroy() -> void {
    for (auto& entry : mEntries) {
        if (entry.fs != nullptr) {
            entry.fs->destroy();
        }
    }
}

auto PartitionFileSystemBase::getAttributes(std::string_view path, fs::DirectoryEntry* entry) const -> Result {
    if (entry == nullptr) {
        return INVALID;
    }

    std::string_view subpath;
    const auto name = fs::FirstComponent(path, std::addressof(subpath));

    if (name.empty() || fs::IsPathSeparator(name[0])) {
        entry->type = fs::Type::Directory;
        entry->createTime = mInitTime;
        return SUCCESS;
    }

    const auto pfsEntry = getEntry(name);
    if (pfsEntry == nullptr) {
        return NO_FILE;
    }

    if (pfsEntry->fs == nullptr) {
        // file
        if (!subpath.empty()) {
            return NO_FILE;
        }
        entry->type = fs::Type::File;
        entry->createTime = mInitTime;
        entry->fileSize = pfsEntry->size;
    } else {
        // directory
        return pfsEntry->fs->getAttributes(subpath, entry);
    }

    return SUCCESS;
}

auto PartitionFileSystemBase::access(std::string_view path, fs::OpenMode mode) const -> Result {
    std::string_view subpath;
    const auto name = fs::FirstComponent(path, std::addressof(subpath));
    if (name.empty()) {
        if (mode != fs::OpenMode::Read) {
            return PERMISSION_ERROR;
        }

        return SUCCESS;
    }

    const auto entry = getEntry(name);
    if (entry == nullptr) {
        return NO_FILE;
    }

    if (entry->fs != nullptr) {
        return entry->fs->access(subpath, mode);
    }

    if (mode != fs::OpenMode::Read) {
        return PERMISSION_ERROR;
    }

    return SUCCESS;
}

auto PartitionFileSystemBase::openFile(std::unique_ptr<fs::IFile>* out, std::string_view path, fs::OpenMode mode) const -> Result {
    if (out == nullptr) {
        return INVALID;
    }

    std::string_view subpath;
    const auto name = fs::FirstComponent(path, std::addressof(subpath));
    std::size_t index;
    const auto entry = getEntry(name, std::addressof(index));
    if (entry == nullptr) {
        return NO_FILE;
    }

    if (entry->fs != nullptr) {
        return entry->fs->openFile(out, subpath, mode);
    }
    
    if (mode != fs::OpenMode::Read) {
        return PERMISSION_ERROR;
    }

    if (entry->isNull()) {
        *out = std::make_unique<fs::NullFile>();
    } else {
        *out = std::make_unique<File>(*this, index);
    }
    return SUCCESS;
}

auto PartitionFileSystemBase::openDirectory(std::unique_ptr<fs::IDirectory>* dir, std::string_view path) const -> Result {
    if (dir == nullptr) {
        return INVALID;
    }
    std::string_view subpath;
    const auto name = fs::FirstComponent(path, std::addressof(subpath));
    if (name.empty()) {
        *dir = getRoot();
        return SUCCESS;
    }

    const auto entry = getEntry(name);
    if (entry != nullptr && entry->fs != nullptr) {
        return entry->fs->openDirectory(dir, subpath);
    }

    return NO_FILE;
}

auto PartitionFileSystemBase::getContentMetaReader(std::uint64_t id, fs::IFileSystem** fsOut) const -> ContentMetaReader {
    for (auto& entry : mEntries) {
        if (entry.fs == nullptr || entry.name.empty()) {
            continue;
        }

        std::unique_ptr<fs::IDirectory> metaDir;
        if (NX_FAILED(entry.fs->openDirectory(std::addressof(metaDir), "Meta/0"))) {
            continue;
        }

        std::string cnmtPath = "";
        for (const auto& dirEntry : *metaDir) {
            if (dirEntry.type == fs::Type::Directory) {
                continue;
            }

            cnmtPath = std::string("Meta/0/") + dirEntry.name;
        }

        if (cnmtPath.empty()) {
            continue;
        }

        std::unique_ptr<fs::IFile> cnmtFile;
        if (NX_FAILED(entry.fs->openFile(std::addressof(cnmtFile), cnmtPath, fs::OpenMode::Read))) {
            continue;
        }

        const auto reader = ContentMetaReader(std::make_unique<provider::FileProvider>(std::move(cnmtFile)));

        if (reader.isValid() && reader.getApplicationId() == id) {
            if (fsOut != nullptr) {
                *fsOut = entry.fs.get();
            }
            return reader;
        }
    }

    return ContentMetaReader();
}

[[nodiscard]] static auto SupportsPatching(PackagedContentType type) -> bool {
    return type == PackagedContentType::Program || type == PackagedContentType::Data || type == PackagedContentType::HtmlDocument;
}

[[nodiscard]] static auto FormatContentPath(const ContentId& id) -> std::string {
    std::string path;
    fmt::format_to(std::back_inserter(path), "{}.nca", id);
    return path;
}

auto PartitionFileSystemBase::applyUpdate(std::unique_ptr<PartitionFileSystemBase> update) -> void {
    bool applied = false;
    // we assume that if there is metadata to be able to apply the patch, then we will have already rearranged the NCAs properly
    auto entriesToAdd = std::vector<Entry>{};
    for (auto& entry : update->mEntries) {
        if (entry.name.empty() || entry.fs == nullptr) {
            continue;
        }

        std::unique_ptr<fs::IDirectory> metaDir;
        if (NX_FAILED(entry.fs->openDirectory(std::addressof(metaDir), "Meta/0"))) {
            continue;
        }

        std::string cnmtPath = "";
        for (const auto& dirEntry : *metaDir) {
            if (dirEntry.type == fs::Type::Directory) {
                continue;
            }

            cnmtPath = "Meta/0/" + dirEntry.name;
        }

        if (cnmtPath.empty()) {
            continue;
        }

        std::unique_ptr<fs::IFile> cnmtFile;
        if (NX_FAILED(entry.fs->openFile(std::addressof(cnmtFile), cnmtPath, fs::OpenMode::Read))) {
            continue;
        }

        const auto reader = ContentMetaReader(std::make_unique<provider::FileProvider>(std::move(cnmtFile)));
        if (!reader.isValid()) {
            continue;
        }

        if (reader.getType() != ContentMetaType::Patch) { // TODO: delta + data patches, dlc
            LOG_INFO("Skipping {} which is not a patch", cnmtPath);
            continue;
        }

        const auto patchHeader = reader.getExtendedMetaHeader<PatchMetaExtendedHeader>();
        if (patchHeader == nullptr) {
            continue;
        }

        fs::IFileSystem* baseWrapper = nullptr;
        const auto baseReader = getContentMetaReader(patchHeader->applicationId, std::addressof(baseWrapper));
        if (!baseReader.isValid() || baseWrapper == nullptr) {
            continue;
        }

        const auto patchReader = reader.getExtendedDataReader<PatchMetaExtendedDataReader>();
        if (patchReader.isValid()) {
            LOG_INFO("PatchHistoryHeader");
            for (const auto& header : patchReader.getPatchHistoryHeader()) {
                LOG_INFO("  ContentMetaKey:");
                LOG_INFO("    ID: {:016x}", header.contentMetaKey.id);
                LOG_INFO("    Version: {:#x}", header.contentMetaKey.version);
                LOG_INFO("    ContentMetaType: {:#x}", std::to_underlying(header.contentMetaKey.metaType));
                LOG_INFO("    ContentInstallType: {:#x}", std::to_underlying(header.contentMetaKey.installType));
                LOG_INFO("  Digest: {}", header.digest);
                LOG_INFO("  ContentInfoCount: {}", header.contentInfoCount);
                LOG_INFO("  ----------");
            }
            LOG_INFO("PatchDeltaHistory");
            for (const auto& history : patchReader.getPatchDeltaHistory()) {
                LOG_INFO("  SourcePatchID: {:016x}", history.sourcePatchId);
                LOG_INFO("  DestinationPatchID: {:016x}", history.destinationPatchId);
                LOG_INFO("  SourceVersion: {:#x}", history.sourceVersion);
                LOG_INFO("  DestinationVersion: {:#x}", history.destinationVersion);
                LOG_INFO("  DownloadSize: {:#x}", history.downloadSize);
                LOG_INFO("  ----------");
            }
            LOG_INFO("PatchDeltaHeader");
            for (const auto& header : patchReader.getPatchDeltaHeader()) {
                LOG_INFO("  SourcePatchID: {:016x}", header.sourcePatchId);
                LOG_INFO("  DestinationPatchID: {:016x}", header.destinationPatchId);
                LOG_INFO("  SourceVersion: {:#x}", header.sourceVersion);
                LOG_INFO("  DestinationVersion: {:#x}", header.destinationVersion);
                LOG_INFO("  FragmentSetCount: {}", header.fragmentSetCount);
                LOG_INFO("  ContentInfoCount: {}", header.contentInfoCount);
                LOG_INFO("  ----------");
            }
            LOG_INFO("FragmentSet");
            for (const auto& set : patchReader.getFragmentSet()) {
                LOG_INFO("  SourceContentId: {}", set.sourceContentId);
                LOG_INFO("  DestinationContentId: {}", set.destinationContentId);
                LOG_INFO("  SourceSize: {:#x}", static_cast<std::uint64_t>(set.sourceSizeLow) | static_cast<std::uint64_t>(set.sourceSizeHigh) << 0x20);
                LOG_INFO("  DestinationSize: {:#x}", static_cast<std::uint64_t>(set.destinationSizeLow) | static_cast<std::uint64_t>(set.destinationSizeHigh) << 0x20);
                LOG_INFO("  FragmentIndicatorCount: {}", set.fragmentIndicatorCount);
                LOG_INFO("  FragmentTargetContentType: {}", ToString(set.fragmentTargetContentType));
                LOG_INFO("  UpdateType: {}", std::to_underlying(set.updateType));
                LOG_INFO("  ----------");
            }
            LOG_INFO("PatchHistoryContentInfo");
            for (const auto& info : patchReader.getPatchHistoryContentInfo()) {
                LOG_INFO("  ContentId: {}", info.contentId);
                LOG_INFO("  Size: {}", static_cast<std::uint64_t>(info.sizeLow) | static_cast<std::uint64_t>(info.sizeHigh) << 0x20);
                LOG_INFO("  Attr: {}", info.attr);
                LOG_INFO("  ContentType: {}", ToString(info.contentType));
                LOG_INFO("  IdOffset: {}", info.idOffset);
                LOG_INFO("  ----------");
            }
            LOG_INFO("PatchDeltaPackagedContentInfo");
            for (const auto& info : patchReader.getPatchDeltaPackagedContentInfo()) {
                LOG_INFO("  Hash: {}", info.hash);
                LOG_INFO("  ContentId: {}", info.contentId);
                LOG_INFO("  Size: {}", static_cast<std::uint64_t>(info.sizeLow) | static_cast<std::uint64_t>(info.sizeHigh) << 0x20);
                LOG_INFO("  Attr: {}", info.attr);
                LOG_INFO("  ContentType: {}", ToString(info.contentType));
                LOG_INFO("  IdOffset: {}", info.idOffset);
                LOG_INFO("  ----------");
            }
            LOG_INFO("FragmentIndicator");
            for (const auto& indicator : patchReader.getFragmentIndicator()) {
                LOG_INFO("  ContentInfoIndex: {}", indicator.contentInfoIndex);
                LOG_INFO("  FragmentIndex: {}", indicator.fragmentIndex);
                LOG_INFO("  ----------");
            }
        }

        for (const auto& info : reader.getContentInfo()) {
            if (!SupportsPatching(info.contentType)) {
                continue;
            }

            const auto baseInfo = baseReader.findContentInfo(info.contentType);
            if (baseInfo == nullptr) {
                continue;
            }

            NintendoContentArchiveFileSystem* baseFs = nullptr;
            NintendoContentArchiveFileSystem* updateFs = nullptr;

            const auto path = FormatContentPath(info.contentId);;
            std::string_view updateFsName;
            for (const auto& [name, fs] : static_cast<const fs::WrapperFs*>(entry.fs.get())->getFileSystems()) {
                if (fs->getRoot()->getName() == path) {
                    updateFs = static_cast<NintendoContentArchiveFileSystem*>(fs.get());
                    updateFsName = name;
                    break;
                }
            }

            const auto basePath = FormatContentPath(baseInfo->contentId);
            for (const auto& [_, fs] : static_cast<const fs::WrapperFs*>(baseWrapper)->getFileSystems()) {
                if (fs->getRoot()->getName() == basePath) {
                    baseFs = static_cast<NintendoContentArchiveFileSystem*>(fs.get());
                    break;
                }
            }

            if (baseFs != nullptr && updateFs != nullptr) {
                updateFs->setBase(*baseFs);
                auto& updateEntry = mEntries.emplace_back();
                updateEntry.name = std::move(entry.name);
                updateEntry.fs = std::move(entry.fs);
                updateEntry.offset = 0;
                updateEntry.size = 0;
                applied = true;
            } else {
                LOG_WARNING("Missing file {} {} for type {}", path, basePath, std::to_underlying(info.contentType));
            }
        }
    }

    if (!applied) {
        LOG_INFO("No applicable updates found");
    } else {
        auto& entry = mEntries.emplace_back();
        entry.fs = std::move(update); // take ownership of the parent fs bc we need it still
        entry.name = "";
        entry.offset = 0;
        entry.size = 0;
        LOG_INFO("Applied update to {}", mName);
    }
}

auto PartitionFileSystemBase::applyAddOnContent(std::unique_ptr<PartitionFileSystemBase> aoc) -> void {
    bool applied = false;
    // we assume that if there is metadata to be able to apply the patch, then we will have already rearranged the NCAs properly
    auto entriesToAdd = std::vector<Entry>{};
    for (auto& entry : aoc->mEntries) {
        if (entry.name.empty() || entry.fs == nullptr) {
            continue;
        }

        std::unique_ptr<fs::IDirectory> metaDir;
        if (NX_FAILED(entry.fs->openDirectory(std::addressof(metaDir), "Meta/0"))) {
            continue;
        }

        std::string cnmtPath = "";
        for (const auto& dirEntry : *metaDir) {
            if (dirEntry.type == fs::Type::Directory) {
                continue;
            }

            cnmtPath = "Meta/0/" + dirEntry.name;
        }

        if (cnmtPath.empty()) {
            continue;
        }

        std::unique_ptr<fs::IFile> cnmtFile;
        if (NX_FAILED(entry.fs->openFile(std::addressof(cnmtFile), cnmtPath, fs::OpenMode::Read))) {
            continue;
        }

        const auto reader = ContentMetaReader(std::make_unique<provider::FileProvider>(std::move(cnmtFile)));
        if (!reader.isValid()) {
            continue;
        }

        if (reader.getType() != ContentMetaType::AddOnContent) {
            LOG_INFO("Skipping {} which is not add-on-content", cnmtPath);
            continue;
        }

        const auto aocHeader = reader.getExtendedMetaHeader<AddOnContentMetaExtendedHeaderOld>();
        if (aocHeader == nullptr) {
            continue;
        }

        bool applicable = false;
        for (const auto& contentEntry : mEntries) {
            if (contentEntry.fs == nullptr || contentEntry.name.empty()) {
                continue;
            }

            std::unique_ptr<fs::IDirectory> metaDir;
            if (NX_FAILED(contentEntry.fs->openDirectory(std::addressof(metaDir), "Meta/0"))) {
                continue;
            }

            std::string cnmtPath = "";
            for (const auto& dirEntry : *metaDir) {
                if (dirEntry.type == fs::Type::Directory) {
                    continue;
                }

                cnmtPath = std::string("Meta/0/") + dirEntry.name;
            }

            if (cnmtPath.empty()) {
                continue;
            }

            std::unique_ptr<fs::IFile> cnmtFile;
            if (NX_FAILED(contentEntry.fs->openFile(std::addressof(cnmtFile), cnmtPath, fs::OpenMode::Read))) {
                continue;
            }

            const auto baseReader = ContentMetaReader(std::make_unique<provider::FileProvider>(std::move(cnmtFile)));
            if (!baseReader.isValid()) {
                continue;
            }

            if (baseReader.getVersion() < aocHeader->requiredApplicationVersion) {
                continue;
            }

            applicable = true;
            break;
        }
        if (applicable) {
            auto& aocEntry = mEntries.emplace_back();
            aocEntry.name = std::move(entry.name);
            aocEntry.fs = std::move(entry.fs);
            aocEntry.size = 0;
            aocEntry.offset = 0;
            applied = true;
        }
    }

    if (!applied) {
        LOG_INFO("No applicable DLC found");
    } else {
        auto& entry = mEntries.emplace_back();
        entry.fs = std::move(aoc);
        entry.name = "";
        entry.offset = 0;
        entry.size = 0;
        LOG_INFO("Applied DLC to {}", mName);
    }
}

auto PartitionFileSystemBase::processEntry(Entry& entry, std::size_t index) -> bool {
    if (entry.name.ends_with(".nca")) {
        if (entry.fs == nullptr) {
            entry.fs = std::make_unique<NintendoContentArchiveFileSystem>(
                std::make_shared<provider::FileProvider>(std::make_unique<File>(*this, index)), entry.name
            );
        }
        return true;
    } else {
        return false;
    }
}

auto PartitionFileSystemBase::File::read(void* dst, std::size_t size, std::size_t offset) const -> std::size_t {
    const auto entry = mParentFileSystem.getEntry(mIndex);
    if (offset >= entry->size) {
        return 0;
    }
    if (offset + size > entry->size) {
        size = entry->size - offset;
    }
    return mParentFileSystem.read(dst, size, entry->offset + offset);
}

auto PartitionFileSystemBase::Directory::read(std::size_t* entryCount, fs::DirectoryEntry* entries, std::size_t maxEntries, std::size_t offset) const -> Result {
    if (entries == nullptr) {
        return INVALID;
    }

    std::size_t totalCount = 0;
    for (const auto& entry : mParentFileSystem.mEntries) {
        if (!entry.name.empty()) {
            ++totalCount;
        }
    }

    if (offset >= totalCount) {
        if (entryCount != nullptr) {
            *entryCount = 0;
        }
        return SUCCESS;
    }

    std::size_t i = 0, current = 0;;
    for (const auto& entry : mParentFileSystem.mEntries) {
        if (entry.name.empty()) {
            continue;
        }
        if (i++ < offset) {
            continue;
        }
        if (current >= maxEntries) {
            break;
        }
        entries[current].name = entry.name;
        entries[current].type = entry.fs != nullptr ? fs::Type::Directory : fs::Type::File;
        entries[current].createTime = 0;
        entries[current++].fileSize = entry.size;
    }

    if (entryCount != nullptr) {
        *entryCount = current;
    }

    return SUCCESS;
}

} // namespace nxmount::formats