#pragma once

#include <climits>
#include <cstdint>
#include <type_traits>

namespace nxmount::common {

template <std::size_t SIZE>
constexpr std::int64_t SEXT(std::uint64_t value) {
    static_assert(SIZE < sizeof(std::uint64_t) * CHAR_BIT, "SIZE must fit within a 64-bit integer");
    constexpr std::size_t nbits = sizeof(std::uint64_t) * CHAR_BIT;
    return static_cast<std::int64_t>(value << (nbits - SIZE)) >> (nbits - SIZE);
}

template <std::size_t SIZE>
constexpr std::int32_t SEXT(std::uint32_t value) {
    static_assert(SIZE < sizeof(std::uint32_t) * CHAR_BIT, "SIZE must fit within a 32-bit integer");
    constexpr std::size_t nbits = sizeof(std::uint32_t) * CHAR_BIT;
    return static_cast<std::int32_t>(value << (nbits - SIZE)) >> (nbits - SIZE);
}

template <std::size_t SIZE>
constexpr std::int16_t SEXT(std::uint16_t value) {
    static_assert(SIZE < sizeof(std::uint16_t) * CHAR_BIT, "SIZE must fit within a 16-bit integer");
    constexpr std::size_t nbits = sizeof(std::uint16_t) * CHAR_BIT;
    return static_cast<std::int16_t>(value << (nbits - SIZE)) >> (nbits - SIZE);
}

template <std::size_t SIZE>
constexpr std::int8_t SEXT(std::uint8_t value) {
    static_assert(SIZE < sizeof(std::uint8_t) * CHAR_BIT, "SIZE must fit within a 8-bit integer");
    constexpr std::size_t nbits = sizeof(std::uint8_t) * CHAR_BIT;
    return static_cast<std::int8_t>(value << (nbits - SIZE)) >> (nbits - SIZE);
}

template <typename T>
struct Underlying;

template <typename T> requires std::is_enum_v<T>
struct Underlying<T> {
    using type = std::underlying_type_t<T>;
};

template <typename T> requires std::is_integral_v<T>
struct Underlying<T> {
    using type = T;
};

template <typename T>
using UnderlyingType = Underlying<T>::type;

template <std::size_t OFFSET, std::size_t NBITS, typename T = std::uint64_t>
struct BitRange {
    using UnderlyingT = UnderlyingType<T>;
    static_assert(std::is_integral_v<UnderlyingT>, "Underlying type must be an integer!");
    static_assert(OFFSET + NBITS <= sizeof(T) * CHAR_BIT, "Range exceeds size of integer type");

    constexpr operator T() const noexcept {
        return value();
    }

    constexpr auto value() const noexcept -> T {
        return static_cast<T>((raw >> OFFSET) & cMask);
    }

    static constexpr const std::size_t cOffset = OFFSET;
    static constexpr const std::size_t cNBits = NBITS;
    static constexpr const UnderlyingT cMask = (1ull << NBITS) - 1;

    UnderlyingT raw;
};

} // namespace nxmount::common