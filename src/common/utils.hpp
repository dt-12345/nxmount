#pragma once

#include <cstdint>
#include <utility>

#if defined(__GNUC__)
    #define ALWAYS_INLINE [[gnu::always_inline]] inline
    #define PACKED(DECL) DECL [[gnu::packed]]
    #define PACKED_STRUCT(DECL) struct [[gnu::packed]] DECL
#else
    #define ALWAYS_INLINE __forceinline
    #define PACKED(DECL) __pragma(pack(push, 1)); DECL; __pragma(pack(pop))
    #define PACKED_STRUCT(DECL) __pragma(pack(push, 1)); struct DECL; __pragma(pack(pop))
#endif

namespace nxmount::common {

consteval inline auto MakeMagic(const char (&magic)[5]) -> std::uint32_t {
    return static_cast<std::uint32_t>(+magic[0]) << 0x00
            | static_cast<std::uint32_t>(+magic[1]) << 0x08
            | static_cast<std::uint32_t>(+magic[2]) << 0x10
            | static_cast<std::uint32_t>(+magic[3]) << 0x18;
}

ALWAYS_INLINE constexpr auto AlignUp(std::size_t value, std::size_t align) -> std::size_t {
    return (value + align - 1) / align * align;
}

ALWAYS_INLINE constexpr auto AlignDown(std::size_t value, std::size_t align) -> std::size_t {
    return value / align * align;
}

template <typename... Ts>
ALWAYS_INLINE constexpr auto Unused(Ts&&... args) -> void {
    (static_cast<void>(args), ...);
}

} // namespace nxmount::common

#define ENUM_OPERATORS(EnumType)                                                                            \
    constexpr inline auto operator|(EnumType lhs, EnumType rhs) -> EnumType {                               \
        return static_cast<EnumType>(std::to_underlying(lhs) | std::to_underlying(rhs));                    \
    }                                                                                                       \
                                                                                                            \
    constexpr inline auto operator|=(EnumType& lhs, EnumType rhs) -> EnumType& {                            \
        return lhs = lhs | rhs;                                                                             \
    }                                                                                                       \
                                                                                                            \
    constexpr inline auto operator&(EnumType lhs, EnumType rhs) -> EnumType {                               \
        return static_cast<EnumType>(std::to_underlying(lhs) & std::to_underlying(rhs));                    \
    }                                                                                                       \
                                                                                                            \
    constexpr inline auto operator&=(EnumType& lhs, EnumType rhs) -> EnumType& {                            \
        return lhs = lhs & rhs;                                                                             \
    }
