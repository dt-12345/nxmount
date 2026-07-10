cmake_minimum_required(VERSION 3.21)

project(nxmount LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENABLE_LOGGING "Enable logging" ON)
option(USE_WINFUSE "Use WinFsp FUSE (only available on Windows, required for MinGW)" OFF)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
endif()

if (MINGW)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

if (NOT MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "-g3")
endif()

set(CRYPTOPP_BUILD_TESTING OFF)
set(ZLIB_BUILD_TESTING OFF)
set(ZLIB_BUILD_SHARED OFF)
set(ZLIB_BUILD_MINIZIP OFF)
set(ZLIB_INSTALL OFF)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/lib/cryptopp-cmake/cmake/)
set(cryptopp_SOURCE_DIR ${PROJECT_SOURCE_DIR}/lib/cryptopp/)

add_subdirectory(${PROJECT_SOURCE_DIR}/lib/cryptopp-cmake/cryptopp/)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/fmt/)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/zlib/)

add_executable(${PROJECT_NAME}
    src/main.cpp

    src/common/mem_mapped_file.cpp

    src/crypto/crypto.cpp
    src/crypto/key_mgr.cpp

    src/driver/driver.cpp

    src/formats/bktr.cpp
    src/formats/cnmt.cpp
    src/formats/hfs0.cpp
    src/formats/nacp.cpp
    src/formats/nca.cpp
    src/formats/npdm.cpp
    src/formats/pfs0.cpp
    src/formats/romfs.cpp
    src/formats/titles.cpp
    src/formats/xci.cpp

    src/fs/filesystem.cpp

    src/log/logging.cpp

    src/hactool/pki.c

    src/provider/atmosphere.cpp
)

if (ENABLE_LOGGING)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_LOGGING)
endif()

if (USE_WINFUSE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_WINFUSE)
else()
    if (MINGW)
        message(FATAL_ERROR "Standard WinFsp is not supported for MINGW")
    endif()
endif()

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /wd4244 /wd4127 /Zc:__cplusplus)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++exp)
endif()
    
if (WIN32)
    set(WINFSP_LIBRARY_NAME winfsp-x64.lib)
    set(WINFSP_BIN_NAME winfsp-x64.dll)
    find_path(WINFSP_LIBRARY NAMES ${WINFSP_LIBRARY_NAME} PATHS $ENV{ProgramFiles}/WinFsp/lib/ $ENV{ProgramFiles\(x86\)}/WinFsp/lib/ REQUIRED)
    find_path(WINFSP_BIN NAMES ${WINFSP_BIN_NAME} PATHS $ENV{ProgramFiles}/WinFsp/bin/ $ENV{ProgramFiles\(x86\)}/WinFsp/bin/ REQUIRED)

    target_link_libraries(${PROJECT_NAME} PRIVATE ${WINFSP_LIBRARY}/${WINFSP_LIBRARY_NAME})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${WINFSP_BIN}/${WINFSP_BIN_NAME} ${CMAKE_BINARY_DIR})

    if (MINGW)
        find_package(Msys REQUIRED)

        set(WINFSP_FUSE_LIBRARY_NAME libfuse3.dll.a)
        find_path(WINFSP_FUSE_INCLUDE NAMES fuse/winfsp_fuse.h PATHS ${MSYS_INSTALL_PATH}/usr/include/ REQUIRED)
        find_path(WINFSP_FUSE_LIBRARY NAMES ${WINFSP_FUSE_LIBRARY_NAME} PATHS ${MSYS_INSTALL_PATH}/usr/lib/ REQUIRED)
        
        target_include_directories(${PROJECT_NAME} PRIVATE ${WINFSP_FUSE_INCLUDE})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${WINFSP_FUSE_LIBRARY}/${WINFSP_FUSE_LIBRARY_NAME})
    else()
        find_path(WINFSP_INCLUDE NAMES winfsp/winfsp.h PATHS $ENV{ProgramFiles}/WinFsp/inc/ $ENV{ProgramFiles\(x86\)}/WinFsp/inc/ REQUIRED)
        target_include_directories(${PROJECT_NAME} PRIVATE ${WINFSP_INCLUDE})
    endif()
else()
    set(FUSE_LIBRARY_NAME libfuse3.so)

    find_path(FUSE_INCLUDE_DIR NAMES fuse.h PATHS /usr/local/include/fuse3/ /usr/include/fuse3/ REQUIRED)
    find_path(FUSE_LIBRARIES NAMES ${FUSE_LIBRARY_NAME} PATHS /lib64/ /lib/ /usr/lib64/ /usr/lib/ /usr/local/lib64/ /usr/local/lib/ REQUIRED)

    target_include_directories(${PROJECT_NAME} PRIVATE ${FUSE_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${FUSE_LIBRARIES}/${FUSE_LIBRARY_NAME})
endif()

if (MINGW)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0xa00)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE fmt cryptopp zlibstatic)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/lib/fmt/include/
        $<BUILD_INTERFACE:${cryptopp_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CRYPTOPP_PREFIXED_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
        ${PROJECT_SOURCE_DIR}/lib/
)
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src/)