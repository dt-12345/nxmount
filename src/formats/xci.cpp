#include "crypto/crypto.hpp"
#include "crypto/key_mgr.hpp"
#include "crypto/key_utils.hpp"
#include "log/logging.hpp"
#include "formats/xci.hpp"
#include "provider/offset_provider.hpp"
#include "provider/provider.hpp"

#include <sha.h>

#include <stdexcept>

namespace nxmount::formats {

static constinit const unsigned char sXCIHeaderPublicKey[0x100] = {
    0x98, 0xC7, 0x26, 0xB6, 0x0D, 0x0A, 0x50, 0xA7, 0x39, 0x21, 0x0A, 0xE3, 0x2F, 0xE4, 0x3E, 0x2E,
    0x5B, 0xA2, 0x86, 0x75, 0xAA, 0x5C, 0xEE, 0x34, 0xF1, 0xA3, 0x3A, 0x7E, 0xBD, 0x90, 0x4E, 0xF7,
    0x8D, 0xFA, 0x17, 0xAA, 0x6B, 0xC6, 0x36, 0x6D, 0x4C, 0x9A, 0x6D, 0x57, 0x2F, 0x80, 0xA2, 0xBC,
    0x38, 0x4D, 0xDA, 0x99, 0xA1, 0xD8, 0xC3, 0xE2, 0x99, 0x79, 0x36, 0x71, 0x90, 0x20, 0x25, 0x9D,
    0x4D, 0x11, 0xB8, 0x2E, 0x63, 0x6B, 0x5A, 0xFA, 0x1E, 0x9C, 0x04, 0xD1, 0xC5, 0xF0, 0x9C, 0xB1,
    0x0F, 0xB8, 0xC1, 0x7B, 0xBF, 0xE8, 0xB0, 0xD2, 0x2B, 0x47, 0x01, 0x22, 0x6B, 0x23, 0xC9, 0xD0,
    0xBC, 0xEB, 0x75, 0x6E, 0x41, 0x7D, 0x4C, 0x26, 0xA4, 0x73, 0x21, 0xB4, 0xF0, 0x14, 0xE5, 0xD9,
    0x8D, 0xB3, 0x64, 0xEE, 0xA8, 0xFA, 0x84, 0x1B, 0xB8, 0xB8, 0x7C, 0x88, 0x6B, 0xEF, 0xCC, 0x97,
    0x04, 0x04, 0x9A, 0x67, 0x2F, 0xDF, 0xEC, 0x0D, 0xB2, 0x5F, 0xB5, 0xB2, 0xBD, 0xB5, 0x4B, 0xDE,
    0x0E, 0x88, 0xA3, 0xBA, 0xD1, 0xB4, 0xE0, 0x91, 0x81, 0xA7, 0x84, 0xEB, 0x77, 0x85, 0x8B, 0xEF,
    0xA5, 0xE3, 0x27, 0xB2, 0xF2, 0x82, 0x2B, 0x29, 0xF1, 0x75, 0x2D, 0xCE, 0xCC, 0xAE, 0x9B, 0x8D,
    0xED, 0x5C, 0xF1, 0x8E, 0xDB, 0x9A, 0xD7, 0xAF, 0x42, 0x14, 0x52, 0xCD, 0xE3, 0xC5, 0xDD, 0xCE,
    0x08, 0x12, 0x17, 0xD0, 0x7F, 0x1A, 0xAA, 0x1F, 0x7D, 0xE0, 0x93, 0x54, 0xC8, 0xBC, 0x73, 0x8A,
    0xCB, 0xAD, 0x6E, 0x93, 0xE2, 0x19, 0x72, 0x6B, 0xD3, 0x45, 0xF8, 0x73, 0x3D, 0x2B, 0x6A, 0x55,
    0xD2, 0x3A, 0x8B, 0xB0, 0x8A, 0x42, 0xE3, 0x3D, 0xF1, 0x92, 0x23, 0x42, 0x2E, 0xBA, 0xCC, 0x9C,
    0x9A, 0xC1, 0xDD, 0x62, 0x86, 0x9C, 0x2E, 0xE1, 0x2D, 0x6F, 0x62, 0x67, 0x51, 0x08, 0x0E, 0xCF
};

[[nodiscard]] static auto CheckHeaderHash(const void* data, std::size_t size, const void* hash, const std::uint8_t* suffix) {
    auto sha = CryptoPP::SHA256();
    sha.Update(static_cast<const std::uint8_t*>(data), size);
    if (suffix) {
        sha.Update(suffix, sizeof(*suffix));
    }
    std::uint8_t digest[CryptoPP::SHA256::DIGESTSIZE];
    sha.Final(digest);
    return std::memcmp(digest, hash, sizeof(digest)) == 0;
}

GameCardFileSystem::GameCardFileSystem(provider::UniqueProvider provider, std::string_view name) {
    GameCardImageHeader header;
    if (provider->read(std::addressof(header), sizeof(header), 0) != sizeof(header)) {
        LOG_ERROR("Failed to read XCI header!");
        throw std::runtime_error("GameCardFileSystem");
    }

    if (header.magic != GameCardImageHeader::cMagic) {
        LOG_ERROR("Invalid XCI header magic!");
        throw std::runtime_error("GameCardFileSystem");
    }

    if (!crypto::RsaPkcs1Verify(
        std::addressof(header.magic), sizeof(header) - sizeof(header.headerSignature), header.headerSignature,
        sXCIHeaderPublicKey, sizeof(sXCIHeaderPublicKey)
    )) {
        LOG_ERROR("XCI header is corrupted!");
        throw std::runtime_error("GameCardFileSystem");
    }

    bool decryptedHeader = false;
    if (const auto& key = crypto::KeyManager::instance()->getKeySet().xci_header_key; !crypto::IsNull(key)) {
        std::uint8_t iv[0x10];
        for (std::size_t i = 0; i < sizeof(iv); ++i) {
            iv[i] = header.iv[0xf - i];
        }

        decryptedHeader = crypto::AesCbcDecrypt(
            std::addressof(header.encryptedData), std::addressof(header.encryptedData), sizeof(header.encryptedData), key, sizeof(key), iv
        );
    }

    {
        auto data = std::vector<std::uint8_t>(header.partitionFsHeaderSize);
        if (provider->read(data.data(), data.size(), header.partitionFsHeaderAddress) != data.size()) {
            LOG_ERROR("Failed to read XCI partition file system header!");
            throw std::runtime_error("GameCardFileSystem");
        }
        if (decryptedHeader) {
            if (!CheckHeaderHash(
                data.data(), data.size(), header.partitionFsHeaderSha256Digest,
                header.encryptedData.compatType == CompatibilityType::Terra
                    ? reinterpret_cast<const std::uint8_t*>(std::addressof(header.encryptedData.compatType))
                    : nullptr
            )) {
                LOG_ERROR("XCI partition file system header is corrupted!");
                throw std::runtime_error("GameCardFileSystem");
            }
        } else {
            if (!CheckHeaderHash(data.data(), data.size(), header.partitionFsHeaderSha256Digest, nullptr)) {
                const auto suffix = std::to_underlying(CompatibilityType::Terra);
                if (!CheckHeaderHash(data.data(), data.size(), header.partitionFsHeaderSha256Digest, std::addressof(suffix))) {
                    LOG_ERROR("XCI partition file system header is corrupted!");
                    throw std::runtime_error("GameCardFileSystem");
                }
            }
        }
    }

    mFileSystem = std::make_unique<RootSha256FileSystem>(std::make_unique<provider::SharedOffsetProvider>(std::move(provider), header.partitionFsHeaderAddress), name);
}

} // namespace nxmount::formats