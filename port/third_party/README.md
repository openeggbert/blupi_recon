# Third-party components of the port

Unmodified upstream material. Do not edit files in here; update by replacing them (vendored
headers) or moving the submodule pin, and record the new version and checksum below.

## tinysoundfont/ (vendored headers)

TinySoundFont v0.9 (MIT) and TinyMidiLoader v0.7 (zlib) by Bernhard Schelling; details and
SHA-256 in [tinysoundfont/README.md](tinysoundfont/README.md).

## enet/ (vendored sources)

ENet 1.3.18 (MIT) by Lee Salzman, the UDP transport under the native multiplayer
(`port/common/net/`); release tag, archive and per-file SHA-256 in [enet/README.md](enet/README.md).

## Mbed TLS (not in the repository; a pinned release archive)

Mbed TLS 3.6.7, the 3.6 long-term-support branch (Apache-2.0 OR GPL-2.0-or-later; the packages
use it under Apache-2.0 and carry its `LICENSE` as `LICENSE-MbedTLS.txt`). The desktop and Android
programs speak `wss://` to a Free Direct server through it (`port/common/net/tls.h`,
[MULTIPLAYER.md](../docs/MULTIPLAYER.md)); the web version uses the browser's TLS. It is 8 MB of
sources, so it is not vendored: the build compiles `library/*.c` of its release archive as it comes
(its default configuration), from

| | |
|---|---|
| Release | https://github.com/Mbed-TLS/mbedtls/releases/tag/mbedtls-3.6.7 (2026-07-07) |
| Archive | `mbedtls-3.6.7.tar.bz2`, 5,473,689 bytes, SHA-256 `a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6` (as in the release's `mbedtls-3.6.7-sha256sum.txt` and GitHub's asset digest) |
| Where the build looks | `BLUPI_MBEDTLS_SOURCE_DIR`, by default `build/third_party/mbedtls-3.6.7` of the repository (the archive unpacked; Android: `-Pblupi.mbedtls.dir`, release scripts: `BLUPI_MBEDTLS_SRC`) |
| Without it | CMake downloads the archive and checks the SHA-256 (FetchContent); `-DBLUPI_TLS=OFF` builds without TLS, and the programs then refuse `wss://` |

CMake checks that the tree is 3.6.7 (`MBEDTLS_VERSION_STRING`). To update: move to a newer 3.6.x
release, change the version and the SHA-256 in `port/CMakeLists.txt` (and the default paths in
`port/android/app/build.gradle` and the release scripts), and record them here.

## generaluser-gs/ (git submodule)

GeneralUser GS v2.0.3, the General MIDI / GS SoundFont by S. Christian Collins that the port uses
to play the game's MIDI music (the original used the Windows MIDI synthesizer through MCI).

| | |
|---|---|
| Repository | https://github.com/mrbumpy409/GeneralUser-GS (the author's), project page https://www.schristiancollins.com/generaluser |
| Pinned commit | `684543d5e5efaef08d02be50dcda8d552478fa60` ("update documentation to r6", 2026-02-23) |
| File used | `GeneralUser-GS.sf2`, 32,319,396 bytes, SHA-256 `9575028c7a1f589f5770fccc8cff2734566af40cd26ed836944e9a5152688cfe` (identical to the copy Free Eggbert ships) |
| Licence | GeneralUser GS License v2.0, `generaluser-gs/documentation/LICENSE.txt`: free use including in software projects; the author asks for local copies rather than links to his download files |

The submodule is `shallow = true` in `.gitmodules`: initialising it fetches only the pinned
commit (about 62 MB, of which 31 MB are upstream's `demo MIDIs` folder that the port does not
use), instead of the full 94 MB history. Our repository stores only the commit pin.

```powershell
git submodule update --init port/third_party/generaluser-gs
```

The build copies `GeneralUser-GS.sf2` next to the executable as `soundfont.sf2`
(`BLUPI_BUNDLE_SOUNDFONT`, on by default). Without the submodule, CMake warns and the game runs
without music.
