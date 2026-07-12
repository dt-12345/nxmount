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

# Linux
mkdir ~/Game
nxmount --base MyGame.nsp --update GameUpdate.nsp --mount ~/Game
```

Note that on Windows, Windows Defender causes a significant performance penalty if active on the mounted drive.

## TODO:
- Proper handling of add-on content
- Support data patches and delta patches maybe?
- Performance optimizations
  - Be smarter about what we cache and what we don't
  - Maybe increase verification + romfs lookup caches
  - Also figure out why the WinFsp driver runs worse than the WinFsp FUSE driver (surely it's not just because MSVC is that much worse...)
- Convert WinFsp FUSE version into a background service
  - Currently runs in the foreground regardless of the CLI option because only the CygWin version supports daemonization
- Register WinFsp version as a service
  - WinFsp provides a relatively straightforward way to do this
- Memory usage is too high for such a simple project
- Refactor error handling to use std::expected maybe?