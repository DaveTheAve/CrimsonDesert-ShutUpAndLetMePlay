"""Game-file-free checks on the additional native sequence fingerprints."""
import re
from pathlib import Path
import unittest
from test_dialogue_contracts import array

ROOT = Path(__file__).resolve().parent.parent

class SequencerContracts(unittest.TestCase):
    def test_complete_shapes_have_matching_masks(self):
        text = (ROOT/'source/SequencerSignatures.h').read_text()
        for name, size in (('Skip', 705), ('Finish', 808)):
            with self.subTest(route=name):
                code = array(text, 'Sequencer' + name + 'Bytes')
                mask = array(text, 'Sequencer' + name + 'Mask')
                self.assertEqual(len(code), size)
                self.assertEqual(len(mask), size)
                self.assertEqual(set(mask), {0, 255})
                self.assertEqual(mask[:15], b'\xff' * 15)

    def test_only_declared_relative_operands_are_masked(self):
        text = (ROOT/'source/SequencerSignatures.h').read_text()
        symbol_count = int(re.search(r'SequencerSymbolCount\s*=\s*(\d+)', text).group(1))
        for name in ('Skip', 'Finish'):
            with self.subTest(route=name):
                code = array(text, 'Sequencer' + name + 'Bytes')
                mask = array(text, 'Sequencer' + name + 'Mask')
                body = re.search(r'Sequencer' + name + r'Refs\[\]\s*=\s*\{(.*?)\};', text, re.S).group(1)
                refs = re.findall(r'\{(\d+),(\d+),(\d+),(\d+)\}', body)
                self.assertGreater(len(refs), 10)
                masked = set()
                for displacement, following, symbol, executable in refs:
                    displacement, following, symbol, executable = map(int, (displacement, following, symbol, executable))
                    self.assertLessEqual(displacement + 4, following)
                    self.assertLessEqual(following, len(code))
                    self.assertLess(symbol, symbol_count)
                    self.assertIn(executable, (0, 1))
                    self.assertFalse(masked.intersection(range(displacement, displacement + 4)))
                    masked.update(range(displacement, displacement + 4))
                    self.assertEqual(mask[displacement:displacement + 4], b'\0' * 4)
                self.assertEqual({i for i, value in enumerate(mask) if not value}, masked)

    def test_native_unwind_descriptors_cover_declared_codes(self):
        text = (ROOT/'source/SequencerSignatures.h').read_text()
        for name in ('Skip', 'Finish'):
            with self.subTest(route=name):
                unwind = array(text, 'Sequencer' + name + 'Unwind')
                self.assertEqual(unwind[0] & 7, 1)
                self.assertEqual(len(unwind), 4 + 2 * unwind[2])
                self.assertGreater(unwind[1], 15)

if __name__ == '__main__':
    unittest.main()
