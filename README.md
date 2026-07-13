# NXMount (very WIP but functional)

Mount NSP/XCI/NCA filesystems (requires a WinFsp installation on Windows). Inspired by [shadow's nxmount](https://github.com/shadowninja108/nxmount).

Thank you to the following for being great references:
- https://github.com/atmosphere-nx/atmosphere
- https://github.com/sciresm/hactool
- https://github.com/darkmattercore/nxdumptool
- https://switchbrew.org/wiki/Main_Page

Key derivation and management code was taken from Hactool and is under the ISC license (see `src/hactool/LICENSE`), the rest of the non-third party code is licensed under GPLv3. The code in `src/provider/atmosphere.hpp` and `src/provider/atmosphere/cpp` is heavily based on the `fs::IStorage`-derived classes in the Atmosphère codebase.

## Basic Usage

```sh
# Windows
nxmount --base MyGame.nsp --update GameUpdate.nsp --mount Z:
# to unmount, kill the process with Ctrl-C

# Linux
mkdir ~/Game
nxmount --base MyGame.nsp --update GameUpdate.nsp --mount ~/Game
# to unmount
umount ~/Game
```

**Note that on Windows, Windows Defender causes a significant performance penalty if real-time scanning is active on the mounted drive.**

For basic usage, run:
```sh
nxmount --help
```

### Windows Daemon

Since Windows has no concept of daemonization (outside of CygWin), the nxmount process will always run in the foreground by default. To convert it into a background service, you need to register it with the WinFsp.Launcher service.

To register the service with the WinFsp.Launcher, the correct registry keys must be added (see the WinFsp documentation for more details). The following is an example `.reg` file that you can use as a base (after modifying it, double click it to run it and add the corresponding registry entries). The launcher unfortunately limits the total number of arguments to 9.

```
Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\WinFsp\Services\nxmount]
"Executable"="C:\\Dev\\nxmount\\build\\nxmount.exe"
"CommandLine"="--base %1 --mount %2"
"Security"="D:P(A;;RPWPLC;;;WD)"
"JobControl"=dword:00000001
"RunAs"="."
```

```sh
# to start the service
launchctl-x64 start nxmount InstanceName Arguments... # InstanceName is the name of the mount instance and can be anything
# to stop the service
launchctl-x64 stop nxmount InstanceName # use the same InstanceName you started the service with
```

Note that when used as a service, all path arguments **must be absolute paths** to be resolved correctly. The `Stderr` option also seems to cause a crash if it's present (unsure if this is my fault or if it's just bugged);

## Building

### Windows

Prerequisites
1. [WinFsp](https://github.com/winfsp/winfsp/releases)
2. A C++ compiler (MSVC, gcc, clang, etc.)
3. CMake 3.21+
4. If building with MinGW, make sure to run the CygFuse `install.sh` script to make the FUSE headers available

```sh
git clone --recurse-submodules https://github.com/dt-12345/nxmount.git
cd nxmount

cmake -B build -DCMAKE_BUILD_TYPE=Release # note: if building with MinGW, add -DUSE_WINFUSE=ON
cmake --build build
```

Building with CygWin is untested.

### Linux

Prerequisites
1. FUSE 3
2. A C++ compiler (gcc, clang)
3. CMake 3.21+

```sh
git clone --recurse-submodules https://github.com/dt-12345/nxmount.git
cd nxmount

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## TODO
- Proper handling of add-on content
  - Data patches (I don't even know any games that use these)
- Support applying delta patches (update to update patch)
- Performance optimizations
  - Be smarter about what we cache and what we don't
  - Maybe increase verification + romfs lookup caches
- Memory usage is too high for such a simple project
  - For performance, we keep the entire romfs file tables in memory but for TotK, that's ~25 mb per version
  - Memory usage is actually totally fine for games not named Tears of the Kingdom
- Refactor error handling to use std::expected maybe?
- General code clean-up
- Maybe don't copy the WinFsp dll into the build directory