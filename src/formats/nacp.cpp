#include "formats/nacp.hpp"
#include "log/logging.hpp"

#define ZLIB_CONST
#include <zlib/zlib.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace nxmount::formats {
    
auto GetDisplayName(provider::UniqueProvider& provider) -> std::string {
    ApplicationControlProperty header;
    if (provider->read(std::addressof(header), sizeof(header), 0) != sizeof(header)) {
        return "";
    }

    std::string name = "";
    if (header.titleFormat == TitleFormat::Uncompressed) {
        for (std::size_t i = 0; i < 0x10; ++i) {
            if (header.titles[i].name[0]) {
                name = header.titles[i].name;
            }
        }
    } else {
        auto stream = z_stream{};
        auto titleBlock = std::vector<ApplicationTitle>(0x20);
        stream.next_in = reinterpret_cast<z_const Bytef*>(header.compressedTitles.data);
        stream.avail_in = header.compressedTitles.dataSize;
        stream.next_out = reinterpret_cast<Bytef*>(titleBlock.data());
        stream.avail_out = titleBlock.size() * sizeof(ApplicationTitle);

        if (inflateInit2(&stream, -15) != Z_OK) {
            LOG_ERROR("Failed to initialize z_stream for ApplicationControlProperty title block decompression!");
            return "";
        }

        if (inflate(&stream, Z_FINISH) != Z_OK) {
            LOG_ERROR("Failed to decompress ApplicationControlProperty title block!");
            inflateEnd(&stream);
            return "";
        }

        if (inflateEnd(&stream) != Z_OK) {
            LOG_ERROR("Failed to finalize z_stream for ApplicationControlProperty title block decompression!");
            return "";
        }

        for (const auto& title : titleBlock) {
            name = title.name;
        }
    }

    if (header.displayVersion[0]) {
        char displayVersion[sizeof(header.displayVersion) + 1];
        std::memcpy(displayVersion, header.displayVersion, sizeof(header.displayVersion));
        displayVersion[sizeof(header.displayVersion)] = '\0';
        fmt::format_to(std::back_inserter(name), " {}", displayVersion);
    }

#if defined(WIN32)
    static constinit const std::array<char, 9> cInvalidPathChars = {
        '<', '>', ':', '"', '/', '\\', '|', '?', '*',
    };
#else
    static constinit const std::array<char, 1> cInvalidPathChars = {
        '/',
    };
#endif
    for (const auto c : cInvalidPathChars) {
        name.erase(std::remove(name.begin(), name.end(), c), name.end());
    }

    return name;
}

} // namespace nxmount::formats