#pragma once

#include "common/errors.hpp"
#include "common/fuse.hpp"
#include "common/utils.hpp"
#include "fs/directory.hpp"
#include "fs/file.hpp"
#include "fs/path.hpp"

#if defined(WIN32)
    #include <io.h>
#else
    #include <sys/io.h>
#endif

#include <cstring>
#include <memory>

namespace nxmount::fs {

enum class OpenMode {
    Execute = 1 << 0,
    Write   = 1 << 1,
    Read    = 1 << 2,
    Rw      = Read | Write,
    Rx      = Read | Execute,
    Wx      = Write | Execute,
    Rwx     = Read | Write | Execute,
    All     = Rwx,
};

ENUM_OPERATORS(OpenMode)

enum class RenameMode {
    Default,
    Replace,
    Exchange,
};

class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    [[nodiscard]] virtual auto getRoot() const -> std::unique_ptr<IDirectory> = 0;

    virtual auto init() -> void = 0;
    virtual auto destroy() -> void = 0;
    
    virtual auto getAttributes(std::string_view path, fuse_wrapper::stat* stat) const -> Result = 0;

    virtual auto createFile(std::unique_ptr<IFile>* out, std::string_view path, OpenMode mode) -> Result = 0;
    virtual auto deleteFile(std::string_view path) -> Result = 0;

    virtual auto createDirectory(std::unique_ptr<IDirectory>* out, std::string_view path) -> Result = 0;
    virtual auto deleteDirectory(std::string_view path) -> Result = 0;

    virtual auto stat(std::string_view path, fuse_wrapper::statvfs* statfs) const -> Result = 0;
    virtual auto access(std::string_view path, OpenMode mode) const -> Result = 0;

    virtual auto link(std::string_view path, std::string_view newpath) -> Result = 0;
    virtual auto symLink(std::string_view path, std::string_view linkPath) -> Result = 0;
    virtual auto readLink(std::string_view linkPath, char* outPath, std::size_t outSize) const -> Result = 0;

    virtual auto openFile(std::unique_ptr<IFile>* out, std::string_view path, OpenMode mode) const -> Result = 0;
    virtual auto renameFile(std::string_view path, std::string_view newpath, RenameMode mode) -> Result = 0;

    virtual auto openDirectory(std::unique_ptr<IDirectory>* out, std::string_view path) const -> Result = 0;

    virtual auto copyFileRange(std::string_view fromPath, std::size_t fromOffset, std::string_view toPath, std::size_t toOffset, std::size_t size) -> Result = 0;

    static const fuse_operations cFuseOperations;
};

class ReadOnlyFileSystemBase : public IFileSystem {
public:
    auto createFile(std::unique_ptr<IFile>*, std::string_view, OpenMode) -> Result override final { return READ_ONLY; }
    auto deleteFile(std::string_view) -> Result override final { return READ_ONLY; }

    auto createDirectory(std::unique_ptr<IDirectory>*, std::string_view) -> Result override final { return READ_ONLY; }
    auto deleteDirectory(std::string_view) -> Result override final { return READ_ONLY; }

    auto stat(std::string_view, fuse_wrapper::statvfs* statfs) const -> Result override /* not final */ {
        if (statfs == nullptr) {
            return INVALID;
        }
        std::memset(statfs, 0, sizeof(fuse_wrapper::statvfs));
        // statfs->f_namemax = cMaxPath;
        // statfs->f_bsize = 0x200;
        statfs->f_flag |= R_OK;
        return SUCCESS;
    }

    auto link(std::string_view, std::string_view) -> Result override final { return READ_ONLY; }
    auto symLink(std::string_view, std::string_view) -> Result override final { return READ_ONLY; }

    auto renameFile(std::string_view, std::string_view, RenameMode) -> Result override final { return READ_ONLY; }

    auto copyFileRange(std::string_view, std::size_t, std::string_view, std::size_t, std::size_t) -> Result override final { return READ_ONLY; }
};

} // namespace nxmount::fs