#!/usr/bin/env python3
"""Explicit source inventory shared by local checks and ZIP packaging."""
from __future__ import annotations
from pathlib import Path, PurePosixPath
import subprocess

ROOT_FILES = {"README.md", "CHANGELOG.md", "CONTRIBUTING.md", "SECURITY.md",
              ".gitignore", ".gitattributes", ".editorconfig", "build.sh", "build.cmd", "LICENSE"}
DIRECTORIES = {"source", "tests", "tools", "docs", "release", ".github"}
MEDIA_FILES = {
    "assets/Shut Up and Let Me Play - Header Image.png",
    "assets/Shut Up and Let Me Play - Title Image.png",
}
EXTENSIONS = {".cpp", ".h", ".inc", ".def", ".py", ".sh", ".cmd", ".md", ".json", ".yml", ".yaml"}
SKIP_DIRS = {".git", ".build", ".ci-bin", "dist", "__pycache__", ".pytest_cache", ".venv", "private", "game-files"}


def allowed(name: str) -> bool:
    p = PurePosixPath(name)
    if name.startswith('/') or '\\' in name or '..' in p.parts: return False
    if any(ord(c) < 32 or c in '<>:"|?*' for c in name): return False
    if name in ROOT_FILES or name in MEDIA_FILES or name == "release/NEXUS_DESCRIPTION.txt": return True
    if not p.parts or p.parts[0] not in DIRECTORIES: return False
    if p.suffix.lower() not in EXTENSIONS: return False
    lower = p.name.lower()
    if any(word in lower for word in ("updatereport", "bindingprobe", "childselectorprobe", "interactiontrace", "runtimeprobe")):
        return False
    return all(part not in SKIP_DIRS for part in p.parts)


def inventory(root: Path, tracked_only: bool = False) -> dict[str, bytes]:
    if tracked_only:
        found = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z'], text=True)
        names = [name for name in found.split('\0') if name]
        if not names: raise ValueError("No tracked files; stage the intended repository files first")
    else:
        names = []
        for p in root.rglob('*'):
            relative = p.relative_to(root)
            if any(part in SKIP_DIRS for part in relative.parts): continue
            if p.is_dir(): continue
            # Build output is permitted on disk, never in source archives.
            if relative.as_posix() in ('ShutUpAndLetMePlay.asi', 'CHECKSUMS.md'): continue
            names.append(relative.as_posix())
    result = {}
    for name in sorted(names):
        p = root / name
        if not allowed(name): raise ValueError(f"Disallowed repository file: {name}")
        if p.is_symlink(): raise ValueError(f"Symlink not permitted: {name}")
        if not p.is_file(): raise ValueError(f"Missing repository file: {name}")
        limit = 4 * 1024 * 1024 if name in MEDIA_FILES else 1024 * 1024
        if p.stat().st_size > limit: raise ValueError(f"Unexpectedly large source file: {name}")
        data = p.read_bytes()
        if name in MEDIA_FILES:
            if not data.startswith(b'\x89PNG\r\n\x1a\n'): raise ValueError(f'Approved media is not PNG: {name}')
        elif data.startswith((b'MZ', b'\x7fELF', b'PK\x03\x04')): raise ValueError(f"Binary/archive disguised as source: {name}")
        result[name] = data
    return result
