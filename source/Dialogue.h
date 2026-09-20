#pragma once
#include "Behavior.h"
#include "DialogueResolver.h"
namespace crimson {
using DialogueUpdate=U32*(NATIVE_ABI*)(void*,U32*,void*,void*,float,U64);
using DialogueAdvance=void(NATIVE_ABI*)(void*,void*);
using DialogueCurrent=void*(NATIVE_ABI*)(void*);
using DialogueInput=void(NATIVE_ABI*)(void*,void*,void*,U8);
using DialogueStop=void(NATIVE_ABI*)(void*,U32);
using DialogueClock=U64(*)();
enum DialogueReason:U32 { DIdle,DReady,DRequested,DProcessing,DChoice,DEnd,DStale,DChanged,DGuard,DStalled,DBudget,DDisabled };
struct DialogueStats {
    U32 updates=0,prompts=0,promptVerified=0,requests=0,accepted=0,steps=0,choices=0,completed=0;
    U32 stale=0,guards=0,yields=0,stalls=0,offThread=0,reason=DIdle;
    U32 candidates=0,offers=0,lastKind=255,lastMode=255,lastChoice=255;
};
struct DialogueTicket {
    U64 actor=0,player=0,definition=0,participants=0;
    U32 group=0,owner=0;
    bool sameConversation(const DialogueTicket&t)const {
        return actor==t.actor&&player==t.player&&definition==t.definition&&participants==t.participants&&group==t.group&&owner==t.owner;
    }
};
struct DialogueCursor { U32 entry=0,branch=0,voice=0;U8 line=0; };
inline bool sameCursor(const DialogueCursor&a,const DialogueCursor&b){return a.entry==b.entry&&a.branch==b.branch&&a.voice==b.voice&&a.line==b.line;}
struct DialogueOffer { DialogueTicket ticket;DialogueCursor cursor;U64 time=0;bool available=false; };
struct Dialogue {
    Behavior*core=nullptr;DialogueResolved resolved;
    DialogueUpdate originalUpdate=nullptr;DialogueInput originalInput=nullptr;
    DialogueAdvance advance=nullptr;DialogueCurrent current=nullptr;DialogueStop stopVoice=nullptr;
    DialogueClock clock=nullptr;
    U32 enabled=0,mailboxLock=0,updateLock=0,dirty=0;
    DialogueOffer offer,request;bool requested=false;
    // Only the natural dialogue-update callback owns the job. No dialogue/context
    // pointer is kept: the game's callers pass temporary stack copies of it.
    DialogueTicket job;bool running=false;U64 jobStarted=0;U32 jobSteps=0;
    DialogueStats counters;
    static U64 qword(const void*p,U32 o){U64 v=0;copy(&v,(const U8*)p+o,8);return v;}
    bool ok(const void*p,U32 n)const{return core&&core->ok(p,n);}
    void*ptr(const void*p,U32 o)const{return (void*)qword(p,o);}
    static bool lock(U32&v){U32 zero=0;return __atomic_compare_exchange_n(&v,&zero,1,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED);}
    static void unlock(U32&v){__atomic_store_n(&v,0,__ATOMIC_RELEASE);}
    void count(U32&v){__atomic_fetch_add(&v,1,__ATOMIC_RELAXED);__atomic_store_n(&dirty,1,__ATOMIC_RELEASE);}
    void reason(DialogueReason r){__atomic_store_n(&counters.reason,(U32)r,__ATOMIC_RELAXED);__atomic_store_n(&dirty,1,__ATOMIC_RELEASE);}
    bool active()const{return __atomic_load_n(&enabled,__ATOMIC_ACQUIRE)!=0;}
    DialogueStats snapshot()const {
        DialogueStats s;
#define GET(f) s.f=__atomic_load_n(&counters.f,__ATOMIC_RELAXED)
        GET(updates);GET(prompts);GET(promptVerified);GET(requests);GET(accepted);GET(steps);GET(choices);GET(completed);
        GET(stale);GET(guards);GET(yields);GET(stalls);GET(offThread);GET(reason);
        GET(candidates);GET(offers);GET(lastKind);GET(lastMode);GET(lastChoice);
#undef GET
        return s;
    }
    void*gameState()const {
        void*g=ptr(core->image.base+resolved.symbols[DialogueSymWorld],0);
        if(!ok(g,0x28))return nullptr;void*s=ptr(g,0x20);
        if(!ok(s,0x11D8))return nullptr;
        void*m=ptr(core->image.base+core->resolved.symbols[Sym_GameManager],0);
        if(!ok(m,0x98)||ptr(m,0x90)!=s)return nullptr;
        return s;
    }
    // Read the already-loaded rule cache, never load a rule, chase a stale stack
    // address, or call dialogue-native methods from the UI/reporting threads.
    void*record(void*ctx)const {
        void*store=ptr(core->image.base+resolved.symbols[DialogueSymRuleStore],0);
        if(!ok(store,0x60))return nullptr;
        U32 id=u32((U8*)ctx+4),index=u32((U8*)ctx+8),count=u32((U8*)store+8);
        if(count>1000000||id>=count||index>65535)return nullptr;
        void**cache=(void**)ptr(store,0x58);
        if(!ok(cache,count*8)||!ok(cache[id],0x130))return nullptr;
        void*database=cache[id];U32 rows=u32((U8*)database+0x128);
        if(!rows||rows>65536||index>=rows)return nullptr;
        U8*data=(U8*)ptr(database,0x120);
        if(!ok(data,rows*0x90))return nullptr;return data+index*0x90;
    }
    bool vectorReadable(const void*owner,U32 offset,U32 stride,U32 limit=65536)const {
        U32 n=u32((const U8*)owner+offset+8),cap=u32((const U8*)owner+offset+12);
        return n<=cap&&cap<=limit&&(!n||ok(ptr(owner,offset),n*stride));
    }
    bool context(void*ctx,void*actor,void*player,DialogueTicket&t)const {
        if(!ok(ctx,0x80)||!ok(actor,0x70)||!ok(player,0x70))return false;
        U8*c=(U8*)ctx;
        if(c[0]!=1||c[0x3D]||(!c[0x40]&&!c[0x41]))return false;
        U32 n=u32(c+0x70),cap=u32(c+0x74);
        if(n>cap||cap>65536||(n&&!ok(ptr(c,0x68),n*24)))return false;
        void*r=record(ctx);if(!r||((U8*)r)[2]>1)return false;
        // Mode 3 is claimed by the native interaction-dialogue path only for
        // a record without the separate topic-selection list.
        if(u32((U8*)r+0x70)!=0||u16(c+0x66)!=0xFFFFu)return false;
        if(!vectorReadable(r,0x48,0x70)||!vectorReadable(r,0x78,0x78))return false;
        t.actor=(U64)actor;t.player=(U64)player;t.definition=qword(c,4);
        t.participants=qword(c,0x10);t.group=u32(c+0x30);t.owner=u32(c+0x1C);
        return true;
    }
    DialogueCursor cursor(void*ctx)const {U8*c=(U8*)ctx;return {u32(c+0x58),u32(c+0x64),u32(c+0x50),c[0x5C]};}
    bool node(void*ctx,void*&entry)const {
        entry=current(ctx);
        if(!entry)return true;
        if(!ok(entry,0x70))return false;
        U32 n=u32((U8*)entry+0x68);int line=(signed char)((U8*)ctx)[0x5C];
        return n<=127&&line>=-1&&(line<0||(U32)line<n)&&(!n||ok(ptr(entry,0x60),n*0x30));
    }
    bool eligible(void*ctx,void*actor,void*player,DialogueTicket&t,void*&entry)const {
        return context(ctx,actor,player,t)&&!((U8*)ctx)[0x5D]&&node(ctx,entry)&&entry;
    }
    void announce(void*ctx,void*actor,void*player) {
        DialogueOffer o;void*entry=nullptr;
        o.available=!running&&eligible(ctx,actor,player,o.ticket,entry);
        // The native update claims mode 3 before setting its ownership byte.
        // Pre-arm only while that mode is not yet owned; subsequent offers must
        // come from the dialogue which actually owns the cinematic control row.
        if(o.available&&!((U8*)ctx)[0x41]&&core->mode()==3)o.available=false;
        if(o.available){o.cursor=cursor(ctx);o.time=clock();count(counters.offers);}
        if(lock(mailboxLock)){
            if(o.available)offer=o;
            else if(offer.ticket.actor==(U64)actor&&offer.ticket.player==(U64)player)offer.available=false;
            unlock(mailboxLock);
        }
    }
    bool available() {
        if(!active())return false;
        bool ready=false;
        if(lock(mailboxLock)){ready=offer.available&&!requested&&clock()-offer.time<=500;unlock(mailboxLock);}
        return ready;
    }
    bool fixedViews(void*self,void*&wrap,void*&guide,bool row)const {
        if(!ok(self,0x308))return false;wrap=ptr(self,0x2B8);guide=ptr(self,0x2C0);
        void*ff=ptr(self,0x2B0),*fw=ptr(self,0x2A8);
        if(!core->widget(wrap)||!core->guide(guide,0x8653EBE6u)||!core->guide(ff,0x5EFE1F8Eu)||!core->widget(fw))return false;
        return guide!=ff&&ptr(guide,0)==ptr(ff,0)&&ptr(wrap,0)==ptr(fw,0)&&(!row||!((U8*)fw)[0xBE]);
    }
    void beforeInteraction(void*self,U8 enabledValue) {
        if(!enabledValue||core->mode()!=3||!available())return;
        void*w=nullptr,*g=nullptr;if(fixedViews(self,w,g,false))core->show(w);
    }
    void afterInteraction(void*self,U8 enabledValue) {
        if(!active()||core->mode()!=3)return;
        void*w=nullptr,*g=nullptr;
        if(!fixedViews(self,w,g,enabledValue!=0))return;
        if(enabledValue&&available()){core->show(w);core->show(g);}
        else if(!enabledValue)core->hide(w);
    }
    void afterAppearance(void*self,U8 enabledValue) {
        if(!enabledValue||core->mode()!=3||!available())return;
        void*w=nullptr,*g=nullptr;RootState before;
        if(!fixedViews(self,w,g,true)||!core->root(g,before)){count(counters.guards);return;}
        core->show(w);core->show(g);core->setAppearance(g,1);count(counters.prompts);
        RootState after;if(core->root(g,after)&&after.appear==1&&after.disappear==0)count(counters.promptVerified);
    }
    bool input(void*self,void*event,void*guide,U8 phase) {
        if(!active()||core->mode()!=3||!ok(self,0x360)||guide!=ptr(self,0x2C0))return false;
        // Never route this restored dialogue button into the cutscene-only
        // completion branch, even if a stale/repeated notification is rejected.
        if(phase!=0)return true;
        count(counters.requests);
        void*w=nullptr,*g=nullptr;
        if(!(((U8*)self)[0x35C]&2)||!fixedViews(self,w,g,true)||((U8*)w)[0xBE]||!event){count(counters.guards);reason(DGuard);return true;}
        U32 n=u32((U8*)self+0x230);void**events=(void**)ptr(self,0x228);bool found=false;
        if(n<=1024&&ok(events,n*8))for(U32 i=0;i<n;++i)if(events[i]==event)found=true;
        if(!found){count(counters.guards);reason(DGuard);return true;}
        if(lock(mailboxLock)){
            if(offer.available&&!requested&&clock()-offer.time<=500){request=offer;requested=true;offer.available=false;reason(DRequested);}
            else{count(counters.stale);reason(DStale);}
            unlock(mailboxLock);
        }
        return true;
    }
    U32*update(void*ctx,U32*result,void*actor,void*player,float dt,U64 gameTime) {
        count(counters.updates);
        if(!active()||!lock(updateLock)){count(counters.offThread);return originalUpdate(ctx,result,actor,player,dt,gameTime);}
        // Native callbacks can synchronously notify UI; the mailbox is NEVER held
        // across a game call. Nested updates bypass the optional feature.
        U32*out=updateLocked(ctx,result,actor,player,dt,gameTime);
        unlock(updateLock);return out;
    }
    U32*updateLocked(void*ctx,U32*result,void*actor,void*player,float dt,U64 gameTime) {
        if(ok(ctx,0x80)){
            __atomic_store_n(&counters.lastKind,(U32)((U8*)ctx)[0],__ATOMIC_RELAXED);
            __atomic_store_n(&counters.lastMode,(U32)core->mode(),__ATOMIC_RELAXED);
            __atomic_store_n(&counters.lastChoice,(U32)((U8*)ctx)[0x5D],__ATOMIC_RELAXED);
            if(((U8*)ctx)[0]==1)count(counters.candidates);
        }
        DialogueTicket t;void*entry=nullptr;bool valid=eligible(ctx,actor,player,t,entry);
        bool received=false;DialogueOffer r;
        if(lock(mailboxLock)){
            if(requested){
                if(clock()-request.time>1000){requested=false;count(counters.stale);reason(DStale);}
                else if(valid&&request.ticket.sameConversation(t)){r=request;received=true;requested=false;}
            }
            unlock(mailboxLock);
        }
        if(received){
            if(!sameCursor(r.cursor,cursor(ctx))||core->mode()!=3||qword(ctx,0x48)>gameTime){count(counters.stale);reason(DStale);}
            else if(!running&&gameState()) {job=t;running=true;jobStarted=clock();jobSteps=0;count(counters.accepted);}
        }
        if(running&&(core->mode()!=3||clock()-jobStarted>2000)){
            running=false;count(counters.guards);reason(DChanged);
        }
        // Other NPCs can update between frames of the foreground conversation.
        // They neither consume its job nor invalidate its fresh UI offer.
        if(running&&(job.actor!=(U64)actor||job.player!=(U64)player)){
            U32*out=originalUpdate(ctx,result,actor,player,dt,gameTime);
            announce(ctx,actor,player);return out;
        }
        if(running&&(!valid||!job.sameConversation(t))){
            running=false;count(counters.guards);reason(DChanged);
        }
        if(running){
            reason(DProcessing);
            void*state=gameState();void*manager=state?ptr(state,0x11D0):nullptr;
            if(!ok(manager,8)||!ok(result,4)){running=false;count(counters.guards);reason(DGuard);}
            else {
                U8*c=(U8*)ctx;U32 voice=u32(c+0x50);
                if(voice!=0xFFFFFFFFu){
                    stopVoice(manager,voice);
                    // Exact bookkeeping used by the native Fast-Forward branch;
                    // the global Fast-Forward flag itself is never written.
                    put32(c+0x50,0xFFFFFFFFu);
                    put32(c+0x18,u32(core->image.base+resolved.symbols[DialogueSymInvalidVoiceKey]));
                }
                put32(c+0x38,0); // Only this conversation's inter-line wait.
                U64 start=clock();
                for(U32 i=0;i<64;++i){
                    if(c[0x5D]){running=false;count(counters.choices);reason(DChoice);break;}
                    if(!entry){running=false;count(counters.completed);reason(DEnd);break;}
                    DialogueCursor previous=cursor(ctx);
                    advance(ctx,actor);++jobSteps;count(counters.steps);
                    if(c[0x5D]){running=false;count(counters.choices);reason(DChoice);break;}
                    DialogueTicket next;
                    if(!context(ctx,actor,player,next)||!job.sameConversation(next)||!node(ctx,entry)){
                        running=false;count(counters.guards);reason(DGuard);*result=0;announce(ctx,actor,player);return result;
                    }
                    if(!entry){running=false;count(counters.completed);reason(DEnd);break;}
                    if(sameCursor(previous,cursor(ctx))||jobSteps>=4096){
                        running=false;count(counters.stalls);reason(DStalled);*result=0;announce(ctx,actor,player);return result;
                    }
                    if(clock()-start>=16)break;
                }
                if(running){count(counters.yields);reason(DBudget);*result=0;announce(ctx,actor,player);return result;}
            }
        }
        // At a choice native update sees its wait flag and returns normally.
        // At the end it sees a null current entry and uses its OWN finish call,
        // arguments, events and result code. We do not invent a cancellation.
        announce(ctx,actor,player);
        U32*out=originalUpdate(ctx,result,actor,player,dt,gameTime);
        announce(ctx,actor,player);
        return out;
    }
};
}
