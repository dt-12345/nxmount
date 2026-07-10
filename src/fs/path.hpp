#pragma once

#include <string_view>

namespace nxmount::fs {

inline constexpr const std::size_t cMaxPath = 0x300;

[[nodiscard]] inline auto Normalize(std::string_view path) -> std::string_view {
    if (path.empty()) {
        return path;
    }
    if (path[0] == '/') {
        return { path.data() + 1, path.size() - 1 };
    }
    return path;
}
/*
    "" => "", ""
    "/" => "", ""
    "/foo" => "foo", ""
    "/foo/" => "foo", ""
    "/foo/bar" => "foo", "bar"
*/
[[nodiscard]] inline auto FirstComponent(std::string_view path, std::string_view* remaining = nullptr) -> std::string_view {
    path = Normalize(path);
    const auto pos = path.find_first_of("/");
    if (pos == std::string_view::npos) {
        if (remaining != nullptr) {
            *remaining = "";
        }
        return path;
    }
    if (remaining != nullptr) {
        *remaining = { path.data() + pos + 1, path.size() - pos - 1 };
    }
    return { path.data(), pos };
}

[[nodiscard]] inline auto LastComponent(std::string_view path) -> std::string_view {
    if (path.ends_with("/")) {
        path = { path.data(), path.size() - 1 };
    }
    const auto pos = path.find_last_of("/");
    if (pos == std::string_view::npos) {
        return path;
    }
    if (pos + 1 >= path.size()) {
        return "";
    }
    return { path.data() + pos + 1, path.size() - pos - 1 };
}

} // namespace nxmount::fs