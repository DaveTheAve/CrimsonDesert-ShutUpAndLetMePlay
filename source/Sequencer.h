#pragma once
#include "Dialogue.h"
#include "SequencerSignatures.h"
namespace crimson {
struct SequencerResolved {
    U32 skip=0,finish=0,symbols[SequencerSymbolCount]{};
    bool ready=false;
    const char* failure="sequencer route not resolved";
};
inline bool sequencerReferences(const Image&im,U32 r,const DialoguePattern&p,SequencerResolved&z){
    for(U32 i=0;i<p.refCount;++i){const auto&q=p.refs[i];U32 target=relative(im,r,q.displacement,q.next);
        if(q.symbol>=SequencerSymbolCount||!target||!im.range(target,8)||
           (q.executable&&!executableRange(im,target,5)))return false;
        U32&v=z.symbols[q.symbol];if(v&&v!=target)return false;v=target;}
    return dialogueUnwind(im,r,p);
}
inline bool resolveSequencer(const Image&im,const DialogueResolved&dialogue,SequencerResolved&z){
    z=SequencerResolved{};
    if(!dialogue.ready){z.failure="shared native input route unavailable";return false;}
    z.skip=unique(im,SequencerSkipPattern.shape);z.finish=unique(im,SequencerFinishPattern.shape);
    if(!z.skip||!z.finish){z.failure="sequencer Skip or completion shape missing or ambiguous";return false;}
    if(!sequencerReferences(im,z.skip,SequencerSkipPattern,z)||!sequencerReferences(im,z.finish,SequencerFinishPattern,z)){
        z.failure="sequencer cross-reference or unwind validation failed";return false;}
    // This CALL is inside the fully verified native fixed/normal Skip branch.
    // The mod never calls the endpoint itself; the native input callback retains
    // its event checks, live actor handle acquisition, lifetime and arguments.
    if(!im.code(dialogue.input,0x244)||im.base[dialogue.input+0x23F]!=0xE8||
       relative(im,dialogue.input,0x240,0x244)!=z.skip){
        z.failure="native Skip input does not reach the verified sequencer route";return false;}
    U32 callback=z.symbols[SequencerSymCallback];
    if(!im.code(callback,5)||im.base[callback]!=0xE9||relative(im,callback,1,5)!=z.finish){
        z.failure="native sequencer completion callback does not agree";return false;}
    z.ready=true;z.failure=nullptr;return true;
}
struct SequencerStats {
    U32 prompts=0,verified=0,forwarded=0,modeExit=0,guards=0,preUnlocks=0,inputShows=0,cleanup=0;
    U32 mode0Prompts=0,mode2Prompts=0,lastMode=255,lastRequestMode=255;
    U32 appearanceModes[6]{},inputModes[6]{};
};
struct Sequencer {
    Dialogue*dialogue=nullptr;
    SequencerResolved resolved;
    SequencerStats counters;
    U32 dirty=0;
    void count(U32&v){__atomic_fetch_add(&v,1,__ATOMIC_RELAXED);__atomic_store_n(&dirty,1,__ATOMIC_RELEASE);}
    static U32 slot(U8 mode){return mode<=4?mode:5;}
    static bool supported(U8 mode){return mode==0||mode==2;}
    bool active()const{return dialogue&&dialogue->active()&&resolved.ready;}
    SequencerStats snapshot()const{
        SequencerStats s;
#define GET(f) s.f=__atomic_load_n(&counters.f,__ATOMIC_RELAXED)
        GET(prompts);GET(verified);GET(forwarded);GET(modeExit);GET(guards);GET(preUnlocks);GET(inputShows);GET(cleanup);
        GET(mode0Prompts);GET(mode2Prompts);GET(lastMode);GET(lastRequestMode);
#undef GET
        for(U32 i=0;i<6;++i){s.appearanceModes[i]=__atomic_load_n(&counters.appearanceModes[i],__ATOMIC_RELAXED);
            s.inputModes[i]=__atomic_load_n(&counters.inputModes[i],__ATOMIC_RELAXED);}
        return s;
    }
    // No cached actor, stage, or UI object is dereferenced by this route. The
    // native input handler reacquires the live sequencer target on each hold.
    bool readyMode(U8 mode)const{
        if(!active()||!supported(mode))return false;
        Behavior*c=dialogue->core;
        void*facade=dialogue->ptr(c->image.base+resolved.symbols[SequencerSymStateFacade],0);
        if(!dialogue->ok(facade,8))return false;void*state=dialogue->ptr(facade,0);
        if(!dialogue->ok(state,0x1080))return false;
        void*manager=dialogue->ptr(c->image.base+c->resolved.symbols[Sym_GameManager],0);
        return dialogue->ok(manager,0x98)&&dialogue->ptr(manager,0x90)==state&&
            ((U8*)state)[0x107E]==mode&&!((U8*)state)[0x107D];
    }
    void beforeInteraction(void*self,U8 enabled){
        if(!dialogue||!enabled||!readyMode(dialogue->core->mode()))return;
        void*w=nullptr,*g=nullptr;
        if(dialogue->fixedViews(self,w,g,false)){dialogue->core->show(w);count(counters.preUnlocks);}
    }
    void afterInteraction(void*self,U8 enabled){
        if(!active())return;U8 mode=dialogue->core->mode();
        if(!supported(mode))return;void*w=nullptr,*g=nullptr;
        if(!dialogue->fixedViews(self,w,g,enabled!=0))return;
        if(enabled&&readyMode(mode)){dialogue->core->show(w);dialogue->core->show(g);count(counters.inputShows);}
        else if(!enabled){dialogue->core->hide(w);count(counters.cleanup);}
    }
    void afterAppearance(void*self,U8 enabled){
        if(!dialogue||!dialogue->core)return;U8 mode=dialogue->core->mode();
        count(counters.appearanceModes[slot(mode)]);__atomic_store_n(&counters.lastMode,(U32)mode,__ATOMIC_RELAXED);
        if(!enabled||!readyMode(mode))return;
        void*w=nullptr,*g=nullptr;RootState before;
        if(!dialogue->fixedViews(self,w,g,true)||!dialogue->core->root(g,before)){count(counters.guards);return;}
        Behavior*c=dialogue->core;c->show(w);c->show(g);c->setAppearance(g,1);count(counters.prompts);
        count(mode==0?counters.mode0Prompts:counters.mode2Prompts);
        RootState after;if(c->root(g,after)&&after.appear==1&&after.disappear==0)count(counters.verified);
    }
    bool input(void*self,void*event,void*guide,U8 phase){
        if(!dialogue||!dialogue->core)return false;U8 mode=dialogue->core->mode();
        count(counters.inputModes[slot(mode)]);
        if(!active()||!supported(mode)||!dialogue->ok(self,0x360)||guide!=dialogue->ptr(self,0x2C0))return false;
        // Non-completion phases stay entirely native. The actual hold duration
        // and progress state are neither replaced nor synthesized.
        if(phase!=0)return false;
        __atomic_store_n(&counters.lastRequestMode,(U32)mode,__ATOMIC_RELAXED);
        void*w=nullptr,*g=nullptr;
        if(!readyMode(mode)||!(((U8*)self)[0x35C]&2)||!dialogue->fixedViews(self,w,g,true)||
           ((U8*)w)[0xBE]||((U8*)g)[0xBE]||!dialogue->ok(event,8)){count(counters.guards);return true;}
        U32 n=u32((U8*)self+0x230);void**events=(void**)dialogue->ptr(self,0x228);bool found=false;
        if(n&&n<=1024&&dialogue->ok(events,n*8))for(U32 i=0;i<n;++i)if(events[i]==event)found=true;
        if(!found){count(counters.guards);return true;}
        count(counters.forwarded);
        // Do not feed a sequencer conversation into interaction-dialogue advance.
        // This exact native callback already handles both Skip widgets and owns
        // the sequence transition. It still validates the event and actor handle.
        dialogue->originalInput(self,event,guide,phase);
        if(dialogue->core->mode()!=mode)count(counters.modeExit);
        return true;
    }
};
}
