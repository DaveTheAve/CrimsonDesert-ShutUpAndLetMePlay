#!/usr/bin/env python3
"""Reject tracked build output, game binaries, private captures and unsafe source paths."""
from pathlib import Path
import argparse
import json
import re
from repository_files import inventory

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tracked', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    files = inventory(root, tracked_only=args.tracked)
    for name, data in files.items():
        if name.endswith('.json'): json.loads(data)
    version = re.search(r'#define\s+SULMP_VERSION\s+"([0-9.]+)"', files['source/Version.h'].decode()).group(1)
    for name, data in files.items():
        if name.startswith('.github/workflows/') and name.endswith('.yml'):
            text = data.decode()
            for action in re.findall(r'^\s*-?\s*uses:\s*(\S+)', text, re.M):
                if not re.fullmatch(r'actions/[a-z-]+@[0-9a-f]{40}', action):
                    raise ValueError(f'Unpinned/non-reviewed action in {name}: {action}')
            if 'pull_request_target:' in text: raise ValueError('Privileged PR trigger not permitted')
    print(f'PASS {len(files)} source/repository files: allowlist, size, binary magic, JSON and workflow action pins')
