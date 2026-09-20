#!/usr/bin/env python3
"""Prepare checksummed assets and release notes for the manual release workflow."""
from __future__ import annotations
import argparse
from datetime import date
import hashlib
from pathlib import Path
import re
import shutil

REPOSITORY = 'https://github.com/DaveTheAve/CrimsonDesert-ShutUpAndLetMePlay'


def release_notes(root: Path, version: str) -> str:
    """Use the dated changelog entry for this version, not older release notes."""
    changelog = (root/'CHANGELOG.md').read_text(encoding='utf-8')
    pattern = rf'^## {re.escape(version)} — (\d{{4}}-\d{{2}}-\d{{2}})[ \t]*$'
    matches = list(re.finditer(pattern, changelog, re.M))
    if len(matches) != 1:
        raise ValueError('Changelog must contain one dated entry for the source version')
    entry = matches[0]
    release_date = date.fromisoformat(entry.group(1)).isoformat()
    remaining = changelog[entry.end():]
    next_entry = re.search(r'^## ', remaining, re.M)
    changes = remaining[:next_entry.start() if next_entry else len(remaining)].strip()
    if not changes:
        raise ValueError('The matching changelog entry is empty')
    tagged_source = f'{REPOSITORY}/blob/v{version}'
    return (
        f'# Shut Up & Let Me Play: An Actual Skip Button {version}\n\n'
        'Yes, it actually skips cutscenes and supported NPC dialogue. '
        'Fast-forward remains a different verb.\n\n'
        f'## {version} — {release_date}\n\n{changes}\n\n'
        '## Installation\n\n'
        f'Download **ShutUpAndLetMePlay-{version}.zip** for installation; '
        'the Source ZIP is optional. A compatible x64 ASI loader is required separately. '
        f'See the [installation instructions]({tagged_source}/README.md#installation).\n\n'
        '## Build and validation\n\n'
        'The compiler, source commit, and binary checksum are recorded in `BUILD_INFO.json`. '
        'CI checks the build, tooling, and packages without executing the game. '
        f'See the [validation record]({tagged_source}/docs/VALIDATION.md) '
        'for native isolation coverage and separately reported in-game observations.\n\n'
        '**Hold Skip. Resume game. Enjoy your time. #VivaLaSkip**\n'
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tag')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    header = (root/'source/Version.h').read_text(encoding='utf-8')
    version = re.search(r'#define\s+SULMP_VERSION\s+"([0-9.]+)"', header).group(1)
    if args.tag != f'v{version}':
        parser.error('Tag does not match source/Version.h')
    try:
        notes = release_notes(root, version)
    except ValueError as error:
        parser.error(str(error))
    names = [f'ShutUpAndLetMePlay-{version}.zip', f'ShutUpAndLetMePlay-{version}-Source.zip', 'BUILD_INFO.json']
    for name in names:
        if not (root/'dist'/name).is_file():
            parser.error(f'Missing build asset: {name}; run tools/ci.sh first')
    output = root/'dist/release-assets'
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    for name in names:
        shutil.copy2(root/'dist'/name, output/name)
    (output/'RELEASE_NOTES.md').write_text(notes, encoding='utf-8')
    names.append('RELEASE_NOTES.md')
    (output/'CHECKSUMS.md').write_text(''.join(
        f'{hashlib.sha256((output/name).read_bytes()).hexdigest()}  {name}\n'
        for name in names), encoding='utf-8')
    print(f'Prepared release assets for {args.tag}; no upload or publication performed.')


if __name__ == '__main__':
    main()
