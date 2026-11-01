"""Non-game-file regression assertions for the updated native resolver contract."""
from pathlib import Path
import json
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent

def byte_array(name):
    source = (ROOT/'source/New3479Signatures.h').read_text()
    match = re.search(r'static const U8\s+' + re.escape(name) + r'\[\] = \{(.*?)\};', source, re.S)
    assert match, name
    return bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{2})', match.group(1)))

class UpdatedNativeContracts(unittest.TestCase):
    def test_reference_file_and_profiles(self):
        manifest = json.loads((ROOT/'tests/reference.json').read_text())
        profiles = [manifest] + manifest['additional_references']
        self.assertEqual([p['file_version'] for p in profiles], ['1.0.0.2944','1.0.0.2949','1.0.0.3479'])
        self.assertEqual(profiles[-1]['profile'], 'refactored')
        self.assertEqual(profiles[-1]['executable_sha256'], 'df531a585bf84ceb47779304cc8b3caaf976aea88424c32605885623cd4519c7')
        self.assertEqual(len(profiles[-1]['functions']),14)

    def test_updated_masked_shapes_have_complete_matching_lengths(self):
        names=['Interaction','Appearance','SetAppearance','Input','Update','Advance','Current',
               'Finish','Database','SequenceSkip','SequenceFinish','SequenceHelper','GuideBinding','GuideRefresh']
        sizes=[506,617,459,2787,2332,4781,285,884,394,705,808,330,466,432]
        text=(ROOT/'source/New3479Signatures.h').read_text()
        for name, size in zip(names,sizes):
            with self.subTest(name=name):
                b=byte_array('New'+name+'Bytes')
                mask=byte_array('New'+name+'Mask')
                self.assertEqual((len(b),len(mask)),(size,size))
                self.assertEqual(set(mask),{0,255})
                self.assertGreater(mask.count(255),size//3)
                self.assertIn('New'+name+'Signature',text)

    def test_compiled_installation_selects_both_abi_profiles(self):
        source=(ROOT/'source/ShutUpAndLetMePlay.cpp').read_text()
        header=(ROOT/'source/DialogueResolver.h').read_text()
        self.assertIn('resolve3479(engine.image, engine.resolved)',source)
        self.assertIn('resolveDialogue3479(engine.image,engine.resolved,dialogue.resolved)',source)
        self.assertIn('resolveSequencer3479(engine.image,dialogue.resolved,sequencer.resolved)',source)
        self.assertIn('current?NewInputTrampolineUnwind:DialogueInputTrampolineUnwind',source)
        self.assertIn('current?17:15',source)
        self.assertIn('U8 appearanceUnwind[8]',(ROOT/'source/Resolver.h').read_text())
        self.assertIn('NewInputTrampolineUnwind[16]',header)

    def test_native_entry_relocation_and_unwind(self):
        source=(ROOT/'source/ShutUpAndLetMePlay.cpp').read_text()
        self.assertIn('h.allocation[11]=0x75;h.allocation[12]=14',source)
        self.assertIn('absoluteJump(h.allocation+13,(void*)taken)',source)
        self.assertIn('absoluteJump(h.allocation+27,(void*)resume)',source)
        self.assertIn('copy(h.allocation+64,unwind,8)',source)
        self.assertIn('*h.table={0,41,64}',source)
        self.assertEqual(byte_array('NewAppearanceBytes')[:13],bytes.fromhex('40 53 48 83 EC 20 48 8B D9 84 D2 0F 84'))
        self.assertEqual(byte_array('NewInputBytes')[:15],bytes.fromhex('48 89 5c 24 20 48 89 4c 24 08 55 56 57 41 54'))
        self.assertEqual(byte_array('NewUpdateBytes')[:15],bytes.fromhex('48 8b c4 48 89 58 10 4c 89 48 20 4c 89 40 18'))
        info=(ROOT/'source/DialogueResolver.h').read_text()
        m=re.search(r'NewInputTrampolineUnwind\[16\]\s*=\s*\{([^}]+)\}',info,re.S)
        self.assertTrue(m)
        values=[int(v,0) for v in re.findall(r'0x[0-9a-fA-F]+|\d+',m.group(1))]
        self.assertEqual(values,[1,15,6,0,15,0xC0,13,0x70,12,0x60,11,0x50,5,0x34,8,0])

    def test_cutscene_pause_wrapper_may_be_hidden(self):
        behavior = (ROOT/'source/Behavior.h').read_text()
        diagnostics = (ROOT/'source/Diagnostics.h').read_text()
        harness = (ROOT/'tests/updated_compiled_harness.cpp').read_text()
        self.assertIn('if(requireRow&&((U8*)fw)[0xBE]!=0)return 7;',behavior)
        self.assertNotIn('((U8*)pw)[0xBE]==0',behavior)
        self.assertIn('!widget(pw)',behavior)
        self.assertIn('ptr(w,0)!=ptr(pw,0)',behavior)
        self.assertIn('appearance_view_blocks',diagnostics)
        self.assertIn('last_cutscene_gate_code',diagnostics)
        self.assertIn('if(scenario=="new_pause_hidden")f.guides[5].wrapper[0xBE]=1',harness)
        self.assertIn('CHECK(f.guides[5].wrapper[0xBE]==1)',harness)

    def test_native_keyguide_fields_remain_unmasked(self):
        fields={'GuideBinding':[(0x97,0x1E8),(0x18E,0x1F0),(0x187,0x1F1)],
                'GuideRefresh':[(0xA9,0x1E8),(0x181,0x1F0),(0x18B,0x1F1)]}
        for name, values in fields.items():
            code=byte_array('New'+name+'Bytes');mask=byte_array('New'+name+'Mask')
            for offset, expected in values:
                with self.subTest(name=name,offset=offset):
                    self.assertEqual(int.from_bytes(code[offset:offset+4],'little'),expected)
                    self.assertEqual(mask[offset:offset+4],b'\xff'*4)
        resolver=(ROOT/'source/Resolver.h').read_text()
        self.assertIn('newCall(im,z.guideBinding,0x195,z.guideRefresh)',resolver)
        self.assertIn('resolveGuideLayout3479(im,z)',resolver)

    def test_shared_keyguide_validation_uses_resolved_layout(self):
        text=(ROOT/'source/Behavior.h').read_text()
        self.assertIn('ptr(g,resolved.guideActionOffset)',text)
        self.assertIn('((U8*)g)[resolved.guideFamilyOffset]',text)
        self.assertIn('((U8*)g)[resolved.guideSubtypeOffset]',text)
        self.assertNotIn('ptr(g,0x1F8)',text)
        self.assertNotIn('((U8*)g)[0x200]',text)
        self.assertIn('return actual==hash?0:4;',text)

    def test_current_harness_initializes_bindings_through_native_methods(self):
        text=(ROOT/'tests/updated_compiled_harness.cpp').read_text()
        self.assertIn('((Bind)(base+0xE434C0))',text)
        self.assertIn('CHECK(nativeBindingLookups==lookups+2)',text)
        self.assertNotIn('mockSetAppearance',text)
        self.assertIn('new_bad_action',text)
        self.assertIn('new_null_action',text)
        self.assertIn('new_binding_shape',text)
        self.assertIn('new_binding_link',text)

    def test_new_input_unwind_recovers_native_caller_frame(self):
        # Model the copied instructions' RSP delta and the actual Windows
        # UWOP_PUSH_NONVOL / UWOP_SAVE_NONVOL stack-slot offsets. No Windows
        # RtlVirtualUnwind is available in this offline harness.
        register={3:0x333,5:0x555,6:0x666,7:0x777,12:0xCCC}
        frame={};sp=0x1000
        frame[sp+0x20]=register[3]
        for reg in (5,6,7,12):
            sp-=8;frame[sp]=register[reg]
        self.assertEqual(sp,0xFE0)
        for reg in (12,7,6,5):
            self.assertEqual(frame[sp],register[reg])
            sp+=8
        self.assertEqual(frame[sp+0x20],register[3])
        self.assertEqual(sp,0x1000)

    def test_new_branch_preserves_native_control(self):
        source=(ROOT/'source/Sequencer.h').read_text()
        self.assertIn('readyMode',source)
        self.assertIn('0x10B6',source)
        self.assertIn('0x10B5',source)
        self.assertIn('core->mode()!=3',(ROOT/'source/Dialogue.h').read_text())
        self.assertIn('0x110u:0x120u',(ROOT/'source/Dialogue.h').read_text())

if __name__=='__main__': unittest.main()
