#!/usr/bin/env python3
"""Generate a deterministic Windows VERSIONINFO resource without a Windows SDK."""
from __future__ import annotations
import argparse
from pathlib import Path
import re
import struct

def align(data: bytes) -> bytes:
    return data + bytes((-len(data)) % 4)

def block(key: str, value: bytes = b"", children: bytes = b"", text: bool = False) -> bytes:
    header = struct.pack("<HHH", 0, len(value) // 2 if text else len(value), int(text))
    body = align(header + (key + "\0").encode("utf-16le")) + value
    if children:
        body = align(body) + children
    if len(body) > 65535:
        raise ValueError("VERSIONINFO block is too large")
    return struct.pack("<H", len(body)) + body[2:]

def make_resource(version: str) -> bytes:
    nums = tuple(map(int, version.split(".")))
    if len(nums) != 3 or any(not 0 <= n <= 65535 for n in nums):
        raise ValueError("Expected a three-component numeric version")
    ms, ls = (nums[0] << 16) | nums[1], nums[2] << 16
    fixed = struct.pack("<13I", 0xFEEF04BD, 0x10000, ms, ls, ms, ls, 0x3F, 0,
                        0x40004, 2, 0, 0, 0)
    strings = {
        "FileDescription": "Crimson Desert actual native cutscene Skip button",
        "FileVersion": version,
        "InternalName": "ShutUpAndLetMePlay",
        "OriginalFilename": "ShutUpAndLetMePlay.asi",
        "ProductName": "Shut Up & Let Me Play: An Actual Skip Button",
        "ProductVersion": version,
    }
    values = b"".join(align(block(k, (v + "\0").encode("utf-16le"), text=True))
                      for k, v in strings.items())
    string_info = block("StringFileInfo", children=block("040904B0", children=values, text=True), text=True)
    var_info = block("VarFileInfo", children=block("Translation", struct.pack("<HH", 0x409, 1200)), text=True)
    data = block("VS_VERSION_INFO", fixed, align(string_info) + var_info)
    null = struct.pack("<IIHHHHIHHII", 0, 32, 0xFFFF, 0, 0xFFFF, 0, 0, 0, 0, 0, 0)
    header = struct.pack("<IIHHHHIHHII", len(data), 32, 0xFFFF, 16, 0xFFFF, 1, 0, 0x30, 0x409, 0, 0)
    return null + header + align(data)

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version_header", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    found = re.search(r'#define\s+SULMP_VERSION\s+"([0-9]+\.[0-9]+\.[0-9]+)"',
                      args.version_header.read_text(encoding="utf-8"))
    if not found:
        parser.error("Version header has no supported SULMP_VERSION")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(make_resource(found.group(1)))

if __name__ == "__main__":
    main()
