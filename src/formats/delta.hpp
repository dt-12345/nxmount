#pragma once

#include "common/bitutils.hpp"
#include "common/utils.hpp"

// TODO: support applying delta patches?

namespace nxmount::formats {

struct DeltaHeader {
    static constexpr const auto cMagic = common::MakeMagic("NDV0");

    std::uint32_t magic;
    std::uint32_t reserve0;
    std::uint64_t srcSize;
    std::uint64_t dstSize;
    std::uint64_t headerSize;
    std::uint64_t dataSize;
    std::uint8_t reserve1[0x18];
};
static_assert(sizeof(DeltaHeader) == 0x40);

enum class SizeType : std::int8_t {
    U8      = 0,
    U16     = 1,
    U24     = 2,
    U32     = 3,

    None    = -1,
};

union DeltaSegmentHeader {
    common::BitRange<0, 3, SizeType> srcSize;
    common::BitRange<3, 3, SizeType> dstSize;
};
static_assert(sizeof(DeltaSegmentHeader) == 1);

} // namespace nxmount::formats