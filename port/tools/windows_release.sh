#!/bin/sh
# Windows x64 release package of the port, cross-compiled on Linux with MinGW-w64 GCC.
#
# Builds SDL 3.4.16 for MinGW (SDL3.dll), then the port from this working tree with
# port/cmake/mingw-w64-x86_64.cmake (the programs link the GCC runtimes in), runs the test suite
# under Wine (port/tests/CMakeLists.txt, BLUPI_TEST_LAUNCHER) and packs
# build/release/speedy-blupi-sdl3-<version>-windows-x64.zip with package_release.py.
#
#   sh port/tools/windows_release.sh                 # build, test, pack
#   sh port/tools/windows_release.sh -R '<regex>'    # a test subset (ctest arguments)
#
#   BLUPI_WINDOWS_WORK   work directory (default: build/port-windows of this repository)
#   BLUPI_WINDOWS_TESTS  0: build and pack without the test run
#   BLUPI_SDL3_SRC       SDL3 source tree (default: build/third_party/SDL3-3.4.16)
#   BLUPI_MBEDTLS_SRC    Mbed TLS source tree for wss:// (default: build/third_party/mbedtls-3.6.7;
#                        without one CMake downloads it, port/third_party/README.md)
#   BLUPI_DATA, BLUPI2_DATA, BLUPI2_TEST_DATA_BLUPI, BLUPI_TEST_DATA_EGGBERT,
#   BLUPI_TEST_DATA_EGGBERT_DEMO, BLUPI_TEST_DATA_EGGBERT_VALUE, BLUPI_TEST_DATA_BLUPI_DEMO
#                        game data for the tests, as for linux_release.sh (each optional)
#   BLUPI_TEST_RELAY_SERVER  a free-direct-server for the relay multiplayer tests (optional; a Linux
#                        server serves the Windows programs under Wine)
#
# Needs MinGW-w64 with POSIX threads (g++-mingw-w64-x86-64-posix), Wine (wine64), CMake, Ninja,
# Python 3 and ccache. The test run uses its own Wine prefix inside the work directory.
set -eu

REPO=$(cd "$(dirname "$0")/../.." && pwd)
WORK=${BLUPI_WINDOWS_WORK:-$REPO/build/port-windows}
SDL=${BLUPI_SDL3_SRC:-$REPO/build/third_party/SDL3-3.4.16}
MBEDTLS=${BLUPI_MBEDTLS_SRC:-$REPO/build/third_party/mbedtls-3.6.7}
VERSION=$(head -n 1 "$REPO/port/VERSION.txt" | tr -d '\r')
TOOLCHAIN=$REPO/port/cmake/mingw-w64-x86_64.cmake

command -v x86_64-w64-mingw32-g++-posix >/dev/null ||
    { echo 'Needs MinGW-w64 with POSIX threads (g++-mingw-w64-x86-64-posix).' >&2; exit 1; }
command -v wine >/dev/null || { echo 'Needs Wine for the test run.' >&2; exit 1; }
command -v ccache >/dev/null || { echo 'Needs ccache.' >&2; exit 1; }
export CCACHE_DIR=/rv/cnaccache
export CCACHE_BASEDIR=/rv
LAUNCH="-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
mkdir -p "$WORK"

# SDL3 as SDL3.dll, once.
if [ ! -f "$WORK/sdl3/lib/cmake/SDL3/SDL3Config.cmake" ]; then
    cmake -S "$SDL" -B "$WORK/sdl3-build" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$WORK/sdl3" $LAUNCH \
        -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
    cmake --build "$WORK/sdl3-build" -j 4
    cmake --install "$WORK/sdl3-build" >/dev/null
fi

# The tests' emulator: Wine with its own prefix, quiet, and the MinGW runtime DLLs on its path for
# the test programs (the game programs link them in).
GCCDIR=$(dirname "$(x86_64-w64-mingw32-g++-posix -print-libgcc-file-name)")
winpath() { printf 'Z:%s' "$1" | tr '/' '\\'; }
RUN=$WORK/wine-run
cat > "$RUN" <<EOF
#!/bin/sh
# Made by port/tools/windows_release.sh: runs a Windows program of the cross build under Wine.
export WINEPREFIX="$WORK/wine"
export WINEDEBUG=-all
export WINEDLLOVERRIDES="mscoree,mshtml="
export WINEPATH='$(winpath "$GCCDIR");$(winpath /usr/x86_64-w64-mingw32/lib)'
unset DISPLAY WAYLAND_DISPLAY
exec wine "\$@"
EOF
chmod 0755 "$RUN"
if [ ! -f "$WORK/wine/system.reg" ]; then
    WINEPREFIX="$WORK/wine" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" wineboot -i
    WINEPREFIX="$WORK/wine" wineserver -w
fi

B=$WORK/port
cmake -S "$REPO/port" -B "$B" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH="$WORK/sdl3" \
    -DCMAKE_FIND_ROOT_PATH="$WORK/sdl3" $LAUNCH -DCMAKE_CROSSCOMPILING_EMULATOR="$RUN" \
    -DBLUPI_MBEDTLS_SOURCE_DIR="$MBEDTLS" \
    -DBLUPI_TEST_DATA="${BLUPI_DATA:-}" -DBLUPI2_TEST_DATA="${BLUPI2_DATA:-}" \
    -DBLUPI2_TEST_DATA_BLUPI="${BLUPI2_TEST_DATA_BLUPI:-}" \
    -DBLUPI_TEST_DATA_EGGBERT="${BLUPI_TEST_DATA_EGGBERT:-}" \
    -DBLUPI_TEST_DATA_EGGBERT_DEMO="${BLUPI_TEST_DATA_EGGBERT_DEMO:-}" \
    -DBLUPI_TEST_DATA_EGGBERT_VALUE="${BLUPI_TEST_DATA_EGGBERT_VALUE:-}" \
    -DBLUPI_TEST_DATA_BLUPI_DEMO="${BLUPI_TEST_DATA_BLUPI_DEMO:-}" \
    -DBLUPI_TEST_RELAY_SERVER="${BLUPI_TEST_RELAY_SERVER:-}"
cmake --build "$B" -j 4

if [ "${BLUPI_WINDOWS_TESTS:-1}" != 0 ]; then
    # The provenance map check needs the repository, not the program: run it natively.
    python3 "$REPO/port/tools/check_fidelity.py" --check
    ctest --test-dir "$B" -j 4 --output-on-failure -E '^fidelity_map$' "$@"
fi

python3 "$REPO/port/tools/package_release.py" windows --build "$B" --runner "$RUN"
NAME=speedy-blupi-sdl3-$VERSION-windows-x64
echo "imports of the programs (none may be a MinGW runtime DLL):"
for app in blupi-sdl3 blupi2-sdl3 eggbert-demo-sdl3 eggbert-value-sdl3 blupi-demo-sdl3; do
    printf '  %s:' "$app"
    x86_64-w64-mingw32-objdump -p "$B/$app.exe" | sed -n 's/^\tDLL Name: //p' | sort | tr '\n' ' '
    echo
done
echo "package: build/release/$NAME.zip"
