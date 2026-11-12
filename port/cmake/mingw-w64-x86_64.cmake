# Cross-compiling the port for 64-bit Windows from Linux with MinGW-w64 GCC
# (Debian/Ubuntu: g++-mingw-w64-x86-64-posix), used by port/tools/windows_release.sh.
# The POSIX thread model gives std::thread and std::mutex.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(BLUPI_MINGW_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${BLUPI_MINGW_PREFIX}-gcc-posix)
set(CMAKE_CXX_COMPILER ${BLUPI_MINGW_PREFIX}-g++-posix)
set(CMAKE_RC_COMPILER ${BLUPI_MINGW_PREFIX}-windres)
# SDL3's prefix (CMAKE_PREFIX_PATH / CMAKE_FIND_ROOT_PATH on the command line) and the MinGW
# sysroot; never the host's own packages.
list(APPEND CMAKE_FIND_ROOT_PATH /usr/${BLUPI_MINGW_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
