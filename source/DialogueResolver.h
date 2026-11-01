#pragma once
#include "Resolver.h"
#include "DialogueSignatures.h"
namespace crimson {
// These prologues contain no RIP-relative instructions. Each trampoline copies
// exactly 15 bytes and has its own unwind description; the native body keeps its
// original exception/unwind metadata.
static const U8 DialogueInputTrampolineUnwind[16] =
    {1,15,6,0,15,0x34,8,0,15,0xC0,13,0x70,12,0x60,11,0x50};
static const U8 DialogueUpdateTrampolineUnwind[16] =
    {1,15,6,0,15,0x74,4,0,15,0x64,3,0,15,0x34,2,0};
// The refactored update's copied entry moves no stack pointer or nonvolatile
// register before byte 15. Windows therefore needs a zero-operation UNWIND_INFO.
// The refactored native input's first 15 bytes save RBX at [entry RSP+0x20],
// then push RBP, RSI, RDI, R12. At the continuation SP has moved 0x20, so
// RBX resides at [current RSP+0x40], encoded as 8 scaled stack slots.
static const U8 NewInputTrampolineUnwind[16] =
    {1,15,6,0,15,0xC0,13,0x70,12,0x60,11,0x50,5,0x34,8,0};
static const U8 NewUpdateTrampolineUnwind[4] = {1,0,0,0};
struct DialogueResolved {
    U32 input=0,update=0,advance=0,current=0,finish=0,database=0;
    U32 symbols[DialogueSymbolCount]{};
    const char* failure="dialogue resolver not run";
    bool ready=false;
};
inline bool executableRange(const Image& im,U32 r,U32 n=1) {
    return im.code(r,n);
}
inline bool dialogueUnwind(const Image&im,U32 r,const DialoguePattern&p) {
    U32 lo=0,hi=im.exceptionSize/12;
    while(lo<hi){U32 m=lo+(hi-lo)/2;const U8*e=im.base+im.exceptionRva+12*m;if(u32(e)<r)lo=m+1;else hi=m;}
    if(lo==im.exceptionSize/12)return false;
    const U8*e=im.base+im.exceptionRva+lo*12;U32 uw=u32(e+8);
    return u32(e)==r&&u32(e+4)==r+p.shape.size&&im.accessible(uw,p.unwindSize)&&same(im.base+uw,p.unwind,p.unwindSize);
}
inline bool dialogueReferences(const Image&im,U32 r,const DialoguePattern&p,DialogueResolved&z) {
    for(U32 i=0;i<p.refCount;++i){const auto&q=p.refs[i];U32 target=relative(im,r,q.displacement,q.next);
        if(!target||!im.range(target,8)||(q.executable&&!executableRange(im,target,5)))return false;
        U32&v=z.symbols[q.symbol];if(v&&v!=target)return false;v=target;}
    return dialogueUnwind(im,r,p);
}

inline bool resolveDialogue3479(const Image&im,const Resolved&core,DialogueResolved&z){
    z=DialogueResolved{};
    if(core.profile!=1){z.failure="updated dialogue profile not active";return false;}
    z.input=unique(im,NewInputSignature);
    z.update=unique(im,NewUpdateSignature);
    z.advance=unique(im,NewAdvanceSignature);
    z.current=unique(im,NewCurrentSignature);
    z.finish=unique(im,NewFinishSignature);
    if(!z.input||!z.update||!z.advance||!z.current||!z.finish){
        z.failure="updated native dialogue shape missing or ambiguous";return false;
    }
    U32 database=relative(im,z.current,0xD,0x11);
    U32 finishDB=relative(im,z.finish,0x33,0x37);
    if(!database||database!=finishDB||!im.code(database,NewDatabaseSignature.size)||
       !masked(im.base+database,NewDatabaseSignature)){
        z.failure="linked dialogue rule-cache getter no longer matches";return false;
    }
    z.database=database;
    U32 world=relative(im,z.update,0x60,0x64);
    U32 game=relative(im,z.input,0x134,0x138);
    U32 rule=relative(im,database,0x1D,0x21);
    U32 stop=relative(im,z.update,0x87,0x8B);
    U32 invalidKey=relative(im,z.update,0x94,0x98);
    U32 finished=relative(im,z.update,0x14E,0x152);
    if(!world||!game||!rule||!stop||!invalidKey||!finished||
       !im.accessible(world,8)||!im.accessible(rule,8)||
       !im.accessible(invalidKey,4)||!im.accessible(finished,4)||
       game!=core.symbols[Sym_GameManager]||
       z.input+0x168!=core.skipSemantic||
       !im.code(stop,5)||im.base[stop]!=0xE9||
       !im.code(relative(im,stop,1,5),16)||
       !newCall(im,z.update,0xA6,z.current)||
       !newCall(im,z.update,0x11C,z.advance)||
       !newCall(im,z.update,0x147,z.finish)){
        z.failure="updated native dialogue call chain or data globals mismatch";return false;
    }
    z.symbols[DialogueSymWorld]=world;
    z.symbols[DialogueSymGame]=game;
    z.symbols[DialogueSymRuleStore]=rule;
    z.symbols[DialogueSymStopVoice]=stop;
    z.symbols[DialogueSymInvalidVoiceKey]=invalidKey;
    z.symbols[DialogueSymFinishedResult]=finished;
    z.symbols[DialogueSymDatabase]=database;
    z.symbols[DialogueSymAdvance]=z.advance;
    z.symbols[DialogueSymCurrent]=z.current;
    z.symbols[DialogueSymFinish]=z.finish;
    const U8 inputUw[]={0x11,0x24,0x0b,0,0x24,0x34,0x45,0,0x24,0x01,0x3a,0,0x15,0xf0,0x13,0xe0,0x11,0xd0,0x0f,0xc0,0x0d,0x70,0x0c,0x60,0x0b,0x50,0,0};
    const U8 updateUw[]={0x11,0x32,0x0f,0,0x32,0x78,0x44,0,0x2d,0x68,0x45,0,0x28,0x34,0x95,0,0x28,0x01,0x8c,0,0x1a,0xf0,0x18,0xe0,0x16,0xd0,0x14,0xc0,0x12,0x70,0x11,0x60,0x10,0x50,0,0};
    const U8 advanceUw[]={0x11,0x25,0x0a,0,0x25,0x01,0x37,0,0x16,0xf0,0x14,0xe0,0x12,0xd0,0x10,0xc0,0x0e,0x70,0x0d,0x60,0x0c,0x30,0x0b,0x50};
    const U8 currentUw[]={1,6,2,0,6,0x32,2,0x30};
    const U8 finishUw[]={0x11,0x22,0x0a,0,0x22,0x01,0x2f,0,0x1b,0xf0,0x19,0xe0,0x17,0xd0,0x15,0xc0,0x13,0x70,0x12,0x60,0x11,0x50,0x10,0x30};
    const U8 databaseUw[]={0x11,0x18,0x0a,0,0x18,0x64,0x13,0,0x18,0x54,0x12,0,0x18,0x34,0x10,0,0x18,0xb2,0x14,0xf0,0x12,0xe0,0x10,0x70};
    if(!newUnwind(im,z.input,NewInputSignature.size,inputUw,sizeof(inputUw))||
       !newUnwind(im,z.update,NewUpdateSignature.size,updateUw,sizeof(updateUw))||
       !newUnwind(im,z.advance,NewAdvanceSignature.size,advanceUw,sizeof(advanceUw))||
       !newUnwind(im,z.current,NewCurrentSignature.size,currentUw,sizeof(currentUw))||
       !newUnwind(im,z.finish,NewFinishSignature.size,finishUw,sizeof(finishUw))||
       !newUnwind(im,z.database,NewDatabaseSignature.size,databaseUw,sizeof(databaseUw))){
        z.failure="updated dialogue unwind or function boundary validation failed";return false;
    }
    z.ready=true;z.failure=nullptr;return true;
}
inline bool resolveDialogue(const Image&im,const Resolved&core,DialogueResolved&z) {
    z=DialogueResolved{};
    U32*dest[]={&z.input,&z.update,&z.advance,&z.current,&z.finish,&z.database};
    const DialoguePattern*p[]={&DialogueInputPattern,&DialogueUpdatePattern,&DialogueAdvancePattern,
        &DialogueCurrentPattern,&DialogueFinishPattern,&DialogueDatabasePattern};
    for(U32 i=0;i<5;++i){*dest[i]=unique(im,p[i]->shape);
        if(!*dest[i]){z.failure="dialogue function shape missing or ambiguous; cutscene support retained";return false;}}
    for(U32 i=0;i<5;++i)if(!dialogueReferences(im,*dest[i],*p[i],z)){
        z.failure="dialogue cross-reference or unwind validation failed; cutscene support retained";return false;}
    // The cache getter is a repeated generic function shape. Resolve it only
    // through the agreeing calls in the unique dialogue functions, not by
    // accepting an arbitrary match of that generic shape.
    z.database=z.symbols[DialogueSymDatabase];
    if(!im.code(z.database,DialogueDatabasePattern.shape.size)||
       !masked(im.base+z.database,DialogueDatabasePattern.shape)||
       !dialogueReferences(im,z.database,DialogueDatabasePattern,z)){
        z.failure="linked dialogue database getter failed validation";return false;
    }
    if(z.input+0x172!=core.skipSemantic||z.symbols[DialogueSymGame]!=core.symbols[Sym_GameManager]||
       z.symbols[DialogueSymAdvance]!=z.advance||z.symbols[DialogueSymCurrent]!=z.current||
       z.symbols[DialogueSymFinish]!=z.finish||z.symbols[DialogueSymDatabase]!=z.database){
        z.failure="dialogue and cinematic routes do not agree; cutscene support retained";return false;}
    U32 stop=z.symbols[DialogueSymStopVoice];
    if(im.base[stop]!=0xE9||!executableRange(im,relative(im,stop,1,5),16)){
        z.failure="native dialogue voice-stop thunk not recognized; cutscene support retained";return false;}
    z.ready=true;z.failure=nullptr;return true;
}
}
