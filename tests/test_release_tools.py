"""Game-file-free regression tests. No network or external account access."""
from __future__ import annotations
import contextlib
import hashlib
from datetime import date
import json
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
from package_release import archive, STAMP
from prepare_release import release_notes

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
    def test_generated_source_manifest_not_repackaged(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);(root/'README.md').write_bytes(b'example')
            (root/'CHECKSUMS.md').write_bytes(b'generated manifest')
            self.assertEqual(inventory(root), {'README.md':b'example'})
    def test_archive_integrity_and_manifest(self):
        with tempfile.TemporaryDirectory() as t, contextlib.redirect_stdout(io.StringIO()):
            p=Path(t)/'a.zip';archive(p, {'README.md':b'example'}, prefix='Source/')
            with zipfile.ZipFile(p) as z:
                self.assertEqual(z.read('Source/README.md'),b'example')
                self.assertIn('Source/CHECKSUMS.md',z.namelist())
    def test_png_bytes_are_stored_unchanged(self):
        with tempfile.TemporaryDirectory() as t, contextlib.redirect_stdout(io.StringIO()):
            p=Path(t)/'media.zip';data=b'\x89PNG\r\n\x1a\nunchanged'
            archive(p, {'assets/image.png':data})
            with zipfile.ZipFile(p) as z:
                self.assertEqual(z.read('assets/image.png'), data)
                self.assertEqual(z.getinfo('assets/image.png').compress_type, zipfile.ZIP_STORED)
    def test_approved_media_allowlist(self):
        self.assertTrue(allowed('assets/Shut Up and Let Me Play - Header Image.png'))
        self.assertTrue(allowed('assets/Shut Up and Let Me Play - Title Image.png'))
        self.assertFalse(allowed('assets/random.png'))
    def test_draft_version_mismatch_rejected(self):
        result=subprocess.run([sys.executable,str(ROOT/'tools/prepare_release.py'),'v99999.99999.99999'],capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('Tag does not match',result.stderr)
    def test_no_automatic_release_workflow(self):
        workflows = set(p.name for p in (ROOT/'.github/workflows').glob('*.yml'))
        self.assertEqual(workflows, {'ci.yml', 'release.yml'})
        text=(ROOT/'.github/workflows/release.yml').read_text()
        self.assertIn('workflow_dispatch:', text)
        self.assertNotRegex(text, r'(?m)^  (push|pull_request):')
        self.assertIn("if: github.ref == 'refs/heads/main'", text)
    def test_nexus_text_inventory(self):
        name = 'release/NEXUS_DESCRIPTION.txt'
        self.assertTrue(allowed(name))
        self.assertFalse(allowed('release/unrelated.txt'))
        path = ROOT/name
        self.assertEqual(list(path.parent.glob(path.stem + '.*')), [path])
        self.assertEqual(inventory(ROOT)[name], path.read_bytes())
    def test_nexus_uses_approved_bbcode_and_cutscene_joke(self):
        text=(ROOT/'release/NEXUS_DESCRIPTION.txt').read_text()
        self.assertTrue(text.startswith('[heading]An Actual Skip Button for Crimson Desert[/heading]'))
        quote=re.search(r'\[quote\](.*?)\[/quote\]', text, re.S).group(1)
        self.assertNotIn('dialogue', quote.lower())
        self.assertIn('What if the cutscene happened at ludicrous speed?', quote)
        self.assertIn('What if the cutscene stopped happening?', quote)
        self.assertIn('cutscenes and dialogue.', text)
        self.assertNotIn('<h', text)
    def test_readme_header_and_quote(self):
        text=(ROOT/'README.md').read_text()
        self.assertIn('Header%20Image.png',text.splitlines()[0])
        quote='\n'.join(line for line in text.splitlines() if line.startswith('> **Other') or line.startswith('> **This mod'))
        self.assertNotIn('dialogue',quote.lower())
        self.assertIn('What if the cutscene stopped happening?',quote)
        intro = text.split('## The revolutionary feature list', 1)[0]
        self.assertEqual([line for line in intro.splitlines() if line.startswith('>')], [
            '> **Other “skip” mods:** *What if the cutscene happened at ludicrous speed?*  ',
            '> **This mod:** *What if the cutscene stopped happening?*',
        ])
        self.assertIn('Enter a supported gameplay cutscene, hold the native Skip button, '
                      'and enjoy the breathtaking cinematic experience of '
                      '**not being in the cinematic anymore**.', text)
        self.assertIn('**latest release** from [Releases](', text)
        self.assertIn('/releases/latest)', text)
    def test_release_metadata_is_consistent(self):
        version = re.search(r'#define\s+SULMP_VERSION\s+"([0-9.]+)"',
                            (ROOT/'source/Version.h').read_text()).group(1)
        fields = json.loads((ROOT/'release/NEXUS_FIELDS.json').read_text())
        self.assertEqual(fields['version'], version)
        self.assertEqual(fields['main_file'], f'ShutUpAndLetMePlay-{version}.zip')
        self.assertEqual(fields['optional_source'], f'ShutUpAndLetMePlay-{version}-Source.zip')
        self.assertNotIn('publication_state', fields)
        heading = (ROOT/'docs/VALIDATION.md').read_text().splitlines()[0]
        self.assertEqual(heading, f'# Validation — {version}')
        changelog = (ROOT/'CHANGELOG.md').read_text()
        entries = re.findall(r'^## ([0-9.]+) — (\d{4}-\d{2}-\d{2})$', changelog, re.M)
        self.assertEqual(entries[0][0], version)
        self.assertEqual(date.fromisoformat(entries[0][1]), date(*STAMP[:3]))
    def test_release_notes_use_current_changelog(self):
        version = json.loads((ROOT/'release/NEXUS_FIELDS.json').read_text())['version']
        notes = release_notes(ROOT, version)
        self.assertIn(f'## {version} — ', notes)
        self.assertIn('supported NPC dialogue', notes)
        self.assertNotIn('# Changelog', notes)
        self.assertNotIn('First public release.', notes)
        links = re.findall(r'\]\(([^)]+)\)', notes)
        self.assertTrue(links)
        for link in links:
            self.assertIn(f'/blob/v{version}/', link)
        self.assertTrue(notes.endswith('**Hold Skip. Resume game. Enjoy your time. #VivaLaSkip**\n'))
    def test_release_notes_require_one_dated_entry(self):
        samples = ('# Changelog\n', '## 2.3.4\n\nExample\n',
                   '## 2.3.4 — 2026-02-30\n\nExample\n',
                   '## 2.3.4 — 2026-09-20\n\n',
                   '## 2.3.4 — 2026-09-20\n\nExample\n## 2.3.4 — 2026-09-20\nExample\n')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for text in samples:
                with self.subTest(changelog=text):
                    (root/'CHANGELOG.md').write_text(text, encoding='utf-8')
                    with self.assertRaises(ValueError):
                        release_notes(root, '2.3.4')
    def test_release_notes_do_not_include_other_versions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'CHANGELOG.md').write_text(
                '# Changelog\n\n## 2.3.4 — 2026-09-20\n\n- Current change.\n\n'
                '## 2.3.3 — 2026-09-19\n\n- Previous change.\n', encoding='utf-8')
            notes = release_notes(root, '2.3.4')
            self.assertIn('- Current change.', notes)
            self.assertNotIn('Previous change.', notes)
            self.assertNotIn('2.3.3', notes)
    def test_publishing_guides_match_release_metadata(self):
        fields = json.loads((ROOT/'release/NEXUS_FIELDS.json').read_text())
        version = fields['version']
        changelog = (ROOT/'CHANGELOG.md').read_text()
        heading = re.search(r'^## ' + re.escape(version) + r' — (\d{4}-\d{2}-\d{2})$', changelog, re.M)
        self.assertIsNotNone(heading)
        guide = (ROOT/'docs/PUBLISHING.md').read_text()
        self.assertIn(f'Version: **{version}**. Tag: **v{version}**.', guide)
        self.assertIn(f'**{heading.group(1)}**', guide)
        nexus_guide = (ROOT/'release/PUBLISHING_GUIDE.md').read_text()
        self.assertIn(f'Version: **{version}**', nexus_guide)
        self.assertEqual(fields['nexus_publishing_kit'],
                         f'ShutUpAndLetMePlay-{version}-Nexus-Publishing-Kit.zip')
        for key in ('main_file', 'optional_source', 'nexus_publishing_kit'):
            self.assertIn(fields[key], nexus_guide)

    def test_release_workflow_includes_nexus_kit(self):
        workflow = (ROOT/'.github/workflows/release.yml').read_text()
        self.assertIn('python3 tools/prepare_release.py "$TAG"', workflow)
        for suffix in ('', '-Source', '-Nexus-Publishing-Kit'):
            self.assertIn(f'"ShutUpAndLetMePlay-$VERSION{suffix}.zip"', workflow)
        self.assertIn('--verify-tag --draft', workflow)

    def test_prepare_release_handoff_is_complete_and_checksummed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'tools').mkdir()
            (root/'source').mkdir()
            (root/'dist').mkdir()
            (root/'tools/prepare_release.py').write_bytes((ROOT/'tools/prepare_release.py').read_bytes())
            (root/'source/Version.h').write_text('#define SULMP_VERSION "2.3.4"\n', encoding='utf-8')
            (root/'CHANGELOG.md').write_text('## 2.3.4 — 2026-09-21\n\n- Current change.\n', encoding='utf-8')
            assets = {
                'ShutUpAndLetMePlay-2.3.4.zip': b'player fixture',
                'ShutUpAndLetMePlay-2.3.4-Source.zip': b'source fixture',
                'ShutUpAndLetMePlay-2.3.4-Nexus-Publishing-Kit.zip': b'publishing fixture',
                'BUILD_INFO.json': b'{}\n',
            }
            for name, data in assets.items():
                (root/'dist'/name).write_bytes(data)
            result = subprocess.run([sys.executable, str(root/'tools/prepare_release.py'), 'v2.3.4'],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            out = root/'dist/release-assets'
            self.assertEqual({p.name for p in out.iterdir()},
                             set(assets) | {'CHECKSUMS.md', 'RELEASE_NOTES.md'})
            for name, data in assets.items():
                self.assertEqual((out/name).read_bytes(), data)
            manifest = {}
            for line in (out/'CHECKSUMS.md').read_text().splitlines():
                digest, name = line.split('  ', 1)
                self.assertNotIn(name, manifest)
                manifest[name] = digest
                self.assertEqual(digest, hashlib.sha256((out/name).read_bytes()).hexdigest())
            self.assertEqual(set(manifest), set(assets) | {'RELEASE_NOTES.md'})
            self.assertIn('## 2.3.4 — 2026-09-21', (out/'RELEASE_NOTES.md').read_text())
            self.assertFalse((root/'.git').exists())

    def test_prepare_release_checks_all_inputs_before_replacing_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'tools').mkdir()
            (root/'source').mkdir()
            out = root/'dist/release-assets'
            out.mkdir(parents=True)
            (out/'preserved.md').write_text('existing handoff', encoding='utf-8')
            (root/'tools/prepare_release.py').write_bytes((ROOT/'tools/prepare_release.py').read_bytes())
            (root/'source/Version.h').write_text('#define SULMP_VERSION "2.3.4"\n', encoding='utf-8')
            (root/'CHANGELOG.md').write_text('## 2.3.4 — 2026-09-21\n\n- Current change.\n', encoding='utf-8')
            for suffix in ('', '-Source'):
                (root/'dist'/f'ShutUpAndLetMePlay-2.3.4{suffix}.zip').write_bytes(b'fixture')
            (root/'dist/BUILD_INFO.json').write_text('{}', encoding='utf-8')
            result = subprocess.run([sys.executable, str(root/'tools/prepare_release.py'), 'v2.3.4'],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Missing build asset: ShutUpAndLetMePlay-2.3.4-Nexus-Publishing-Kit.zip', result.stderr)
            self.assertEqual((out/'preserved.md').read_text(), 'existing handoff')
            self.assertEqual({p.name for p in out.iterdir()}, {'preserved.md'})

    def test_package_rejects_windows_escape(self):
        with tempfile.TemporaryDirectory() as t:
            for name in ('C:/bad.md', r'..\bad.md'):
                with self.subTest(name=name), self.assertRaises(ValueError):
                    archive(Path(t)/'bad.zip',{name:b'x'})
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
if __name__ == '__main__': unittest.main()
