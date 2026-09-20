// Linux x86-64 test harness. It executes ONLY the reviewed native UI routines
// through the Microsoft x64 ABI. Rendering, OS APIs, and input devices are mocked.
// This is not an in-game test and never launches the executable's entry point.
#include <array>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>
#include "../source/Behavior.h"
using namespace crimson;
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<"\n";std::abort();}}while(0)
struct Memory {
    U8*b;U32 n;
    Memory(U32 size):n(size){b=(U8*)mmap(nullptr,n,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(b==MAP_FAILED)throw std::runtime_error("mmap");}
    ~Memory(){munmap(b,n);}
    void executable(U32 r,U32 len){U32 start=r&~4095u,end=(r+len+4095)&~4095u;CHECK(mprotect(b+start,end-start,PROT_READ|PROT_WRITE|PROT_EXEC)==0);}
};
static std::vector<std::pair<const U8*,size_t>> regions;
static bool validRead(const void*p,U32 n){U64 a=(U64)p;for(auto&r:regions){U64 b=(U64)r.first;if(a>=b&&a-b<=r.second&&n<=r.second-(a-b))return true;}return false;}
static Behavior behavior;static U8*base;static unsigned invalidations=0,shows=0,hideCalls=0,activations=0,deactivations=0;static void*skipWrapper=nullptr;
static std::vector<void*> allowedWidgets;
static void set(void*p,unsigned o,void*v){std::memcpy((U8*)p+o,&v,8);}
static void NATIVE_ABI mockShow(void*p){CHECK(p);bool valid=false;for(void*w:allowedWidgets)valid|=w==p;CHECK(valid);((U8*)p)[0xBD]=1;((U8*)p)[0xBE]=0;++shows;}
static void NATIVE_ABI mockHide(void*p){CHECK(p);bool valid=false;for(void*w:allowedWidgets)valid|=w==p;CHECK(valid);((U8*)p)[0xBD]=0;((U8*)p)[0xBE]=1;++hideCalls;}
struct StyleVector{U32*data;U32 count,capacity;};
static void NATIVE_ABI noOperation(void*){}
static void NATIVE_ABI invalidate(void*){++invalidations;}
static void NATIVE_ABI activate(void*){CHECK(skipWrapper&&((U8*)skipWrapper)[0xBE]==0);++activations;}
static void NATIVE_ABI deactivate(void*){++deactivations;}
static void NATIVE_ABI entryInteraction(void*p,U8 b){behavior.interaction(p,b);}
static void NATIVE_ABI entryAppearance(void*p,U8 b){behavior.appearance(p,b);}
static void jump(void*p,const void*t){U8 bytes[14]={0xFF,0x25,0,0,0,0};std::memcpy(bytes+6,&t,8);std::memcpy(p,bytes,14);}
static void stub(Memory&m,U32 r,const void*t){m.executable(r,14);jump(m.b+r,t);}
static constexpr U32 hashes[3]={0x5EFE1F8Eu,0x8653EBE6u,0xED9DCCC4u};
struct Guide {
    std::array<U8,0x300> object{},wrapper{};
    std::array<U8,0x100> backing{},wrapBacking{};
    std::array<U8,0x40> action{};
    std::array<void*,2> controller{};
    std::array<U32,16> styles{};
    StyleVector vector{};
    void init(unsigned role,U8 family){
        vector={styles.data(),1,16};styles[0]=u32(base+behavior.resolved.symbols[Sym_DisappearStyle]);
        controller={backing.data(),&vector};set(backing.data(),0x20,controller.data());
        set(object.data(),0,base+0x564B020);set(object.data(),8,backing.data());set(object.data(),0x1F8,action.data());
        set(wrapper.data(),0,base+0x5590310);set(wrapper.data(),8,wrapBacking.data());
        put32(action.data()+0x2C,hashes[role]);object[0x200]=family;object[0x201]=2;object[0x202]=5;
        // Sentinels in hold state/cache, never interpreted by mocked lower layers.
        put32(object.data()+0x1D8,0x3F800000);object[0x1E8]=1;object[0x1E9]=1;object[0x1EA]=1;
        object[0x233]=0;object[0x240]=0;wrapper[0xBE]=1;
        allowedWidgets.push_back(object.data());allowedWidgets.push_back(wrapper.data());
    }
    bool has(U32 id)const{for(U32 i=0;i<vector.count;++i)if(styles[i]==id)return true;return false;}
};
struct Fixture {
    std::array<Guide,6> guides{};std::array<U8,0x400> view{};
    std::array<U8,0x200> manager{};std::array<U8,0x1100> gameState{};
    std::array<U8,0x100> event{},eventD{},target{};std::array<void*,32> eventVT{},targetVT{};void*eventPointer=nullptr;
    Fixture(U8 cinema=1,U8 family=0){
        regions.clear();regions.push_back({base,behavior.image.size});regions.push_back({(const U8*)this,sizeof(*this)});allowedWidgets.clear();
        for(unsigned i=0;i<6;++i){guides[i].init(i%3,family);set(view.data(),0x2A8+i*16,guides[i].wrapper.data());set(view.data(),0x2B0+i*16,guides[i].object.data());}
        set(manager.data(),0x90,gameState.data());set(base,behavior.resolved.symbols[Sym_GameManager],manager.data());gameState[0x107E]=cinema;
        set(event.data(),0,eventVT.data());set(event.data(),0xD0,eventD.data());set(eventD.data(),0x28,target.data());set(target.data(),0,targetVT.data());
        eventVT[4]=(void*)activate;eventVT[5]=(void*)deactivate;targetVT[19]=(void*)noOperation;
        eventPointer=event.data();set(view.data(),0x228,&eventPointer);put32(view.data()+0x230,cinema==1?1:0);skipWrapper=guides[4].wrapper.data();
    }
    void rowShown(){for(unsigned i=3;i<6;++i)guides[i].wrapper[0xBE]=0;}
    bool appears(unsigned i)const{return guides[i].has(u32(base+behavior.resolved.symbols[Sym_AppearStyle]));}
    bool disappears(unsigned i)const{return guides[i].has(u32(base+behavior.resolved.symbols[Sym_DisappearStyle]));}
    std::array<unsigned,18> summary()const{std::array<unsigned,18>v{};for(unsigned i=0;i<6;++i){v[i*3]=appears(i);v[i*3+1]=disappears(i);v[i*3+2]=guides[i].wrapper[0xBE];}return v;}
};
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("usage: native_harness path/to/CrimsonDesert.exe");
    std::ifstream in(argv[1],std::ios::binary);std::vector<U8>file((std::istreambuf_iterator<char>(in)),{});CHECK(file.size()>0x1000);
    U32 nt=u32(file.data()+0x3C),opt=nt+24,size=u32(file.data()+opt+56);Memory memory(size);base=memory.b;
    std::memcpy(base,file.data(),0x1000);U32 n=u16(file.data()+nt+6),os=u16(file.data()+nt+20);
    for(U32 i=0;i<n;++i){const U8*s=file.data()+opt+os+40*i;U32 va=u32(s+12),raw=u32(s+20),len=u32(s+16);CHECK(raw+len<=file.size()&&va+len<=size);std::memcpy(base+va,file.data()+raw,len);}
    Image image;CHECK(parseImage(base,image));Resolved resolved;CHECK(resolve(image,resolved));
    CHECK(resolved.interaction==0x1069280&&resolved.appearance==0x10689F0&&resolved.setAppearance==0xCBE480);
    std::cout<<"PASS resolver: unique complete function shapes, symbol cross-checks, literal style names, native Skip call, unwind records\n";
    // Wrong native field offsets, wrong literal identity, and existing entry hooks must fail closed.
    U8 original=base[resolved.appearance+0x38];base[resolved.appearance+0x38]^=1;Resolved bad;CHECK(!resolve(image,bad));base[resolved.appearance+0x38]=original;
    original=base[resolved.interaction];base[resolved.interaction]=0xE9;CHECK(!resolve(image,bad));base[resolved.interaction]=original;
    original=base[0x5649EF8];base[0x5649EF8]='X';CHECK(!resolve(image,bad));base[0x5649EF8]=original;
    CHECK(resolve(image,bad));std::cout<<"PASS fail-closed checks: changed code, existing detour, wrong appearance literal\n";
    behavior.image=image;behavior.resolved=resolved;behavior.readable=validRead;
    behavior.show=(WidgetCall)(base+resolved.symbols[Sym_Show]);behavior.hide=(WidgetCall)(base+resolved.symbols[Sym_Hide]);
    behavior.setAppearance=(Toggle)(base+resolved.setAppearance);
    put32(base+resolved.symbols[Sym_AppearStyle],0x4321);put32(base+resolved.symbols[Sym_DisappearStyle],0x4320);
    memory.executable(resolved.interaction,InteractionSignature.size);memory.executable(resolved.appearance,CinemaAppearanceSignature.size);memory.executable(resolved.setAppearance,SetKeyguideAppearanceSignature.size);
    // Execute the real style insertion/removal and reset routines too; our fixture
    // preallocates sufficient vector capacity, so the game allocator is never called.
    memory.executable(resolved.symbols[Sym_ResetKeyguide],0x149);
    stub(memory,resolved.symbols[Sym_Show],(void*)mockShow);stub(memory,resolved.symbols[Sym_Hide],(void*)mockHide);
    memory.executable(resolved.symbols[Sym_AddStyle],0x99);memory.executable(resolved.symbols[Sym_RemoveStyle],0x70);
    stub(memory,resolved.symbols[Sym_InvalidateStyle],(void*)invalidate);stub(memory,resolved.symbols[Sym_SortStyles],(void*)noOperation);stub(memory,resolved.symbols[Sym_ClearEvent],(void*)noOperation);
    for(U32 r:{0xCB6990u,0xCC0190u,0xCC2A70u,0xCC2100u,0x3EF80B0u})stub(memory,r,(void*)noOperation);
    // Construct exactly the 15-byte relocation-free prologue trampolines used by the ASI.
    Memory trampolines(4096);trampolines.executable(0,4096);
    std::memcpy(trampolines.b,base+resolved.interaction,15);jump(trampolines.b+15,base+resolved.interaction+15);
    std::memcpy(trampolines.b+128,base+resolved.appearance,15);jump(trampolines.b+143,base+resolved.appearance+15);
    behavior.originalInteraction=(Toggle)trampolines.b;behavior.originalAppearance=(Toggle)(trampolines.b+128);
    jump(base+resolved.interaction,(void*)entryInteraction);base[resolved.interaction+14]=0x90;
    jump(base+resolved.appearance,(void*)entryAppearance);base[resolved.appearance+14]=0x90;
    Toggle interaction=(Toggle)(base+resolved.interaction),appearance=(Toggle)(base+resolved.appearance);
    {Fixture f;f.rowShown();behavior.originalAppearance(f.view.data(),1);
        CHECK(f.appears(3)&&!f.disappears(3));CHECK(!f.appears(4)&&f.disappears(4));CHECK(f.appears(5)&&!f.disappears(5));
        std::cout<<"PASS reference reproduction: actual native code assigns disappear to Skip while FF and Pause appear\n";}
    for(U8 family:{0,1,2}){
        Fixture f(1,family);interaction(f.view.data(),1);CHECK(f.event[0xF2]==1);CHECK(f.guides[4].wrapper[0xBE]==0);
        auto hold=f.guides[4].object;
        appearance(f.view.data(),1);CHECK(f.appears(4)&&!f.disappears(4)&&!f.guides[4].wrapper[0xBE]);
        CHECK(std::memcmp(hold.data()+0xD0,f.guides[4].object.data()+0xD0,0x178)==0); // includes binding/cache/hold/default state
        CHECK(f.guides[4].object[0x200]==family&&f.guides[4].object[0x201]==2);
        for(unsigned cycle=0;cycle<100;++cycle){
            appearance(f.view.data(),0);CHECK(!f.appears(4)&&f.disappears(4)&&f.guides[4].wrapper[0xBE]==1);
            appearance(f.view.data(),1);CHECK(f.appears(4)&&!f.disappears(4)&&f.guides[4].wrapper[0xBE]==0);
            CHECK(f.appears(3)&&f.appears(5));
        }
        appearance(f.view.data(),0);interaction(f.view.data(),0);CHECK(f.event[0xF2]==0&&f.guides[4].wrapper[0xBE]==1);
    }
    std::cout<<"PASS actual-code hook/trampoline integration: activation before event list, 300 hide/reappear cycles, three cache-family sentinels, hold/cache bytes unchanged by repair\n";
    for(U8 mode:{0,2,3,4})for(U8 on:{0,1}){
        std::array<unsigned,18> expected;
        {Fixture f(mode);behavior.originalInteraction(f.view.data(),on);behavior.originalAppearance(f.view.data(),on);expected=f.summary();}
        {Fixture f(mode);interaction(f.view.data(),on);appearance(f.view.data(),on);CHECK(f.summary()==expected);}
    }
    std::cout<<"PASS non-gameplay cinema modes and their disabled paths match unmodified native results\n";
    {Fixture f;interaction(f.view.data(),1);put32(f.guides[4].action.data()+0x2C,0x12345678);U32 repairs=behavior.stats.appearanceRepairs;
        appearance(f.view.data(),1);CHECK(behavior.stats.appearanceRepairs==repairs&&f.disappears(4));}
    {Fixture f;interaction(f.view.data(),1); // A separately hidden sibling row must not be forced visible.
        f.guides[3].wrapper[0xBE]=1;U32 repairs=behavior.stats.appearanceRepairs;appearance(f.view.data(),1);CHECK(behavior.stats.appearanceRepairs==repairs);}
    CHECK(behavior.stats.appearanceVerified==behavior.stats.appearanceRepairs);
    CHECK(behavior.stats.blocks>=2&&activations>=3&&deactivations>=3);
    std::cout<<"PASS action identity / hidden-row guards and verified root-style observations\n";
    std::cout<<"TOTAL assertions: "<<checks<<"; repairs: "<<behavior.stats.appearanceRepairs<<"; verified: "<<behavior.stats.appearanceVerified<<"\n";
    std::cout<<"LIMITATION: Windows loader, actual renderer/fonts, physical controller input, and in-game appearance are NOT tested.\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}}
