#pragma once

#include "common/utils.hpp"

#include <ctime>

#if defined(WIN32)
    #if defined(USE_WINFUSE)
        #include <fuse3/fuse.h>
        #include <fuse3/winfsp_fuse.h>
        #if !defined(_MSC_VER)
            #include <sys/stat.h>
            #include <unistd.h>
        #else
            #define S_IRUSR     0x0100
            #define S_IWUSR     0x0080
            #define S_IXUSR     0x0040
            #define S_IRGRP    (S_IRUSR >> 3)
            #define S_IWGRP    (S_IWUSR >> 3)
            #define S_IXGRP    (S_IXUSR >> 3)
            #define S_IROTH    (S_IRGRP >> 3)
            #define S_IWOTH    (S_IWGRP >> 3)
            #define S_IXOTH    (S_IXGRP >> 3)
            #define	X_OK        1
            #define	W_OK        2
            #define	R_OK        4
        #endif

        #define FUSE_FILL_DIR_DEFAULTS fuse_fill_dir_flags(0)

        namespace fuse_wrapper {
            using stat = fuse_stat;
            using statvfs = fuse_statvfs;
            using mode_t = fuse_mode_t;
            using dev_t = fuse_dev_t;
            using off_t = fuse_off_t;

            ALWAYS_INLINE auto FillStat(stat& s, time_t createTime) -> void {
                s.st_mode = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
                s.st_atim = { ::time(nullptr), 0 };
                s.st_mtim = { createTime, 0 };
                s.st_ctim = { createTime, 0 };
                s.st_birthtim = { createTime, 0 };
            }
        } // namespace fuse_wrapper
    #endif
#else
    #define FUSE_USE_VERSION 30
    #define _FILE_OFFSET_BITS 64
    #include <fuse.h>
    #include <sys/stat.h>
    #include <unistd.h>

    namespace fuse_wrapper {
        using stat = struct stat;
        using statvfs = struct statvfs;
        using mode_t = mode_t;
        using dev_t = dev_t;
        using off_t = off_t;

        ALWAYS_INLINE auto FillStat(stat& s, time_t createTime) -> void {
            s.st_mode = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
            s.st_uid = getuid();
            s.st_gid = getgid();
            s.st_atim = { ::time(nullptr), 0 };
            s.st_mtim = { createTime, 0 };
            s.st_ctim = { createTime, 0 };
        }
    } // namespace fuse_wrapper
#endif