// Execute the compiled ASI and native fixed-row input/appearance routines.
// Actor acquisition and sequence completion are mocked; no full-game run.
#include <signal.h>
#include <ucontext.h>
#define DIALOGUE_FIXTURE_LIBRARY
#include "dialogue_harness.cpp"
#include "../source/Sequencer.h"
static SequencerResolved sequenceResolved;
static Memory* sequenceGame=nullptr;
static unsigned sequenceCalls=0,actorLookups=0,actorReleases=0;
static void* expectedActor=nullptr,*expectedSequence=nullptr,*expectedActorManager=nullptr;
static U8* expectedState=nullptr;
static bool NATIVE_ABI nativeType(void*,void*){return true;}
static bool NATIVE_ABI rejectedType(void*,void*){return false;}
static bool NATIVE_ABI releaseActor(void*actor){CHECK(actor==expectedActor);++actorReleases;return false;}
static void* NATIVE_ABI actorHandle(void*manager,void*out){
    CHECK(manager==expectedActorManager);++actorLookups;
    std::memset(out,0,24);
    if(scenario!="seq_no_actor"){set(out,8,expectedActor);((U8*)out)[16]=1;}
    return out;
}
static void NATIVE_ABI nativeSequenceSkip(void*self,float fade){
    CHECK(self==expectedSequence);float expected=0;std::memcpy(&expected,base+0x6CC85B8,4);CHECK(fade==expected);
    ++sequenceCalls;expectedState[0x107E]=4;
}
struct SequenceFixture:DialogueFixture {
    std::array<U8,0x40> facade{},componentHolder{},sequenceTarget{};
    std::array<U8,0x200> components{};
    std::array<void*,64> eventTable{},actorTable{};
    explicit SequenceFixture(U8 mode):DialogueFixture(){
        regions.push_back({(U8*)this,sizeof(*this)});state[0x107E]=mode;
        set(base,sequenceResolved.symbols[SequencerSymStateFacade],facade.data());set(facade.data(),0,state.data());
        set(world.data(),0x30,store.data());expectedActorManager=store.data();
        set(actor.data(),0,actorTable.data());actorTable[1]=(void*)releaseActor;
        set(actor.data(),0x68,components.data());set(components.data(),0xA8,componentHolder.data());
        set(componentHolder.data(),0x18,sequenceTarget.data());
        expectedActor=actor.data();expectedSequence=sequenceTarget.data();expectedState=state.data();
        eventTable[4]=(void*)noOperation;eventTable[5]=(void*)noOperation;
        eventTable[0x190/8]=(void*)nativeType;set(ui.event.data(),0,eventTable.data());
        // Type checks are the only vtable dispatch used by these native input paths.
        set(base+0x564B020,0x190,(void*)nativeType);
    }
};
static bool disabledSequence(){return scenario=="seq_shape"||scenario=="seq_callback"||scenario=="seq_ambiguous";}
static void runSequencer(){
    CHECK(virtualAllocations.size()==4&&registeredTables.size()==4);
    // Patch only the final external boundaries after the real ASI resolver ran.
    stub(*sequenceGame,0x8B4480,(void*)actorHandle);
    stub(*sequenceGame,sequenceResolved.skip,(void*)nativeSequenceSkip);
    U8 mode=scenario=="seq_mode0"?0:2;
    if(scenario=="seq_mode4")mode=4;
    SequenceFixture f(mode);
    auto interaction=(Toggle)(base+cinematicResolved.interaction);
    auto appearance=(Toggle)(base+cinematicResolved.appearance);
    auto input=(DialogueInput)(base+npcResolved.input);
    if(scenario=="seq_wrong_state")set(f.facade.data(),0,f.ui.gameState.data());
    if(scenario=="seq_paused")f.state[0x107D]=1;
    interaction(f.ui.view.data(),1);appearance(f.ui.view.data(),1);
    if(disabledSequence()||scenario=="seq_paused"||scenario=="seq_mode4"||scenario=="seq_wrong_state"){
        CHECK(f.ui.guides[1].wrapper[0xBE]);CHECK(f.ui.disappears(1));
        CHECK(sequenceCalls==0&&actorLookups==0);return;
    }
    CHECK(f.ui.appears(1)&&!f.ui.disappears(1)&&!f.ui.guides[1].wrapper[0xBE]);
    CHECK(f.ui.event[0xF2]==1);
    auto button=f.ui.guides[1].object;auto contextBytes=f.ctx;
    for(unsigned cycle=0;cycle<100;++cycle){
        appearance(f.ui.view.data(),0);CHECK(f.ui.guides[1].wrapper[0xBE]);
        appearance(f.ui.view.data(),1);CHECK(f.ui.appears(1)&&!f.ui.disappears(1)&&!f.ui.guides[1].wrapper[0xBE]);
    }
    CHECK(std::memcmp(button.data()+0xD0,f.ui.guides[1].object.data()+0xD0,0x178)==0);
    input(f.ui.view.data(),f.ui.event.data(),f.ui.guides[1].object.data(),1);
    CHECK(!sequenceCalls&&!actorLookups); // release is not hold completion
    if(scenario=="seq_hidden")f.ui.guides[0].wrapper[0xBE]=1;
    if(scenario=="seq_inactive")f.ui.view[0x35C]=0;
    if(scenario=="seq_rejected_type")f.eventTable[0x190/8]=(void*)rejectedType;
    void*event=scenario=="seq_bad_event"?f.actor.data():f.ui.event.data();
    input(f.ui.view.data(),event,f.ui.guides[1].object.data(),0);
    bool blocked=scenario=="seq_hidden"||scenario=="seq_inactive"||scenario=="seq_bad_event"||scenario=="seq_rejected_type";
    CHECK(sequenceCalls==((blocked||scenario=="seq_no_actor")?0u:1u));
    CHECK(actorLookups==(blocked?0u:1u));
    CHECK(actorReleases==((blocked||scenario=="seq_no_actor")?0u:1u));
    CHECK(f.ctx==contextBytes&&voicesStopped==0&&finishes==0); // no interaction advance/cancel/choice calls
    CHECK(f.state[0x107C]==0); // no fast-forward flag writes
    if(!blocked&&scenario!="seq_no_actor")CHECK(f.state[0x107E]==4);
    else CHECK(f.state[0x107E]==mode);
    f.state[0x107E]=mode;interaction(f.ui.view.data(),0);CHECK(f.ui.guides[1].wrapper[0xBE]);
}
static void faultHandler(int,siginfo_t*si,void*v){auto*u=(ucontext_t*)v;auto rip=u->uc_mcontext.gregs[REG_RIP];
    fprintf(stderr,"FAULT %p rip=%llx game=%llx asi=%llx rcx=%llx ret=%llx\n",si->si_addr,(long long)rip,(long long)(rip-(U64)base),(long long)(rip-(U64)asiBase),(long long)u->uc_mcontext.gregs[REG_RCX],*(long long*)u->uc_mcontext.gregs[REG_RSP]-(U64)base);_Exit(100);}
int main(int argc,char**argv){try{
    CHECK(argc==4||argc==5);scenario=argv[3];
    struct sigaction sa{};sa.sa_sigaction=faultHandler;sa.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&sa,nullptr);sigaction(SIGILL,&sa,nullptr);
    auto game=loadPE(argv[1],false);base=game->b;sequenceGame=game.get();
    CHECK(parseImage(base,gameImage)&&resolve(gameImage,cinematicResolved)&&resolveDialogue(gameImage,cinematicResolved,npcResolved));
    CHECK(resolveSequencer(gameImage,npcResolved,sequenceResolved));
    CHECK(sequenceResolved.skip==0xA991D0&&sequenceResolved.finish==0xA994A0);
    behavior.image=gameImage;behavior.resolved=cinematicResolved;
    for(auto r:{std::pair<U32,U32>{cinematicResolved.interaction,484},{cinematicResolved.appearance,2178},{cinematicResolved.setAppearance,267},
        {cinematicResolved.symbols[Sym_ResetKeyguide],0x149},{cinematicResolved.symbols[Sym_AddStyle],0x99},{cinematicResolved.symbols[Sym_RemoveStyle],0x70},
        {npcResolved.input,DialogueInputPattern.shape.size}})game->executable(r.first,r.second);
    put32(base+cinematicResolved.symbols[Sym_AppearStyle],0x4321);put32(base+cinematicResolved.symbols[Sym_DisappearStyle],0x4320);
    typedShowStub(*game,cinematicResolved.symbols[Sym_Show],(void*)mockShow);typedShowStub(*game,cinematicResolved.symbols[Sym_Hide],(void*)mockHide);
    for(U32 r:{cinematicResolved.symbols[Sym_InvalidateStyle],cinematicResolved.symbols[Sym_SortStyles],cinematicResolved.symbols[Sym_ClearEvent],
        referenceUi(0xCB6990u),referenceUi(0xCC0190u),referenceUi(0xCC2A70u),referenceUi(0xCC2100u),referenceUi(0x3EF80B0u)})stub(*game,r,(void*)noOperation);
    if(scenario=="seq_shape")base[sequenceResolved.skip]^=1;
    if(scenario=="seq_callback")base[sequenceResolved.symbols[SequencerSymCallback]+1]^=1;
    if(scenario=="seq_ambiguous"){
        U32 hole=gameImage.sections[0].rva+gameImage.sections[0].size-4096;
        std::memcpy(base+hole,base+sequenceResolved.skip,SequencerSkipPattern.shape.size);
    }
    auto asi=loadPE(argv[2],true);asiBase=asi->b;linkImports(asi->b);U32 op=u32(asi->b+0x3C)+24;
    asi->executable(u32(asi->b+op+20),u32(asi->b+op+4));dllMain=(int(NATIVE_ABI*)(void*,W32,void*))(asi->b+u32(asi->b+op+16));
    workerSimulation=runSequencer;
    CHECK(dllMain(asi->b,1,nullptr)==1&&worker);CHECK(worker(nullptr)==0);
    CHECK(suspendedCount==0&&fileHandles.empty()&&files.size()==2);
    std::string report=files.at("C:\\TestGame\\ShutUpAndLetMePlay_UpdateReport.json"),log=files.at("C:\\TestGame\\ShutUpAndLetMePlay.log");
    CHECK(report.find("\"status\": \"active\"")!=std::string::npos);
    CHECK(report.find("\"npc_dialogue\": {\n    \"enabled\": 1")!=std::string::npos);
    CHECK(report.find(disabledSequence()?"\"sequencer_dialogue\": {\n    \"enabled\": 0":"\"sequencer_dialogue\": {\n    \"enabled\": 1")!=std::string::npos);
    if(argc==5){auto out=std::filesystem::path(argv[4])/scenario;std::filesystem::create_directories(out);
        std::ofstream(out/"report.json")<<report;std::ofstream(out/"report.log")<<log;}
    std::cout<<"PASS compiled ASI sequencer / "<<scenario<<" / assertions="<<checks<<" / native sequence calls="<<sequenceCalls<<"\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}}
