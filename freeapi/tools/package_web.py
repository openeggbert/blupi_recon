#!/usr/bin/env python3
"""Refresh the browser shell around existing WASM builds; optionally package it."""
import argparse
from pathlib import Path
from shutil import copyfile
from zipfile import ZIP_DEFLATED, ZipFile


def main():
    repo = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site', type=Path, default=repo / 'build/freeapi-web/site')
    parser.add_argument('--archive', type=Path)
    args = parser.parse_args()
    site, web = args.site.resolve(), repo / 'freeapi/web'
    for game in ('sb1', 'sb2'):
        for edition in ('blupi', 'eggbert'):
            destination = site / f'{game}-{edition}'
            for name in ('game.js', 'game.wasm', 'game.data'):
                if not (destination / name).is_file():
                    parser.error(f'Missing compiled game: {destination / name}')
            html = (web / 'game.html.in').read_text().replace('@BLUPI_GAME@', game).replace('@BLUPI_EDITION@', edition)
            (destination / 'index.html').write_text(html)
            for name in ('player.js', 'style.css'):
                copyfile(web / name, destination / name)
    for name in ('index.html', 'style.css', 'README.txt', 'bootstrap.js', 'coi-serviceworker.js'):
        copyfile(web / name, site / name)
    copyfile(repo / 'freeapi/tools/serve_web.py', site / 'serve_web.py')
    (site / 'licenses').mkdir(exist_ok=True)
    copyfile(web / 'coi-serviceworker.LICENSE.txt', site / 'licenses/coi-serviceworker.txt')
    if args.archive:
        with ZipFile(args.archive, 'w', compression=ZIP_DEFLATED, compresslevel=9) as archive:
            for file in sorted(site.rglob('*')):
                if file.is_file():
                    assert file.suffix.lower() not in {'.exe', '.dll', '.com', '.bat'}
                    archive.write(file, file.relative_to(site))
        print(f'Web archive: {args.archive} ({args.archive.stat().st_size} bytes)')
    print(f'Browser shell refreshed: {site}')


if __name__ == '__main__':
    main()
