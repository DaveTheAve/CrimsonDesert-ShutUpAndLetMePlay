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
struct DialogueResolved {
    U32 input=0,update=0,advance=0,current=0,finish=0,database=0;
    U32 symbols[DialogueSymbolCount]{};
    const char* failure="dialogue resolver not run";
    bool ready=false;
};
inline bool executableRange(const Image& im,U32 r,U32 n=1) {
    if(!im.range(r,n))return false;
    const U8*b=im.base;U32 nt=u32(b+0x3C),os=u16(b+nt+20),count=u16(b+nt+6);
    for(U32 i=0;i<count;++i){const U8*s=b+nt+24+os+i*40;U32 va=u32(s+12),size=u32(s+8);
        if((u32(s+36)&0x20000000u)&&r>=va&&r-va<size&&n<=size-(r-va))return true;}
    return false;
}
inline bool dialogueUnwind(const Image&im,U32 r,const DialoguePattern&p) {
    U32 lo=0,hi=im.exceptionSize/12;
    while(lo<hi){U32 m=lo+(hi-lo)/2;const U8*e=im.base+im.exceptionRva+12*m;if(u32(e)<r)lo=m+1;else hi=m;}
    if(lo==im.exceptionSize/12)return false;
    const U8*e=im.base+im.exceptionRva+lo*12;U32 uw=u32(e+8);
    return u32(e)==r&&u32(e+4)==r+p.shape.size&&im.range(uw,p.unwindSize)&&same(im.base+uw,p.unwind,p.unwindSize);
}
inline bool dialogueReferences(const Image&im,U32 r,const DialoguePattern&p,DialogueResolved&z) {
    for(U32 i=0;i<p.refCount;++i){const auto&q=p.refs[i];U32 target=relative(im,r,q.displacement,q.next);
        if(!target||!im.range(target,8)||(q.executable&&!executableRange(im,target,5)))return false;
        U32&v=z.symbols[q.symbol];if(v&&v!=target)return false;v=target;}
    return dialogueUnwind(im,r,p);
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
