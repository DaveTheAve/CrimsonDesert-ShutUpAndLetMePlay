"""Safety contracts for independently validated modern routes and layout recovery."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent


def array(path, name):
    text=(ROOT/path).read_text()
    m=re.search(r'\b'+re.escape(name)+r'\[(?:\d+)?\]\s*=\s*\{(.*?)\};', text, re.S)
    if not m:
        raise AssertionError(name)
    return bytes(int(v,0) for v in re.findall(r'0x[\da-fA-F]+|\b\d+\b', m[1]))


class ResilienceContracts(unittest.TestCase):
    def test_all_3663_shapes_have_full_length_masks(self):
        source='source/New3663Signatures.h'
        for name,length in {'Update':2339,'Advance':4782,'Current':285,'Finish':904,
                            'Database':117,'SequenceSkip':705,'SequenceFinish':808}.items():
            with self.subTest(name=name):
                code=array(source,'Patch'+name+'Bytes');mask=array(source,'Patch'+name+'Mask')
                self.assertEqual(len(code),length)
                self.assertEqual(len(mask),length)
                self.assertEqual(set(mask),{0,255})
                self.assertGreater(mask.count(255),length*3//4)

    def test_all_3840_shapes_have_full_length_masks(self):
        source='source/New3840Signatures.h'
        shapes={'Dlg3840Update':2539,'Dlg3840Advance':4849,'Dlg3840Current':301,'Dlg3840Finish':900,'Dlg3840Getter':326}
        for name,length in shapes.items():
            with self.subTest(name=name):
                code=array(source,name+'Bytes');mask=array(source,name+'Mask')
                self.assertEqual((len(code),len(mask)),(length,length))
                self.assertEqual(set(mask),{0,255})
                self.assertGreater(mask.count(255),length*3//4)
        # Sequencer 3840 keeps the reviewed 3663 masks; only private fields moved.
        for name,length in {'Skip':705,'Finish':808}.items():
            with self.subTest(name='Seq3840'+name):
                self.assertEqual(len(array(source,'Seq3840'+name+'Bytes')),length)
                self.assertIn('Seq3840'+name+'Bytes,PatchSequence'+name+'Mask',(ROOT/source).read_text())

    def test_3840_update_entry_reuses_home_slot_trampoline(self):
        code=array('source/New3840Signatures.h','Dlg3840UpdateBytes')
        legacy=array('source/New3479Signatures.h','NewUpdateBytes')
        # mov rax,rsp; three home-slot stores. No RSP move, push or RIP operand.
        self.assertEqual(code[:15],bytes.fromhex('48 8b c4 48 89 58 10 4c 89 48 20 4c 89 40 18'))
        self.assertEqual(code[:15],legacy[:15])
        self.assertEqual(code[15],0x55)
        text=(ROOT/'source/DialogueResolver.h').read_text()
        self.assertIn('z.updateUnwind=patch?PatchUpdateTrampolineUnwind:NewUpdateTrampolineUnwind;',text)
        self.assertIn('!same(Dlg3840UpdateBytes,NewUpdateBytes,15)',text)

    def test_3840_cache_reader_never_invokes_loader(self):
        text=(ROOT/'source/Dialogue.h').read_text()
        branch=text.split('if(resolved.cacheLayout==2){',1)[1].split('}else if',1)[0]
        self.assertIn('u32((U8*)store+0x20)',branch)
        self.assertIn('ptr(store,0x18)',branch)
        self.assertIn('ok(entries,count*48)',branch)
        self.assertIn('ptr(entries+id*48,0x20)',branch)
        # Plain bounded reads only: no native getter, symbol or call is used.
        self.assertNotIn('symbols[',branch)
        self.assertNotIn('resolved.database',text)
        resolver=(ROOT/'source/DialogueResolver.h').read_text()
        for needle in ('newCall(im,current,0x13,getter)','newCall(im,finish,0x2E,getter)','newCall(im,advance,0x3A,getter)',
                       'relative(im,finish,0x2A,0x2E)==store','relative(im,advance,0x36,0x3A)==store',
                       'relative(im,finish,0x36,0x3A)==fallback','relative(im,advance,0x42,0x46)==fallback'):
            self.assertIn(needle,resolver)

    def test_update_trampoline_copies_complete_push_instruction(self):
        code=array('source/New3663Signatures.h','PatchUpdateBytes')
        self.assertEqual(code[:16],bytes.fromhex('48 8b c4 48 89 58 10 4c 89 48 20 55 56 57 41 54'))
        self.assertEqual(code[14:16],bytes.fromhex('41 54'))
        text=(ROOT/'source/DialogueResolver.h').read_text()
        self.assertIn('z.updateLength=patch?16:15;',text)
        self.assertIn('copy(h.allocation,h.original,length);',(ROOT/'source/ShutUpAndLetMePlay.cpp').read_text())

    def test_update_unwind_recovers_each_partial_prologue(self):
        # FrameBase is captured at the interrupted RSP and remains fixed while
        # PUSH codes adjust the unwound RSP. SAVE_NONVOL must not run against an
        # unfinished frame; RBX still holds its caller value until then.
        entries = (
            ('NewInputTrampolineUnwind', 15, 0x20, 5, [(11,5),(12,6),(13,7),(15,12)]),
            ('PatchUpdateTrampolineUnwind', 16, 0x10, 7, [(12,5),(13,6),(14,7),(16,12)]),
        )
        registers={3:0x333,5:0x555,6:0x666,7:0x777,12:0xCCC}
        for name,length,saved_offset,store_end,pushes in entries:
            uw=array('source/DialogueResolver.h', name)
            self.assertEqual(uw[:4],bytes([1,length,6,0]))
            self.assertEqual(uw[4:8],bytes([length,0x34,(saved_offset+32)//8,0]))
            for pc in range(length+2):
                with self.subTest(name=name,pc=pc):
                    stack={0x1000:0xF00D};sp=0x1000;restored=dict(registers)
                    if pc>=store_end:stack[sp+saved_offset]=registers[3]
                    for end,reg in pushes:
                        if end<=pc:
                            sp-=8;stack[sp]=registers[reg]
                    frame_base=sp
                    # At full entry, poisoning registers proves that restoration
                    # actually reads the correct slots; partial entries preserve
                    # caller RBX and must not read its as-yet-unbased save slot.
                    if pc>=length:restored={reg:0xBAD for reg in registers}
                    slot=4;previous=256
                    while slot<4+uw[2]*2:
                        off,op=uw[slot:slot+2];slot+=2
                        self.assertLessEqual(off,previous);previous=off
                        if op&15==0:
                            if off<=pc:
                                restored[op>>4]=stack[sp];sp+=8
                        elif op&15==4:
                            displacement=int.from_bytes(uw[slot:slot+2],'little')*8;slot+=2
                            if off<=pc:
                                restored[op>>4]=stack[frame_base+displacement]
                        else:self.fail('unexpected unwind opcode')
                    self.assertEqual(sp,0x1000)
                    self.assertEqual(stack[sp],0xF00D)
                    self.assertEqual(restored,registers)

    def test_legacy_empty_update_unwind_has_full_storage(self):
        self.assertEqual(array('source/DialogueResolver.h','NewUpdateTrampolineUnwind'),bytes([1,0,0,0]))
        self.assertIn('NewUpdateTrampolineUnwind[16]',(ROOT/'source/DialogueResolver.h').read_text())

    def test_skip_branch_is_scoped_to_verified_native_owner(self):
        text=(ROOT/'source/Resolver.h').read_text()
        self.assertIn('runtimeOwner(im,r,52)',text)
        self.assertIn('relative(im,r,28,32)!=exit',text)
        self.assertIn('relative(im,r,38,42)!=z.appearance',text)
        self.assertIn('relative(im,r,48,52)!=exit',text)
        self.assertIn('end-exit!=epilogSize',text)
        self.assertIn('relative(im,z.nativeInput,0xFE,0x102)!=exit',text)
        current=text.split('inline bool resolve3479',1)[1]
        self.assertNotIn('unique(im,NewInputSignature)',current)

    def test_layout_must_agree_across_native_setter_and_reader(self):
        text=(ROOT/'source/Resolver.h').read_text()
        self.assertIn('family!=action+8||subtype!=family+1',text)
        self.assertIn('action<0xC0||action>0x200||(action&7)',text)
        self.assertIn('u32(im.base+z.guideRefresh+0xA9)!=action',text)
        self.assertIn('u32(im.base+z.guideRefresh+0x181)!=family',text)
        self.assertIn('u32(im.base+z.guideRefresh+0x18B)!=subtype',text)

    def test_cache_chain_and_non_loading_record_reader(self):
        resolver=(ROOT/'source/DialogueResolver.h').read_text()
        self.assertIn('same(im.base+uw+8,parent,12)',resolver)
        self.assertIn('relative(im,r,0x4B,0x4F)==fallback',resolver)
        self.assertIn('relative(im,r,0x60,0x64)==fallback',resolver)
        behavior=(ROOT/'source/Dialogue.h').read_text()
        read=behavior.split('void*record(',1)[1].split('bool vectorReadable',1)[0]
        self.assertIn('count>1000000||id>=count',read)
        self.assertIn('database=ptr(entries+id*24,8)',read)
        self.assertNotIn('resolved.database)',read)
        self.assertNotIn('(*',read)

    def test_sequence_input_can_survive_unavailable_interaction(self):
        cpp=(ROOT/'source/ShutUpAndLetMePlay.cpp').read_text()
        self.assertIn('dialogue.resolved.inputReady&&sequencer.resolved.ready',cpp)
        self.assertIn('installHooks(true,true)',cpp)
        self.assertIn('hookCount=inputOnly?1:2',cpp)
        self.assertIn('dialogue->inputEnabled',(ROOT/'source/Sequencer.h').read_text())
        diagnostics=(ROOT/'source/Diagnostics.h').read_text()
        for key in ('input_hook_validated','input_hook_enabled','dialogue_cache_layout','dialogue_update_stolen_bytes'):
            self.assertIn('"'+key+'"',diagnostics)


if __name__=='__main__':unittest.main()
