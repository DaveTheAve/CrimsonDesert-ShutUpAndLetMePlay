"""Game-file-free regression tests. No network or external account access."""
from __future__ import annotations
import contextlib
import io
import os
import re
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT/'tools'))
from make_version_resource import make_resource
from verify_binary import version_block, verify
from repository_files import allowed, inventory
from package_release import archive

class ReleaseToolsTests(unittest.TestCase):
    def test_resource_is_deterministic(self):
        self.assertEqual(make_resource('1.0.0'), make_resource('1.0.0'))
    def test_version_resource_roundtrip(self):
        key, value, children = version_block(make_resource('2.3.4')[64:])
        self.assertEqual(key, 'VS_VERSION_INFO')
        self.assertEqual(struct.unpack('<13I', value)[2:4], (0x20003, 0x40000))
        self.assertTrue(children)
    def test_invalid_versions_rejected(self):
        for bad in ('1.0', '1.2.3.4', '-1.0.0', '65536.0.0', 'v1.0.0'):
            with self.subTest(version=bad), self.assertRaises(ValueError): make_resource(bad)
    def test_source_allowlist(self):
        for name in ('README.md', '.gitignore', '.github/workflows/ci.yml', 'source/Behavior.h', 'tests/reference.json'):
            self.assertTrue(allowed(name), name)
    def test_approved_media_allowlist(self):
        self.assertTrue(allowed('assets/Shut Up and Let Me Play - Header Image.png'))
        self.assertTrue(allowed('assets/Shut Up and Let Me Play - Title Image.png'))
        self.assertFalse(allowed('assets/random.png'))
    def test_unsafe_paths_and_captures_rejected(self):
        for name in ('../bad.txt', '/absolute.txt', r'source\bad.cpp', 'CrimsonDesert.exe',
                     'source/mod.dll', 'tests/fake.zip', 'tests/report.log',
                     'tests/ShutUpAndLetMePlay_UpdateReport.json', 'source/../private/data.txt'):
            self.assertFalse(allowed(name), name)
    def test_binary_disguised_as_source_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);(root/'source').mkdir();(root/'source/fake.cpp').write_bytes(b'MZbinary')
            with self.assertRaises(ValueError): inventory(root)
    def test_oversized_file_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);(root/'README.md').write_bytes(b'a'*(1024*1024+1))
            with self.assertRaises(ValueError): inventory(root)
    @unittest.skipUnless(os.name == 'posix', 'POSIX symlink regression')
    def test_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);(root/'README.md').write_text('source');(root/'CONTRIBUTING.md').symlink_to(root/'README.md')
            with self.assertRaises(ValueError): inventory(root)
    def test_archive_integrity_and_manifest(self):
        with tempfile.TemporaryDirectory() as t, contextlib.redirect_stdout(io.StringIO()):
            p=Path(t)/'a.zip';archive(p, {'README.md':b'example'}, prefix='Source/')
            with zipfile.ZipFile(p) as z:
                self.assertEqual(z.read('Source/README.md'),b'example')
                self.assertIn('Source/CHECKSUMS.md',z.namelist())
    def test_png_is_stored_without_archive_compression(self):
        with tempfile.TemporaryDirectory() as t, contextlib.redirect_stdout(io.StringIO()):
            p=Path(t)/'media.zip';data=b'\x89PNG\r\n\x1a\nunchanged'
            archive(p, {'assets/image.png':data})
            with zipfile.ZipFile(p) as z:
                self.assertEqual(z.read('assets/image.png'), data)
                self.assertEqual(z.getinfo('assets/image.png').compress_type, zipfile.ZIP_STORED)
    def test_archive_is_reproducible(self):
        with tempfile.TemporaryDirectory() as t, contextlib.redirect_stdout(io.StringIO()):
            a,b=Path(t)/'a.zip',Path(t)/'b.zip'
            for p in (a,b): archive(p,{'README.md':b'a','source/X.h':b'b'})
            self.assertEqual(a.read_bytes(), b.read_bytes())
    def test_repository_zip_has_root_files_and_no_generated_manifest(self):
        with tempfile.TemporaryDirectory() as t, contextlib.redirect_stdout(io.StringIO()):
            p=Path(t)/'repo.zip';archive(p, {'README.md':b'public'}, manifest=False)
            with zipfile.ZipFile(p) as z: self.assertEqual(z.namelist(), ['README.md'])
    def test_zip_path_traversal_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            with self.assertRaises(ValueError): archive(Path(t)/'bad.zip',{'../bad.txt':b'x'})
    def test_built_binary_matches_declared_version(self):
        version=re.search(r'#define\s+SULMP_VERSION\s+"([0-9.]+)"', (ROOT/'source/Version.h').read_text()).group(1)
        with contextlib.redirect_stdout(io.StringIO()): verify(ROOT/'ShutUpAndLetMePlay.asi', version)
    def test_wrong_binary_version_rejected(self):
        with self.assertRaises(AssertionError): verify(ROOT/'ShutUpAndLetMePlay.asi', '65535.65535.65535')
    def test_draft_version_mismatch_rejected(self):
        result=subprocess.run([sys.executable,str(ROOT/'tools/prepare_release.py'),'v99999.99999.99999'],capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('Tag does not match',result.stderr)

if __name__ == '__main__': unittest.main()
