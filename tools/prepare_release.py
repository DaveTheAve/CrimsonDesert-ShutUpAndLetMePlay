#!/usr/bin/env python3
"""Prepare a bounded, checksummed asset set for a manually reviewed GitHub draft."""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import re
import shutil

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tag')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    version = re.search(r'#define\s+SULMP_VERSION\s+"([0-9.]+)"', (root/'source/Version.h').read_text()).group(1)
    if args.tag != f'v{version}': parser.error('Tag does not match source/Version.h')
    output = root/'dist/release-assets'
    if output.exists(): shutil.rmtree(output)
    output.mkdir(parents=True)
    names = [f'ShutUpAndLetMePlay-{version}.zip', f'ShutUpAndLetMePlay-{version}-Source.zip', 'BUILD_INFO.json']
    for name in names: shutil.copy2(root/'dist'/name, output/name)
    notes = (f'# Shut Up & Let Me Play: An Actual Skip Button {version}\n\n'
             'Yes, it actually skips. Download the main versioned ZIP for installation; the Source ZIP is optional. '
             'A compatible x64 ASI loader is required separately. Fast-forward remains a different verb.\n\n'
             '## Validation boundary\n\n'
             'This draft was built and structurally checked by CI without game files. '
             'CI did not execute the licensed reference routines, the complete game or physical controller input. '
             'Review the exact ASI in-game before publishing; recorded reference tests are not a new CI gameplay test.\n\n'
             'The compiler, source commit and binary checksum are in BUILD_INFO.json. '
             'Keep the same player ZIP on GitHub and Nexus for a given release.\n\n'
             '## Changelog\n\n' + (root/'CHANGELOG.md').read_text())
    (output/'RELEASE_NOTES.md').write_text(notes)
    names.append('RELEASE_NOTES.md')
    (output/'CHECKSUMS.md').write_text(''.join(
        f'{hashlib.sha256((output/name).read_bytes()).hexdigest()}  {name}\n' for name in names))
    print(f'Prepared draft assets for {args.tag}; nothing has been uploaded by this script.')
