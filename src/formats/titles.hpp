#pragma once

#include <cstdint>
#include <string_view>

namespace nxmount::formats {

auto TryGetTitleName(std::uint64_t id) -> std::string_view;

} // namespace nxmount::formats