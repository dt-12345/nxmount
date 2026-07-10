#pragma once

#include <cstddef>

namespace nxmount::crypto {

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