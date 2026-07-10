#include "common/errors.hpp"
#include "common/fuse.hpp"
#include "common/utils.hpp"
#include "fs/filesystem.hpp"
#include "fs/directory.hpp"
#include "fuse3/fuse_common.h"
#include "log/logging.hpp"

#include <fcntl.h>
#include <stdio.h>

#include <vector>

namespace nxmount::fs {

[[nodiscard]] static auto ConvertMode(fuse_wrapper::mode_t mode) -> OpenMode {
    OpenMode out{};
    if ((mode & S_IRUSR) || (mode & S_IRGRP) || (mode & S_IROTH)) {
        out |= OpenMode::Read;
    }
    if ((mode & S_IWUSR) || (mode & S_IWGRP) || (mode & S_IWOTH)) {
        out |= OpenMode::Write;
    }
    if ((mode & S_IXUSR) || (mode & S_IXGRP) || (mode & S_IXOTH)) {
        out |= OpenMode::Execute;
    }
    return out;
}

[[nodiscard]] static auto ConvertOpenFlags(int flags) -> OpenMode {
    if (flags & O_RDONLY) {
        return OpenMode::Read;
    }
    if (flags & O_WRONLY) {
        return OpenMode::Write;
    }
    if (flags & O_RDWR) {
        return OpenMode::Rw;
    }
    return OpenMode::Read;
}

[[nodiscard]] static auto ConvertAccessFlags(int mask) -> OpenMode {
    OpenMode out{};
    if (mask & R_OK) {
        out |= OpenMode::Read;
    }
    if (mask & W_OK) {
        out |= OpenMode::Write;
    }
    if (mask & X_OK) {
        out |= OpenMode::Execute;
    }
    return out;
}

[[nodiscard]] static auto GetDirEntryAttributes(const DirectoryEntry& entry) -> fuse_wrapper::stat {
    fuse_wrapper::stat stat{};
    fuse_wrapper::FillStat(stat, ::time(nullptr));
    if (entry.type == Type::Directory) {
        stat.st_mode |= S_IFDIR;
    } else {
        stat.st_mode |= S_IFREG;
        stat.st_size = entry.fileSize;
    }
    stat.st_nlink = 1;
    
    return stat;
}

[[nodiscard]] ALWAYS_INLINE static auto GetFs() -> fs::IFileSystem* {
    return reinterpret_cast<fs::IFileSystem*>(fuse_get_context()->private_data);
}

#define LOG_RETURN(EXPR)        \
    {                           \
        const auto _r = (EXPR); \
        LOG_INFO("{}", _r);     \
        return _r;              \
    }

static auto GetAttr(const char* path, fuse_wrapper::stat* stat, fuse_file_info*) -> int {
    LOG_INFO("GetAttr: {}", path);
    std::memset(stat, 0, sizeof(*stat));
    return GetFs()->getAttributes(path, stat);
}

static auto ReadLink(const char *path, char* buf, size_t size) -> int {
    LOG_INFO("ReadLink: {}", path);
    return GetFs()->readLink(path, buf, size);
}

static auto MkNod(const char* path, fuse_wrapper::mode_t mode, fuse_wrapper::dev_t) -> int {
    LOG_INFO("MkNod: {}", path);
    return GetFs()->createFile(nullptr, path, ConvertMode(mode));
}

static auto MkDir(const char* path, fuse_wrapper::mode_t) -> int {
    LOG_INFO("MkDir: {}", path);
    return GetFs()->createDirectory(nullptr, path);
}

static auto Unlink(const char* path) -> int {
    LOG_INFO("Unlink: {}", path);
    return GetFs()->deleteFile(path);
}

static auto RmDir(const char* path) -> int {
    LOG_INFO("RmDir: {}", path);
    return GetFs()->deleteDirectory(path);
}

static auto SymLink(const char* from, const char* to) -> int {
    LOG_INFO("SymLink: {} {}", from, to);
    return GetFs()->symLink(from, to);
}

static auto Rename(const char* path, const char* name, unsigned int) -> int {
    LOG_INFO("Rename: {} {}", path, name);
    return GetFs()->renameFile(path, name, RenameMode::Default);
}

static auto Link(const char* from, const char* to) -> int {
    LOG_INFO("Link: {} {}", from, to);
    return GetFs()->link(from, to);
}

static auto Truncate(const char* path, fuse_wrapper::off_t offset, fuse_file_info* info) -> int {
    LOG_INFO("Truncate: {}", path);
    if (info != nullptr && info->fh != 0) {
        auto file = reinterpret_cast<IFile*>(info->fh);
        return file->setSize(offset);
    } else {
        std::unique_ptr<IFile> file;
        const auto res = GetFs()->openFile(std::addressof(file), path, OpenMode::Write);
        if (SUCCEEDED(res)) {
            return file->setSize(offset);
        } else {
            return res;
        }
    }
}

static auto Open(const char* path, fuse_file_info* info) -> int {
    LOG_INFO("Open: {}", path);
    std::unique_ptr<IFile> file;
    const auto mode = ConvertOpenFlags(info->flags);
    const auto res = GetFs()->openFile(std::addressof(file), path, mode);
    if (SUCCEEDED(res)) {
        if ((info->flags & O_TRUNC) && (mode & OpenMode::Write) == OpenMode::Write) {
            file->setSize(0);
        }
        info->fh = reinterpret_cast<std::uint64_t>(file.release());
        return SUCCESS;
    } else if (info->flags & O_CREAT) {
        const auto createRes = GetFs()->createFile(std::addressof(file), path, mode);
        if (SUCCEEDED(createRes)) {
            info->fh = reinterpret_cast<std::uint64_t>(file.release());
            return SUCCESS;
        } else {
            return createRes;
        }
    } else {
        return res;
    }
}

static auto Read(const char* path, char* dst, size_t size, fuse_wrapper::off_t offset, fuse_file_info* info) -> int {
    LOG_INFO("Read: {} {:#x} {:#x}", path, size, offset);

    auto file = reinterpret_cast<IFile*>(info->fh);
    return static_cast<int>(file->read(dst, size, offset));
}

static auto Write(const char* path, const char* src, size_t size, fuse_wrapper::off_t offset, fuse_file_info* info) -> int {
    LOG_INFO("Write: {} {:#x} {:#x}", path, size, offset);

    auto file = reinterpret_cast<IFile*>(info->fh);
    return static_cast<int>(file->write(src, size, offset));
}

static auto StatFs(const char* path, fuse_wrapper::statvfs* statfs) -> int {
    LOG_INFO("StatFs: {}", path);
    return GetFs()->stat(path, statfs);
}

static auto Flush(const char* path, fuse_file_info* info) -> int {
    LOG_INFO("Flush: {}", path);

    auto file = reinterpret_cast<IFile*>(info->fh);
    return file->flush();
}

static auto Release(const char* path, fuse_file_info* info) -> int {
    LOG_INFO("Release: {}", path);

    if (info->fh == 0) {
        return SUCCESS;
    }

    delete reinterpret_cast<IFile*>(info->fh);
    info->fh = 0;
    return SUCCESS;
}

static auto FSync(const char* path, int flags, fuse_file_info* info) -> int {
    LOG_INFO("FSync: {}", path);

    auto file = reinterpret_cast<IFile*>(info->fh);
    return file->sync(flags == 0);
}

static auto OpenDir(const char* path, fuse_file_info* info) -> int {
    LOG_INFO("OpenDir: {}", path);

    std::unique_ptr<IDirectory> dir;
    const auto res = GetFs()->openDirectory(std::addressof(dir), path);
    if (SUCCEEDED(res)) {
        info->fh = reinterpret_cast<std::uint64_t>(dir.release());
        return SUCCESS;
    }

    return res;
}

static auto ReadDir(const char* path, void* buf, fuse_fill_dir_t filler, fuse_wrapper::off_t /* offset */, fuse_file_info* info, fuse_readdir_flags /* flags */) -> int {
    LOG_INFO("ReadDir: {}", path);

    auto dir = reinterpret_cast<IDirectory*>(info->fh);
    std::size_t count;
    if (const auto res = dir->getCount(std::addressof(count)); FAILED(res)) {
        return res;
    }

    auto entries = std::vector<DirectoryEntry>(count);
    std::size_t read;
    if (const auto res = dir->read(std::addressof(read), entries.data(), entries.size(), 0); FAILED(res)) {
        return res;
    }

    {
        fs::DirectoryEntry thisEntry{
            .name = std::string(dir->getName()),
            .type = fs::Type::Directory,
            .attributes = 0,
            .fileSize = 0,
        };
        const auto stat = GetDirEntryAttributes(thisEntry);
        filler(buf, ".", std::addressof(stat), 0, FUSE_FILL_DIR_DEFAULTS);
        filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_DEFAULTS);
    }

    for (std::size_t i = 0; i < read; ++i) {
        const auto& entry = entries[i];
        const auto stat = GetDirEntryAttributes(entry);
        if (filler(buf, entry.name.c_str(), std::addressof(stat), 0, FUSE_FILL_DIR_DEFAULTS) != 0) {
            break;
        }
    }

    return SUCCESS;
}

static auto ReleaseDir(const char* path, fuse_file_info* info) -> int {
    LOG_INFO("ReleaseDir: {}", path);

    if (info->fh == 0) {
        return SUCCESS;
    }

    delete reinterpret_cast<IDirectory*>(info->fh);
    info->fh = 0;
    return SUCCESS;
}

static auto FSyncDir(const char* path, int flags, fuse_file_info* info) -> int {
    LOG_INFO("FSyncDir: {}", path);

    auto dir = reinterpret_cast<IDirectory*>(info->fh);
    return dir->sync(flags == 0);
}

static auto SetXAttr(const char* /* path */, const char* /* attr */, const char* /* value */, size_t /* size */, int /* flags */) -> int {
    return UNSUPPORTED;
}

static auto GetXAttr(const char* /* path */, const char* /* attr */, char* /* value */, size_t /* size */) -> int {
    return UNSUPPORTED;
}

static auto ListXAttr(const char* /* path */, char* /* attrs */, size_t /* size */) -> int {
    return UNSUPPORTED;
}

static auto RemoveXAttr(const char* /* path */, const char* /* attr */) -> int {
    return UNSUPPORTED;
}

static auto Init(fuse_conn_info* conn, fuse_config*) -> void* {
    LOG_INFO("Init");
    conn->want |= (conn->capable & FUSE_CAP_READDIRPLUS);
    auto fs = GetFs();
    fs->init();
    return fs;
}

static auto Destroy(void* data) -> void {
    LOG_INFO("Destroy");
    auto fs = reinterpret_cast<fs::IFileSystem*>(data);
    fs->destroy();
    fs->~IFileSystem();
}

static auto Access(const char* path, int mask) -> int {
    LOG_INFO("Access: {}", path);
    return GetFs()->access(path, ConvertAccessFlags(mask));
}

static auto Create(const char* path, fuse_wrapper::mode_t mode, fuse_file_info* info) -> int {
    LOG_INFO("Create: {}", path);
    std::unique_ptr<IFile> file;
    const auto res = GetFs()->createFile(std::addressof(file), path, ConvertMode(mode));
    if (SUCCEEDED(res)) {
        info->fh = reinterpret_cast<std::uint64_t>(file.release());
        return SUCCESS;
    }
    return res;
}

const fuse_operations IFileSystem::cFuseOperations {
    .getattr = GetAttr,
    .readlink = ReadLink,
    .mknod = MkNod,
    .mkdir = MkDir,
    .unlink = Unlink,
    .rmdir = RmDir,
    .symlink = SymLink,
    .rename = Rename,
    .link = Link,
    .chmod = nullptr,       // unimplemented
    .chown = nullptr,       // unimplemented
    .truncate = Truncate,
    .open = Open,
    .read = Read,
    .write = Write,
    .statfs = StatFs,
    .flush = Flush,
    .release = Release,
    .fsync = FSync,
    .setxattr = SetXAttr,    // unimplemented
    .getxattr = GetXAttr,    // unimplemented
    .listxattr = ListXAttr,   // unimplemented
    .removexattr = RemoveXAttr, // unimplemented
    .opendir = OpenDir,
    .readdir = ReadDir,
    .releasedir = ReleaseDir,
    .fsyncdir = FSyncDir,
    .init = Init,
    .destroy = Destroy,
    .access = Access,
    .create = Create,
    .lock = nullptr,        // unimplemented
    .utimens = nullptr,     // unimplemented
    .bmap = nullptr,        // unimplemented
    .ioctl = nullptr,       // unimplemented
    .poll = nullptr,        // unimplemented
    .write_buf = nullptr,   // unimplemented
    .read_buf = nullptr,    // unimplemented
    .flock = nullptr,       // unimplemented
    .fallocate = nullptr,   // unimplemented
#if !defined(WIN32)
    .copy_file_range = nullptr,
    .lseek = nullptr,
    .statx = nullptr,
#endif
};

} // namespace nxmount::fs