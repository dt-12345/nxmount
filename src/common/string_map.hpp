#pragma once

#include <string>
#include <unordered_map>

namespace nxmount::common {

struct StringHash {
    using is_transparent = void;

    [[nodiscard]] auto operator()(const char* value) const -> std::size_t {
        return std::hash<std::string_view>{}(value);
    }

    [[nodiscard]] auto operator()(std::string_view value) const -> std::size_t {
        return std::hash<std::string_view>{}(value);
    }

    [[nodiscard]] auto operator()(const std::string& value) const -> std::size_t {
        return std::hash<std::string>{}(value);
    }
};

template <typename T>
using StringMap = std::unordered_map<std::string, T, StringHash, std::equal_to<>>;

} // namespace nxmount::common