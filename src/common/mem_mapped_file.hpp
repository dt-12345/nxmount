#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>

namespace nxmount::common {

class MemMappedFile {
public:
    MemMappedFile() = default;

    explicit MemMappedFile(std::string_view path);

    ~MemMappedFile();

    auto open(std::string_view path) -> bool;
    auto close() -> void;
    
    [[nodiscard]] auto isOpen() const -> bool {
        return mFileHandle != cNullHandle;
    }

    [[nodiscard]] auto read(void* dst, std::size_t size, std::size_t offset) const -> std::size_t;

    [[nodiscard]] auto getSize() const -> std::size_t { return mSize; }

private:
#if defined(WIN32)
    using Handle = void*;
    static constexpr const Handle cNullHandle = nullptr;
#else
    using Handle = std::int32_t;
    static constexpr const Handle cNullHandle = 0;
#endif

    Handle mFileHandle = cNullHandle;
    std::uint8_t* mMapped = nullptr;
    std::size_t mSize = 0;
};

} // namespace nxmount::common