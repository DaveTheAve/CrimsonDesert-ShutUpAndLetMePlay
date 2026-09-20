// Executes the supplied dialogue update/current/advance routines against explicit
// fixtures. Audio stopping and final actor/UI completion effects are mocked.
// Choice-boundary policy tests use a mock choice notification, not game rendering.
#define DIALOGUE_TEST_LIBRARY
#include "compiled_asi_harness.cpp"
static DialogueResolved npcResolved;
static Image gameImage;static Resolved cinematicResolved;
static U64 tickValue=10000;
static U64 testClock(){return tickValue;}
static unsigned voicesStopped=0,finishes=0,nativeCalls=0;
static std::vector<U32> dispatchedEntries;
static void* NATIVE_ABI lookupFixture(void*){return nullptr;}
static void NATIVE_ABI dispatchFixture(void*,void*,void*,void*,void*,U32*entry,void*,void*,void*,void*,void*){CHECK(entry);dispatchedEntries.push_back(*entry);}
static void NATIVE_ABI stopFixtureVoice(void*,U32 handle){CHECK(handle==777);++voicesStopped;}
static void NATIVE_ABI finishFixture(void*ctx,void*actor,void*player,U8 normal){
    CHECK(normal==1&&actor&&player);++finishes;((U8*)ctx)[0x3D]=1;((U8*)ctx)[0x41]=0;
    void*world=*(void**)(base+npcResolved.symbols[DialogueSymWorld]);void*state=*(void**)((U8*)world+0x20);
    ((U8*)state)[0x107E]=4;
}
static U32* NATIVE_ABI countedNativeUpdate(void*c,U32*r,void*a,void*p,float dt,U64 time){
    ++nativeCalls;return ((DialogueUpdate)(base+npcResolved.update))(c,r,a,p,dt,time);
}
struct DialogueFixture {
    Fixture ui;
    std::array<U8,0x80> ctx{};
    std::array<U8,0x1400> state{};
    std::array<U8,0x100> world{},store{},actor{},player{},voiceManager{};
    std::array<U8,0x200> database{};
    std::array<U8,0x90> record{};
    std::array<U8,0x70*200> entries{};
    std::array<U8,0x30*127> lines{};
    std::array<U8,24*16> journal{};
    std::array<void*,1> cache{};
    U32 result=999;Dialogue d;
    explicit DialogueFixture(U32 count=3,U32 lineCount=3):ui(3){
        regions.push_back({(U8*)this,sizeof(*this)});tickValue=10000;
        voicesStopped=finishes=nativeCalls=0;dispatchedEntries.clear();
        d.core=&behavior;d.resolved=npcResolved;d.enabled=1;d.clock=testClock;
        d.current=(DialogueCurrent)(base+npcResolved.current);d.advance=(DialogueAdvance)(base+npcResolved.advance);
        d.originalUpdate=countedNativeUpdate;d.stopVoice=stopFixtureVoice;
        set(base,npcResolved.symbols[DialogueSymWorld],world.data());set(world.data(),0x20,state.data());
        set(ui.manager.data(),0x90,state.data());set(state.data(),0x11D0,voiceManager.data());state[0x107E]=3;
        set(base,npcResolved.symbols[DialogueSymRuleStore],store.data());put32(store.data()+8,1);
        cache[0]=database.data();set(store.data(),0x58,cache.data());
        set(database.data(),0x120,record.data());put32(database.data()+0x128,1);put32(database.data()+0x12C,1);
        record[2]=0;set(record.data(),0x48,entries.data());put32(record.data()+0x50,count);put32(record.data()+0x54,count);
        for(U32 i=0;i<count;++i){U8*e=entries.data()+i*0x70;e[4]=e[5]=e[0x30]=e[0x31]=255;
            set(e,0x60,lines.data());put32(e+0x68,lineCount);put32(e+0x6C,lineCount);}
        ctx[0]=1;ctx[0x40]=ctx[0x41]=1;ctx[0x78]=ctx[0x79]=255;
        put32(ctx.data()+0x64,0xFFFFFFFFu);put32(ctx.data()+0x50,777);
        set(ctx.data(),0x68,journal.data());put32(ctx.data()+0x74,16);
        float timer=60.0f;std::memcpy(ctx.data()+0x38,&timer,4);
        put32(ctx.data()+0x1C,5);put32(actor.data()+0x60,1);put32(player.data()+0x60,2);
        ui.view[0x35C]=2;put32(ui.view.data()+0x230,1);skipWrapper=ui.guides[1].wrapper.data();
        put32(base+npcResolved.symbols[DialogueSymFinishedResult],99);
    }
    U32*update(){return d.update(ctx.data(),&result,actor.data(),player.data(),0.016f,100000);}
    void prompt(){
        d.beforeInteraction(ui.view.data(),1);behavior.interaction(ui.view.data(),1);d.afterInteraction(ui.view.data(),1);
        behavior.appearance(ui.view.data(),1);d.afterAppearance(ui.view.data(),1);
    }
    void ready(){CHECK(update()==&result);CHECK(result==0);CHECK(d.available());prompt();
        CHECK(ui.appears(1)&&!ui.disappears(1)&&!ui.guides[1].wrapper[0xBE]);}
    bool press(U8 phase=0){return d.input(ui.view.data(),ui.event.data(),ui.guides[1].object.data(),phase);}
};
#ifndef DIALOGUE_FIXTURE_LIBRARY
int main(int argc,char**argv){
    CHECK(argc==2);auto game=loadPE(argv[1],false);base=game->b;
    CHECK(parseImage(base,gameImage)&&resolve(gameImage,cinematicResolved));
    CHECK(resolveDialogue(gameImage,cinematicResolved,npcResolved));
    CHECK(npcResolved.input==0x1063520&&npcResolved.update==0x5B7B70&&npcResolved.advance==0x5B5640);
    std::cout<<"PASS dialogue resolver: distinct input/update/advance/current/finish shapes, linked cache getter, cross-reference and unwind checks\n";
    // Reject one-byte structural changes and inconsistent relative targets.
    for(U32 r:{npcResolved.input,npcResolved.update,npcResolved.advance,npcResolved.current,npcResolved.finish,npcResolved.database}){
        U8 saved=base[r];base[r]^=1;DialogueResolved bad;CHECK(!resolveDialogue(gameImage,cinematicResolved,bad));base[r]=saved;}
    CHECK(resolveDialogue(gameImage,cinematicResolved,npcResolved));
    behavior=Behavior{};behavior.image=gameImage;behavior.resolved=cinematicResolved;behavior.readable=validRead;
    behavior.show=mockShow;behavior.hide=mockHide;behavior.setAppearance=(Toggle)(base+cinematicResolved.setAppearance);
    behavior.originalAppearance=(Toggle)(base+cinematicResolved.appearance);behavior.originalInteraction=(Toggle)(base+cinematicResolved.interaction);
    for(auto r:{std::pair<U32,U32>{cinematicResolved.interaction,484},{cinematicResolved.appearance,2178},{cinematicResolved.setAppearance,267},
        {cinematicResolved.symbols[Sym_ResetKeyguide],0x149},{cinematicResolved.symbols[Sym_AddStyle],0x99},
        {cinematicResolved.symbols[Sym_RemoveStyle],0x70},{npcResolved.advance,DialogueAdvancePattern.shape.size},
        {npcResolved.current,DialogueCurrentPattern.shape.size},{npcResolved.update,DialogueUpdatePattern.shape.size},
        {npcResolved.database,DialogueDatabasePattern.shape.size}})game->executable(r.first,r.second);
    put32(base+cinematicResolved.symbols[Sym_AppearStyle],0x4321);put32(base+cinematicResolved.symbols[Sym_DisappearStyle],0x4320);
    stub(*game,cinematicResolved.symbols[Sym_Show],(void*)mockShow);stub(*game,cinematicResolved.symbols[Sym_Hide],(void*)mockHide);
    for(U32 r:{cinematicResolved.symbols[Sym_InvalidateStyle],cinematicResolved.symbols[Sym_SortStyles],cinematicResolved.symbols[Sym_ClearEvent],
        0xCB6990u,0xCC0190u,0xCC2A70u,0xCC2100u,0x3EF80B0u})stub(*game,r,(void*)noOperation);
    stub(*game,npcResolved.finish,(void*)finishFixture);
    stub(*game,0x388E00,(void*)lookupFixture);stub(*game,0x433650,(void*)lookupFixture);stub(*game,0xC007B0,(void*)dispatchFixture);
    // Test-only seam AFTER the actual native wait-for-choice flag assignment:
    // bypass allocation/rendering of choice UI, retaining normal journal dispatch.
    game->executable(0x5B592D,14);jump(base+0x5B592D,base+0x5B67CE);
    {DialogueFixture f;f.ready();auto hold=f.ui.guides[1].object;
        CHECK(f.press(1));CHECK(f.d.counters.requests==0);CHECK(f.press());
        CHECK(f.update()==&f.result);CHECK(f.result==99);CHECK(finishes==1&&voicesStopped==1);
        CHECK(f.d.counters.accepted==1&&f.d.counters.completed==1&&f.d.counters.steps==11);
        CHECK(!f.d.available()&&!f.d.running);CHECK(f.ctx[0x3D]&&f.state[0x107E]==4);
        CHECK(f.state[0x107C]==0);CHECK(std::memcmp(hold.data(),f.ui.guides[1].object.data(),hold.size())==0);
        std::cout<<"PASS actual native progression: skip 3 entries / 9 speech segments without starting intermediate voice playback; natural update calls normal completion\n";}
    {DialogueFixture f;put32(f.entries.data()+0x48,1);f.ready();CHECK(f.press());f.update();
        CHECK(f.ctx[0x5D]==1&&finishes==0&&!f.d.running&&f.result==0);CHECK(f.d.counters.choices==1);
        U32 steps=f.d.counters.steps;CHECK(f.press());f.update();CHECK(f.d.counters.steps==steps);
        std::cout<<"PASS choice policy: stop on native wait flag; no automatic response selection or carry-over request\n";}
    {DialogueFixture f;for(U32 i=0;i<3;++i)f.entries[i*0x70+0x38]=1;f.ready();CHECK(f.press());f.update();
        CHECK(dispatchedEntries==std::vector<U32>({0,1,2}));CHECK(u32(f.ctx.data()+0x70)==0&&finishes==1);
        std::cout<<"PASS actual native per-entry event journal: 3 entries dispatched once, in order, before normal completion\n";}
    {DialogueFixture f;f.ready();tickValue+=600;CHECK(f.press());f.update();CHECK(!voicesStopped&&!finishes&&f.d.counters.accepted==0);}
    {DialogueFixture f;f.ready();CHECK(f.press());f.ctx[0x5C]=1;f.update();CHECK(f.d.counters.accepted==0&&!voicesStopped);}
    {DialogueFixture f;f.ready();CHECK(f.press());f.state[0x107E]=1;f.update();CHECK(f.d.counters.accepted==0&&!voicesStopped);}
    {DialogueFixture f;f.ready();f.ui.view[0x35C]=0;CHECK(f.press());f.update();CHECK(!f.d.counters.accepted);}
    {DialogueFixture f;f.ready();f.ui.guides[0].wrapper[0xBE]=1;CHECK(f.press());f.update();CHECK(!f.d.counters.accepted);}
    {DialogueFixture f;f.ready();int unrelated=0;CHECK(f.d.input(f.ui.view.data(),&unrelated,f.ui.guides[1].object.data(),0));f.update();CHECK(!f.d.counters.accepted);}
    {DialogueFixture f;f.ready();CHECK(!f.d.input(f.ui.view.data(),f.ui.event.data(),f.ui.guides[0].object.data(),0));}
    {DialogueFixture f;f.ctx[0x5D]=1;f.update();CHECK(!f.d.available());}
    {DialogueFixture f;f.ctx[0]=0;f.update();CHECK(!f.d.available());}
    {DialogueFixture f;put32(f.record.data()+0x70,1);f.update();CHECK(!f.d.available());}
    {DialogueFixture f;f.ready();put32(f.database.data()+0x128,0);f.update();CHECK(!f.d.available());}
    {DialogueFixture f(100,3);f.ready();CHECK(f.press());unsigned frames=0;do {f.update();CHECK(++frames<10);}while(f.d.running);
        CHECK(finishes==1&&f.d.counters.completed==1&&f.d.counters.yields>0&&voicesStopped==1);
        CHECK(f.d.counters.steps==399);std::cout<<"PASS bounded work: 300 speech segments skipped across "<<frames<<" updates without raising global speed\n";}
    {DialogueFixture f(100,3);f.ready();CHECK(f.press());f.update();CHECK(f.d.running);
        auto other=f.ctx;other[0x5D]=1;other[0x41]=other[0x40]=0;U32 r=1;
        f.d.update(other.data(),&r,f.player.data(),f.actor.data(),0.016f,100000);CHECK(f.d.running&&r==0);
        while(f.d.running)f.update();CHECK(finishes==1);}
    {DialogueFixture f;f.ready();auto other=f.ctx;other[0x5D]=1;U32 r=1;
        f.d.update(other.data(),&r,f.player.data(),f.actor.data(),0.016f,100000);CHECK(f.d.available());}
    {DialogueFixture f(100,3);f.ready();CHECK(f.press());f.update();CHECK(f.d.running);
        std::array<U8,0x80> transient=f.ctx;regions.push_back({transient.data(),transient.size()});
        while(f.d.running)f.d.update(transient.data(),&f.result,f.actor.data(),f.player.data(),0.016f,100000);
        CHECK(finishes==1&&f.result==99&&transient[0x3D]);}
    std::cout<<"PASS temporary context copies: a bounded request follows identity, not a saved context pointer\n";
    std::cout<<"PASS unrelated NPC updates preserve the foreground offer and bounded job\n";
    std::cout<<"PASS requests rejected for stale snapshots, changed lines/modes, hidden row, wrong event, existing choices, non-target dialogues and unloaded cache\n";
    std::cout<<"TOTAL dialogue assertions: "<<checks<<"\n";
}

#endif
