#include "driver/driver_internal.hpp"

namespace nxmount::driver::detail {

auto RunImpl(const Config& config, std::unique_ptr<fs::IFileSystem> fs) -> std::int32_t {
    auto args = std::vector<const char*>{
        "nxmount", config.mountPoint.c_str(),
#if !defined(WIN32)
        "-o", "allow_other",
#endif
    };

    if (config.foreground) {
        args.push_back("-f");
    }

    if (config.debug) {
        args.push_back("-d");
    }

    return fuse_main(static_cast<int>(args.size()), const_cast<char**>(args.data()), std::addressof(fs::IFileSystem::cFuseOperations), fs.release());
}
    
} // namespace nxmount::driver::detail