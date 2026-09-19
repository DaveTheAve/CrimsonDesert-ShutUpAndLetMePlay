#!/usr/bin/env python3
"""Build reproducible player, source, GitHub-root and Nexus-publisher ZIP archives."""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import re
import zipfile
from repository_files import inventory

STAMP = (2026, 9, 19, 0, 0, 0)
MEDIA = (
    'assets/Shut Up and Let Me Play - Header Image.png',
    'assets/Shut Up and Let Me Play - Title Image.png',
)

def sha(data: bytes) -> str: return hashlib.sha256(data).hexdigest()

def checksum_manifest(files: dict[str, bytes]) -> bytes:
    return ''.join(f'{sha(data)}  {name}\n' for name, data in sorted(files.items())).encode()

def archive(output: Path, files: dict[str, bytes], prefix: str = '', manifest: bool = True) -> None:
    files = dict(files)
    if manifest:
        files['CHECKSUMS.md'] = checksum_manifest(files)
    with zipfile.ZipFile(output, 'w') as z:
        for name, data in sorted(files.items()):
            if name.startswith('/') or '..' in Path(name).parts: raise ValueError(name)
            info = zipfile.ZipInfo(prefix + name, STAMP)
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            method = zipfile.ZIP_STORED if name.lower().endswith('.png') else zipfile.ZIP_DEFLATED
            z.writestr(info, data, compress_type=method, compresslevel=None if method == zipfile.ZIP_STORED else 9)
    with zipfile.ZipFile(output) as z:
        if z.testzip() is not None: raise ValueError('ZIP CRC failure')
        if len(z.namelist()) != len(files): raise ValueError('Unexpected archive member count')
        for name, data in files.items():
            if z.read(prefix + name) != data: raise ValueError('ZIP content mismatch')
            if name.lower().endswith('.png') and z.getinfo(prefix + name).compress_type != zipfile.ZIP_STORED:
                raise ValueError('PNG unexpectedly compressed in archive')
        if manifest:
            for line in z.read(prefix + 'CHECKSUMS.md').decode().splitlines():
                digest, name = line.split('  ', 1)
                if sha(z.read(prefix + name)) != digest: raise ValueError('Internal checksum mismatch')
    print(f'PASS {output.name}: {len(files)} entries, content/CRC/checksums validated')

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    version = re.search(r'#define\s+SULMP_VERSION\s+"([0-9.]+)"', (root/'source/Version.h').read_text()).group(1)
    output = args.output or root/'dist'; output.mkdir(parents=True, exist_ok=True)
    source = inventory(root)
    player = {name: (root/name).read_bytes() for name in ('ShutUpAndLetMePlay.asi', 'README.md', 'CHANGELOG.md', 'LICENSE')}
    publisher = {p.name: p.read_bytes() for p in sorted((root/'release').iterdir()) if p.is_file()}
    publisher['CHANGELOG.md'] = (root/'CHANGELOG.md').read_bytes()
    for media in MEDIA:
        publisher[Path(media).name] = (root/media).read_bytes()
    names = [f'ShutUpAndLetMePlay-{version}.zip', f'ShutUpAndLetMePlay-{version}-Source.zip',
             f'CrimsonDesert-ShutUpAndLetMePlay-{version}-GitHub.zip', f'ShutUpAndLetMePlay-{version}-Nexus-Publishing-Kit.zip']
    archive(output/names[0], player)
    archive(output/names[1], source, f'ShutUpAndLetMePlay-{version}-Source/')
    archive(output/names[2], source, manifest=False)
    archive(output/names[3], publisher)
    (output/f'ShutUpAndLetMePlay-{version}-CHECKSUMS.md').write_bytes(
        checksum_manifest({name: (output/name).read_bytes() for name in names}))

if __name__ == '__main__': main()
