// UI-thread-only behavior. No child-selector Show calls, forced glyph families,
// binding rewrites, direct rebuilds, or writes to native hold/progress state.
#pragma once
#include "Resolver.h"
#if defined(_WIN32)
#define NATIVE_ABI __fastcall
#else
#define NATIVE_ABI __attribute__((ms_abi))
#endif
namespace crimson {
using Toggle=void(NATIVE_ABI*)(void*,U8);using WidgetCall=void(NATIVE_ABI*)(void*);
using Readable=bool(*)(const void*,U32);
struct RootState { I32 appear=-1,disappear=-1; };
struct Observation {
    U32 interactionCalls=0,appearanceCalls=0,preUnlocks=0,inputShows=0,appearanceRepairs=0,appearanceVerified=0,blocks=0,cleanupCalls=0;
    U32 lastMode=255,lastInputFamily=255,lastInputSubtype=255;
    I32 lastAppearBefore=-1,lastDisappearBefore=-1,lastAppearAfter=-1,lastDisappearAfter=-1;
    U64 lastView=0,lastInput=0;
};
struct Behavior {
    Image image;Resolved resolved;Readable readable=nullptr;
    Toggle originalInteraction=nullptr,originalAppearance=nullptr,setAppearance=nullptr;
    WidgetCall show=nullptr,hide=nullptr;
    Observation stats;
    // Observations are handed off by the outer callback, never by chasing stored
    // game pointers from the reporting thread.
    bool ok(const void*p,U32 n)const{return p&&readable&&readable(p,n);}
    void*ptr(const void*p,U32 o)const{return *(void*const*)((const U8*)p+o);}
    U8 mode()const{
        const void*g=image.base+resolved.symbols[Sym_GameManager];if(!ok(g,8))return 255;
        void*m=ptr(g,0);if(!ok(m,0x98))return 255;void*s=ptr(m,0x90);if(!ok(s,0x107F))return 255;
        return ((const U8*)s)[0x107E];
    }
    bool widget(void*w)const{
        if(!ok(w,0xC0))return false;
        const void*vt=ptr(w,0);if(!image.pointer(vt,8))return false;
        return ok(ptr(w,8),0xA8);
    }
    bool guide(void*g,U32 hash)const{
        if(!widget(g)||!ok(g,0x248))return false;
        void*a=ptr(g,0x1F8);return ok(a,0x30)&&u32((U8*)a+0x2C)==hash;
    }
    bool views(void*self,void*&w,void*&g,bool requireRow)const{
        if(!ok(self,0x308))return false;w=ptr(self,0x2E8);g=ptr(self,0x2F0);
        if(!widget(w)||!guide(g,0x8653EBE6u))return false;
        void*ff=ptr(self,0x2E0),*pause=ptr(self,0x300);
        if(!guide(ff,0x5EFE1F8Eu)||!guide(pause,0xED9DCCC4u)||ff==g||pause==g||ff==pause)return false;
        if(ptr(ff,0)!=ptr(g,0)||ptr(g,0)!=ptr(pause,0))return false;
        void*fw=ptr(self,0x2D8),*pw=ptr(self,0x2F8);
        if(!widget(fw)||!widget(pw)||ptr(w,0)!=ptr(fw,0)||ptr(w,0)!=ptr(pw,0))return false;
        return !requireRow||(((U8*)fw)[0xBE]==0&&((U8*)pw)[0xBE]==0);
    }
    // The actual style vector is guide+8 -> backing+20 -> controller+8.
    // It contains 32-bit interned style IDs; it is NOT a child widget.
    bool root(void*g,RootState&st)const{
        void*b=ptr(g,8);if(!ok(b,0xA8))return false;
        void*c=ptr(b,0x20);if(!ok(c,16)||ptr(c,0)!=b)return false;
        void*backend=ptr(b,0xA0);
        if(backend&&(!ok(backend,16)||ptr(backend,8)))return false;
        void*v=ptr(c,8);if(!ok(v,16))return false;
        U32 n=u32((U8*)v+8),cap=u32((U8*)v+12);void*data=ptr(v,0);
        if(n>cap||cap>4096||n>256||(n&&!ok(data,n*4)))return false;
        const U8*a=image.base+resolved.symbols[Sym_AppearStyle],*d=image.base+resolved.symbols[Sym_DisappearStyle];
        if(!ok(a,4)||!ok(d,4))return false;U32 aid=u32(a),did=u32(d);
        if(!aid||!did||aid==did)return false;st.appear=st.disappear=0;
        for(U32 i=0;i<n;++i){U32 id=u32((U8*)data+4*i);if(id==aid)st.appear=1;if(id==did)st.disappear=1;}
        return true;
    }
    void interaction(void*self,U8 enabled){
        ++stats.interactionCalls;U8 m=mode();stats.lastMode=m;
        void*w=nullptr,*g=nullptr;
        // Preserve the previously working ordering: wrapper before native event
        // activation, not after the native event list has already been enabled.
        if(enabled&&m==1){if(views(self,w,g,false)){show(w);++stats.preUnlocks;}else ++stats.blocks;}
        originalInteraction(self,enabled);
        if(enabled&&mode()==1){
            if(views(self,w,g,true)){show(w);show(g);++stats.inputShows;}else ++stats.blocks;
        }else if(!enabled&&m==1){
            // Native Interaction's disable branch repeats Pause and omits Skip.
            // Close only the verified normal Skip wrapper, on the same UI thread.
            if(views(self,w,g,false)){hide(w);++stats.cleanupCalls;}
        }
    }
    void appearance(void*self,U8 enabled){
        ++stats.appearanceCalls;
        originalAppearance(self,enabled);
        U8 m=mode();stats.lastMode=m;
        // Native appearance(false) and every non-gameplay mode stay native.
        if(!enabled||m!=1)return;
        void*w=nullptr,*g=nullptr;RootState before;
        if(!views(self,w,g,true)||!root(g,before)){++stats.blocks;return;}
        // A preceding native hide update can close the wrapper, independently
        // of the root's appear/disappear styles. Restore BOTH layers here.
        show(w);show(g);setAppearance(g,1);++stats.appearanceRepairs;
        RootState after;bool got=root(g,after);
        if(got&&after.appear==1&&after.disappear==0)++stats.appearanceVerified;
        stats.lastAppearBefore=before.appear;stats.lastDisappearBefore=before.disappear;
        stats.lastAppearAfter=after.appear;stats.lastDisappearAfter=after.disappear;
        stats.lastView=(U64)self;stats.lastInput=(U64)g;
        stats.lastInputFamily=((U8*)g)[0x200];stats.lastInputSubtype=((U8*)g)[0x201];
    }
};
}
