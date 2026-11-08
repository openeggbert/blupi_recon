#!/usr/bin/env bash
# Build dependencies once, then each executable and its game data sequentially.
set -euo pipefail
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
build_root="$repo_root/build/freeapi-web"
web_root="$build_root/site"
data_root="${BLUPI_WEB_DATA_ROOT:-/rv/tmp/speedy-blupi-games}"
soundfont="${BLUPI_WEB_SOUNDFONT:-$repo_root/../free-eggbert/assets/soundfont/default.sf2}"
soundfont_license="${BLUPI_WEB_SOUNDFONT_LICENSE:-$(dirname -- "$soundfont")/LICENSE.txt}"
if [[ ! -f "$soundfont" ]]; then
    echo "Missing bundled MIDI SoundFont: $soundfont (set BLUPI_WEB_SOUNDFONT)" >&2
    exit 1
fi
if [[ ! -f "$soundfont_license" ]]; then
    echo "Missing bundled SoundFont licence: $soundfont_license (set BLUPI_WEB_SOUNDFONT_LICENSE)" >&2
    exit 1
fi
command -v emcmake >/dev/null || { echo 'Activate emsdk first: source /path/to/emsdk/emsdk_env.sh' >&2; exit 1; }
command -v ccache >/dev/null || { echo 'ccache is required for shared incremental builds.' >&2; exit 1; }
export CCACHE_DIR=/rv/cnaccache
export CCACHE_BASEDIR=/rv
cmake_args=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
if [[ ! -f "$build_root/deps/build.ninja" || $# -gt 0 ]]; then
    emcmake cmake -S "$repo_root/freeapi/deps" -B "$build_root/deps" -G Ninja \
        -DCMAKE_INSTALL_PREFIX="$build_root/sdk" "${cmake_args[@]}" "$@"
fi
cmake --build "$build_root/deps" --parallel 4
cmake --install "$build_root/deps"
mkdir -p "$web_root"
cp "$repo_root/freeapi/web/index.html" "$repo_root/freeapi/web/style.css" "$web_root/"
cp "$repo_root/freeapi/web/README.txt" "$repo_root/freeapi/tools/serve_web.py" "$web_root/"
mkdir -p "$web_root/licenses"
cp "$build_root/sdk/share/licenses/SDL3/LICENSE.txt" "$web_root/licenses/SDL3.txt"
cp "$build_root/sdk/share/licenses/SDL3_image/LICENSE.txt" "$web_root/licenses/SDL3_image.txt"
cp "$build_root/sdk/share/licenses/SDL3_mixer/LICENSE.txt" "$web_root/licenses/SDL3_mixer.txt"
cp "$repo_root/../free-api/LICENSE" "$web_root/licenses/FreeAPI.txt"
cp "$repo_root/../free-direct/LICENSE" "$web_root/licenses/FreeDirect.txt"
# These headers carry their own upstream notices; retain them verbatim.
cp "$repo_root/../free-api/external/tsf.h" "$repo_root/../free-api/external/tml.h" "$web_root/licenses/"
cp "$repo_root/LICENSE" "$web_root/COPYING.txt"
mkdir -p "$web_root/soundfont"
if ! cmp -s "$soundfont" "$web_root/soundfont/default.sf2"; then
    cp "$soundfont" "$web_root/soundfont/default.sf2"
fi
cp "$soundfont_license" "$web_root/licenses/SoundFont.txt"
for game in sb1 sb2; do
    for edition in blupi eggbert; do
        variant="$game-$edition"
        emcmake cmake -S "$repo_root/freeapi" -B "$build_root/$variant" -G Ninja \
            -DCMAKE_BUILD_TYPE=Release -DCMAKE_FIND_ROOT_PATH="$build_root/sdk" \
            -DCMAKE_PREFIX_PATH="$build_root/sdk" \
            -DBLUPI_GAME="$game" -DBLUPI_EDITION="$edition" \
            -DBLUPI_WEB_DATA_ROOT="$data_root" "${cmake_args[@]}"
        cmake --build "$build_root/$variant" --parallel 4
        mkdir -p "$web_root/$variant"
        cp "$build_root/$variant/web/"* "$web_root/$variant/"
    done
done
python3 "$repo_root/freeapi/tools/package_web.py" --site "$web_root" \
    --archive "$build_root/blupi-four-games-web.zip"
printf 'Built all four games: %s\n' "$web_root"
