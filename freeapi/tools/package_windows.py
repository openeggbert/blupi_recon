#!/usr/bin/env python3
"""Lay out a Windows package next to the executable and zip it (README.md, "Windows").

A game package holds the executable with SDL's DLLs, the game's own folders (DATA, IMAGE08,
IMAGE16, SOUND) from an installation, optional higher-resolution images in hires/, the SoundFont
and the notices. A server package holds free-direct-server.exe. The installation, the image pack
and the original executables are never modified, packaged or run: images and sounds are
hard-linked into the package folder where possible (they are only read), DATA is copied because
the game writes its players and saved games there.
"""
import argparse
import os
import re
import shutil
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WINDOWS = REPO / 'freeapi' / 'windows'
# Installation folder (under the data root, and under speedyblupi-hd's games/), package name, title.
VARIANTS = {
    'sb1-blupi': ('Speedy_Blupi_I', 'SpeedyBlupi1', 'Speedy Blupi 1'),
    'sb1-eggbert': ('Speedy Eggbert/Game', 'SpeedyEggbert1', 'Speedy Eggbert 1'),
    'sb2-blupi': ('Speedy_Blupi_II', 'SpeedyBlupi2', 'Speedy Blupi 2'),
    'sb2-eggbert': ('Speedy_Eggbert_2', 'SpeedyEggbert2', 'Speedy Eggbert 2'),
}
# Official levels, recorded demos and the configuration; no player profiles, saved games,
# benchmark results or user missions of the installation (as tools/stage_web_data.py).
DATA_FILES = re.compile(r'(?:world\d{3}|demo\d{3})\.blp|config\.def', re.IGNORECASE)


def child(directory, name):
    """The entry of directory called name, ignoring case (installations differ)."""
    for entry in directory.iterdir():
        if entry.name.lower() == name.lower():
            return entry
    raise FileNotFoundError(f'No {name} in {directory}')


def link_tree(source, target):
    """Hard-link (or copy, across file systems) every file of source into target."""
    count = 0
    for file in sorted(source.rglob('*')):
        if not file.is_file():
            continue
        destination = target / file.relative_to(source)
        destination.parent.mkdir(parents=True, exist_ok=True)
        try:
            os.link(file, destination)
        except OSError:
            shutil.copy2(file, destination)
        count += 1
    return count


def fresh(directory):
    """An empty package folder inside build/: never write into an existing one, because its
    files may be hard links to an installation."""
    if not directory.resolve().is_relative_to(REPO / 'build'):
        raise ValueError(f'Packages are written under {REPO / "build"} only')
    if directory.exists():
        shutil.rmtree(directory)
    directory.mkdir(parents=True)


def write_zip(folder, archive):
    archive.parent.mkdir(parents=True, exist_ok=True)
    partial = archive.with_suffix('.zip.partial')
    with zipfile.ZipFile(partial, 'w', zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as z:
        for file in sorted(folder.rglob('*')):
            if file.is_file():
                z.write(file, Path(folder.name) / file.relative_to(folder))
    partial.replace(archive)
    print(f'{archive} ({archive.stat().st_size // (1024 * 1024)} MB)')


def copy_notices(licenses, notices):
    licenses.mkdir(parents=True, exist_ok=True)
    for name, source in notices:
        shutil.copy2(source, licenses / name)


def package_game(args):
    install_name, package_name, title = VARIANTS[args.variant]
    install = args.data_root / install_name
    pack = args.hires_root / install_name / f'{args.hires_scale}x' if args.hires_scale else None
    if pack is not None and not pack.is_dir():
        raise FileNotFoundError(f'No {args.hires_scale}x image pack: {pack}')
    suffix = f'-{args.hires_scale}x' if pack else ''
    folder = args.output / f'{package_name}-Windows-x64{suffix}'
    fresh(folder)

    shutil.copy2(args.exe, folder / f'{package_name}.exe')
    for dll in args.dll:
        shutil.copy2(dll, folder / dll.name)
    for name in ('IMAGE08', 'IMAGE16', 'SOUND'):
        link_tree(child(install, name), folder / name)
    data = folder / 'DATA'
    data.mkdir()
    for file in sorted(child(install, 'DATA').iterdir()):
        if file.is_file() and DATA_FILES.fullmatch(file.name):
            shutil.copy2(file, data / file.name)
    config = next(p for p in data.iterdir() if p.name.lower() == 'config.def')
    # The game's own full screen shows the images at the display's resolution (README.md).
    text = config.read_bytes().decode('latin-1')
    setting = f'FullScreen={args.fullscreen}'
    text, found = re.subn(r'FullScreen=\d+', setting, text)
    if not found:
        text = text.rstrip('\r\n') + '\r\n' + setting + '\r\n'
    config.write_bytes(text.encode('latin-1'))
    if pack:
        for name in ('IMAGE08', 'IMAGE16'):
            link_tree(child(pack, name), folder / 'hires' / name)
    (folder / 'soundfont').mkdir()
    shutil.copy2(args.soundfont, folder / 'soundfont' / 'default.sf2')

    readme = (WINDOWS / 'README-game.txt').read_text(encoding='utf-8')
    readme = readme.replace('@TITLE@', title).replace('@EXE@', f'{package_name}.exe')
    hires = (f'The folder hires holds images {args.hires_scale} times the original resolution; '
             'the game uses them automatically.') if pack else 'This package has no hires folder.'
    readme = readme.replace('@HIRES@', hires)
    (folder / 'README.txt').write_text(readme.replace('\n', '\r\n'), encoding='utf-8', newline='')
    shutil.copy2(REPO / 'LICENSE', folder / 'COPYING.txt')
    copy_notices(folder / 'licenses', args.notice)
    write_zip(folder, args.output / f'{folder.name}.zip')


def package_server(args):
    folder = args.output / 'FreeDirectServer-Windows-x64'
    fresh(folder)
    shutil.copy2(args.exe, folder / 'free-direct-server.exe')
    script = (WINDOWS / 'start-server.bat').read_text(encoding='ascii')
    (folder / 'start-server.bat').write_text(script.replace('\n', '\r\n'), encoding='ascii', newline='')
    readme = (WINDOWS / 'README-server.txt').read_text(encoding='utf-8')
    (folder / 'README.txt').write_text(readme.replace('\n', '\r\n'), encoding='utf-8', newline='')
    copy_notices(folder / 'licenses', args.notice)
    write_zip(folder, args.output / f'{folder.name}.zip')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=['game', 'server'])
    parser.add_argument('--variant', choices=sorted(VARIANTS))
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--dll', type=Path, action='append', default=[])
    parser.add_argument('--data-root', type=Path)
    parser.add_argument('--hires-root', type=Path)
    parser.add_argument('--hires-scale', type=int, default=0, help='0: no hires folder')
    parser.add_argument('--fullscreen', type=int, choices=[0, 1], default=1)
    parser.add_argument('--soundfont', type=Path)
    parser.add_argument('--notice', nargs=2, action='append', default=[], metavar=('NAME', 'FILE'),
                        type=str, help='licenses/NAME from FILE')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.notice = [(name, Path(file)) for name, file in args.notice]
    if args.kind == 'game':
        if not (args.variant and args.data_root and args.soundfont):
            parser.error('game packages need --variant, --data-root and --soundfont')
        if args.hires_scale and not args.hires_root:
            parser.error('--hires-scale needs --hires-root')
        package_game(args)
    else:
        package_server(args)


if __name__ == '__main__':
    main()
