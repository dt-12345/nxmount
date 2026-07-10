#pragma once

#include <fs/filesystem.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace nxmount::driver::detail {

struct Config {
    std::string mountPoint;
    std::string_view basePath;
    std::string_view keyPath;
    std::string_view titlePath;
    std::string_view intype;
    std::string_view titlekey;
    std::string_view updatePath;
    std::vector<std::string_view> updates;
    std::string_view updateType;
#ifdef ENABLE_LOGGING
    std::string_view logLevel;
#endif
    bool foreground = false;
    bool debug = false;
};

auto RunImpl(const Config& config, std::unique_ptr<fs::IFileSystem> fs) -> std::int32_t;

} // namespace nxmount::driver::detail