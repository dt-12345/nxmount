#pragma once

#include "log/logging.hpp"

#include <aes.h>
#include <modes.h>

#include <atomic>
#include <array>
#include <cstddef>

namespace nxmount::crypto {

class IDecryptor {
public:
    virtual ~IDecryptor() = default;

    virtual auto setKey(const void* key, std::size_t size, const void* iv) -> void = 0;
    virtual auto setIV(const void* iv, std::size_t size, std::size_t offset = 0) -> void = 0;
    virtual auto decrypt(void* dst, const void* src, std::size_t size) -> bool = 0;
};

class AesCtrDecryptor final : public IDecryptor {
public:
    ~AesCtrDecryptor() override = default;

    auto setKey(const void* key, std::size_t size, const void* iv) -> void override {
        mImpl.SetKeyWithIV(static_cast<const std::uint8_t*>(key), size, static_cast<const std::uint8_t*>(iv));
    }

    auto setIV(const void* iv, std::size_t size, std::size_t offset = 0) -> void override {
        mImpl.Resynchronize(static_cast<const std::uint8_t*>(iv), static_cast<std::int32_t>(size));
        if (offset > 0) {
            mImpl.Seek(offset);
        }
    }

    auto decrypt(void* dst, const void* src, std::size_t size) -> bool override {
        try {
            mImpl.ProcessData(static_cast<std::uint8_t*>(dst), static_cast<const std::uint8_t*>(src), size);
            return true;
        } catch (const CryptoPP::Exception& e) {
            LOG_ERROR("AES-CTR decryption failed: {}", e.what());
            return false;
        }
    }

private:
    CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption mImpl;
};

template <typename T, std::size_t PoolSize> requires (std::is_base_of_v<IDecryptor, T> && PoolSize > 0)
class DecryptorPool {
public:
    DecryptorPool() = default;

    auto initialize(const void* key, std::size_t size, const void* iv) -> void {
        for (std::size_t i = 0; i < PoolSize; ++i) {
            mPool[i].setKey(key, size, iv);
            mFree[i] = std::addressof(mPool[i]);
        }
    }

    struct ScopedDecryptor {
        DecryptorPool* pool;
        T* ptr;

        ScopedDecryptor(DecryptorPool* pool_) : pool(pool_) {
            ptr = pool->tryAcquire();
        }

        ~ScopedDecryptor() {
            if (pool != nullptr && ptr != nullptr) {
                pool->free(ptr);
            }
        }

        constexpr auto operator->() const -> T* {
            return ptr;
        }

        constexpr operator bool() const noexcept {
            return pool != nullptr && ptr != nullptr;
        }
    };

    auto acquire() -> ScopedDecryptor {
        return ScopedDecryptor(this);
    }

    auto tryAcquire() -> T* {
        for (auto& free : mFree) {
            if (T* decryptor = free; decryptor != nullptr) {
                while (!free.compare_exchange_strong(decryptor, nullptr, std::memory_order_release)) { /* ... */ }
                return decryptor;
            }
        }
        return nullptr;
    }

    auto free(T* decryptor) -> void {
        for (std::size_t i = 0; i < PoolSize; ++i) {
            if (std::addressof(mPool[i]) == decryptor) {
                mFree[i] = decryptor;
            }
        }
    }

private:
    std::array<T, PoolSize> mPool;
    std::array<std::atomic<T*>, PoolSize> mFree;
};

auto AesEcbDecrypt(void* dst, const void* src, std::size_t size, const void* key, std::size_t keySize) -> bool;
auto AesCtrDecrypt(void* dst, const void* src, std::size_t size, const void* key, std::size_t keySize, const void* iv, std::size_t offset = 0) -> bool;
auto AesXtsDecrypt(void* dst, const void* src, std::size_t size, const void* key, std::size_t keySize, std::size_t sector, std::size_t sectorSize) -> bool;
auto AesCbcDecrypt(void* dst, const void* src, std::size_t size, const void* key, std::size_t keySize, const void* iv) -> bool;
auto AesCalculateCmac(void* dst, const void* src, std::size_t size, const void* key, std::size_t keySize) -> bool;

auto RsaPkcs1Verify(const void* data, std::size_t size, const void* signature, const void* modulus, std::size_t keySize) -> bool;
auto RsaPssVerify(const void* src, std::size_t srcSize, const void* signature, const void* modulus) -> bool;
auto RsaOaepDecryptVerify(
    void* dst, std::size_t dstSize,
    const void* signature,
    const void* modulus,
    const void* exponent, std::size_t exponentSize,
    const void* labelHash,
    std::size_t* outSize
) -> bool;

auto Sha256(const void* data, std::size_t size, void* hash) -> void;
auto Sha256Verify(const void* data, std::size_t size, const void* hash) -> bool;

} // namespace nxmount::crypto