// Executes the compiled ASI through the same mocked Win32 exports as the
// original regression harness, against a mapped image of 1.0.0.3479.
// This does NOT launch the game; native UI helpers are isolated/mocked.
#define DIALOGUE_TEST_LIBRARY
#include "compiled_asi_harness.cpp"
#include "../source/Sequencer.h"
#include <execinfo.h>
#include <signal.h>
#include <ucontext.h>
static void segvHandler(int signal,siginfo_t*info,void*ctx){
 auto*uc=(ucontext_t*)ctx;
 U64 rip=(U64)uc->uc_mcontext.gregs[REG_RIP];
 fprintf(stderr,"SIGNAL %d at %p rip=%p relative_game=%llx relative_asi=%llx\n",signal,info->si_addr,(void*)rip,(unsigned long long)(rip-(U64)base),(unsigned long long)(rip-(U64)asiBase));
 void*bt[48];int count=backtrace(bt,48);backtrace_symbols_fd(bt,count,2);_exit(128+signal);
}

static U32 actualAppearance=0;
static U32 actualSequencerState=0;
static Memory* runtimeGame=nullptr;
// Do not construct the current keyguide using the legacy fixture's fields.
// Execute the supplied executable's native binding setter AND cache refresh.
// Only the external binding registry, refcount release and rendering are mocks.
static std::array<U8,0x100> nativeGuideData{};
static std::array<U8,0x40> nativeGuideBackend{},nativeGuideEntries{},nativeBindingRegistry{};
static StyleVector emptyBindings{};
static void* expectedAction=nullptr;
static U32 expectedActionHash=0,nativeBindingLookups=0;
static void* NATIVE_ABI nativeData(void*guide){
    CHECK(guide);return nativeGuideData.data();
}
static void* NATIVE_ABI internAction(void*out,const char*,U32 one,U32 flags){
    CHECK(one==1&&flags==0x2ffff);put32((U8*)out,expectedActionHash);return out;
}
static void* NATIVE_ABI acquireAction(void*registry,void**out,U32*id,U8){
    CHECK(registry==nativeBindingRegistry.data()&&*id==expectedActionHash);
    *out=expectedAction;return out;
}
static void* NATIVE_ABI bindingLookup(void*registry,U32 hash){
    CHECK(registry==nativeBindingRegistry.data()&&hash==expectedActionHash);
    ++nativeBindingLookups;return &emptyBindings;
}
static void prepareNativeKeyguides(){
    set(nativeGuideData.data(),0x78,nativeGuideBackend.data());
    set(nativeGuideBackend.data(),0x20,nativeGuideEntries.data());
    put32(nativeGuideBackend.data()+0x28,1);
    set(nativeGuideEntries.data(),0x10,nativeBindingRegistry.data());
    set(base+0x564B020,0x118,(void*)nativeData);
    runtimeGame->executable(0xE434C0,0x1D2); // actual binding setter
    runtimeGame->executable(0xE43100,0x1B0); // actual binding/cache refresh
    stub(*runtimeGame,0x149DE20,(void*)internAction);
    stub(*runtimeGame,0x30C5540,(void*)acquireAction);
    stub(*runtimeGame,0x30C5C10,(void*)bindingLookup);
    for(U32 r:{0x3D76C0u,0xE43F80u,0x30B31F0u,0xE44E80u,
               0xE432B0u,0xE433B0u,0xE42F00u,0x717660u,0xE41310u,0xE42500u,0xE45D30u,0xE45DE0u,0xE4A770u})
        stub(*runtimeGame,r,(void*)noOperation);
    // Execute real style-vector add/remove logic. Preallocated capacity avoids
    // the allocator. Only sort/invalidation callbacks are inert lower layers.
    runtimeGame->executable(behavior.resolved.setAppearance,NewSetAppearanceSignature.size);
    runtimeGame->executable(0x3896A70,0xA0);
    stub(*runtimeGame,0x38933C0,(void*)noOperation);
    stub(*runtimeGame,0x38974A0,(void*)noOperation);
    runtimeGame->executable(behavior.resolved.symbols[Sym_ResetKeyguide],0x1D0);
}
static void bindUpdatedGuides(Fixture&f,U8 family=0,U8 subtype=0){
    for(unsigned i=0;i<6;++i){
        auto&g=f.guides[i];
        // Stale old offset deliberately has no valid action; only native code
        // assigns the new slot. New cache bytes are inputs to the native setter.
        set(g.object.data(),0x1F8,nullptr);
        set(g.object.data(),0x1E8,nullptr);
        g.object[0x1F0]=family;g.object[0x1F1]=subtype;
        expectedAction=g.action.data();expectedActionHash=hashes[i%3];
        U32 lookups=nativeBindingLookups;
        using Bind=void(NATIVE_ABI*)(void*,const char*,U8);
        ((Bind)(base+0xE434C0))(g.object.data(),"fixture-action",1);
        CHECK(nativeBindingLookups==lookups+2);
        CHECK(Behavior{}.ptr(g.object.data(),0x1E8)==g.action.data());
        CHECK(Behavior{}.ptr(g.object.data(),0x1F8)==nullptr);
        CHECK(g.object[0x1F0]==(family==2?1:family)&&g.object[0x1F1]==subtype);
    }
}
static void testCurrentHooks(){
    CHECK(virtualAllocations.size()==4&&registeredTables.size()==4);
    bool foundBranch=false;
    for(auto&[key,_]:registeredTables){
        U8*t=(U8*)key;
        if(u32(t+4)==41){CHECK(!foundBranch);foundBranch=true;CHECK(u32(t+8)==64);}
    }
    CHECK(foundBranch);
    // The compiled detour, not a second reimplementation, calls the real
    // native appearance function through its relocated branch trampoline.
    prepareNativeKeyguides();
    Fixture f;bindUpdatedGuides(f,1,2);
    f.targetVT[20]=(void*)noOperation;
    std::array<U8,0x1400>state{};
    state[0x10B6]=1;
    regions.push_back({state.data(),state.size()});
    set(f.manager.data(),0x90,state.data());
    // Use the native interaction function to create the row, not rowShown().
    // Its mode-1 path hides then shows Pause again. Hidden-Pause eligibility is
    // covered separately; it is not a substitute for testing native activation.
    Toggle interaction=(Toggle)(base+behavior.resolved.interaction);
    interaction(f.view.data(),1);
    CHECK(f.event[0xF2]==1&&activations==1);
    CHECK(f.guides[4].wrapper[0xBE]==0); // unlocked before native event activation
    CHECK(!f.guides[3].wrapper[0xBE]&&!f.guides[5].wrapper[0xBE]);
    set(base,behavior.resolved.symbols[Sym_GameManager],f.manager.data());
    // The updated native appearance must return through both halves of
    // its stolen JE branch, preserving the original Skip/FF/Pause distinctions.
    Toggle appearance=(Toggle)(base+actualAppearance);
    auto holdAndBinding=f.guides[4].object;
    appearance(f.view.data(),1);
    {
        Behavior oldProbe=behavior;oldProbe.readable=validRead;
        oldProbe.resolved.guideActionOffset=0x1F8;
        oldProbe.resolved.guideFamilyOffset=0x200;
        oldProbe.resolved.guideSubtypeOffset=0x201;
        void*w=nullptr,*g=nullptr;
        CHECK(oldProbe.viewFailure(f.view.data(),w,g,true)==3);
    }
    CHECK(f.appears(4)&&!f.disappears(4));
    CHECK(std::memcmp(holdAndBinding.data()+0xC0,f.guides[4].object.data()+0xC0,0x178)==0);
    CHECK(!f.guides[4].wrapper[0xBE]);
    CHECK(f.guides[3].wrapper[0xBE]==0);  // Fast Forward stays native/visible
    CHECK(f.guides[5].wrapper[0xBE]==0);  // Preserve the native Pause state
    CHECK(f.appears(3)&&f.appears(5));
    // The game can hide/rebuild the cinematic row repeatedly within one scene.
    // Recreate the row using native interaction on each cycle. The mod must
    // unlock Skip before native event activation and leave other controls alone.
    for(int cycle=0;cycle<50;++cycle){
        appearance(f.view.data(),0);
        CHECK(f.guides[4].wrapper[0xBE]==1);
        interaction(f.view.data(),0);
        CHECK(f.event[0xF2]==0&&f.guides[4].wrapper[0xBE]==1);
        interaction(f.view.data(),1);
        CHECK(f.event[0xF2]==1);
        CHECK(!f.guides[3].wrapper[0xBE]&&!f.guides[5].wrapper[0xBE]);
        holdAndBinding=f.guides[4].object;
        appearance(f.view.data(),1);
        CHECK(std::memcmp(holdAndBinding.data()+0xC0,f.guides[4].object.data()+0xC0,0x178)==0);
        CHECK(f.appears(4)&&!f.disappears(4));
        CHECK(f.guides[4].wrapper[0xBE]==0);
        CHECK(f.guides[5].wrapper[0xBE]==0);
    }
    appearance(f.view.data(),0);
    CHECK(f.guides[3].wrapper[0xBE]==0); // fixed row is native; no fake overlay
    interaction(f.view.data(),0);
    CHECK(activations==51&&deactivations==51&&f.event[0xF2]==0);
    // An actually hidden row must remain blocked; relaxing Pause's visibility
    // must NOT permit a Skip overlay when Fast Forward is unavailable.
    {
        Fixture hidden(1);bindUpdatedGuides(hidden);
        std::array<U8,0x1400>hiddenState{};
        hiddenState[0x10B6]=1;
        regions.push_back({hiddenState.data(),hiddenState.size()});
        set(hidden.manager.data(),0x90,hiddenState.data());
        set(base,behavior.resolved.symbols[Sym_GameManager],hidden.manager.data());
        CHECK(hidden.guides[3].wrapper[0xBE]==1);
        CHECK(hidden.guides[5].wrapper[0xBE]==1);
        appearance(hidden.view.data(),1);
        CHECK(hidden.guides[4].wrapper[0xBE]==1);
        CHECK(!hidden.appears(4)&&hidden.disappears(4));
    }
    // The game routes modes 0 and 2 through the distinct fixed-row Skip
    // control. Each route must repair the native style without changing the
    // game's mode/paused bytes or performing automatic dialogue advancement.
    for(U8 mode:{U8(0),U8(2)}){
        Fixture other(mode);bindUpdatedGuides(other);
        std::array<U8,0x1400>dialogueState{};
        dialogueState[0x10B6]=mode;
        std::array<U8,0x20>facade{};
        set(facade.data(),0,dialogueState.data());
        regions.push_back({dialogueState.data(),dialogueState.size()});
        regions.push_back({facade.data(),facade.size()});
        set(other.manager.data(),0x90,dialogueState.data());
        set(base,behavior.resolved.symbols[Sym_GameManager],other.manager.data());
        set(base,actualSequencerState,facade.data());
        other.rowShown();
        appearance(other.view.data(),1);
        CHECK(other.appears(1)&&!other.disappears(1));
        CHECK(dialogueState[0x10B6]==mode&&dialogueState[0x10B5]==0);
    }
}
static void testRejectedCurrentGuide(){
    prepareNativeKeyguides();
    Fixture f;bindUpdatedGuides(f);
    std::array<U8,0x1400>state{};state[0x10B6]=1;
    regions.push_back({state.data(),state.size()});
    set(f.manager.data(),0x90,state.data());
    set(base,behavior.resolved.symbols[Sym_GameManager],f.manager.data());
    f.guides[3].wrapper[0xBE]=0;
    // A tempting valid Skip action in the OLD slot must never be accepted.
    set(f.guides[4].object.data(),0x1F8,f.guides[1].action.data());
    std::array<U8,16>lockedBackend{};
    if(scenario=="new_bad_action")put32(f.guides[4].action.data()+0x2C,0x12345678);
    if(scenario=="new_null_action")set(f.guides[4].object.data(),0x1E8,nullptr);
    if(scenario=="new_unreadable_action")set(f.guides[4].object.data(),0x1E8,(void*)1);
    if(scenario=="new_neighbor_action")put32(f.guides[3].action.data()+0x2C,0x12345678);
    if(scenario=="new_bad_guide_type")set(f.guides[3].object.data(),0,base+0x564B028);
    if(scenario=="new_style_guard"){
        set(lockedBackend.data(),8,(void*)1);
        set(f.guides[4].backing.data(),0xA0,lockedBackend.data());
        regions.push_back({lockedBackend.data(),lockedBackend.size()});
    }
    if(scenario=="new_pause_hidden")f.guides[5].wrapper[0xBE]=1;
    ((Toggle)(base+actualAppearance))(f.view.data(),1);
    if(scenario=="new_pause_hidden"){
        CHECK(f.guides[4].wrapper[0xBE]==0&&f.appears(4)&&!f.disappears(4));
        CHECK(f.guides[5].wrapper[0xBE]==1);
        return;
    }
    CHECK(f.guides[4].wrapper[0xBE]==1);
    CHECK(!f.appears(4)&&f.disappears(4));
}
int main(int argc,char**argv){try{
    struct sigaction sa{};sa.sa_sigaction=segvHandler;sa.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&sa,nullptr);
    CHECK(argc>=3&&argc<=5);
    scenario=argc>=4?argv[3]:"new_success";
    bool guarded=scenario=="new_bad_action"||scenario=="new_null_action"||
        scenario=="new_unreadable_action"||scenario=="new_neighbor_action"||
        scenario=="new_bad_guide_type"||scenario=="new_style_guard";
    bool expectSuccess=scenario=="new_success"||scenario=="new_pause_hidden"||guarded;
    CHECK(expectSuccess||scenario=="new_shape"||scenario=="new_ambiguous"||
          scenario=="allocation"||scenario=="unwind"||scenario=="new_input_changed"||
          scenario=="new_binding_shape"||scenario=="new_binding_link"||scenario=="new_binding_ambiguous"||
          scenario=="new_cache_shape"||scenario=="new_binding_unwind");
    auto game=loadPE(argv[1],false);
    base=game->b;
    CHECK(parseImage(base,behavior.image));
    CHECK(validateImageMemory(behavior.image,[](const void*,U32){return true;}));
    CHECK(resolve3479(behavior.image,behavior.resolved));
    CHECK(behavior.resolved.profile==1);
    DialogueResolved dr;
    SequencerResolved sr;
    CHECK(resolveDialogue3479(behavior.image,behavior.resolved,dr));
    CHECK(resolveSequencer3479(behavior.image,dr,sr));
    CHECK(behavior.resolved.interaction==0x12145E0u);
    CHECK(behavior.resolved.appearance==0x1214370u);
    actualAppearance=behavior.resolved.appearance;
    actualSequencerState=sr.symbols[SequencerSymStateFacade];
    put32(base+behavior.resolved.symbols[Sym_AppearStyle],0x4321);
    put32(base+behavior.resolved.symbols[Sym_DisappearStyle],0x4320);
    U8 originalAppear[17];std::memcpy(originalAppear,base+actualAppearance,17);
    U32 show=behavior.resolved.symbols[Sym_Show],hide=behavior.resolved.symbols[Sym_Hide];
    typedShowStub(*game,show,(void*)mockShow);
    typedShowStub(*game,hide,(void*)mockHide);
    auto asi=loadPE(argv[2],true);
    asiBase=asi->b;
    linkImports(asi->b);
    U32 op=u32(asi->b+0x3C)+24;
    asi->executable(u32(asi->b+op+20),u32(asi->b+op+4));
    dllMain=(int(NATIVE_ABI*)(void*,W32,void*))(asi->b+u32(asi->b+op+16));
    runtimeGame=game.get();
    workerSimulation=(guarded||scenario=="new_pause_hidden")?testRejectedCurrentGuide:(expectSuccess?testCurrentHooks:nullptr);
    // Corrupt a validated native function or create an ambiguous code copy.
    // No hooks may install in either case, even when older signatures exist.
    if(scenario=="new_shape")base[actualAppearance+30]^=1;
    if(scenario=="new_input_changed")base[dr.input+0x400]^=1;
    if(scenario=="new_binding_shape")base[behavior.resolved.guideBinding+0x97]^=0x10;
    if(scenario=="new_binding_link")base[behavior.resolved.guideBinding+0x196]^=0x10;
    if(scenario=="new_cache_shape")base[behavior.resolved.guideRefresh+0xA9]^=0x10;
    if(scenario=="new_binding_ambiguous"){
        U32 dup=0x13000000u;CHECK(behavior.image.code(dup,NewGuideBindingSignature.size));
        std::memcpy(base+dup,base+behavior.resolved.guideBinding,NewGuideBindingSignature.size);
    }
    if(scenario=="new_binding_unwind"){
        bool found=false;
        for(U32 i=0;i<behavior.image.exceptionSize;i+=12){
            U8*entry=base+behavior.image.exceptionRva+i;
            if(u32(entry)==behavior.resolved.guideBinding){base[u32(entry+8)+4]^=1;found=true;break;}
        }
        CHECK(found);
    }
    if(scenario=="new_ambiguous"){
        const U32 duplicate=0x13000000u;
        CHECK(behavior.image.code(duplicate,NewAppearanceSignature.size));
        std::memcpy(base+duplicate,base+actualAppearance,NewAppearanceSignature.size);
    }
    CHECK(dllMain(asi->b,1,nullptr)==1&&worker);
    CHECK(worker(nullptr)==0);
    CHECK(suspendedCount==0);
    CHECK(files.count("C:\\TestGame\\ShutUpAndLetMePlay_UpdateReport.json"));
    auto report=files.at("C:\\TestGame\\ShutUpAndLetMePlay_UpdateReport.json");
    if(argc==5){
        const auto out=std::filesystem::path(argv[4])/scenario;
        std::filesystem::create_directories(out);
        std::ofstream(out/"report.json",std::ios::binary)<<report;
        std::ofstream(out/"report.log",std::ios::binary)<<files.at("C:\\TestGame\\ShutUpAndLetMePlay.log");
    }
    if(!expectSuccess){
        CHECK(!gameRan&&registeredTables.empty()&&virtualAllocations.empty());
        CHECK(report.find("\"status\": \"disabled\"")!=std::string::npos);
        if(scenario!="new_shape"&&scenario!="new_ambiguous"&&scenario!="new_input_changed")
            CHECK(std::memcmp(originalAppear,base+actualAppearance,17)==0);
        std::cout<<"PASS compiled ASI 3479 / "<<scenario<<": fail-closed, no active hooks; assertions="<<checks<<"\n";
        return 0;
    }
    CHECK(gameRan&&unwindCalled==4&&registeredTables.size()==4&&virtualAllocations.size()==4);
    CHECK(std::memcmp(originalAppear,base+actualAppearance,17)!=0);
    CHECK(report.find("\"status\": \"active\"")!=std::string::npos);
    CHECK(report.find("\"npc_dialogue\": {")!=std::string::npos);
    CHECK(report.find("\"sequencer_dialogue\": {")!=std::string::npos);
    CHECK(report.find("\"version\": \"2.0.2\"")!=std::string::npos);
    CHECK(report.find("\"build_id\": \"" SULMP_BUILD_ID "\"")!=std::string::npos);
    CHECK(report.find("\"keyguide_action_offset\": 488")!=std::string::npos);
    CHECK(report.find("\"keyguide_family_offset\": 496")!=std::string::npos);
    CHECK(report.find("\"keyguide_subtype_offset\": 497")!=std::string::npos);
    if(scenario=="new_pause_hidden"){
        CHECK(report.find("\"appearance_repairs\": 1")!=std::string::npos);
        CHECK(report.find("\"safety_gate_blocks\": 0")!=std::string::npos);
        std::cout<<"PASS compiled ASI 3479 / hidden Pause: Skip restored without changing Pause; assertions="<<checks<<"\n";
        return 0;
    }
    if(guarded){
        CHECK(report.find("\"appearance_repairs\": 0")!=std::string::npos);
        const bool style=scenario=="new_style_guard";
        CHECK(report.find(style?"\"appearance_style_blocks\": 1":"\"appearance_view_blocks\": 1")!=std::string::npos);
        U32 gate=style?0:(scenario=="new_neighbor_action"?4:(scenario=="new_bad_guide_type"?5:3));
        CHECK(report.find("\"last_cutscene_gate_code\": "+std::to_string(gate))!=std::string::npos);
        if(gate==3||gate==4){
            U32 reason=(scenario=="new_bad_action"||scenario=="new_neighbor_action")?4:3;
            CHECK(report.find("\"last_cutscene_guide_failure\": "+std::to_string(reason))!=std::string::npos);
        }
        std::cout<<"PASS compiled ASI 3479 / "<<scenario<<": incorrect guide rejected without old-slot fallback; assertions="<<checks<<"\n";
        return 0;
    }
    CHECK(report.find("\"last_input_family\": 1")!=std::string::npos);
    CHECK(report.find("\"last_input_subtype\": 2")!=std::string::npos);
    CHECK(report.find("\"appearance_repairs\": 51")!=std::string::npos);
    CHECK(report.find("\"appearance_view_blocks\": 1")!=std::string::npos);
    CHECK(report.find("\"appearance_style_blocks\": 0")!=std::string::npos);
    CHECK(report.find("\"last_cutscene_gate_code\": 7")!=std::string::npos);
    std::cout<<"PASS compiled ASI 3479: 4 hooks and relocated trampoline, all routes enabled; assertions="<<checks<<"\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}}
