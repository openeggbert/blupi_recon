#!/usr/bin/env python3
"""Copy one installation's game data into an isolated Emscripten preload tree."""
import argparse
import hashlib
import json
import re
from pathlib import Path


ASSET_EXTENSIONS = {'.blp', '.bmp', '.wav', '.mid', '.midi', '.pal', '.png', '.jpg', '.jpeg'}
DIRECTORIES = {'DATA': 'data_seed', 'IMAGE08': 'image08', 'IMAGE16': 'image16', 'SOUND': 'sound'}


def stage(source, output):
    repo = Path(__file__).resolve().parents[2]
    if not any(output.is_relative_to(repo / name) for name in ('build', 'scratch')):
        raise ValueError('Stage game data inside the repository build/ or scratch/ directory')
    sources = {p.name.lower(): p for p in source.iterdir() if p.is_dir()}
    files, excluded, destinations = [], [], set()
    for original, virtual in DIRECTORIES.items():
        directory = sources.get(original.lower())
        if directory is None:
            raise ValueError(f'Missing {original} in {source}')
        count = 0
        for file in sorted(directory.rglob('*')):
            if not file.is_file():
                continue
            relative = file.relative_to(directory).as_posix().lower()
            # Seed official levels and replays, without installation-specific
            # player profiles, saved games, benchmark output or user missions.
            accepted = (bool(re.fullmatch(r'(?:world\d{3}|demo\d{3})\.blp|config\.def', relative))
                        if original == 'DATA' else file.suffix.lower() in ASSET_EXTENSIONS)
            if not accepted:
                excluded.append(file.relative_to(source).as_posix())
                continue
            destination = f'{virtual}/{relative}'
            if destination in destinations:
                raise ValueError(f'Case-insensitive filename collision: {destination}')
            destinations.add(destination)
            content = file.read_bytes()
            target = output / destination
            target.parent.mkdir(parents=True, exist_ok=True)
            if not target.is_file() or target.read_bytes() != content:
                target.write_bytes(content)
            files.append({'source': file.relative_to(source).as_posix(), 'path': destination,
                          'size': len(content), 'sha256': hashlib.sha256(content).hexdigest()})
            count += 1
        if not count:
            raise ValueError(f'No supported game data in {directory}')
    if 'data_seed/config.def' not in destinations:
        raise ValueError(f'Missing DATA/config.def in {source}')
    # Prune only the four generated subtrees, never the source installation.
    for virtual in DIRECTORIES.values():
        for target in (output / virtual).rglob('*'):
            if target.is_file() and target.relative_to(output).as_posix() not in destinations:
                target.unlink()
    manifest = {'source': str(source), 'files': files, 'excluded': excluded}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'Staged {len(files)} files ({sum(f["size"] for f in files)} bytes) from {source}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    stage(args.source.resolve(), args.output.resolve())


if __name__ == '__main__':
    main()
