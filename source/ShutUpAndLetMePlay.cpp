// ShutUpAndLetMePlay: native cutscene and interaction-dialogue Skip.
// The optional dialogue pair fails independently; existing cutscene support stays active.
#include "Win32Minimal.h"
#include "Behavior.h"
#include "Sequencer.h"
#include "Diagnostics.h"
using namespace crimson;
extern "C" { int _fltused = 0; }
extern "C" void* memcpy(void*d,const void*s,SIZE_T n){volatile U8*x=(volatile U8*)d;const volatile U8*y=(const volatile U8*)s;for(SIZE_T i=0;i<n;++i)x[i]=y[i];return d;}
extern "C" void* memset(void*d,int v,SIZE_T n){volatile U8*x=(volatile U8*)d;for(SIZE_T i=0;i<n;++i)x[i]=(U8)v;return d;}
static HANDLE const InvalidHandle=(HANDLE)(I64)-1;
static HMODULE module;static Behavior engine;static Dialogue dialogue;static Sequencer sequencer;
static U64 monotonicMilliseconds(){return GetTickCount64();}
static U32 ownerThread=0,offThreadCalls=0,observationLock=0,changed=0,stop=0;
static Observation published;
static Diagnostics diagnostics;
static bool protectionWarning=false;
static HANDLE instanceGuard=nullptr;
static const char*installFailure=nullptr;
static bool readable(const void*p,U32 n){
    if(!p||!n)return false;U64 a=(U64)p,end=a+n;if(end<a)return false;
    while(a<end){MemoryInfo m{};if(VirtualQuery((void*)a,&m,sizeof(m))!=sizeof(m)||m.state!=0x1000u||(m.protect&0x100u))return false;
        DWORD basic=m.protect&0xFFu;if(basic!=2&&basic!=4&&basic!=8&&basic!=0x20&&basic!=0x40&&basic!=0x80)return false;
        U64 next=(U64)m.base+m.regionSize;if(next<=a)return false;a=next;}
    return true;
}
static void publish(){
    U32 expected=0;if(!__atomic_compare_exchange_n(&observationLock,&expected,1,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED))return;
    published=engine.stats;__atomic_store_n(&observationLock,0,__ATOMIC_RELEASE);__atomic_store_n(&changed,1,__ATOMIC_RELEASE);
}
static bool onOwnerThread(){
    U32 id=GetCurrentThreadId(),expected=0;
    __atomic_compare_exchange_n(&ownerThread,&expected,id,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
    if(__atomic_load_n(&ownerThread,__ATOMIC_ACQUIRE)==id)return true;
    __atomic_fetch_add(&offThreadCalls,1,__ATOMIC_RELAXED);return false;
}
static void __fastcall InteractionEntry(void*self,U8 enabled){
    if(!onOwnerThread()){engine.originalInteraction(self,enabled);return;}
    sequencer.beforeInteraction(self,enabled);dialogue.beforeInteraction(self,enabled);
    engine.interaction(self,enabled);dialogue.afterInteraction(self,enabled);sequencer.afterInteraction(self,enabled);publish();
}
static void __fastcall AppearanceEntry(void*self,U8 enabled){
    if(!onOwnerThread()){engine.originalAppearance(self,enabled);return;}
    engine.appearance(self,enabled);dialogue.afterAppearance(self,enabled);sequencer.afterAppearance(self,enabled);publish();
}
static void __fastcall DialogueInputEntry(void*self,void*event,void*guide,U8 phase){
    if(sequencer.input(self,event,guide,phase))return;
    if(dialogue.input(self,event,guide,phase))return;
    dialogue.originalInput(self,event,guide,phase);
}
static U32* __fastcall DialogueUpdateEntry(void*self,U32*result,void*actor,void*player,float dt,U64 gameTime){
    return dialogue.update(self,result,actor,player,dt,gameTime);
}
static void absoluteJump(U8*where,void*target){
    const U8 j[]={0xFF,0x25,0,0,0,0};copy(where,j,6);copy(where+6,&target,8);
}
struct Hook {
    U8*target=nullptr,*allocation=nullptr;RuntimeFunction*table=nullptr;
    U8 original[15]{},replacement[15]{};DWORD oldProtect=0;
    bool protectedPage=false,registered=false;
};
static Hook cutsceneHooks[2],dialogueHooks[2];
static Hook* hooks=cutsceneHooks;
struct Thread {HANDLE handle=nullptr;bool suspended=false;};
static Thread threads[1024];static U32 threadCount=0;
static void releaseThreads(){
    for(U32 i=threadCount;i>0;--i){auto&t=threads[i-1];if(t.suspended){ResumeThread(t.handle);t.suspended=false;}}
    for(U32 i=0;i<threadCount;++i){CloseHandle(threads[i].handle);threads[i]=Thread{};}threadCount=0;
}
static bool enumerateThreads(){
    threadCount=0;HANDLE snapshot=CreateToolhelp32Snapshot(4,0);if(snapshot==InvalidHandle){installFailure="thread snapshot failed";return false;}
    ThreadEntry e{};e.size=sizeof(e);DWORD pid=GetCurrentProcessId(),own=GetCurrentThreadId();bool success=true;
    BOOL has=Thread32First(snapshot,&e);
    if(!has){CloseHandle(snapshot);installFailure="thread enumeration failed";return false;}
    do {
        if(e.owner==pid&&e.id!=own){
            if(threadCount==1024){success=false;installFailure="thread safety limit reached";break;}
            HANDLE h=OpenThread(0x4A,0,e.id); // suspend/resume, context, query
            if(!h){if(GetLastError()!=87){success=false;installFailure="cannot safely open a process thread";break;}}
            else threads[threadCount++]={h,false};
        }
        e.size=sizeof(e);has=Thread32Next(snapshot,&e);
    }while(has);
    if(success&&GetLastError()!=18){success=false;installFailure="incomplete thread snapshot";}
    CloseHandle(snapshot);if(!success)releaseThreads();return success;
}
static bool freezeThreads(bool&busy){
    busy=false;
    // All allocation, logging, module lookups and handle enumeration happen
    // BEFORE suspension. No game calls or locks are taken while threads pause.
    for(U32 i=0;i<threadCount;++i){auto&t=threads[i];
        if(SuspendThread(t.handle)==0xFFFFFFFFu){DWORD exitCode=259;
            if(GetExitCodeThread(t.handle,&exitCode)&&exitCode!=259)continue;
            installFailure="thread suspension failed";return false;}
        t.suspended=true;
        ThreadContext context{};context.flags=0x100001;
        if(!GetThreadContext(t.handle,&context)){installFailure="thread context read failed";return false;}
        for(U32 hi=0;hi<2;++hi)if(auto&h=hooks[hi];true)if(context.rip>=(U64)h.target&&context.rip<(U64)h.target+15){busy=true;return false;}
    }return true;
}
static void restoreProtections(){
    // Reverse order also handles both entries sharing a memory page.
    for(unsigned i=2;i>0;--i){auto&h=hooks[i-1];if(h.protectedPage){DWORD ignored=0;
        if(!VirtualProtect(h.target,15,h.oldProtect,&ignored))protectionWarning=true;h.protectedPage=false;}}
}
static void discardHooks(){
    restoreProtections();
    for(U32 hi=0;hi<2;++hi)if(auto&h=hooks[hi];true){if(h.registered){RtlDeleteFunctionTable(h.table);h.registered=false;}if(h.allocation)VirtualFree(h.allocation,0,0x8000);h=Hook{};}
}
static bool prepareHook(Hook&h,U32 r,void*entry,const U8*unwind,const U8*expected){
    h.target=engine.image.base+r;
    if(!readable(h.target,15)||!same(h.target,expected,15)){
        installFailure="native entry changed between validation and installation";return false;
    }
    copy(h.original,h.target,15);
    h.allocation=(U8*)VirtualAlloc(nullptr,4096,0x3000,4);if(!h.allocation){installFailure="trampoline allocation failed";return false;}
    copy(h.allocation,h.original,15);absoluteJump(h.allocation+15,h.target+15);
    copy(h.allocation+32,unwind,16);h.table=(RuntimeFunction*)(h.allocation+64);*h.table={0,29,32};
    // Proper x64 unwind info for the exact copied prologue; no stolen relative instructions.
    if(!RtlAddFunctionTable(h.table,1,(U64)h.allocation)){installFailure="trampoline unwind registration failed";return false;}h.registered=true;
    DWORD old=0;if(!VirtualProtect(h.allocation,4096,0x20,&old)||!FlushInstructionCache(GetCurrentProcess(),h.allocation,29)){
        installFailure="trampoline protection or instruction-cache flush failed";return false;}
    absoluteJump(h.replacement,entry);h.replacement[14]=0x90;return true;
}
static bool installHooks(bool npc=false){
    hooks=npc?dialogueHooks:cutsceneHooks;
    bool prepared=npc?
        (prepareHook(hooks[0],dialogue.resolved.input,(void*)&DialogueInputEntry,DialogueInputTrampolineUnwind,DialogueInputBytes)&&
         prepareHook(hooks[1],dialogue.resolved.update,(void*)&DialogueUpdateEntry,DialogueUpdateTrampolineUnwind,DialogueUpdateBytes)):
        (prepareHook(hooks[0],engine.resolved.interaction,(void*)&InteractionEntry,engine.resolved.unwind,InteractionBytes)&&
         prepareHook(hooks[1],engine.resolved.appearance,(void*)&AppearanceEntry,engine.resolved.unwind,CinemaAppearanceBytes));
    if(!prepared){discardHooks();return false;}
    // Publish originals before either entry is modified, for both hook pairs.
    if(npc){dialogue.originalInput=(DialogueInput)hooks[0].allocation;dialogue.originalUpdate=(DialogueUpdate)hooks[1].allocation;}
    else{engine.originalInteraction=(Toggle)hooks[0].allocation;engine.originalAppearance=(Toggle)hooks[1].allocation;}
    bool installed=false;
    for(unsigned attempt=0;attempt<40&&!installed;++attempt){
        installFailure=nullptr;
        if(!enumerateThreads())break;
        bool pages=true;for(U32 hi=0;hi<2;++hi)if(auto&h=hooks[hi];true){if(!VirtualProtect(h.target,15,0x40,&h.oldProtect)){pages=false;installFailure="cannot protect native entry";break;}h.protectedPage=true;}
        if(!pages){releaseThreads();restoreProtections();break;}
        bool busy=false;
        if(!freezeThreads(busy)){releaseThreads();restoreProtections();if(busy){Sleep(25);continue;}break;}
        bool clean=true;for(U32 hi=0;hi<2;++hi)if(auto&h=hooks[hi];true)clean=clean&&same(h.target,h.original,15);
        if(clean){
            for(U32 hi=0;hi<2;++hi)if(auto&h=hooks[hi];true)copy(h.target,h.replacement,15);
            bool flush=true;for(U32 hi=0;hi<2;++hi)if(auto&h=hooks[hi];true)flush=FlushInstructionCache(GetCurrentProcess(),h.target,15)&&flush;
            if(flush){installed=true;}
            else{for(U32 hi=0;hi<2;++hi)if(auto&h=hooks[hi];true)copy(h.target,h.original,15);for(U32 hi=0;hi<2;++hi)if(auto&h=hooks[hi];true)FlushInstructionCache(GetCurrentProcess(),h.target,15);installFailure="entry cache flush failed; original entry bytes restored";}
        }else installFailure="another modification changed a native entry before installation";
        // Instruction pointers inside either replaced prologue cause retry,
        // rather than relocating a live thread context or risking half a jump.
        releaseThreads();restoreProtections();if(!clean||!installed)break;
    }
    if(!installed){if(!installFailure)installFailure="native prologue remained busy; no hooks installed";discardHooks();}
    return installed;
}
// ASI loaders can also be reached by launcher/helper processes. Do not let
// those processes install hooks or overwrite the game's diagnostic files.
static bool isTargetProcess() {
    static wchar_t executable[2048]{};
    DWORD length = GetModuleFileNameW(nullptr, executable, 2048);
    if (!length || length >= 2048) return false;
    U32 begin = length;
    while (begin && executable[begin - 1] != L'\\' && executable[begin - 1] != L'/') --begin;
    const wchar_t target[] = L"crimsondesert.exe";
    U32 i = 0;
    for (; target[i]; ++i) {
        wchar_t c = executable[begin + i];
        if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
        if (c != target[i]) return false;
    }
    return executable[begin + i] == 0;
}

// A named kernel object prevents two copies from installing in the same process.
// The object is not locked/waited on. Its handle lives until process exit.
static bool claimInstance(bool& duplicate) {
    wchar_t name[96] = L"Local\\ShutUpAndLetMePlay.Instance.";
    U32 n = 0;
    while (name[n]) ++n;
    U32 value = GetCurrentProcessId();
    wchar_t digits[10];
    U32 count = 0;
    do { digits[count++] = wchar_t(L'0' + value % 10); value /= 10; } while (value);
    while (count) name[n++] = digits[--count];
    name[n] = 0;
    instanceGuard = CreateMutexW(nullptr, 0, name);
    if (!instanceGuard) return false;
    if (GetLastError() == 183) {
        CloseHandle(instanceGuard);
        instanceGuard = nullptr;
        duplicate = true;
        return false;
    }
    return true;
}

static void writeSnapshot(const char* status, const char* reason) {
    DiagnosticSnapshot snapshot;
    U32 expected = 0;
    if (__atomic_compare_exchange_n(&observationLock, &expected, 1, false,
                                    __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        snapshot.observation = published;
        __atomic_store_n(&observationLock, 0, __ATOMIC_RELEASE);
        snapshot.available = true;
    } else {
        diagnostics.retryPending = true;
        return;
    }
    snapshot.uiThread = __atomic_load_n(&ownerThread, __ATOMIC_ACQUIRE);
    snapshot.offThreadCalls = __atomic_load_n(&offThreadCalls, __ATOMIC_RELAXED);
    snapshot.dialogue=dialogue.snapshot();
    snapshot.dialogueEnabled=dialogue.active();
    snapshot.sequencer=sequencer.snapshot();snapshot.sequencerEnabled=sequencer.active();
    diagnostics.write(status, reason, engine.image, engine.resolved, snapshot,dialogue.resolved,sequencer.resolved);
}

static DWORD __stdcall Worker(void*) {
    if (!isTargetProcess()) return 0;
    diagnostics.initialize(module);
    bool duplicate = false;
    if (!claimInstance(duplicate)) {
        if (!duplicate) writeSnapshot("disabled", "single-instance guard could not be created; no hooks installed");
        return 0;
    }
    Sleep(1200);
    U8* b = (U8*)GetModuleHandleW(nullptr);
    const char* reason = nullptr;
    if (!loadImage(b, engine.image, readable))
        reason = engine.image.failure;
    else if (!resolve(engine.image, engine.resolved))
        reason = engine.resolved.failure;
    if (reason) { writeSnapshot("disabled", reason); return 0; }

    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(1 | 4, (const wchar_t*)(void*)&Worker, &pinned)) {
        writeSnapshot("disabled", "cannot pin ASI module; no hooks installed");
        return 0;
    }
    engine.readable = readable;
    engine.show = (WidgetCall)(b + engine.resolved.symbols[Sym_Show]);
    engine.hide = (WidgetCall)(b + engine.resolved.symbols[Sym_Hide]);
    engine.setAppearance = (Toggle)(b + engine.resolved.setAppearance);
    dialogue.core=&engine;dialogue.clock=monotonicMilliseconds;sequencer.dialogue=&dialogue;
    // Scan before changing any native entry; validation never scans our own detours.
    const bool npcResolved=resolveDialogue(engine.image,engine.resolved,dialogue.resolved);
    resolveSequencer(engine.image,dialogue.resolved,sequencer.resolved);
    if (!installHooks()) { writeSnapshot("disabled", installFailure); return 0; }
    if(npcResolved){
        dialogue.advance=(DialogueAdvance)(b+dialogue.resolved.advance);
        dialogue.current=(DialogueCurrent)(b+dialogue.resolved.current);
        dialogue.stopVoice=(DialogueStop)(b+dialogue.resolved.symbols[DialogueSymStopVoice]);
        if(installHooks(true))__atomic_store_n(&dialogue.enabled,1,__ATOMIC_RELEASE);
        else dialogue.resolved.failure=installFailure;
    }

    const char* status = protectionWarning ? "active_warning" : "active";
    reason = protectionWarning ? "hooks active, but restoration of a native page protection failed"
                               : "cutscene hooks active; NPC dialogue status reported separately";
    writeSnapshot(status, reason);
    while (!__atomic_load_n(&stop, __ATOMIC_RELAXED)) {
        Sleep(5000);
        if (__atomic_exchange_n(&changed, 0, __ATOMIC_ACQ_REL) ||
            __atomic_exchange_n(&dialogue.dirty,0,__ATOMIC_ACQ_REL) ||
            __atomic_exchange_n(&sequencer.dirty,0,__ATOMIC_ACQ_REL) || diagnostics.retryPending)
            writeSnapshot(status, reason);
    }
    return 0;
}
extern "C" BOOL __stdcall DllMain(HMODULE self,DWORD why,void*){
    if(why==1){module=self;DisableThreadLibraryCalls(self);HANDLE thread=CreateThread(nullptr,0,Worker,nullptr,0,nullptr);if(thread)CloseHandle(thread);}
    else if(why==0)__atomic_store_n(&stop,1,__ATOMIC_RELAXED);
    return 1;
}
