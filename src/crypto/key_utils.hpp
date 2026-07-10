#pragma once

#include "common/utils.hpp"

#include <cctype>
#include <cstdint>
#include <string_view>

namespace nxmount::crypto {

template <std::size_t N>
[[nodiscard]] ALWAYS_INLINE auto IsNull(const std::uint8_t (&key)[N]) -> bool {
    for (std::size_t i = 0; i < N; ++i) {
        if (key[i] != 0) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] ALWAYS_INLINE auto IsHexString(std::string_view str) -> bool {
    for (const auto c : str) {
        if (!std::isxdigit(static_cast<std::uint8_t>(c))) {
            return false;
        }
    }
    return true;
}

inline auto ParseKey(std::uint8_t* key, std::size_t size, std::string_view hex) -> bool {
    constexpr auto toByte = [](const char c) -> std::uint8_t {
        if ('a' <= c && c <= 'f') return c - 'a' + 0xa;
        if ('A' <= c && c <= 'F') return c - 'A' + 0xa;
        if ('0' <= c && c <= '9') return c - '0';
        return 0;
    };

    if (hex.size() != size * 2) {
        return false;
    }

    if (!IsHexString(hex)) {
        return false;
    }

    for (std::size_t i = 0; i < size; ++i) {
        key[i] = toByte(hex[i * 2 + 1]) | toByte(hex[i * 2 + 0]) << 4;
    }
    
    return true;
}

} // namespace nxmount::crypto