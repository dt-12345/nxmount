#if defined(WIN32) && !defined(USE_WINFUSE)

#else
    #include "fs/filesystem_fuse.cpp"
#endif