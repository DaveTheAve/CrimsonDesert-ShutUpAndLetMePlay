// Actual compiled PE/ASI integration with reviewed native dialogue routines.
// Windows services, voice stop, final actor completion and choice rendering are
// explicit mocks. This never invokes the game's executable entry point.
#include <signal.h>
#include <ucontext.h>
#define DIALOGUE_FIXTURE_LIBRARY
#include "dialogue_harness.cpp"
static Memory* fixtureGame=nullptr;
static U8 nativeInputPrefix[15]{},nativeUpdatePrefix[15]{};
static bool npcDisabledCase(){return scenario=="npc_late_change"||scenario=="npc_shape"||scenario=="npc_allocation"||scenario=="npc_unwind"||scenario=="npc_protect"||scenario=="npc_flush";}
static void runCompiledDialogue(){
    CHECK(virtualAllocations.size()==(npcDisabledCase()?2u:4u));
    if(npcDisabledCase()){
        if(scenario=="npc_late_change"){
            CHECK(base[npcResolved.input]==0x90&&same(base+npcResolved.input+1,nativeInputPrefix+1,14));
        }else CHECK(same(base+npcResolved.input,nativeInputPrefix,15));
        CHECK(same(base+npcResolved.update,nativeUpdatePrefix,15));
        Fixture f;Toggle i=(Toggle)(base+cinematicResolved.interaction),a=(Toggle)(base+cinematicResolved.appearance);
        i(f.view.data(),1);a(f.view.data(),1);CHECK(f.appears(4)&&!f.disappears(4));return;
    }
    stub(*fixtureGame,npcResolved.finish,(void*)finishFixture);
    stub(*fixtureGame,npcResolved.symbols[DialogueSymStopVoice],(void*)stopFixtureVoice);
    stub(*fixtureGame,0x388E00,(void*)lookupFixture);stub(*fixtureGame,0x433650,(void*)lookupFixture);stub(*fixtureGame,referenceUi(0xC007B0),(void*)dispatchFixture);
    if(scenario=="npc_choice"){
        fixtureGame->executable(0x5B592D,14);jump(base+0x5B592D,base+0x5B67CE);
    }
    DialogueFixture f(scenario=="npc_budget"?100:3,3);
    if(scenario=="npc_choice")put32(f.entries.data()+0x48,1);
    if(scenario=="npc_events")for(U32 i=0;i<3;++i)f.entries[i*0x70+0x38]=1;
    const auto u=(DialogueUpdate)(base+npcResolved.update);
    const auto input=(DialogueInput)(base+npcResolved.input);
    const auto interaction=(Toggle)(base+cinematicResolved.interaction),appearance=(Toggle)(base+cinematicResolved.appearance);
    auto update=[&](){return u(f.ctx.data(),&f.result,f.actor.data(),f.player.data(),0.016f,100000);};
    CHECK(update()==&f.result&&f.result==0);
    interaction(f.ui.view.data(),1);appearance(f.ui.view.data(),1);
    CHECK(f.ui.appears(1)&&!f.ui.disappears(1)&&f.ui.guides[1].wrapper[0xBE]==0&&f.ui.event[0xF2]==1);
    // Hide/reappear maintains the restored fixed dialogue row without changing
    // the native button's hold/cache data or activating another UI family's row.
    auto button=f.ui.guides[1].object;
    for(unsigned n=0;n<10;++n){appearance(f.ui.view.data(),0);CHECK(f.ui.guides[1].wrapper[0xBE]);appearance(f.ui.view.data(),1);CHECK(!f.ui.guides[1].wrapper[0xBE]&&f.ui.appears(1));}
    if(scenario=="npc_stale")simulatedTick+=600;
    input(f.ui.view.data(),f.ui.event.data(),f.ui.guides[1].object.data(),1);
    input(f.ui.view.data(),f.ui.event.data(),f.ui.guides[1].object.data(),0);
    if(scenario=="npc_changed")f.ctx[0x5C]=1;
    if(scenario=="npc_waiting")f.ctx[0x5D]=1;
    CHECK(update()==&f.result);
    if(scenario=="npc_stale"||scenario=="npc_changed"||scenario=="npc_waiting"){
        CHECK(finishes==0&&voicesStopped==0&&f.result==0);return;
    }
    if(scenario=="npc_choice"){
        CHECK(finishes==0&&voicesStopped==1&&f.result==0&&f.ctx[0x5D]==1&&u32(f.ctx.data()+0x58)==0);
        input(f.ui.view.data(),f.ui.event.data(),f.ui.guides[1].object.data(),0);update();CHECK(finishes==0);return;
    }
    unsigned frames=1;
    while(!finishes){CHECK(++frames<=8);update();}
    CHECK(finishes==1&&voicesStopped==1&&f.result==99&&f.state[0x107E]==4);
    CHECK(f.state[0x107C]==0);
    CHECK(std::memcmp(button.data()+0xD0,f.ui.guides[1].object.data()+0xD0,0x178)==0);
    CHECK(u32(f.ctx.data()+0x70)==0);
    if(scenario=="npc_events")CHECK(dispatchedEntries==std::vector<U32>({0,1,2}));
    if(scenario=="npc_budget")CHECK(frames==7);
    // Outside mode 3 the original input trampoline executes normally. Use an
    // inactive row with a matched null event, which the native handler rejects.
    f.ui.eventPointer=nullptr;f.ui.view[0x35C]=0;
    input(f.ui.view.data(),nullptr,nullptr,1);
    f.ui.eventPointer=f.ui.event.data();f.ui.view[0x35C]=2;
    // Original normal cutscene support still repairs the normal Skip row.
    f.state[0x107E]=1;skipWrapper=f.ui.guides[4].wrapper.data();
    interaction(f.ui.view.data(),1);appearance(f.ui.view.data(),1);
    CHECK(f.ui.appears(4)&&!f.ui.disappears(4));
}
static void faultHandler(int, siginfo_t* si, void* v){auto*u=(ucontext_t*)v;
    auto rip=u->uc_mcontext.gregs[REG_RIP];
    fprintf(stderr,"FAULT addr=%p RIP=%llx gameRVA=%llx asiRVA=%llx RCX=%llx RDX=%llx R8=%llx R9=%llx RAX=%llx\n",si->si_addr,(long long)rip,(long long)(rip-(U64)base),(long long)(rip-(U64)asiBase),
        (long long)u->uc_mcontext.gregs[REG_RCX],(long long)u->uc_mcontext.gregs[REG_RDX],(long long)u->uc_mcontext.gregs[REG_R8],(long long)u->uc_mcontext.gregs[REG_R9],(long long)u->uc_mcontext.gregs[REG_RAX]);_Exit(100);
}
int main(int argc,char**argv){try{
    struct sigaction sa{};sa.sa_sigaction=faultHandler;sa.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&sa,nullptr);sigaction(SIGILL,&sa,nullptr);

    CHECK(argc==4||argc==5);scenario=argv[3];auto game=loadPE(argv[1],false);base=game->b;fixtureGame=game.get();
    CHECK(parseImage(base,gameImage)&&resolve(gameImage,cinematicResolved)&&resolveDialogue(gameImage,cinematicResolved,npcResolved));
    behavior.image=gameImage;behavior.resolved=cinematicResolved;
    copy(nativeInputPrefix,base+npcResolved.input,15);copy(nativeUpdatePrefix,base+npcResolved.update,15);
    for(auto r:{std::pair<U32,U32>{cinematicResolved.interaction,484},{cinematicResolved.appearance,2178},{cinematicResolved.setAppearance,267},
        {cinematicResolved.symbols[Sym_ResetKeyguide],0x149},{cinematicResolved.symbols[Sym_AddStyle],0x99},
        {cinematicResolved.symbols[Sym_RemoveStyle],0x70},{npcResolved.advance,DialogueAdvancePattern.shape.size},
        {npcResolved.current,DialogueCurrentPattern.shape.size},{npcResolved.update,DialogueUpdatePattern.shape.size},
        {npcResolved.input,DialogueInputPattern.shape.size},{npcResolved.database,DialogueDatabasePattern.shape.size}})game->executable(r.first,r.second);
    put32(base+cinematicResolved.symbols[Sym_AppearStyle],0x4321);put32(base+cinematicResolved.symbols[Sym_DisappearStyle],0x4320);
    typedShowStub(*game,cinematicResolved.symbols[Sym_Show],(void*)mockShow);typedShowStub(*game,cinematicResolved.symbols[Sym_Hide],(void*)mockHide);
    for(U32 r:{cinematicResolved.symbols[Sym_InvalidateStyle],cinematicResolved.symbols[Sym_SortStyles],cinematicResolved.symbols[Sym_ClearEvent],
        referenceUi(0xCB6990u),referenceUi(0xCC0190u),referenceUi(0xCC2A70u),referenceUi(0xCC2100u),referenceUi(0x3EF80B0u)})stub(*game,r,(void*)noOperation);
    if(scenario=="npc_shape")base[npcResolved.update+15]^=1;
    auto asi=loadPE(argv[2],true);asiBase=asi->b;linkImports(asi->b);U32 op=u32(asi->b+0x3C)+24;
    asi->executable(u32(asi->b+op+20),u32(asi->b+op+4));dllMain=(int(NATIVE_ABI*)(void*,W32,void*))(asi->b+u32(asi->b+op+16));
    workerSimulation=runCompiledDialogue;
    CHECK(dllMain(asi->b,1,nullptr)==1&&worker);CHECK(worker(nullptr)==0);CHECK(suspendedCount==0&&fileHandles.empty()&&files.size()==2);
    std::string report=files.at("C:\\TestGame\\ShutUpAndLetMePlay_UpdateReport.json");
    std::string log=files.at("C:\\TestGame\\ShutUpAndLetMePlay.log");
    CHECK(report.find("\"status\": \"active\"")!=std::string::npos);
    CHECK(report.find(npcDisabledCase()?"\"enabled\": 0":"\"enabled\": 1")!=std::string::npos);
    CHECK(registeredTables.size()==virtualAllocations.size());
    if(argc==5){const auto out=std::filesystem::path(argv[4])/scenario;std::filesystem::create_directories(out);
        std::ofstream(out/"report.json",std::ios::binary)<<report;std::ofstream(out/"report.log",std::ios::binary)<<log;}
    std::cout<<"PASS compiled ASI NPC / "<<scenario<<" / assertions="<<checks<<" / existing cutscene hooks remain active\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}}
