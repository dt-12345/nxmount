#include "common/errors.hpp"
#include "common/utils.hpp"
#include "crypto/crypto.hpp"
#include "crypto/key_mgr.hpp"
#include "crypto/key_utils.hpp"
#include "fs/path.hpp"
#include "provider/atmosphere.hpp"
#include "formats/nca.hpp"
#include "formats/npdm.hpp"
#include "formats/pfs0.hpp"
#include "formats/romfs.hpp"
#include "fs/filesystem.hpp"
#include "log/logging.hpp"
#include "provider/cache_provider.hpp"
#include "provider/file_provider.hpp"
#include "provider/memory_stream_provider.hpp"
#include "provider/offset_provider.hpp"

#include "hactool/pki.h"
#include "provider/provider.hpp"

#include <stdexcept>

#if !defined(WIN32)
    #include <unistd.h>
#endif

namespace nxmount::formats {

auto ToString(ContentType type) -> std::string_view {
    switch (type) {
        case ContentType::Program: return "Program";
        case ContentType::Meta: return "Meta";
        case ContentType::Control: return "Control";
        case ContentType::Manual: return "Manual";
        case ContentType::Data: return "Data";
        case ContentType::PublicData: return "PublicData";
        default: return "Unknown";
    }
}

static constexpr const std::uint8_t sExpectedHash[0x20] = {
    0x9A, 0xBB, 0xD2, 0x11, 0x86, 0x00, 0x21, 0x9D,
    0x7A, 0xDC, 0x5B, 0x43, 0x95, 0xF8, 0x4E, 0xFD,
    0xFF, 0x6B, 0x25, 0xEF, 0x9F, 0x96, 0x85, 0x28,
    0x18, 0x9E, 0x76, 0xB0, 0x92, 0xF0, 0x6A, 0xCB
};

[[nodiscard]] ALWAYS_INLINE static auto VerifyKeyGeneration(KeyGeneration gen) -> std::uint32_t {
    auto value = static_cast<std::uint32_t>(gen);
    if (value >= 0x20) {
        throw std::runtime_error("Invalid key generation!");
    }
    return value > 0 ? value - 1 : 0;
}

[[nodiscard]] ALWAYS_INLINE static auto VerifyKeyIndex(KeyAreaIndex index) -> std::uint32_t {
    auto value = static_cast<std::uint32_t>(index);
    if (value >= 3) {
        throw std::runtime_error("Invalid key area encryption key index!");
    }
    return value;
}

NintendoContentArchiveFileSystem::NintendoContentArchiveFileSystem(provider::SharedProvider provider, std::string_view name)
    : mProvider(std::move(provider)), mName(name), mInitTime(time(nullptr)) {
    mVersion = decryptNCAHeader(mHeader);
    if (mVersion == NCAVersion::Unknown) {
        LOG_ERROR("Failed to decrypt NCA!");
        throw std::runtime_error(mName);
    }

    if (mVersion == NCAVersion::NCA0 || mVersion == NCAVersion::NCA0Beta) {
        LOG_ERROR("Unsupported NCA version!");
        throw std::runtime_error(mName);
    }

    const crypto::KeyManager* const  keyMgr = crypto::KeyManager::instance();
    if (mHeader.header.signatureKeyGeneration >= 2) {
        LOG_ERROR("Invalid signature key generation!");
        throw std::runtime_error(mName);
    } else {
        const auto& keyset = keyMgr->getKeySet();
        if (!crypto::RsaPssVerify(
            std::addressof(mHeader.header.magic), 0x200,
            mHeader.header.headerSignature, keyset.nca_hdr_fixed_key_moduli[mHeader.header.signatureKeyGeneration]
        )) {
            LOG_ERROR("NCA header signature mismatch!");
            throw std::runtime_error(mName);
        }
    }

    const auto keyGen = (std::max)(VerifyKeyGeneration(mHeader.header.keyGenerationOld), VerifyKeyGeneration(mHeader.header.keyGeneration));

    if (crypto::IsNull(mHeader.header.rightsId)) {
        const auto& key = keyMgr->getKeySet().key_area_keys[keyGen][VerifyKeyIndex(mHeader.header.keyAreaEncryptionKeyIndex)];
        crypto::AesEcbDecrypt(mDecryptedKeys, mHeader.header.encryptedKeyArea, sizeof(mDecryptedKeys), key, sizeof(key));
    } else {
        if (!crypto::IsNull(keyMgr->getExternalTitleKey())) {
            const auto& titleKey = keyMgr->getExternalTitleKey();
            const auto& key = keyMgr->getKeySet().titlekeks[keyGen];
            crypto::AesEcbDecrypt(mTitleKey, titleKey, sizeof(titleKey), key, sizeof(key));
        } else if (const auto titleKey = keyMgr->getTitleKey(mHeader.header.rightsId); titleKey != nullptr) {
            const auto& key = keyMgr->getKeySet().titlekeks[keyGen];
            crypto::AesEcbDecrypt(mTitleKey, titleKey->titleKey, sizeof(titleKey->titleKey), key, sizeof(key));
        } else {
            LOG_WARNING("No title key found!");
        }
    }

    initializeFileSystems();
}

auto NintendoContentArchiveFileSystem::setBase(NintendoContentArchiveFileSystem& baseNCA) -> void {
    initializeFileSystems(std::addressof(baseNCA));
}

auto NintendoContentArchiveFileSystem::decryptNCAHeader(EncryptedHeaderRegion& outHeader) -> NCAVersion {
    const auto readSize = mProvider->read(std::addressof(outHeader), sizeof(outHeader), 0);
    if (readSize != sizeof(outHeader) && readSize != 0xa00) {
        LOG_ERROR("Failed to read NCA header!");
        return NCAVersion::Unknown;
    }

    NintendoContentArchiveHeader header;
    if  (!crypto::AesXtsDecrypt(
        std::addressof(header), std::addressof(outHeader.header), sizeof(header),
        crypto::KeyManager::instance()->getKeySet().header_key, sizeof(crypto::KeyManager::instance()->getKeySet().header_key),
        0, 0x200
    )) {
        LOG_ERROR("Failed to decrypt NCA header!");
        return NCAVersion::Unknown;
    }

    if (readSize == 0xa00 && header.magic != NintendoContentArchiveHeader::cMagic0) {
        LOG_ERROR("Failed to decrypt NCA header!");
        return NCAVersion::Unknown;
    }

    switch (header.magic) {
        case NintendoContentArchiveHeader::cMagic0: {
            std::uint8_t keys[0x100];
            std::size_t outSize;
            auto version = NCAVersion::Unknown;
            if (crypto::RsaOaepDecryptVerify(
                keys, sizeof(keys), header.encryptedKeyArea,
                pki_get_beta_nca0_modulus(), pki_get_beta_nca0_exponent(), 0x100,
                pki_get_beta_nca0_label_hash(), std::addressof(outSize)
            )) {
                if (outSize >= 0x20) {
                    version = NCAVersion::NCA0Beta;
                    std::memcpy(mDecryptedKeys, keys, 0x20);
                }
            } else {
                version = NCAVersion::NCA0;
                if (crypto::Sha256Verify(header.encryptedKeyArea, 0x20, sExpectedHash)) {
                    std::memcpy(mDecryptedKeys, header.encryptedKeyArea, 0x40);
                } else {
                    const auto& key = crypto::KeyManager::instance()->getKeySet().key_area_keys[VerifyKeyGeneration(header.keyGenerationOld)][VerifyKeyIndex(header.keyAreaEncryptionKeyIndex)];
                    crypto::AesEcbDecrypt(mDecryptedKeys, header.encryptedKeyArea, 0x20, key, sizeof(key));
                }
            }
            if (version != NCAVersion::Unknown) {
                outHeader.header = header;
                for (std::size_t i = 0; i < NintendoContentArchiveHeader::cFsCount; ++i) {
                    if (header.fsInfo[i].startSector == 0) {
                        continue;
                    }

                    const auto offset = SectorToOffset(header.fsInfo[i].startSector);
                    if (mProvider->read(std::addressof(outHeader.fsHeaders[i]), sizeof(outHeader.fsHeaders[i]), offset) != sizeof(outHeader.fsHeaders[i])) {
                        LOG_ERROR("Failed to read NCA FS header!");
                        return NCAVersion::Unknown;
                    }

                    if (!crypto::AesXtsDecrypt(
                        std::addressof(outHeader.fsHeaders[i]), std::addressof(outHeader.fsHeaders[i]), sizeof(outHeader.fsHeaders[i]),
                        mDecryptedKeys, 0x20,
                        OffsetToSector(offset - 0x400), cSectorSize
                    )) {
                        LOG_ERROR("Failed to decrypt NCA FS header!");
                        return NCAVersion::Unknown;
                    }
                }
            }

            return version;
        }
        case NintendoContentArchiveHeader::cMagic2: {
            for (std::size_t i = 0; i < NintendoContentArchiveHeader::cFsCount; ++i) {
                if (!crypto::AesXtsDecrypt(
                    std::addressof(outHeader.fsHeaders[i]), std::addressof(outHeader.fsHeaders[i]), sizeof(outHeader.fsHeaders[i]),
                    crypto::KeyManager::instance()->getKeySet().header_key, sizeof(crypto::KeyManager::instance()->getKeySet().header_key),
                    0, cSectorSize
                )) {
                    LOG_ERROR("Failed to decrypt FsHeader {} of NCA!", i);
                    return NCAVersion::Unknown;
                }
            }
            
            return NCAVersion::NCA2;
        }
        case NintendoContentArchiveHeader::cMagic3: {
            return crypto::AesXtsDecrypt(
                std::addressof(outHeader), std::addressof(outHeader), sizeof(outHeader),
                crypto::KeyManager::instance()->getKeySet().header_key, sizeof(crypto::KeyManager::instance()->getKeySet().header_key),
                0, cSectorSize
            ) ? NCAVersion::NCA3 : NCAVersion::Unknown;
        }
        default:
            LOG_ERROR("Unknown NCA magic post-decryption!: {:#010x}", header.magic);
            return NCAVersion::Unknown;
    }
}

auto NintendoContentArchiveFileSystem::printFsInfo(std::size_t index) const -> void {
    if (index >= mFileSystems.size()) {
        return;
    }

    const auto& header = mHeader.fsHeaders[index];
    const auto& info = mHeader.header.fsInfo[index];
    LOG_INFO("Fs {}: {:#x} - {:#x}", index, SectorToOffset(info.startSector), SectorToOffset(info.endSector));
    if (header.sparseInfo.generation != 0) {
        if (header.metaDataHashDataInfo.size != 0) {
            LOG_INFO("  Sparse metadata layer");
        } else {
            LOG_INFO("  Sparse layer");
        }
    }

    if (header.patchInfo.indirectSize != 0 && header.metaDataHashDataInfo.size != 0) {
        LOG_INFO("  Patch meta hash layer");
    }

    if (header.patchInfo.aesCtrExSize != 0) {
        LOG_INFO("  AesCtrEx layer");
    } else {
        switch (header.encryptionType) {
            case EncryptionType::None: break;
            case EncryptionType::AesXts: LOG_INFO("  EncryptionType::AesXts"); break;
            case EncryptionType::AesCtr: LOG_INFO("  EncryptionType::AesCtr"); break;
            case EncryptionType::AesCtrSkipLayerHash: LOG_INFO("  EncryptionType::AesCtrSkipLayerHash"); break;
            default: LOG_INFO("  EncryptionType other"); break;
        }
    }

    if (header.patchInfo.indirectSize != 0) {
        LOG_INFO("  Indirect layer");
    }

    switch (header.hashType) {
        case HashType::Auto: LOG_INFO("  HashType::Auto"); break;
        case HashType::None: LOG_INFO("  HashType::None"); break;
        case HashType::HierarchicalSha256Hash: LOG_INFO("  HashType::HierarchicalSha256Hash"); break;
        case HashType::HierarchicalIntegrityHash: LOG_INFO("  HashType::HierarchicalIntegrityHash"); break;
        case HashType::AutoSha3: LOG_INFO("  HashType::AutoSha3"); break;
        case HashType::HierarchicalSha3256Hash: LOG_INFO("  HashType::HierarchicalSha3256Hash"); break;
        case HashType::HierarchicalIntegritySha3Hash: LOG_INFO("  HashType::HierarchicalIntegritySha3Hash"); break;
        default: LOG_INFO("  HashType other");
    }

    if (header.compressionInfo.bucket.offset != 0 && header.compressionInfo.bucket.size != 0) {
        LOG_INFO("  Compression layer");
    }
}

auto NintendoContentArchiveFileSystem::initializeFileSystems(NintendoContentArchiveFileSystem* baseNCA) -> void {
    bool foundRomfs = false;
    bool foundExefs = false;
    bool foundLogo = false;
    for (std::uint32_t i = 0; i < NintendoContentArchiveHeader::cFsCount; ++i) {
        if (mFileSystems[i].fs != nullptr || mHeader.header.fsInfo[i].startSector == 0) {
            // either we already handled this fs or it doesn't exist
            continue;
        }

        printFsInfo(i);

        const auto& header = mHeader.fsHeaders[i];

        auto provider = createProvider(i, baseNCA);
        if (provider != nullptr) {
            switch (header.fsType) {
                case FsType::PartitionFs:
                    mFileSystems[i].fs = std::make_unique<PartitionFileSystem>(std::move(provider), "");
                    break;
                case FsType::RomFs:
                    mFileSystems[i].fs = std::make_unique<RomFileSystem>(std::move(provider));
                    break;
                default:
                    LOG_ERROR("Unknown NCA FS type!");
                    throw std::runtime_error(mName);
            }

            switch (mHeader.header.contentType) {
                case ContentType::Program:
                case ContentType::Data:
                case ContentType::PublicData:
                    if (NX_SUCCEEDED(mFileSystems[i].fs->access("main.npdm", fs::OpenMode::Read))) {
                        if (foundExefs) {
                            LOG_WARNING("Multiple exefs sections found!");
                            fmt::format_to(std::back_inserter(mFileSystems[i].name), "exefs{}", i);
                        } else {
                            mFileSystems[i].name = "exefs";
                            foundExefs = true;
                            verifyNPDMSignature(mFileSystems[i]);
                        }
                    } else if (NX_SUCCEEDED(mFileSystems[i].fs->access("StartupMovie.gif", fs::OpenMode::Read))) {
                        if (foundLogo) {
                            LOG_WARNING("Multiple logo sections found!");
                            fmt::format_to(std::back_inserter(mFileSystems[i].name), "logo{}", i);
                        } else {
                            mFileSystems[i].name = "logo";
                            foundLogo = true;
                        }
                    } else if (header.fsType == FsType::RomFs) {
                        if (foundRomfs) {
                            LOG_WARNING("Multiple romfs sections found!");
                            fmt::format_to(std::back_inserter(mFileSystems[i].name), "romfs{}", i);
                        } else {
                            mFileSystems[i].name = "romfs";
                            foundRomfs = true;
                        }
                    } else {
                        fmt::format_to(std::back_inserter(mFileSystems[i].name), "{}", i);
                    }
                    break;
                case ContentType::Meta:
                case ContentType::Control:
                case ContentType::Manual:
                    fmt::format_to(std::back_inserter(mFileSystems[i].name), "{}", i);
                    break;
                default:
                    LOG_ERROR("Unknown NCA content type!");
                    throw std::runtime_error(mName);
            }
        }
    }
}

auto NintendoContentArchiveFileSystem::verifyNPDMSignature(NCAFileSystem& fs) -> void {
    std::unique_ptr<fs::IFile> file;
    if (NX_SUCCEEDED(fs.fs->openFile(std::addressof(file), "main.npdm", fs::OpenMode::Read))) {
        provider::UniqueProvider provider = std::make_unique<provider::FileProvider>(std::move(file));
        std::uint8_t key[0x100];
        if (GetNCAHeaderKey(provider, key)) {
            if (!crypto::RsaPssVerify(std::addressof(mHeader.header.magic), 0x200, mHeader.header.headerSignatureNpdm, key)) {
                LOG_ERROR("Failed to validate NCA header signature with NPDM key!");
                throw std::runtime_error(mName);
            }
        } else {
            LOG_WARNING("Failed to get NCA header key from main.npdm");
        }
    } else {
        LOG_WARNING("No main.npdm found in exefs section");
    }
}

auto NintendoContentArchiveFileSystem::getAttributes(std::string_view path, fs::DirectoryEntry* entry) const -> Result {
    if (entry == nullptr) {
        return INVALID;
    }

    std::string_view subpath;
    const auto name = fs::FirstComponent(path, std::addressof(subpath));

    if (name.empty() || fs::IsPathSeparator(name[0])) {
        entry->type = fs::Type::Directory;
        entry->createTime = mInitTime;
        return SUCCESS;
    } else if (name == mName && subpath.empty()) {
        entry->type = fs::Type::File;
        entry->createTime = mInitTime;
        entry->fileSize = mProvider->getSize();
        return SUCCESS;
    }

    for (const auto& fs : mFileSystems) {
        if (fs.fs != nullptr) {
            if (name == fs.name) {
                return fs.fs->getAttributes(subpath, entry);
            }
        }
    }

    return NO_FILE;
}

auto NintendoContentArchiveFileSystem::access(std::string_view path, fs::OpenMode mode) const -> Result {
    std::string_view subpath;
    const auto name = fs::FirstComponent(path, std::addressof(subpath));
    if (name.empty() || (name == mName && subpath.empty())) {
        if (mode != fs::OpenMode::Read) {
            return PERMISSION_ERROR;
        }

        return SUCCESS;
    }

    for (const auto& fs : mFileSystems) {
        if (fs.fs != nullptr) {
            if (name == fs.name) {
                return fs.fs->access(subpath, mode);
            }
        }
    }

    return NO_FILE;
}

auto NintendoContentArchiveFileSystem::openFile(std::unique_ptr<fs::IFile>* out, std::string_view path, fs::OpenMode mode) const -> Result {
    if (out == nullptr) {
        return INVALID;
    }

    if (mode != fs::OpenMode::Read) {
        return PERMISSION_ERROR;
    }

    std::string_view subpath;
    const auto name = fs::FirstComponent(path, std::addressof(subpath));

    if (name == mName && subpath.empty()) {
        *out = std::make_unique<NCAFile>(*this);
        return SUCCESS;
    }

    for (const auto& fs : mFileSystems) {
        if (fs.fs != nullptr) {
            if (name == fs.name) {
                return fs.fs->openFile(out, subpath, mode);
            }
        }
    }

    return NO_FILE;
}

auto NintendoContentArchiveFileSystem::openDirectory(std::unique_ptr<fs::IDirectory>* dir, std::string_view path) const -> Result {
    if (dir == nullptr) {
        return INVALID;
    }

    std::string_view subpath;
    const auto name = fs::FirstComponent(path, std::addressof(subpath));
    if (name.empty() || fs::IsPathSeparator(name[0])) {
        *dir = getRoot();
        return SUCCESS;
    }

    for (const auto& fs : mFileSystems) {
        if (fs.fs != nullptr) {
            if (name == fs.name) {
                return fs.fs->openDirectory(dir, subpath);
            }
        }
    }

    return NO_FILE;
}

auto NintendoContentArchiveFileSystem::createProvider(std::size_t fsIndex, NintendoContentArchiveFileSystem* baseNCA) const -> provider::UniqueProvider {
    if (fsIndex >= NintendoContentArchiveHeader::cFsCount) {
        LOG_ERROR("Out of range NCA file system index! {}", fsIndex);
        return nullptr;
    }

    const auto& fsInfo = mHeader.header.fsInfo[fsIndex];
    const auto& fsHeader = mHeader.fsHeaders[fsIndex];

    provider::UniqueProvider out;

    std::uint64_t fsOffset;
    if (fsHeader.sparseInfo.generation != 0) {
        if (fsHeader.metaDataHashDataInfo.size != 0) {
            LOG_WARNING("Unsupported sparse meta hash layer!");
            return nullptr;
        } else {
            LOG_WARNING("Unsupported sparse layer!");
            return nullptr;
        }
    } else {
        const auto span = fsInfo.endSector - fsInfo.startSector;
        if (span == 0) {
            LOG_ERROR("Invalid NCA FS region!");
            return nullptr;
        }
        fsOffset = SectorToOffset(fsInfo.startSector);
        out = std::make_unique<provider::SharedOffsetProvider>(mProvider, fsOffset, SectorToOffset(span));
    }

    provider::SharedProvider aesCtxMeta = nullptr;
    provider::SharedProvider indirectMeta = nullptr;
    if (fsHeader.patchInfo.indirectSize != 0 && fsHeader.metaDataHashDataInfo.size != 0) {
        if (fsHeader.hashType != HashType::HierarchicalIntegrityHash) {
            LOG_ERROR("Invalid hash type for patch meta hash");
            return nullptr;
        }

        if (fsHeader.metaDataHashDataInfo.size != sizeof(MetaDataHashData)) {
            LOG_ERROR("Invalid meta data hash data size");
            return nullptr;
        }

        const auto metaDataHashDataOffset = fsHeader.metaDataHashDataInfo.offset;
        const auto indirectOffset = fsHeader.patchInfo.indirectOffset;
        const auto nonMetaDataHashSize = metaDataHashDataOffset - indirectOffset;
        const auto metaHashSize = common::AlignUp(fsHeader.metaDataHashDataInfo.size, provider::AesCtrProvider::cBlockSize);
        const auto regionSize = metaDataHashDataOffset + metaHashSize - indirectOffset;
        auto encrypted = std::make_unique<provider::SharedOffsetProvider>(mProvider, fsOffset + indirectOffset, regionSize);
        auto decrypted = createAesCtrProvider(std::move(encrypted), fsHeader.aesCtrUpperIv, fsOffset + indirectOffset);

        MetaDataHashData metaDataHashData;
        if (decrypted->read(std::addressof(metaDataHashData), sizeof(metaDataHashData), nonMetaDataHashSize) != sizeof(metaDataHashData)) {
            LOG_ERROR("Failed to read meta data hash data");
            return nullptr;
        }

        if (!crypto::Sha256Verify(std::addressof(metaDataHashData), sizeof(metaDataHashData), fsHeader.metaDataHashDataInfo.hash)) {
            LOG_ERROR("Meta data hash data is corrupted!");
            return nullptr;
        }

        auto meta = std::make_unique<provider::UniqueOffsetProvider>(std::move(decrypted), 0, nonMetaDataHashSize);
        auto integrity = std::make_shared<provider::HierarchicalIntegrityVerificationProvider>(std::move(meta), metaDataHashData.info, metaDataHashData.layerInfoOffset - indirectOffset);
        indirectMeta = std::make_shared<provider::SharedOffsetProvider>(integrity, 0, fsHeader.patchInfo.indirectSize);
        aesCtxMeta = std::make_shared<provider::SharedOffsetProvider>(integrity, fsHeader.patchInfo.aesCtrExOffset - indirectOffset, fsHeader.patchInfo.aesCtrExSize);
    }

    if (fsHeader.patchInfo.aesCtrExSize != 0) {
        out = createAesCtrExProvider(std::move(out), std::move(aesCtxMeta), fsHeader, fsInfo);
    } else {
        out = createDecryptedProvider(std::move(out), fsHeader, fsInfo);
    }

    if (out == nullptr) {
        return nullptr;
    }

    if (fsHeader.patchInfo.indirectSize != 0) {
        auto sharedOut = provider::SharedProvider(std::move(out));
        if (indirectMeta == nullptr) {
            indirectMeta = std::make_shared<provider::SharedOffsetProvider>(sharedOut, fsHeader.patchInfo.indirectOffset, fsHeader.patchInfo.indirectSize);
        }

        out = createIndirectProvider(std::move(sharedOut), std::move(indirectMeta), baseNCA, fsIndex, fsHeader.patchInfo);
        if (out == nullptr) {
            return nullptr;
        }
    }

    switch (fsHeader.hashType) {
        case HashType::Auto: LOG_WARNING("Unsupported Case HashType::Auto"); break;
        case HashType::None: LOG_WARNING("Unsupported Case HashType::None"); break;
        case HashType::HierarchicalSha256Hash:
            out = createSha256Provider(std::move(out), fsHeader.hashData.hierarchicalSha256HashData);
            break;
        case HashType::HierarchicalIntegrityHash:
            out = std::make_unique<provider::HierarchicalIntegrityVerificationProvider>(std::move(out), fsHeader.hashData.integrityMetaInfo, 0);
            break;
        case HashType::AutoSha3: LOG_WARNING("Unsupported Case HashType::AutoSha3"); break;
        case HashType::HierarchicalSha3256Hash: LOG_WARNING("Unsupported Case HashType::HierarchicalSha3256Hash"); break;
        case HashType::HierarchicalIntegritySha3Hash: LOG_WARNING("Unsupported Case HashType::HierarchicalIntegritySha3Hash"); break;
        default: LOG_ERROR("Unknown hash type!"); return nullptr;
    }

    if (fsHeader.compressionInfo.bucket.offset != 0 && fsHeader.compressionInfo.bucket.size != 0) {
        LOG_WARNING("Unsupported compression layer");
        return nullptr;
    }

    return out;
}

auto NintendoContentArchiveFileSystem::createAesCtrExProvider(
    provider::UniqueProvider dataProvider, provider::SharedProvider metaProvider, const FsHeader& header, const FsInfo& info
) const -> provider::UniqueProvider {
    const auto fsOffset = SectorToOffset(info.startSector);
    const auto size = common::AlignUp(header.patchInfo.aesCtrExSize, provider::AesCtrExProvider::cBlockSize);
    if (header.patchInfo.aesCtrExOffset + size > dataProvider->getSize()) {
        LOG_ERROR("Invalid AesCtrEx size");
        return nullptr;
    }

    if (metaProvider == nullptr) {
        if (header.encryptionType != EncryptionType::None) {
            auto aesCtxMetaBase = std::make_unique<provider::SharedOffsetProvider>(mProvider, fsOffset + header.patchInfo.aesCtrExOffset, size);
            metaProvider = createAesCtrProvider(std::move(aesCtxMetaBase), header.aesCtrUpperIv, fsOffset + header.patchInfo.aesCtrExOffset);
        } else {
            metaProvider = std::make_unique<provider::SharedOffsetProvider>(mProvider, fsOffset + header.patchInfo.aesCtrExOffset, size);
        }
    }

    const auto offsetNodeSize = BucketTree::GetOffsetNodeCount(
        provider::AesCtrExProvider::cNodeSize, provider::AesCtrExProvider::cEntrySize, header.patchInfo.aesCtrExHeader.entryCount
    ) * provider::AesCtrExProvider::cNodeSize;
    const auto entryNodeSize = BucketTree::GetEntryNodeCount(
        provider::AesCtrExProvider::cNodeSize, provider::AesCtrExProvider::cEntrySize, header.patchInfo.aesCtrExHeader.entryCount
    ) * provider::AesCtrExProvider::cNodeSize;
    
    auto nodeProvider = std::make_unique<provider::MemoryStreamProvider>(*metaProvider, offsetNodeSize, 0);
    auto entryProvider = std::make_unique<provider::MemoryStreamProvider>(*metaProvider, entryNodeSize, offsetNodeSize);
    auto aesCtrExProvider = std::make_unique<provider::AesCtrExProvider>(
        std::move(dataProvider), getDecryptionKey(DecryptionKey_AesCtrEx), header.aesCtrUpperIv.secureValue, fsOffset,
        header.patchInfo.aesCtrExHeader.entryCount, std::move(nodeProvider), std::move(entryProvider)
    );

    const auto blockSize = (std::min)(aesCtrExProvider->getSize(), std::size_t(0x1000));
    const auto cacheSize = (std::min)(std::size_t(32), (aesCtrExProvider->getSize() + blockSize - 1) / blockSize);
    auto cacheProvider = std::make_unique<provider::CacheProvider<>>(std::move(aesCtrExProvider), blockSize, cacheSize);

    return std::make_unique<provider::AlignedProvider<provider::AesCtrExProvider::cBlockSize>>(std::move(cacheProvider));
}

auto NintendoContentArchiveFileSystem::createAesCtrProvider(provider::UniqueProvider provider, const AesCtrUpperIv& upperIv, std::size_t offset) const -> provider::UniqueProvider {
    auto aesCtrProvider = std::make_unique<provider::AesCtrProvider>(std::move(provider), getDecryptionKey(DecryptionKey_AesCtr), upperIv, offset);

    const auto blockSize = common::AlignUp((std::min)(aesCtrProvider->getSize(), std::size_t(0x800)), provider::AesCtrProvider::cBlockSize);
    const auto cacheSize = (std::min)(std::size_t(16), (aesCtrProvider->getSize() + blockSize - 1) / blockSize);
    auto cacheProvider = std::make_unique<provider::CacheProvider<>>(std::move(aesCtrProvider), blockSize, cacheSize);

    return std::make_unique<provider::AlignedProvider<provider::AesCtrProvider::cBlockSize>>(std::move(cacheProvider));
}

auto NintendoContentArchiveFileSystem::createDecryptedProvider(provider::UniqueProvider provider, const FsHeader& header, const FsInfo& info) const -> provider::UniqueProvider {
    switch (header.encryptionType) {
        case EncryptionType::None: return provider;
        case EncryptionType::AesXts: LOG_WARNING("Unsupported Case EncryptionType::AesXts"); return nullptr;
        case EncryptionType::AesCtr:
            return createAesCtrProvider(std::move(provider), header.aesCtrUpperIv, SectorToOffset(info.startSector));
        case EncryptionType::AesCtrSkipLayerHash: LOG_WARNING("Unsupported Case EncryptionType::AesCtrSkipLayerHash"); return nullptr;
        default: LOG_ERROR("Invalid encryption type!"); return nullptr;
    }
}

auto NintendoContentArchiveFileSystem::createIndirectProvider(
    provider::SharedProvider dataProvider, provider::SharedProvider metaProvider, NintendoContentArchiveFileSystem* baseNCA, std::size_t fsIndex, const PatchInfo& patchInfo
) const -> provider::UniqueProvider {
    if (baseNCA != nullptr) {
        const auto& baseFs = baseNCA->mFileSystems[fsIndex];
        if (baseFs.fs == nullptr) {
            LOG_ERROR("Base NCA is missing the corresponding file system!");
            return nullptr;
        }

        provider::UniqueProvider base;
        std::size_t baseFsOffset;
        const auto& baseFsHeader = baseNCA->mHeader.fsHeaders[fsIndex];
        const auto& baseFsInfo = baseNCA->mHeader.header.fsInfo[fsIndex];
        if (baseFsHeader.sparseInfo.generation != 0 ) {
            LOG_WARNING("Unsupported sparse layer!");
            return nullptr;
        } else {
            const auto span = baseFsInfo.endSector - baseFsInfo.startSector;
            if (span == 0) {
                LOG_ERROR("Invalid NCA FS region!");
                return nullptr;
            }
            baseFsOffset = SectorToOffset(baseFsInfo.startSector);
            base = std::make_unique<provider::SharedOffsetProvider>(baseNCA->mProvider, baseFsOffset, SectorToOffset(span));
        }

        base = baseNCA->createDecryptedProvider(std::move(base), baseFsHeader, baseFsInfo);
        if (base == nullptr) {
            return nullptr;
        }

        const auto offsetNodeSize = BucketTree::GetOffsetNodeCount(
            provider::IndirectProvider::cNodeSize, provider::IndirectProvider::cEntrySize, patchInfo.indirectHeader.entryCount
        ) * provider::IndirectProvider::cNodeSize;
        const auto entryNodeSize = BucketTree::GetEntryNodeCount(
            provider::IndirectProvider::cNodeSize, provider::IndirectProvider::cEntrySize, patchInfo.indirectHeader.entryCount
        ) * provider::IndirectProvider::cNodeSize;

        auto nodeProvider = std::make_unique<provider::MemoryStreamProvider>(*metaProvider, offsetNodeSize, 0);
        auto entryProvider = std::make_unique<provider::MemoryStreamProvider>(*metaProvider, entryNodeSize, offsetNodeSize);

        return std::make_unique<provider::IndirectProvider>(
            std::move(base), std::move(dataProvider), patchInfo.indirectHeader.entryCount,
            std::move(nodeProvider), std::move(entryProvider)
        );
    } else {
        LOG_INFO("No base NCA for indirect layer, skipping for now");
        return nullptr;
    }
}

auto NintendoContentArchiveFileSystem::createSha256Provider(provider::UniqueProvider provider, const HashData::HierarchicalSha256HashData& hashData) const -> provider::UniqueProvider {
    auto shaProvider = std::make_unique<provider::Sha256Provider>(std::move(provider), hashData);

    const auto cacheSize = (std::min)(std::size_t(32), (shaProvider->getSize() + hashData.blockSize - 1) / hashData.blockSize);
    auto cacheProvider = std::make_unique<provider::CacheProvider<>>(std::move(shaProvider), hashData.blockSize, cacheSize);

    return std::make_unique<provider::AlignedProvider<provider::DYNAMIC>>(std::move(cacheProvider), hashData.blockSize);
}

auto NintendoContentArchiveFileSystem::getDecryptionKey(DecryptionKey type) const -> const std::uint8_t(&)[0x10] {
    static constinit const std::uint8_t cNullKey[0x10] = { 0 };

    if (crypto::IsNull(mHeader.header.rightsId)) {
        if (type < DecryptionKey_Count) {
            return mDecryptedKeys[type];
        } else {
            return cNullKey;
        }
    } else {
        return mTitleKey;
    }
}

auto NintendoContentArchiveFileSystem::NCAFile::read(void* dst, std::size_t size, std::size_t offset) const -> std::size_t {
    if (size == 0 || offset >= getSize()) {
        return 0;
    }

    if (offset + size > getSize()) {
        size = getSize() - offset;
    }

    return mParentFileSystem.mProvider->read(dst, size, offset);
}

auto NintendoContentArchiveFileSystem::Directory::read(std::size_t* entryCount, fs::DirectoryEntry* entries, std::size_t maxEntries, std::size_t offset) const -> Result {
    if (entries == nullptr) {
        return INVALID;
    }

    std::size_t count = 0;
    for (const auto& fs : mParentFileSystem.mFileSystems) {
        if (fs.fs != nullptr) {
            ++count;
        }
    }

    if (offset >= count + 1) {
        if (entryCount != nullptr) {
            *entryCount = 0;
        }
        return SUCCESS;
    }

    std::size_t i = 0, entriesRead = 0;
    for (const auto& fs : mParentFileSystem.mFileSystems) {
        if (fs.fs != nullptr) {
            if (i++ < offset) {
                continue;
            }
            entries[entriesRead].name = fs.name;
            entries[entriesRead].type = fs::Type::Directory;
            entries[entriesRead].createTime = 0;
            entries[entriesRead++].fileSize = 0;
            if (entriesRead >= maxEntries) {
                break;
            }
        }
    }

    if (entriesRead < maxEntries && i >= offset) {
        entries[entriesRead].name = mParentFileSystem.mName;
        entries[entriesRead].type = fs::Type::File;
        entries[entriesRead].createTime = 0;
        entries[entriesRead++].fileSize = mParentFileSystem.mProvider->getSize();
    }

    if (entryCount != nullptr) {
        *entryCount = entriesRead;
    }
    return SUCCESS;
}

} // namespace nxmount::formats