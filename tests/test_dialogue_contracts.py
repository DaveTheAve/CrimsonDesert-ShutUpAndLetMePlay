"""Game-file-free signature and partial-prologue unwind consistency checks."""
import re
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parent.parent

def array(text, name):
    body = re.search(r'\b' + name + r'\s*\[[^\]]*\]\s*=\s*\{([^}]+)\}', text).group(1)
    return bytes(int(x, 0) for x in re.findall(r'0x[0-9a-fA-F]+|\b[0-9]+\b', body))

class DialogueContracts(unittest.TestCase):
    def test_signature_lengths_and_masks(self):
        text = (ROOT/'source/DialogueSignatures.h').read_text()
        for name in ('Input', 'Update', 'Advance', 'Current', 'Finish', 'Database'):
            with self.subTest(function=name):
                code = array(text, 'Dialogue' + name + 'Bytes')
                mask = array(text, 'Dialogue' + name + 'Mask')
                self.assertEqual(len(code), len(mask))
                self.assertGreater(len(code), 15)
                self.assertEqual(set(mask), {0, 255})
                if name in ('Input', 'Update'):
                    self.assertEqual(mask[:15], bytes([255])*15)

    def check_unwind(self, name, instructions):
        """Model documented x64 PUSH_NONVOL/SAVE_NONVOL rules at each boundary.

        This is a structural model, not a call to Windows RtlVirtualUnwind.
        """
        text = (ROOT/'source/DialogueResolver.h').read_text()
        unwind = array(text, 'Dialogue' + name + 'TrampolineUnwind')
        self.assertEqual(unwind[:4], bytes([1, 15, 6, 0]))
        for boundary in [0] + [x[0] for x in instructions] + [29]:
            with self.subTest(function=name, boundary=boundary):
                sp, registers, memory = 0x1000, {r: 100+r for r in range(16)}, {}
                original = dict(registers)
                for end, operation, register, offset in instructions:
                    if end > boundary:
                        break
                    if operation == 'push':
                        sp -= 8
                        memory[sp] = registers[register]
                    elif operation == 'save':
                        memory[0x1000+offset] = registers[register]
                i, prior = 0, 256
                while i < unwind[2]:
                    end, packed = unwind[4+2*i:6+2*i]
                    kind, register = packed & 15, packed >> 4
                    self.assertLessEqual(end, prior)
                    prior = end
                    if kind == 4:
                        offset = int.from_bytes(unwind[6+2*i:8+2*i], 'little') * 8
                        if end <= boundary:
                            registers[register] = memory[sp+offset]
                        i += 2
                    elif kind == 0:
                        if end <= boundary:
                            registers[register] = memory[sp]
                            sp += 8
                        i += 1
                    else:
                        self.fail('Unexpected unwind operation')
                self.assertEqual(sp, 0x1000)
                self.assertEqual(registers, original)

    def test_input_trampoline_unwind(self):
        self.check_unwind('Input', [(5,'save',3,32), (10,'volatile',1,8),
                                   (11,'push',5,0), (12,'push',6,0),
                                   (13,'push',7,0), (15,'push',12,0)])

    def test_update_trampoline_unwind(self):
        self.check_unwind('Update', [(3,'volatile',0,0), (7,'save',3,16),
                                    (11,'save',6,24), (15,'save',7,32)])

if __name__ == '__main__':
    unittest.main()
