// Integration fixtures for the two modern executable families. The real
// compiled ASI, native cache getter, input, update, current and advance routines
// execute. Audio, quest recipients, actor handles and final completion are mocks.
static U32 modernFinishes=0,modernVoiceStops=0;
static std::vector<U32> modernEvents;
static U8*modernState=nullptr;
static void* NATIVE_ABI modernLookup(void*){return nullptr;}
static void NATIVE_ABI modernDispatch(void*,void*,void*,void*,void*,U32*entry,void*,void*,void*,void*,void*){
    CHECK(entry);modernEvents.push_back(*entry);
}
static void NATIVE_ABI modernStop(void*,U32 voice){CHECK(voice==777);++modernVoiceStops;}
static void NATIVE_ABI modernFinish(void*ctx,void*actor,void*player,U8 normal){
    CHECK(normal==1&&actor&&player);++modernFinishes;((U8*)ctx)[0x3D]=1;((U8*)ctx)[0x41]=0;
    modernState[0x10B6]=4;
}
struct ModernDialogueFixture {
    Fixture ui;
    std::array<U8,0x80>ctx{};
    std::array<U8,0x1400>state{};
    std::array<U8,0x100>world{},store{},actor{},player{},voice{};
    std::array<U8,0x200>database{};
    std::array<U8,0x90>record{};
    std::array<U8,0x70*200>entries{};
    std::array<U8,0x30*3>lines{};
    std::array<U8,24*16>journal{};
    std::array<U8,24>cache{};
    U32 result=999;
    explicit ModernDialogueFixture(U32 count=3):ui(3){
        regions.push_back({(U8*)this,sizeof(*this)});
        modernFinishes=modernVoiceStops=0;modernEvents.clear();modernState=state.data();
        bindUpdatedGuides(ui);
        ui.targetVT[20]=(void*)noOperation;
        set(base,actualDialogue.symbols[DialogueSymWorld],world.data());
        set(world.data(),0x20,state.data());
        set(ui.manager.data(),0x90,state.data());
        set(state.data(),0x1228,voice.data());state[0x10B6]=3;
        set(base,actualDialogue.symbols[DialogueSymRuleStore],store.data());
        if(modern3663){set(store.data(),0x20,cache.data());put32(store.data()+0x28,1);set(cache.data(),8,database.data());}
        else{set(store.data(),0x58,cache.data());put32(store.data()+8,1);set(cache.data(),0,database.data());}
        set(database.data(),0x110,record.data());put32(database.data()+0x118,1);put32(database.data()+0x11C,1);
        set(record.data(),0x48,entries.data());put32(record.data()+0x50,count);put32(record.data()+0x54,count);
        for(U32 i=0;i<count;++i){U8*e=entries.data()+i*0x70;e[4]=e[5]=e[0x30]=e[0x31]=255;
            set(e,0x60,lines.data());put32(e+0x68,3);put32(e+0x6C,3);}
        ctx[0]=ctx[0x40]=ctx[0x41]=1;ctx[0x78]=ctx[0x79]=255;
        put32(ctx.data()+0x64,0xFFFFFFFFu);put32(ctx.data()+0x50,777);
        set(ctx.data(),0x68,journal.data());put32(ctx.data()+0x74,16);
        float timer=60;std::memcpy(ctx.data()+0x38,&timer,4);
        put32(ctx.data()+0x1C,5);put32(actor.data()+0x60,1);put32(player.data()+0x60,2);
        ui.view[0x35C]=2;put32(ui.view.data()+0x230,1);skipWrapper=ui.guides[1].wrapper.data();
        put32(base+actualDialogue.symbols[DialogueSymFinishedResult],99);
    }
    U32*update(){return ((DialogueUpdate)(base+actualDialogue.update))(ctx.data(),&result,actor.data(),player.data(),0.016f,100000);}
    void show(){((Toggle)(base+behavior.resolved.interaction))(ui.view.data(),1);((Toggle)(base+actualAppearance))(ui.view.data(),1);}
    void press(){((DialogueInput)(base+actualDialogue.input))(ui.view.data(),ui.event.data(),ui.guides[1].object.data(),0);}
};
static void testModernDialogue(){
    prepareNativeKeyguides();
    for(auto r:{std::pair<U32,U32>{actualDialogue.update,modern3663?PatchUpdateSignature.size:NewUpdateSignature.size},
         {actualDialogue.advance,modern3663?PatchAdvanceSignature.size:NewAdvanceSignature.size},
         {actualDialogue.current,modern3663?PatchCurrentSignature.size:NewCurrentSignature.size},
         {actualDialogue.database,modern3663?PatchDatabaseSignature.size:NewDatabaseSignature.size}})
        runtimeGame->executable(r.first,r.second);
    stub(*runtimeGame,actualDialogue.finish,(void*)modernFinish);
    stub(*runtimeGame,actualDialogue.symbols[DialogueSymStopVoice],(void*)modernStop);
    stub(*runtimeGame,modern3663?0x3DB010:0x3DD7E0,(void*)modernLookup);
    stub(*runtimeGame,modern3663?0x4FF910:0x4DAE40,(void*)modernLookup);
    stub(*runtimeGame,modern3663?0xD66740:0xD2C8D0,(void*)modernDispatch);
    if(scenario=="modern_choice"){
        // Bypass ONLY choice rendering after the actual wait-flag write.
        U32 from=modern3663?0x69CDB4:0x670437,to=modern3663?0x69DC36:0x6712B8;
        CHECK(base[from-4]==0xC6&&base[from-3]==0x46&&base[from-2]==0x5D&&base[from-1]==1);
        runtimeGame->executable(from,14);jump(base+from,base+to);
    }
    ModernDialogueFixture f(scenario=="modern_budget"?100:3);
    if(scenario=="modern_choice")put32(f.entries.data()+0x48,1);
    if(scenario=="modern_events")for(U32 i=0;i<3;++i)f.entries[i*0x70+0x38]=1;
    if(scenario.rfind("modern_cache_",0)==0)put32(f.ui.view.data()+0x230,0);
    if(scenario=="modern_cache_empty")set(f.cache.data(),modern3663?8:0,nullptr);
    if(scenario=="modern_cache_count")put32(f.store.data()+(modern3663?0x28:8),1000001);
    if(scenario=="modern_cache_unreadable")set(f.store.data(),modern3663?0x20:0x58,(void*)1);
    CHECK(f.update()==&f.result&&f.result==0);f.show();
    if(scenario.rfind("modern_cache_",0)==0){
        CHECK(f.ui.guides[1].wrapper[0xBE]&&!f.ui.appears(1));f.press();f.update();
        CHECK(modernVoiceStops==0&&modernFinishes==0);return;
    }
    CHECK(f.ui.appears(1)&&!f.ui.disappears(1)&&!f.ui.guides[1].wrapper[0xBE]);
    auto saved=f.ui.guides[1].object;
    if(scenario=="modern_stale")simulatedTick+=600;
    f.press();
    if(scenario=="modern_changed")f.ctx[0x5C]=1;
    if(scenario=="modern_waiting")f.ctx[0x5D]=1;
    CHECK(f.update()==&f.result);
    if(scenario=="modern_stale"||scenario=="modern_changed"||scenario=="modern_waiting"){
        CHECK(modernFinishes==0&&modernVoiceStops==0&&f.result==0);return;
    }
    if(scenario=="modern_choice"){
        CHECK(modernFinishes==0&&modernVoiceStops==1&&f.result==0&&f.ctx[0x5D]==1);
        f.press();f.update();CHECK(modernFinishes==0&&f.ctx[0x5D]==1);return;
    }
    U32 frames=1;while(!modernFinishes){CHECK(++frames<=8);CHECK(f.update()==&f.result);}
    CHECK(modernFinishes==1&&modernVoiceStops==1&&f.result==99&&f.state[0x10B6]==4);
    CHECK(f.state[0x10B4]==0&&u32(f.ctx.data()+0x70)==0);
    CHECK(std::memcmp(saved.data()+0xC0,f.ui.guides[1].object.data()+0xC0,0x178)==0);
    if(scenario=="modern_events")CHECK(modernEvents==std::vector<U32>({0,1,2}));
    if(scenario=="modern_budget")CHECK(frames==7);
}
static U32 modernSequenceCalls=0,modernActorLookups=0,modernActorReleases=0,modernComponentReleases=0;
static void*modernActor=nullptr,*modernManager=nullptr,*modernComponent=nullptr,*modernSequence=nullptr;
static bool NATIVE_ABI modernType(void*,void*){return true;}
static bool NATIVE_ABI modernGuideType(void*,void*){return scenario!="route_type_rejected";}
static bool NATIVE_ABI modernActorRelease(void*p){CHECK(p==modernActor);++modernActorReleases;return false;}
static void NATIVE_ABI modernComponentRelease(void*p){CHECK(p==modernComponent);++modernComponentReleases;}
static void* NATIVE_ABI modernActorHandle(void*manager,void*out){
    CHECK(manager==modernManager);++modernActorLookups;std::memset(out,0,24);
    if(scenario!="route_no_actor"){set(out,8,modernActor);((U8*)out)[16]=1;}return out;
}
static void* NATIVE_ABI modernComponentHandle(void*actor,void*out){
    CHECK(actor==modernActor);std::memset(out,0,56);set(out,8,modernComponent);set(out,40,modernComponent);((U8*)out)[16]=1;return out;
}
static void* NATIVE_ABI modernSequenceAccess(void*p){CHECK(p==modernComponent);return modernSequence;}
static void NATIVE_ABI modernSequenceSkip(void*p,float fade){
    CHECK(p==modernSequence);U32 r=relative(behavior.image,behavior.resolved.skipHelper,0x63,0x67);
    float expected;std::memcpy(&expected,base+r,4);CHECK(fade==expected);++modernSequenceCalls;modernState[0x10B6]=4;
}
static void testModernSequence(){
    prepareNativeKeyguides();
    U32 helper=behavior.resolved.skipHelper;
    runtimeGame->executable(actualDialogue.input,behavior.resolved.nativeInputSize);
    runtimeGame->executable(helper,NewSequenceHelperSignature.size);
    stub(*runtimeGame,relative(behavior.image,helper,0x2A,0x2E),(void*)modernActorHandle);
    stub(*runtimeGame,relative(behavior.image,helper,0x48,0x4C),(void*)modernComponentHandle);
    stub(*runtimeGame,relative(behavior.image,helper,0xA9,0xAD),(void*)noOperation);
    stub(*runtimeGame,actualSequence.skip,(void*)modernSequenceSkip);
    ModernDialogueFixture f;
    std::array<void*,64>eventVT{},actorVT{},componentVT{};
    std::array<U8,0x40>facade{},component{},target{};
    regions.push_back({facade.data(),facade.size()});regions.push_back({component.data(),component.size()});
    regions.push_back({target.data(),target.size()});regions.push_back({(U8*)eventVT.data(),sizeof(eventVT)});
    regions.push_back({(U8*)actorVT.data(),sizeof(actorVT)});regions.push_back({(U8*)componentVT.data(),sizeof(componentVT)});
    set(base,actualSequence.symbols[SequencerSymStateFacade],facade.data());set(facade.data(),0,f.state.data());
    set(f.world.data(),0x30,f.store.data());modernManager=f.store.data();modernActor=f.actor.data();
    modernComponent=component.data();modernSequence=target.data();
    actorVT[1]=(void*)modernActorRelease;set(f.actor.data(),0,actorVT.data());
    componentVT[9]=(void*)modernSequenceAccess;componentVT[0x148/8]=(void*)modernComponentRelease;set(component.data(),0,componentVT.data());
    set(f.ui.view.data(),0x258,f.ui.guides[2].object.data());
    eventVT[4]=(void*)noOperation;eventVT[5]=(void*)noOperation;eventVT[0x1A0/8]=(void*)modernType;
    set(f.ui.event.data(),0,eventVT.data());set(base+0x564B020,0x1A0,(void*)modernGuideType);
    bool cutscene=scenario=="route_cutscene"||scenario=="resilience_input_guard"||scenario=="resilience_both_optional";
    U8 mode=cutscene?1:(scenario=="route_mode0"?0:2);
    f.state[0x10B6]=mode;skipWrapper=f.ui.guides[cutscene?4:1].wrapper.data();
    if(scenario=="route_paused")f.state[0x10B5]=1;
    if(scenario=="route_wrong_state")set(facade.data(),0,f.ui.gameState.data());
    f.show();
    auto&g=f.ui.guides[cutscene?4:1];
    if(scenario=="route_paused"||scenario=="route_wrong_state"){
        CHECK(g.wrapper[0xBE]&&g.has(u32(base+behavior.resolved.symbols[Sym_DisappearStyle])));return;
    }
    CHECK(!g.wrapper[0xBE]);
    if(scenario=="route_hidden")g.wrapper[0xBE]=1;
    if(scenario=="route_inactive")f.ui.view[0x35C]=0;
    auto input=(DialogueInput)(base+actualDialogue.input);
    input(f.ui.view.data(),f.ui.event.data(),g.object.data(),0);
    bool rejected=scenario=="route_hidden"||scenario=="route_inactive"||scenario=="route_type_rejected";
    if(rejected){CHECK(!modernActorLookups&&!modernSequenceCalls);return;}
    CHECK(modernActorLookups==1);
    if(scenario=="route_no_actor")CHECK(modernSequenceCalls==0&&modernActorReleases==0);
    else CHECK(modernSequenceCalls==1&&modernActorReleases==1&&modernComponentReleases==1&&f.state[0x10B6]==4);
    CHECK(modernVoiceStops==0&&modernFinishes==0);
}
