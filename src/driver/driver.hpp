#pragma once

#include <cstdint>

namespace nxmount::driver {

[[nodiscard]] auto Run(std::int32_t argc, const char* argv[]) -> std::int32_t;

} // namespace nxmount::driver