# Releasing the port

A release is four packages built from one commit, each tested before it is packed. Since 1.5.0 all of
them are built on one Linux host (Debian 13); until 1.2.0 the Windows and Android packages were built on
Windows and the Linux one in WSL Ubuntu 22.04 (the PowerShell steps below).

| Package | Built with | Contents |
|---|---|---|
| `speedy-blupi-sdl3-<version>-windows-x64.zip` | MinGW-w64 GCC: `port/tools/windows_release.sh` (cross, tests under Wine), or the `mingw-x64` preset on Windows | the five programs (GCC runtimes linked in, stripped), `SDL3.dll`, `soundfont.sf2` |
| `speedy-blupi-sdl3-<version>-linux-x86_64.tar.gz` | `port/tools/linux_release.sh` (the host's glibc is the minimum) | the five programs (`BLUPI_PORTABLE`: RUNPATH `$ORIGIN`, static libstdc++/libgcc), `libSDL3.so.0` built from SDL 3.4.16 with its backends loaded at run time, `soundfont.sf2` |
| `speedy-blupi-sdl3-<version>-android.apk` | Gradle `assembleRelease` | arm64-v8a and x86_64, signed with the release key |
| `speedy-blupi-sdl3-<version>-web.zip` | Emscripten, `port/tools/package_web.py` (port/docs/WEB.md) | the five programs as WebAssembly, the page, the SoundFont, `serve.py` |

Every desktop package also has `README.txt` (the player's guide, `port/release/README.txt`),
`COPYING.txt` (GPLv3, the port's licence: its source derives from Planet Blupi and the SB2
reconstruction), `THIRD_PARTY.txt` and the licences of SDL and GeneralUser GS. The APK has the
same notices under `assets/licenses/`. The README names the source: the public repository at
the release tag. Game data is never packaged: players use their own copy.

Both desktop programs use `common/app` for discovery, options/help, preferences,
SoundFont lookup and the main loop. CMake targets remain `blupi` and `blupi2`; the
single `blupi_package_app` helper in `cmake/package_app.cmake` packages their runtime
dependencies and selects the Windows GUI subsystem outside Debug. Each can run from
its original game's folder or use the first-run chooser. Settings and environment
names are separate (`blupi-sdl3.json`/`BLUPI_DATA`, `blupi2-sdl3.json`/`BLUPI2_DATA`).
The Android app contains both games (`libmain.so`, `libblupi2.so`). The Linux scripts copy SB2's historical source tree
and enable both SB2 editions' tests when their data is present. Desktop archives now
carry both executables; `package_release.py windows` verifies both version strings.

## Version

`port/VERSION.txt` holds the version (`1.0.0`). CMake makes it the project version and
`BLUPI_PORT_VERSION` (`--version`, the start-up log, the Display options screen, the Windows
version resource). The Android build reads it as `versionName`, with `versionCode`
major·10000 + minor·100 + patch. Git tags are `port-v<version>`, which keeps them apart from
the matching reconstruction's history.

## Icon

`port/res/icon.png` (the game's waving Blupi, sprite 47 of `BLUPI000.BLP`, on a 64×64 canvas)
and `port/res/icon.ico` (Windows, 16–256 px) are made by
`.pyenv\python.exe port\tools\make_icon.py "<game folder>"` and committed. The Linux window
loads the PNG (`common/platform/icon.cpp`), the Windows executable carries the ICO, and the Android
build draws its launcher icons from the PNG (`launcherIcon` task).

## Android signing key

The release key is `~/.android/speedy-blupi-release.jks` (Windows: `%USERPROFILE%\.android\`), described
by `speedy-blupi-release.properties` next to it (`storeFile`, `storePassword`, `keyAlias`, `keyPassword`).
Both stay outside the repository; back them up together. Android installs an update only when it is
signed with the same key, so every release must use this one.

- **Since 1.4.0** (generated 2026-11-12 on the Linux release host, `CN=Speedy Blupi SDL3 port, O=Open
  Eggbert`, RSA 4096, valid until 2056): certificate SHA-256
  `a5:8d:e3:7f:be:c6:05:fc:d7:73:ab:6d:41:79:26:eb:0b:e2:3c:8d:bb:b0:9c:d5:66:1f:07:4a:55:f9:b2:a8`.
- **Until 1.2.0:** `9e:fe:f0:c4:13:d0:22:9b:a3:49:5c:4e:66:05:f5:7b:b2:4b:d1:7f:40:ce:8b:69:19:f9:b0:0b:b6:d6:fd:12`.
  That key was not available when 1.4.0's APK was built, so 1.4.0 and later do not install over 1.2.0:
  players uninstall it first (the release notes say so).

Without the properties file `assembleRelease` leaves the APK unsigned; another location can be given
with `-Pblupi.signing=<file>`.

An APK signed with the release key does not install over a debug build (a different key):
uninstall the debug build first, which also removes its imported game data.

## Steps on Linux (since 1.5.0)

1. Set the version in `port/VERSION.txt`.
2. Each script builds in the repository's `build/`, runs the test suite with the game data given in
   its environment (unplayed copies: the goldens were recorded with those; a played copy holds the game's
   own saves, which change the traces) and packs into `build/release/`:
   ```bash
   export BLUPI_DATA=<Speedy Blupi> BLUPI2_DATA=<Speedy Eggbert 2> BLUPI2_TEST_DATA_BLUPI=<Speedy Blupi 2> \
          BLUPI_TEST_DATA_EGGBERT=<Speedy Eggbert> BLUPI_TEST_DATA_EGGBERT_DEMO=<the demo> \
          BLUPI_TEST_DATA_EGGBERT_VALUE=<Valueware> BLUPI_TEST_DATA_BLUPI_DEMO=<the June demo> \
          BLUPI_TEST_RELAY_SERVER=<free-direct-server>
   BLUPI_LINUX_WORK=$PWD/build/port-release sh port/tools/linux_release.sh
   sh port/tools/windows_release.sh          # MinGW-w64 and Wine
   ```
3. Android (JDK 17 or 21, the SDK with NDK 28.2.13676358, the release key above):
   ```bash
   cd port/android && ANDROID_HOME=~/Android/Sdk CMAKE_CXX_COMPILER_LAUNCHER=ccache \
     CMAKE_C_COMPILER_LAUNCHER=ccache bash ./gradlew --no-daemon assembleRelease && cd ../..
   python3 port/tools/package_release.py android
   ```
4. Web: build as in port/docs/WEB.md, `python3 port/tools/package_web.py`, then
   `port/tools/web_check.mjs` in Chrome and with `--browser firefox`, and with `--relay`.
5. `python3 port/tools/package_release.py sums`, tag `port-v<version>`, push the tag, and publish a
   GitHub release with the four packages, `SHA256SUMS.txt` and the notes from `docs/RELEASE_NOTES.md`.
   Publishing it runs `.github/workflows/pages.yml`, which puts that release's web package on GitHub
   Pages (https://openeggbert.github.io/blupi_recon/).

## Steps on Windows (until 1.2.0)

1. Set the version in `port/VERSION.txt`.
2. Windows: build and test, then pack.
   ```powershell
   $env:SDL3_DIR = "C:\tools\sdl3-3.4.16-mingw\SDL3-3.4.16\x86_64-w64-mingw32"
   $env:BLUPI_DATA = "D:\Prog\blupi_recon\assets\Speedy Blupi"
   cmake --preset mingw-x64 -S port
   cmake --build build/port -j 4
   ctest --test-dir build/port --output-on-failure     # SDL3.dll on PATH
   .pyenv\python.exe port\tools\package_release.py windows
   ```
3. Linux, in the Ubuntu 22.04 distro (builds SDL and the game, runs the whole suite, packs):
   ```powershell
   wsl -d Ubuntu-22.04 -u root -e sh port/tools/linux_release.sh
   ```
   It prints the newest glibc symbol version the package needs; it must stay at 2.35.
4. Android:
   ```powershell
   $env:JAVA_HOME = "<a JDK 17>"; $env:ANDROID_HOME = "$env:LOCALAPPDATA\Android\Sdk"
   cd port\android; .\gradlew.bat --no-daemon assembleRelease; cd ..\..
   .pyenv\python.exe port\tools\package_release.py android sums
   ```
5. Smoke-test each package from a clean folder (bounded runs; on Windows with a PATH of only
   `C:\Windows\System32`, so no MinGW DLL is picked up by accident).
6. Tag `port-v<version>` on the release commit and publish a GitHub release with the three
   packages, `SHA256SUMS.txt` and the notes from `docs/RELEASE_NOTES.md`.

## 1.5.0

All four packages from one commit on the Debian 13 host, with the Linux steps above, and the web
package deployed to GitHub Pages by the new workflow. Test data: unplayed copies of every edition (see
docs/STATUS.md for which tests ran where).

## 1.4.0

The Windows and Android packages were added to the release afterwards: the Windows zip with
`windows_release.sh` from the tag's sources (its suite under Wine, the package checked on Windows 10),
the APK with Gradle from an export of the tag (with the SoundFont submodule, which `git archive` leaves
out), signed with the new key and not run.

As 1.3.0 for the Linux package, plus the web package: the five programs built with Emscripten and
`port/tools/package_web.py` (`port/docs/WEB.md`), checked in Chrome with `port/tools/web_check.mjs`
(each program: import of the game folder, title screen, attract mode, start again from the stored
files).

## 1.3.0

Only the Linux package was built for 1.3.0, on the release host itself (Debian 13, glibc 2.41), with
`BLUPI_LINUX_WORK` inside the repository's `build/`. The SDL 3.4.16 sources came from the shared
`~/deps/SDL3` checkout (its `release-3.4.16` tag). The test run excluded `fidelity_map` (it is run on the
real tree), and the goldens that fail with that host's copies of the game data on the commit before
this round's port changes too (`golden_editor8/16`, `golden_tour16_de`, `golden_eggbert_attract16`;
their state hash differs from the first checkpoint on, so the data differs from the data the goldens
were recorded with). The Speedy Blupi 2 / Speedy Eggbert 2 tests were not run (that host's SB2 data
fails their goldens the same way). The package needs glibc 2.38 (objdump of its programs and libSDL3.so.0), not 2.35. Windows and Android packages of
1.3.0 have not been built.

## The Ubuntu 22.04 build distro

Installed once with `wsl --install -d Ubuntu-22.04 --no-launch` and used as root. Its packages:

```
apt-get install -y build-essential cmake ninja-build rsync pkg-config \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxtst-dev \
  libxfixes-dev libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev \
  libegl-dev libgles-dev libgl-dev libdrm-dev libgbm-dev \
  libasound2-dev libpulse-dev libpipewire-0.3-dev libudev-dev libdbus-1-dev libibus-1.0-dev
```

The everyday Linux check (`port/tools/linux_check.sh`) keeps running in the default distro with
its system SDL; the release distro only exists so the package runs on older systems.
