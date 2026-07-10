#pragma once

#include "common/utils.hpp"
#include "provider/provider.hpp"

namespace nxmount::formats {

struct MetaHeader {
    static constexpr const auto cMagic = common::MakeMagic("META");

    std::uint32_t magic;
    std::uint32_t keyGeneration;
    std::uint32_t reserved0;
    std::uint8_t flags;
    std::uint8_t reserved1;
    std::uint8_t mainThreadPriority;
    std::uint8_t mainThreadCoreNumber;
    std::uint32_t reserved2;
    std::uint32_t systemResourceSize;
    std::uint32_t version;
    std::uint32_t mainThreadStackSize;
    char name[0x10];
    std::uint8_t productCode[0x10];
    std::uint8_t reserved3[0x30];
    std::uint32_t aciOffset;
    std::uint32_t aciSize;
    std::uint32_t acidOffset;
    std::uint32_t acidSize;
};
static_assert(sizeof(MetaHeader) == 0x80);

struct Acid {
    static constexpr const auto cMagic = common::MakeMagic("ACID");

    std::uint8_t signature[0x100];
    std::uint8_t modulus[0x100]; // NCA header key
    std::uint32_t magic;
    std::uint32_t size;
    std::uint8_t version;
    std::uint8_t _209;
    std::uint16_t reserved0;
    std::uint32_t flags;
    std::uint64_t programIdMin;
    std::uint64_t programIdMax;
    std::uint32_t facOffset;
    std::uint32_t facSize;
    std::uint32_t sacOffset;
    std::uint32_t sacSize;
    std::uint32_t kcOffset;
    std::uint32_t kcSize;
    std::uint64_t reserved1;
};
static_assert(sizeof(Acid) == 0x240);

auto GetNCAHeaderKey(provider::UniqueProvider& provider, std::uint8_t* key) -> bool;

} // namespace nxmount::formats