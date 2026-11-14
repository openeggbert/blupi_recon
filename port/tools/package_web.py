#!/usr/bin/env python3
"""Assemble the web build of the SDL3 port (port/docs/WEB.md) into a static site and a zip.

    python3 port/tools/package_web.py [--build build/port-web/port] [--site build/port-web/site]

The site holds the loader (port/web), each program's .js and .wasm in a folder of its own, the
GeneralUser GS SoundFont and the notices. It contains no game data: players choose their own game
folder in the browser. The zip is build/release/speedy-blupi-sdl3-<version>-web.zip.
"""
import argparse
import os
import shutil
import zipfile

PORT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(PORT)
PROGRAMS = ("blupi-sdl3", "blupi2-sdl3", "eggbert-demo-sdl3", "eggbert-value-sdl3", "blupi-demo-sdl3")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", default=os.path.join(ROOT, "build", "port-web", "port"))
    parser.add_argument("--site", default=os.path.join(ROOT, "build", "port-web", "site"))
    parser.add_argument("--sdl3-source", default=os.path.join(ROOT, "build", "third_party", "SDL3-3.4.16"))
    parser.add_argument("--no-zip", action="store_true",
                        help="only the site: leave build/release (a released version's zip) untouched")
    args = parser.parse_args()
    version = open(os.path.join(PORT, "VERSION.txt"), encoding="utf-8").readline().strip()
    shutil.rmtree(args.site, ignore_errors=True)
    os.makedirs(args.site)
    for name in ("index.html", "loader.js", "style.css", "serve.py"):
        shutil.copy(os.path.join(PORT, "web", name), args.site)
    with open(os.path.join(PORT, "web", "README.txt"), encoding="utf-8") as f:
        readme = f.read().replace("@VERSION@", version)
    with open(os.path.join(args.site, "README.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write(readme)
    for program in PROGRAMS:
        os.makedirs(os.path.join(args.site, program))
        for suffix in (".js", ".wasm"):
            source = os.path.join(args.build, program + suffix)
            if not os.path.isfile(source):
                raise SystemExit("missing " + source + ": build the web programs first (port/docs/WEB.md)")
            shutil.copy(source, os.path.join(args.site, program))
    soundfont = os.path.join(args.site, "soundfont")
    os.makedirs(soundfont)
    gs = os.path.join(PORT, "third_party", "generaluser-gs")
    shutil.copy(os.path.join(gs, "GeneralUser-GS.sf2"), os.path.join(soundfont, "soundfont.sf2"))
    shutil.copy(os.path.join(gs, "documentation", "LICENSE.txt"), os.path.join(soundfont, "LICENSE-GeneralUser-GS.txt"))
    for name in ("THIRD_PARTY.txt", "COPYING.txt"):
        shutil.copy(os.path.join(PORT, "release", name), args.site)
    shutil.copy(os.path.join(args.sdl3_source, "LICENSE.txt"), os.path.join(args.site, "LICENSE-SDL3.txt"))
    if args.no_zip:
        print("site: " + os.path.relpath(args.site, ROOT))
        return

    out = os.path.join(ROOT, "build", "release")
    os.makedirs(out, exist_ok=True)
    name = "speedy-blupi-sdl3-%s-web" % version
    path = os.path.join(out, name + ".zip")
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for folder, _dirs, files in sorted(os.walk(args.site)):
            for f in sorted(files):
                full = os.path.join(folder, f)
                z.write(full, name + "/" + os.path.relpath(full, args.site).replace(os.sep, "/"))
    print("site: " + os.path.relpath(args.site, ROOT))
    print("package: " + os.path.relpath(path, ROOT))


if __name__ == "__main__":
    main()
