"""Execute synthetic PE/multi-section scanner checks without any game files."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent

class ImageContracts(unittest.TestCase):
    def test_bounded_parser_and_multi_section_scanner(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'image_harness'
            subprocess.run(['g++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-Wno-misleading-indentation', str(ROOT/'tests/image_harness.cpp'),
                            '-o', str(binary)], check=True, capture_output=True, timeout=60)
            result = subprocess.run([str(binary)], check=True, capture_output=True,
                                    text=True, timeout=60)
            self.assertIn('randomized oracle comparisons: 12000', result.stdout)

if __name__ == '__main__':
    unittest.main()
