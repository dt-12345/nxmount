#include "crypto/crypto.hpp"
#include "log/logging.hpp"

#include <aes.h>
#include <cmac.h>
#include <modes.h>
#include <pssr.h>
#include <rsa.h>
#include <sha.h>
#include <xts.h>

namespace nxmount::crypto {

auto AesEcbDecrypt(void* dst, const void* src, std::size_t size, const void* key, std::size_t keySize) -> bool {
    try {
        CryptoPP::ECB_Mode<CryptoPP::AES>::Decryption d;
        d.SetKey(static_cast<const std::uint8_t*>(key), keySize);
        d.ProcessData(static_cast<std::uint8_t*>(dst), static_cast<const std::uint8_t*>(src), size);
    } catch (const CryptoPP::Exception& e) {
        LOG_ERROR("AES ECB decryption failed! {}", e.what());
        return false;
    }
    return true;
}

auto AesCtrDecrypt(void* dst, const void* src, std::size_t size, const void* key, std::size_t keySize, const void* iv, std::size_t offset) -> bool {
    try {
        CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption d;
        d.SetKeyWithIV(static_cast<const std::uint8_t*>(key), keySize, static_cast<const std::uint8_t*>(iv));
        if (offset > 0) {
            d.Seek(offset);
        }
        d.ProcessData(static_cast<std::uint8_t*>(dst), static_cast<const std::uint8_t*>(src), size);
    } catch (const CryptoPP::Exception& e) {
        LOG_ERROR("AES CTR decryption failed! {}", e.what());
        return false;
    }
    return true;
}

static auto GetTweak(std::uint8_t* tweak, std::size_t sector) -> void {
    for (std::int32_t i = 0xf; i >= 0; --i) {
        tweak[i] = static_cast<std::uint8_t>(sector & 0xff);
        sector >>= 8;
    }
}

auto AesXtsDecrypt(void* dst, const void* src, std::size_t size, const void* key, std::size_t keySize, std::size_t sector, std::size_t sectorSize) -> bool {
    try {
        if (size % sectorSize != 0) {
            LOG_ERROR("Source size is not a multiple of the sector size!");
            return false;
        }

        CryptoPP::XTS<CryptoPP::AES>::Decryption d;
        std::uint8_t tweak[0x10];

        for (std::size_t i = 0; i < size; i += sectorSize) {
            GetTweak(tweak, sector++);
            d.SetKeyWithIV(static_cast<const std::uint8_t*>(key), keySize, tweak);
            d.ProcessData(static_cast<std::uint8_t*>(dst) + i, static_cast<const std::uint8_t*>(src) + i, sectorSize);
        }
    } catch (const CryptoPP::Exception& e) {
        LOG_ERROR("AES XTS decryption failed! {}", e.what());
        return false;
    }
    return true;
}

auto AesCbcDecrypt(void* dst, const void* src, std::size_t size, const void* key, std::size_t keySize, const void* iv) -> bool {
    try {
        CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption d;
        d.SetKeyWithIV(static_cast<const std::uint8_t*>(key), keySize, static_cast<const std::uint8_t*>(iv));
        d.ProcessData(static_cast<std::uint8_t*>(dst), static_cast<const std::uint8_t*>(src), size);
    } catch (const CryptoPP::Exception& e) {
        LOG_ERROR("AES CBC decryption failed! {}", e.what());
        return false;
    }
    return true;
}

auto AesCalculateCmac(void* dst, const void* src, std::size_t size, const void* key, std::size_t keySize) -> bool {
    try {
        CryptoPP::CMAC<CryptoPP::AES> cmac(static_cast<const std::uint8_t*>(key), keySize);
        cmac.Update(static_cast<const std::uint8_t*>(src), size);
        cmac.Final(static_cast<std::uint8_t*>(dst));
    } catch (const CryptoPP::Exception& e) {
        LOG_ERROR("AES CMAC calculation failed! {}", e.what());
        return false;
    }
    return true;
}

auto RsaPkcs1Verify(const void* data, std::size_t size, const void* signature, const void* modulus, std::size_t keySize) -> bool {
    try {
        const std::uint8_t exponent[3] = { 1, 0, 1 };
        auto pubKey = CryptoPP::RSA::PublicKey();
        pubKey.Initialize(
            CryptoPP::Integer(static_cast<const std::uint8_t*>(modulus), keySize),
            CryptoPP::Integer(exponent, sizeof(exponent))
        );

        auto verifier = CryptoPP::RSASSA_PKCS1v15_SHA256_Verifier(pubKey);
        return verifier.VerifyMessage(
            static_cast<const std::uint8_t*>(data), size,
            static_cast<const std::uint8_t*>(signature), keySize
        );
    } catch (const CryptoPP::Exception& e) {
        LOG_ERROR("RSA-PKCS1 error: {}", e.what());
        return false;
    }
}

static auto Mfg1AndXor(std::uint8_t* dst, std::size_t dstSize, const void* src, std::size_t srcSize) -> void {
    std::uint8_t data[0x100] = { 0 };
    std::memcpy(data, src, srcSize);

    std::size_t offset = 0;
    std::uint32_t seed = 0;
    while (offset < dstSize) {
        for (std::uint32_t i = 0; i < sizeof(seed); ++i) {
            data[srcSize + 3 - i] = (seed >> (i * 8)) & 0xff;
        }
        auto h = CryptoPP::SHA256();
        h.Update(data, srcSize + 4);
        std::uint8_t hash[CryptoPP::SHA256::DIGESTSIZE];
        h.Final(hash);
        for (std::size_t i = offset; i < dstSize && i < offset + 0x20; ++i) {
            dst[i] ^= hash[i - offset];
        }
        ++seed; offset += 0x20;
    }
}

auto RsaOaepDecryptVerify(void* dst, std::size_t dstSize, const void* signature, const void* modulus, const void* exponent, std::size_t exponentSize, const void* labelHash, std::size_t* outSize) -> bool {
    try {
        auto pubKey = CryptoPP::RSA::PublicKey();
        pubKey.Initialize(
            CryptoPP::Integer(static_cast<const std::uint8_t*>(modulus), 0x100),
            CryptoPP::Integer(static_cast<const std::uint8_t*>(exponent), exponentSize)
        );

        const auto msg = CryptoPP::Integer(static_cast<const std::uint8_t*>(signature), 0x100);
        auto decrypted = pubKey.ApplyFunction(msg);

        if (decrypted.GetByte(0) != 0) {
            return false;
        }

        const auto size = decrypted.MinEncodedSize();
        if (size >= dstSize) {
            return false;
        }

        auto buf = static_cast<std::uint8_t*>(dst);
        decrypted.Encode(buf, size);

        Mfg1AndXor(buf + 1, 0x20, buf + 0x21, 0x100 - 0x20 - 1);
        Mfg1AndXor(buf + 0x21, 0x100 - 0x20 - 1, buf + 1, 0x20);

        if (std::memcmp(buf + 0x21, labelHash, 0x20) != 0) {
            return false;
        }

        const std::uint8_t* data = buf + 0x21 + 0x20;
        std::size_t remaining = 0x100 - 0x20 - 1 - 0x20;
        while (*data == 0 && remaining) { ++data; --remaining; }
        if (remaining-- == 0 || *data++ != 1) {
            return false;
        }
        if (outSize != nullptr) {
            *outSize = remaining;
        }

        if (remaining > dstSize) {
            remaining = dstSize;
        }
        std::memcpy(dst, data, dstSize);
        return true;
    } catch (const CryptoPP::Exception& e) {
        LOG_ERROR("RSA-OAEP error: {}", e.what());
        return false;
    }
}

auto RsaPssVerify(const void* src, std::size_t srcSize, const void* signature, const void* modulus) -> bool {
    try {
        const std::uint8_t exponent[3] = { 1, 0, 1 };
        auto pubKey = CryptoPP::RSA::PublicKey();
        pubKey.Initialize(
            CryptoPP::Integer(static_cast<const std::uint8_t*>(modulus), 0x100),
            CryptoPP::Integer(static_cast<const std::uint8_t*>(exponent), sizeof(exponent))
        );

        const auto msg = CryptoPP::Integer(static_cast<const std::uint8_t*>(signature), 0x100);
        auto decrypted = pubKey.ApplyFunction(msg);

        std::uint8_t buf[0x100];
        decrypted.Encode(buf, sizeof(buf));

        if (buf[0xff] != 0xbc) {
            return false;
        }

        std::uint8_t hashBuf[0x24] = { 0 };
        std::memcpy(hashBuf, buf + 0x100 - 0x20 - 1, 0x20);

        Mfg1AndXor(buf, 0x100 - 0x20 - 1, hashBuf, 0x20);

        buf[0] &= 0x7f;

        for (std::uint32_t i = 0; i < 0x100 - 0x20 - 0x20 - 1 - 1; ++i) {
            if (buf[i] != 0) {
                return false;
            }
        }

        if (buf[0x100 - 0x20 - 0x20 - 1 - 1] != 1) {
            return false;
        }

        std::uint8_t hash[CryptoPP::SHA256::DIGESTSIZE];
        std::uint8_t validateBuf[8 + 0x20 + 0x20] = { 0 };
        {
            auto sha = CryptoPP::SHA256();
            sha.Update(static_cast<const std::uint8_t*>(src), srcSize);
            sha.Final(validateBuf + 8);
        }
        std::memcpy(validateBuf + 0x28, buf + 0x100 - 0x20 - 0x20 - 1, 0x20);
        {
            auto sha = CryptoPP::SHA256();
            sha.Update(validateBuf, sizeof(validateBuf));
            sha.Final(hash);
        }
        return std::memcmp(hashBuf, hash, 0x20) == 0;
    } catch (const CryptoPP::Exception& e) {
        LOG_ERROR("RSA-PSS error: {}", e.what());
        return false;
    }
}

auto Sha256(const void* data, std::size_t size, void* hash) -> void {
    auto sha = CryptoPP::SHA256();
    sha.Update(static_cast<const std::uint8_t*>(data), size);
    sha.Final(static_cast<std::uint8_t*>(hash));
}

auto Sha256Verify(const void* data, std::size_t size, const void* hash) -> bool {
    std::uint8_t h[CryptoPP::SHA256::DIGESTSIZE];

    auto sha = CryptoPP::SHA256();
    sha.Update(static_cast<const std::uint8_t*>(data), size);
    sha.Final(h);

    return std::memcmp(h, hash, sizeof(h)) == 0;
}

} // namespace nxmount::crypto