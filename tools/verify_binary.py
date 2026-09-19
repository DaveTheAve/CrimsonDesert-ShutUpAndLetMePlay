#!/usr/bin/env python3
"""Validate the ASI's PE structure, imports, version resource and private-path markers."""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import re
import struct

class PE:
    def __init__(self, path: Path):
        self.data = path.read_bytes()
        assert self.data[:2] == b"MZ", "Not a PE image"
        self.nt = self.u32(0x3C)
        assert self.data[self.nt:self.nt + 4] == b"PE\0\0", "Bad PE signature"
        self.op = self.nt + 24
        self.sections = []
        for i in range(self.u16(self.nt + 6)):
            s = self.op + self.u16(self.nt + 20) + 40 * i
            self.sections.append((self.u32(s + 12), max(self.u32(s + 8), self.u32(s + 16)),
                                  self.u32(s + 20), self.u32(s + 16), self.u32(s + 36)))
    def u16(self, o: int) -> int: return struct.unpack_from("<H", self.data, o)[0]
    def u32(self, o: int) -> int: return struct.unpack_from("<I", self.data, o)[0]
    def offset(self, rva: int) -> int:
        for va, span, raw, raw_size, _ in self.sections:
            if va <= rva < va + span:
                assert rva - va < raw_size, "RVA has no raw file data"
                return raw + rva - va
        raise AssertionError(f"Unmapped RVA {rva:#x}")
    def directory(self, index: int) -> tuple[int, int]:
        return struct.unpack_from("<II", self.data, self.op + 112 + index * 8)
    def resource(self) -> bytes:
        rva, size = self.directory(2)
        assert size > 0, "Version resource missing"
        root, directory = self.offset(rva), 0
        for wanted in (16, 1, 0x409):
            pos = root + directory
            count = self.u16(pos + 12) + self.u16(pos + 14)
            entries = [struct.unpack_from("<II", self.data, pos + 16 + i * 8) for i in range(count)]
            found = [v for k, v in entries if k == wanted]
            assert len(found) == 1, "Unexpected VERSIONINFO tree"
            entry = found[0]; directory = entry & 0x7FFFFFFF
        assert not entry & 0x80000000
        data_rva, data_size = struct.unpack_from("<II", self.data, root + directory)
        off = self.offset(data_rva)
        return self.data[off:off + data_size]

def version_block(data: bytes, start: int = 0) -> tuple[str, bytes, list]:
    length, value_length, kind = struct.unpack_from("<HHH", data, start)
    assert length >= 6 and start + length <= len(data), "Malformed VERSIONINFO block"
    pos = end = start + 6
    while data[end:end + 2] != b"\0\0":
        assert end + 2 < start + length, "Unterminated resource key"
        end += 2
    key = data[pos:end].decode("utf-16le")
    pos = (end + 5) & ~3
    value_size = value_length * 2 if kind else value_length
    value = data[pos:pos + value_size]
    pos = (pos + value_size + 3) & ~3
    children = []
    while pos + 6 <= start + length:
        child_length = struct.unpack_from("<H", data, pos)[0]
        if not child_length: break
        children.append(version_block(data, pos))
        pos = (pos + child_length + 3) & ~3
    return key, value, children

def verify(path: Path, version: str) -> None:
    pe = PE(path)
    assert pe.u16(pe.nt + 4) == 0x8664 and pe.u16(pe.op) == 0x20B
    assert pe.u16(pe.nt + 22) & 0x2000
    assert pe.u32(pe.nt + 8) == 0, "Non-deterministic build timestamp"
    assert pe.u16(pe.op + 70) & 0x160 == 0x160, "ASLR/DEP/high-entropy flags missing"
    assert pe.directory(0) == (0, 0), "Unexpected exports"
    assert pe.directory(3)[1] and pe.directory(5)[1], "Missing unwind/relocation data"
    assert pe.directory(6) == (0, 0) and pe.directory(14) == (0, 0)
    for _, _, raw, size, flags in pe.sections:
        assert raw + size <= len(pe.data), "Truncated section"
        assert flags & 0xA0000000 != 0xA0000000, "Writable/executable section"
    pos = pe.offset(pe.directory(1)[0]); modules = []
    while any(pe.data[pos:pos + 20]):
        off = pe.offset(pe.u32(pos + 12)); end = pe.data.index(0, off)
        modules.append(pe.data[off:end].decode("ascii")); pos += 20
    assert modules == ["KERNEL32.dll"], f"Unexpected imports: {modules}"
    key, value, children = version_block(pe.resource())
    assert key == "VS_VERSION_INFO" and len(value) == 52
    nums = tuple(map(int, version.split('.')))
    ms, ls = (nums[0] << 16) | nums[1], nums[2] << 16
    assert struct.unpack("<13I", value)[:6] == (0xFEEF04BD, 0x10000, ms, ls, ms, ls)
    strings = {}
    def walk(node):
        key, value, children = node
        if key in {"FileVersion", "ProductVersion", "OriginalFilename"}:
            strings[key] = value.decode("utf-16le").rstrip("\0")
        for child in children: walk(child)
    for node in children: walk(node)
    assert strings == {"FileVersion": version, "ProductVersion": version,
                       "OriginalFilename": "ShutUpAndLetMePlay.asi"}
    for marker in ("/mnt/data/", "/home/oai/", "C:\\Users\\"):
        for encoded in (marker.encode(), marker.encode("utf-16le")):
            assert encoded not in pe.data, "Private build path embedded"
    print("PASS AMD64 PE32+ ASI; ASLR, high-entropy VA, DEP, relocation and unwind metadata")
    print("PASS only KERNEL32.dll imported; no test exports, CLR or debug directory")
    print(f"PASS parsed file/product VERSIONINFO: {version}")
    print("PASS no writable/executable image sections or private build-path markers")
    print(f"Binary bytes: {len(pe.data)}\nBinary SHA-256: {hashlib.sha256(pe.data).hexdigest()}")
    print("These are structural checks, not an antivirus certification or an in-game test.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--version")
    args = parser.parse_args()
    if not args.version:
        header = (Path(__file__).resolve().parent.parent / 'source/Version.h').read_text()
        args.version = re.search(r'#define\s+SULMP_VERSION\s+"([0-9.]+)"', header).group(1)
    verify(args.binary, args.version)
