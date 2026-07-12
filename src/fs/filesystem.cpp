#if defined(WIN32) && !defined(USE_WINFUSE)
    #include "fs/filesystem_winfsp.cpp"
#else
    #include "fs/filesystem_fuse.cpp"
#endif