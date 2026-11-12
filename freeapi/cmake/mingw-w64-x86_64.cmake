# Cross-compiling the Free API builds for 64-bit Windows from Linux with MinGW-w64 GCC
# (Debian/Ubuntu packages g++-mingw-w64-x86-64-posix). The POSIX thread model gives
# std::thread/std::mutex, which Free API and Free Direct use.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(BLUPI_MINGW_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${BLUPI_MINGW_PREFIX}-gcc-posix)
set(CMAKE_CXX_COMPILER ${BLUPI_MINGW_PREFIX}-g++-posix)
set(CMAKE_RC_COMPILER ${BLUPI_MINGW_PREFIX}-windres)
# The SDK prefix (CMAKE_FIND_ROOT_PATH on the command line) comes first; never the host's packages.
list(APPEND CMAKE_FIND_ROOT_PATH /usr/${BLUPI_MINGW_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
