#pragma once

#include "log/logging.hpp"

#if defined(WIN32)

#include <windows.h>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace nxmount::common {

[[nodiscard]] inline auto Utf16ToUtf8(std::wstring_view str) -> std::string {
    const auto reqSize = WideCharToMultiByte(
        CP_UTF8,
        0,
        str.data(),
        static_cast<std::int32_t>(str.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (reqSize == 0) {
        LOG_ERROR("Input UTF-16 string has zero size or is invalid!");
        return "";
    }

    auto out = std::string(reqSize, '\0');
    const auto res = WideCharToMultiByte(
        CP_UTF8,
        0,
        str.data(),
        static_cast<std::int32_t>(str.size()),
        out.data(),
        static_cast<std::int32_t>(out.size()),
        nullptr,
        nullptr
    );

    if (res == 0) {
        LOG_ERROR("Failed to convert UTF-16 to UTF-8!");
        return "";
    }

    return out;
}

[[nodiscard]] inline auto Utf8ToUtf16(std::string_view str) -> std::wstring {
    const auto reqSize = MultiByteToWideChar(
        CP_UTF8,
        0,
        str.data(),
        static_cast<std::int32_t>(str.size()),
        nullptr,
        0
    );

    if (reqSize == 0) {
        LOG_ERROR("Input UTF-8 string has zero size or is invalid! {}", str);
        return L"";
    }

    auto out = std::wstring(reqSize, 0);
    const auto res = MultiByteToWideChar(
        CP_UTF8,
        0,
        str.data(),
        static_cast<std::int32_t>(str.size()),
        out.data(),
        static_cast<std::int32_t>(out.size())
    );

    if (res == 0) {
        LOG_ERROR("Failed to convert UTF-8 to UTF-16!");
        return L"";
    }

    return out;
}

} // namespace nxmount::common

#endif