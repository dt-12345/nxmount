#include "common/mem_mapped_file.hpp"
#include "log/logging.hpp"

#include <cstring>
#include <string>

#if defined(WIN32)
    #include <windows.h>
#else
    #define _FILE_OFFSET_BITS 64
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

namespace nxmount::common {

MemMappedFile::MemMappedFile(std::string_view path) {
    open(path);
}

MemMappedFile::~MemMappedFile() {
    close();
}

auto MemMappedFile::open(std::string_view path) -> bool {
    if (isOpen()) {
        close();
    }

    auto filepath = std::string(path);

#if defined(WIN32)
    Handle file = ::CreateFileA(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == nullptr) {
        LOG_ERROR("Failed to open {}: {:#x}", path, ::GetLastError());
        mFileHandle = cNullHandle;
        return false;
    }

    LARGE_INTEGER fileSize;
    if (::GetFileSizeEx(file, std::addressof(fileSize)) == FALSE) {
        LOG_ERROR("Failed to get file size for {}: {:#x}", path, ::GetLastError());
        ::CloseHandle(file);
        mFileHandle = cNullHandle;
        return false;
    }

    mSize = static_cast<std::size_t>(fileSize.QuadPart);

    mFileHandle = ::CreateFileMappingA(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (mFileHandle == nullptr) {
        LOG_ERROR("Failed to create file mapping for {}: {:#x}", path, ::GetLastError());
        ::CloseHandle(file);
        return false;
    }

    mMapped = static_cast<std::uint8_t*>(::MapViewOfFile(mFileHandle, FILE_MAP_READ, 0, 0, 0));
    if (mMapped == nullptr) {
        LOG_ERROR("Failed to map view for {}: {:#x}", path, ::GetLastError());
        ::CloseHandle(file);
        ::CloseHandle(mFileHandle);
        mFileHandle = cNullHandle;
        return false;
    }

    // file mapping has a handle so we can close this one
    ::CloseHandle(file);
    return true;
#else
    mFileHandle = ::open(filepath.c_str(), O_RDONLY | O_LARGEFILE);
    if (mFileHandle == -1) {
        LOG_ERROR("Failed to open {}: {}", path, errno);
        mFileHandle = cNullHandle;
        return false;
    }

    struct stat64 fileStat;
    if (fstat64(mFileHandle, std::addressof(fileStat)) != 0) {
        LOG_ERROR("Failed to get file size for {}: {}", path, errno);
        ::close(mFileHandle);
        mFileHandle = cNullHandle;
        return false;
    }

    mSize = fileStat.st_size;

    mMapped = static_cast<std::uint8_t*>(::mmap64(nullptr, mSize, PROT_READ, MAP_SHARED, mFileHandle, 0));
    if (mMapped == nullptr) {
        LOG_ERROR("Failed to map {}: {}", path, errno);
        ::close(mFileHandle);
        mFileHandle = cNullHandle;
        return false;
    }

    ::madvise(mMapped, mSize, MADV_NORMAL | MADV_HUGEPAGE);

    return true;
#endif
}

auto MemMappedFile::close() -> void {
    if (!isOpen()) {
        return;
    }

#if defined(WIN32)
    if (mMapped != nullptr) {
        ::UnmapViewOfFile(mMapped);
        mMapped = nullptr;
    }
    if (mFileHandle != cNullHandle) {
        ::CloseHandle(mFileHandle);
        mFileHandle = cNullHandle;
    }
    mSize = 0;
#else
    if (mMapped != nullptr) {
        ::munmap(mMapped, mSize);
        mMapped = nullptr;
    }
    if (mFileHandle != cNullHandle) {
        ::close(mFileHandle);
        mFileHandle = cNullHandle;
    }
#endif
}

auto MemMappedFile::read(void* dst, std::size_t size, std::size_t offset) const -> std::size_t {
    if (!isOpen() || mMapped == nullptr) {
        return 0;
    }

    if (size == 0 || offset >= mSize) {
        return 0;
    }

    if (offset + size >= mSize) {
        size = mSize - offset;
    }

    std::memcpy(dst, mMapped + offset, size);
    return size;
}

} // namespace nxmount::common