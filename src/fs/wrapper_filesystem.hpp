#pragma once

#include "common/errors.hpp"
#include "common/string_map.hpp"
#include "fs/directory.hpp"
#include "fs/filesystem.hpp"

#include <ctime>
#if !defined(WIN32)
#include <unistd.h>
#endif

namespace nxmount::fs {

class WrapperFs : public ReadOnlyFileSystemBase {
public:
    explicit WrapperFs(std::string_view name) : mName(name), mInitTime(time(nullptr)) {}

    ~WrapperFs() override = default;

    [[nodiscard]] auto getRoot() const -> std::unique_ptr<IDirectory> override { return std::make_unique<Directory>(*this); }

    auto init() -> void override {}
    auto destroy() -> void override {}

    auto getAttributes(std::string_view path, DirectoryEntry* entry) const -> Result override {
        if (entry == nullptr) {
            return INVALID;
        }

        std::string_view subpath;
        const auto name = FirstComponent(path, std::addressof(subpath));

        if (name.empty()) {
            entry->type = Type::Directory;
            return SUCCESS;
        }

        if (const auto res = mFileSystems.find(name); res != mFileSystems.end()) {
            return res->second->getAttributes(subpath, entry);
        }

        return NO_FILE;
    }

    auto access(std::string_view path, OpenMode mode) const -> Result override {
        std::string_view subpath;
        const auto name = FirstComponent(path, std::addressof(subpath));

        if (name.empty()) {
            return NO_FILE;
        }

        if (const auto res = mFileSystems.find(name); res != mFileSystems.end()) {
            if (subpath.empty()) {
                return mode == OpenMode::Read ? SUCCESS : PERMISSION_ERROR;
            } else {
                return res->second->access(subpath, mode);
            }
        }

        return NO_FILE;
    }

    auto readLink(std::string_view, char*, std::size_t) const -> Result override { return UNIMPLEMENTED; }

    auto openFile(std::unique_ptr<IFile>* out, std::string_view path, OpenMode mode) const -> Result override {
        if (out == nullptr) {
            return INVALID;
        }

        std::string_view subpath;
        const auto name = FirstComponent(path, std::addressof(subpath));

        if (name.empty()) {
            return NO_FILE;
        }

        if (const auto res = mFileSystems.find(name); res != mFileSystems.end()) {
            return res->second->openFile(out, subpath, mode);
        }

        return NO_FILE;
    }

    auto openDirectory(std::unique_ptr<IDirectory>* dir, std::string_view path) const -> Result override {
        if (dir == nullptr) {
            return INVALID;
        }

        std::string_view subpath;
        const auto name = FirstComponent(path, std::addressof(subpath));

        if (name.empty()) {
            *dir = getRoot();
            return SUCCESS;
        }

        if (const auto res = mFileSystems.find(name); res != mFileSystems.end()) {
            if (subpath.empty()) {
                *dir = res->second->getRoot();
                return SUCCESS;
            } else {
                return res->second->openDirectory(dir, subpath);
            }
        }

        return NO_FILE;
    }

    // note: these aren't thread-safe so only call these during initialization
    auto addFileSystem(std::string_view name, std::unique_ptr<IFileSystem> fs) -> void {
        mFileSystems.emplace(name, std::move(fs));
    }

    auto removeFileSystem(std::string_view name) -> std::unique_ptr<IFileSystem> {
        const auto res = mFileSystems.find(name);
        if (res == mFileSystems.end()) {
            return nullptr;
        }
        auto fs = std::move(res->second);
        mFileSystems.erase(res);
        return fs;
    }

    [[nodiscard]] auto contains(std::string_view name) const -> bool {
        return mFileSystems.contains(name);
    }

    auto getFileSystems() const -> const common::StringMap<std::unique_ptr<IFileSystem>>& { return mFileSystems; }

private:
    common::StringMap<std::unique_ptr<IFileSystem>> mFileSystems;
    const std::string mName;
    const time_t mInitTime;

    friend class Directory;

    class Directory final : public ReadOnlyDirectoryBase {
    public:
        Directory(const WrapperFs& fs) : mParentFileSystem(fs) {}

        [[nodiscard]] auto getName() const -> std::string_view override { return mParentFileSystem.mName; }

        ~Directory() override = default;

        auto getCount(std::size_t* count) const -> Result override {
            if (count == nullptr) {
                return INVALID;
            }

            *count = mParentFileSystem.mFileSystems.size();
            return SUCCESS;
        }

        auto read(std::size_t* entryCount, DirectoryEntry* entries, std::size_t maxEntries, std::size_t offset) const -> Result override {
            if (entries == nullptr) {
                return INVALID;
            }

            std::size_t i = 0, current = 0;
            for (const auto& [name, fs] : mParentFileSystem.mFileSystems) {
                if (i++ < offset) {
                    continue;
                }
                if (current >= maxEntries) {
                    break;
                }
                entries[current].name = name;
                entries[current].type = Type::Directory;
                entries[current].createTime = 0;
                entries[current++].fileSize = 0;
            }

            if (entryCount != nullptr) {
                *entryCount = current;
            }

            return SUCCESS;
        }

    private:
        const WrapperFs& mParentFileSystem;
    };
};

} // namespace nxmount::fs