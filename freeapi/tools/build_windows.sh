#!/usr/bin/env bash
# Windows x64 builds, cross-compiled with MinGW-w64 (README.md, "Windows"): SDL3 once, then each
# game with its data and optional higher-resolution images next to the executable, and the Free
# Direct server. Arguments: the games to build (default: sb1-eggbert sb2-eggbert).
set -euo pipefail
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
build_root="$repo_root/build/freeapi-windows"
packages="$build_root/packages"
toolchain="$repo_root/freeapi/cmake/mingw-w64-x86_64.cmake"
data_root="${BLUPI_WINDOWS_DATA_ROOT:-/rv/tmp/speedy-blupi-games}"
hires_root="${BLUPI_WINDOWS_HIRES_ROOT:-$repo_root/../speedyblupi-hd/games}"
hires_scale="${BLUPI_WINDOWS_HIRES_SCALE:-4}"      # 0: no hires folder
fullscreen="${BLUPI_WINDOWS_FULLSCREEN:-1}"         # DATA/config.def's FullScreen
soundfont="${BLUPI_WINDOWS_SOUNDFONT:-$repo_root/../free-eggbert/assets/soundfont/default.sf2}"
soundfont_license="${BLUPI_WINDOWS_SOUNDFONT_LICENSE:-$(dirname -- "$soundfont")/LICENSE.txt}"
enet="${FREE_DIRECT_ENET_SOURCE_DIR:-$HOME/deps/enet}"
ixwebsocket="${FREE_DIRECT_IXWEBSOCKET_SOURCE_DIR:-$HOME/deps/IXWebSocket-v11.4.6}"
variants=("$@")
[[ ${#variants[@]} -gt 0 ]] || variants=(sb1-eggbert sb2-eggbert)

command -v x86_64-w64-mingw32-g++-posix >/dev/null ||
    { echo 'Needs MinGW-w64 with POSIX threads (g++-mingw-w64-x86-64-posix).' >&2; exit 1; }
command -v ccache >/dev/null || { echo 'ccache is required for shared incremental builds.' >&2; exit 1; }
for file in "$soundfont" "$soundfont_license" "$enet/CMakeLists.txt" "$ixwebsocket/CMakeLists.txt"; do
    [[ -f "$file" ]] || { echo "Missing $file" >&2; exit 1; }
done
export CCACHE_DIR=/rv/cnaccache
export CCACHE_BASEDIR=/rv
cmake_args=(-G Ninja -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)

# SDL3, SDL3_image and SDL3_mixer as DLLs (deps/CMakeLists.txt), from the web build's verified
# source archives when they are unpacked, else downloaded and checked by FetchContent.
sdk="$build_root/sdk"
if [[ ! -f "$build_root/deps/build.ninja" ]]; then
    sources="$repo_root/build/web-deps/src"
    fetch=()
    if [[ -d "$sources/SDL-release-3.4.0" ]]; then
        fetch=(-DFETCHCONTENT_SOURCE_DIR_SDL3="$sources/SDL-release-3.4.0"
               -DFETCHCONTENT_SOURCE_DIR_SDL3_IMAGE="$sources/SDL_image-release-3.4.0"
               -DFETCHCONTENT_SOURCE_DIR_SDL3_MIXER="$sources/SDL_mixer-release-3.2.0")
    fi
    cmake -S "$repo_root/freeapi/deps" -B "$build_root/deps" "${cmake_args[@]}" \
        -DCMAKE_INSTALL_PREFIX="$sdk" -DFETCHCONTENT_UPDATES_DISCONNECTED=ON "${fetch[@]}"
fi
cmake --build "$build_root/deps" --parallel 4
cmake --install "$build_root/deps" >/dev/null

notices=(--notice SDL3.txt "$sdk/share/licenses/SDL3/LICENSE.txt"
         --notice SDL3_image.txt "$sdk/share/licenses/SDL3_image/LICENSE.txt"
         --notice FreeAPI.txt "$repo_root/../free-api/LICENSE"
         --notice FreeDirect.txt "$repo_root/../free-direct/LICENSE"
         --notice tsf.h "$repo_root/../free-api/external/tsf.h"
         --notice tml.h "$repo_root/../free-api/external/tml.h"
         --notice ENet.txt "$enet/LICENSE"
         --notice IXWebSocket.txt "$ixwebsocket/LICENSE.txt"
         --notice MinGW-w64.txt /usr/share/doc/mingw-w64-x86-64-dev/copyright
         --notice SoundFont.txt "$soundfont_license")
for variant in "${variants[@]}"; do
    game=${variant%%-*}
    edition=${variant#*-}
    cmake -S "$repo_root/freeapi" -B "$build_root/$variant" "${cmake_args[@]}" \
        -DCMAKE_FIND_ROOT_PATH="$sdk" -DCMAKE_PREFIX_PATH="$sdk" \
        -DBLUPI_GAME="$game" -DBLUPI_EDITION="$edition" \
        -DFREE_DIRECT_ENET_SOURCE_DIR="$enet" -DFREE_DIRECT_IXWEBSOCKET_SOURCE_DIR="$ixwebsocket" \
        -DFREE_DIRECT_RELAY_TLS=OFF
    cmake --build "$build_root/$variant" --parallel 4
    exe=blupi-freeapi.exe
    [[ $game == sb2 ]] && exe=blupi2-freeapi.exe
    python3 "$repo_root/freeapi/tools/package_windows.py" game --variant "$variant" \
        --exe "$build_root/$variant/$exe" --dll "$sdk/bin/SDL3.dll" --dll "$sdk/bin/SDL3_image.dll" \
        --data-root "$data_root" --hires-root "$hires_root" --hires-scale "$hires_scale" \
        --fullscreen "$fullscreen" --soundfont "$soundfont" "${notices[@]}" --output "$packages"
done

# The server needs neither SDL nor Free API; -static leaves out the MinGW runtime DLLs.
cmake -S "$repo_root/../free-direct/server" -B "$build_root/server" "${cmake_args[@]}" \
    -DFREE_DIRECT_IXWEBSOCKET_SOURCE_DIR="$ixwebsocket" -DFREE_DIRECT_RELAY_TLS=OFF \
    -DCMAKE_EXE_LINKER_FLAGS=-static
cmake --build "$build_root/server" --parallel 4
python3 "$repo_root/freeapi/tools/package_windows.py" server --exe "$build_root/server/free-direct-server.exe" \
    --notice FreeDirect.txt "$repo_root/../free-direct/LICENSE" \
    --notice IXWebSocket.txt "$ixwebsocket/LICENSE.txt" \
    --notice MinGW-w64.txt /usr/share/doc/mingw-w64-x86-64-dev/copyright --output "$packages"
printf 'Windows packages: %s\n' "$packages"
