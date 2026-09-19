// PE-level smoke/fault-injection test of the ACTUAL compiled ASI under Linux.
// Win32 services are explicit mocks, not Wine or a real Windows loader.
#define main source_harness_main
#include "native_harness.cpp"
#undef main
#include <map>
#include <memory>
#include <string>
#include <filesystem>
#include "../source/Version.h"
using W32=unsigned int;using W64=unsigned long long;
struct WMemory {void*base;void*allocation;W32 allocationProtect;unsigned short partition,pad;W64 size;W32 state,protect,type,pad2;};
struct WThread {W32 size,usage,id,owner,priority,delta,flags;};
static_assert(sizeof(WMemory)==48&&sizeof(WThread)==28);
struct Range {U8*b;size_t size;};
static std::vector<Range> additional;
static std::map<void*,size_t> virtualAllocations;
static std::map<void*,bool> registeredTables;
static std::map<std::string,std::string> files;
static std::map<void*,std::string> fileHandles;
static W32 lastError=0,currentThread=777,enumeration=0,allocationsCalled=0,unwindCalled=0,contextCalled=0,cacheCalls=0;
static W32 writesCalled=0, movesCalled=0, fileCounter=0, mutexCalls=0, sleepsCalled=0;
static int suspendedCount=0;static bool injected=false,gameRan=false;
static std::string scenario;
static void*asiBase=nullptr;static W32(NATIVE_ABI*worker)(void*)=nullptr;
static int(NATIVE_ABI*dllMain)(void*,W32,void*)=nullptr;
static std::string narrow(const char16_t*p){std::string s;while(*p)s.push_back((char)*p++);return s;}
static std::unique_ptr<Memory> loadPE(const char*path,bool reloc){
    std::ifstream in(path,std::ios::binary);std::vector<U8>f((std::istreambuf_iterator<char>(in)),{});CHECK(f.size()>4096);
    U32 nt=u32(f.data()+0x3C),op=nt+24,n=u16(f.data()+nt+6),os=u16(f.data()+nt+20);auto m=std::make_unique<Memory>(u32(f.data()+op+56));
    std::memcpy(m->b,f.data(),4096);for(U32 i=0;i<n;++i){U8*s=f.data()+op+os+i*40;CHECK(u32(s+20)+u32(s+16)<=f.size());std::memcpy(m->b+u32(s+12),f.data()+u32(s+20),u32(s+16));}
    if(reloc){U64 preferred;std::memcpy(&preferred,m->b+op+24,8);U64 delta=(U64)m->b-preferred;U32 start=u32(m->b+op+112+40),size=u32(m->b+op+112+44),end=start+size;
        for(U32 r=start;r<end;){U32 page=u32(m->b+r),len=u32(m->b+r+4);CHECK(len>=8&&r+len<=end);for(U32 i=8;i<len;i+=2){U16 e=u16(m->b+r+i);if(e>>12==10){U64 v;std::memcpy(&v,m->b+page+(e&4095),8);v+=delta;std::memcpy(m->b+page+(e&4095),&v,8);}else CHECK(e>>12==0);}r+=len;}}
    additional.push_back({m->b,m->n});return m;
}
static void NATIVE_ABI wSleep(W32 ms){
    CHECK(suspendedCount==0);
    CHECK(++sleepsCalled<10);
    if(ms==5000&&!gameRan){gameRan=true;currentThread=888;
        Fixture f;Toggle i=(Toggle)(base+behavior.resolved.interaction),a=(Toggle)(base+behavior.resolved.appearance);
        i(f.view.data(),1);CHECK(f.event[0xF2]==1);auto before=f.guides[4].object;
        a(f.view.data(),1);CHECK(f.appears(4)&&!f.disappears(4));CHECK(!f.guides[4].wrapper[0xBE]);
        CHECK(std::memcmp(before.data()+0xD0,f.guides[4].object.data()+0xD0,0x178)==0);
        for(int cycle=0;cycle<25;++cycle){a(f.view.data(),0);CHECK(f.disappears(4)&&f.guides[4].wrapper[0xBE]);a(f.view.data(),1);CHECK(f.appears(4)&&!f.disappears(4)&&!f.guides[4].wrapper[0xBE]);}
        a(f.view.data(),0);i(f.view.data(),0);CHECK(f.event[0xF2]==0&&f.guides[4].wrapper[0xBE]);
        currentThread=777;CHECK(dllMain(asiBase,0,nullptr)==1);
    }
}
static void* NATIVE_ABI wGetModuleHandle(const char16_t*){return base;}
static int NATIVE_ABI wGetModuleHandleEx(W32 flags,const char16_t*,void**out){CHECK(suspendedCount==0&&flags==5);if(scenario=="pin")return 0;*out=asiBase;return 1;}
static W32 NATIVE_ABI wGetModuleFileName(void*module,char16_t*p,W32 n){
    const char16_t* path=module?u"C:\\TestGame\\ShutUpAndLetMePlay.asi":
        (scenario=="helper"?u"C:\\TestGame\\CrimsonDesertLauncher.exe":
         scenario=="case"?u"C:\\TestGame\\CRIMSONDESERT.EXE":u"C:\\TestGame\\CrimsonDesert.exe");
    W32 len=0;while(path[len])++len;CHECK(n>len);std::memcpy(p,path,(len+1)*2);return len;
}
static void NATIVE_ABI wSystemTime(unsigned short*p){const unsigned short t[]={2026,9,6,19,14,24,33,123};std::memcpy(p,t,16);}
static void* NATIVE_ABI wMutex(void*,int owner,const char16_t*name){
    CHECK(!suspendedCount&&!owner);CHECK(narrow(name)=="Local\\ShutUpAndLetMePlay.Instance.42");++mutexCalls;
    if(scenario=="mutex"){lastError=5;return nullptr;}
    lastError=(scenario=="duplicate"||mutexCalls>1)?183:0;return (void*)57;
}
static int NATIVE_ABI wDeleteFile(const char16_t*name){CHECK(!suspendedCount);return files.erase(narrow(name))?1:0;}

static int NATIVE_ABI wDisableThreadCalls(void*){return 1;}
static void* NATIVE_ABI wAlloc(void*,W64 size,W32 type,W32 protect){CHECK(!suspendedCount&&type==0x3000&&protect==4);++allocationsCalled;if(scenario=="allocation"&&allocationsCalled==2)return nullptr;
    void*p=mmap(nullptr,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);CHECK(p!=MAP_FAILED);virtualAllocations[p]=size;return p;}
static int NATIVE_ABI wFree(void*p,W64 size,W32 type){CHECK(!suspendedCount&&size==0&&type==0x8000);auto it=virtualAllocations.find(p);CHECK(it!=virtualAllocations.end());munmap(p,it->second);virtualAllocations.erase(it);return 1;}
static int NATIVE_ABI wProtect(void*p,W64 n,W32 value,W32*old){
    if(scenario=="protect"&&!injected&&p==base+behavior.resolved.appearance&&value==0x40){injected=true;return 0;}
    *old=0x20;U64 a=(U64)p&~4095ull,end=((U64)p+n+4095)&~4095ull;int prot=PROT_READ;if(value==4||value==0x40)prot|=PROT_WRITE;if(value==0x20||value==0x40)prot|=PROT_EXEC;return mprotect((void*)a,end-a,prot)==0;}
static W64 NATIVE_ABI wQuery(const void*p,WMemory*m,W64 n){CHECK(n==48);U64 a=(U64)p;Range hit{};
    for(auto&r:additional)if(a>=(U64)r.b&&a-(U64)r.b<r.size)hit=r;
    for(auto&r:regions)if(a>=(U64)r.first&&a-(U64)r.first<r.second)hit={(U8*)r.first,r.second};
    for(auto&r:virtualAllocations)if(a>=(U64)r.first&&a-(U64)r.first<r.second)hit={(U8*)r.first,r.second};
    if(!hit.b)return 0;*m={hit.b,hit.b,4,0,0,hit.size,0x1000,4,0x20000,0};return 48;}
static int NATIVE_ABI wFlush(void*,const void*p,W64){++cacheCalls;if(scenario=="flush"&&!injected&&p==base+behavior.resolved.interaction){injected=true;return 0;}return 1;}
static void* NATIVE_ABI wCurrentProcess(){return (void*)-1;}
static W32 NATIVE_ABI wProcessId(){return 42;}
static W32 NATIVE_ABI wThreadId(){return currentThread;}
static W32 NATIVE_ABI wError(){return lastError;}
static void* NATIVE_ABI wCreateThread(void*,W64,W32(NATIVE_ABI*fn)(void*),void*,W32,W32*){worker=fn;return (void*)55;}
static int NATIVE_ABI wClose(void*h){CHECK(!suspendedCount);fileHandles.erase(h);return 1;}
static void* NATIVE_ABI wSnapshot(W32 flags,W32){CHECK(!suspendedCount&&flags==4);enumeration=0;return (void*)56;}
static int NATIVE_ABI wNextThread(void*,WThread*e){CHECK(!suspendedCount&&e->size==28);if(enumeration>=2){lastError=18;return 0;}e->owner=42;e->id=100+enumeration++;return 1;}
static void* NATIVE_ABI wOpenThread(W32 rights,int,W32 id){CHECK(!suspendedCount&&rights==0x4A);return (void*)(U64)id;}
static W32 NATIVE_ABI wSuspend(void*h){if(scenario=="suspend"&&h==(void*)101)return 0xFFFFFFFFu;++suspendedCount;return 0;}
static W32 NATIVE_ABI wResume(void*){CHECK(suspendedCount>0);--suspendedCount;return 1;}
static int NATIVE_ABI wContext(void*h,U8*c){CHECK(suspendedCount>0&&u32(c+48)==0x100001);++contextCalled;if(scenario=="context"&&h==(void*)101)return 0;
    U64 rip=0xDEADBEEF;if(scenario=="busy"&&!injected){injected=true;rip=(U64)base+behavior.resolved.interaction+5;}std::memcpy(c+248,&rip,8);return 1;}
static int NATIVE_ABI wExitCode(void*,W32*c){*c=259;return 1;}
static U8 NATIVE_ABI wAddTable(U8*t,W32 n,U64 b){CHECK(!suspendedCount&&n==1&&u32(t)==0&&u32(t+4)==29&&u32(t+8)==32);CHECK(std::memcmp((void*)(b+32),behavior.resolved.unwind,16)==0);++unwindCalled;if(scenario=="unwind"&&unwindCalled==2)return 0;registeredTables[t]=true;return 1;}
static U8 NATIVE_ABI wDeleteTable(void*t){CHECK(!suspendedCount&&registeredTables.count(t));registeredTables.erase(t);return 1;}
static void* NATIVE_ABI wCreateFile(const char16_t*p,W32 access,W32,void*,W32 disposition,W32,void*){CHECK(!suspendedCount&&access==0x40000000&&disposition==2);if(scenario=="readonly"){lastError=5;return (void*)-1;}void*h=(void*)(U64)(1000+fileCounter++);fileHandles[h]=narrow(p);files[fileHandles[h]].clear();return h;}
static int NATIVE_ABI wWrite(void*h,const void*p,W32 n,W32*out,void*){
    CHECK(!suspendedCount&&fileHandles.count(h));++writesCalled;
    if(scenario=="write"&&writesCalled==1){lastError=29;*out=0;return 0;}
    if(scenario=="zero"&&writesCalled==1){*out=0;return 1;}
    if(scenario=="short")n=std::min(n,17u);
    files[fileHandles[h]].append((const char*)p,n);*out=n;return 1;
}
static int NATIVE_ABI wMove(const char16_t*from,const char16_t*to,W32 flags){
    CHECK(!suspendedCount&&flags==9);++movesCalled;
    if(scenario=="rename"&&movesCalled==1){lastError=5;return 0;}
    files[narrow(to)]=files.at(narrow(from));files.erase(narrow(from));return 1;
}
static void linkImports(U8*p){
    std::map<std::string,void*> api={
#define API(n,f) {n,(void*)f}
      API("GetSystemTime",wSystemTime),API("CreateMutexW",wMutex),API("DeleteFileW",wDeleteFile),API("Sleep",wSleep),API("GetModuleHandleW",wGetModuleHandle),API("GetModuleHandleExW",wGetModuleHandleEx),API("GetModuleFileNameW",wGetModuleFileName),API("DisableThreadLibraryCalls",wDisableThreadCalls),
      API("VirtualAlloc",wAlloc),API("VirtualFree",wFree),API("VirtualProtect",wProtect),API("VirtualQuery",wQuery),API("FlushInstructionCache",wFlush),API("GetCurrentProcess",wCurrentProcess),API("GetCurrentProcessId",wProcessId),API("GetCurrentThreadId",wThreadId),API("GetLastError",wError),API("CreateThread",wCreateThread),API("CloseHandle",wClose),API("CreateToolhelp32Snapshot",wSnapshot),API("Thread32First",wNextThread),API("Thread32Next",wNextThread),API("OpenThread",wOpenThread),API("SuspendThread",wSuspend),API("ResumeThread",wResume),API("GetThreadContext",wContext),API("GetExitCodeThread",wExitCode),API("RtlAddFunctionTable",wAddTable),API("RtlDeleteFunctionTable",wDeleteTable),API("CreateFileW",wCreateFile),API("WriteFile",wWrite),API("MoveFileExW",wMove)
#undef API
    };
    U32 op=u32(p+0x3C)+24,r=u32(p+op+120);for(;u32(p+r);r+=20){CHECK(std::string((char*)p+u32(p+r+12))=="KERNEL32.dll");U32 lookup=u32(p+r),iat=u32(p+r+16);
        for(U32 i=0;;++i){U64 name;std::memcpy(&name,p+lookup+i*8,8);if(!name)break;CHECK(name<0x80000000u);std::string s=(char*)p+name+2;CHECK(api.count(s));void*fn=api.at(s);std::memcpy(p+iat+i*8,&fn,8);}}
}
static void typedShowStub(Memory&m,U32 r,void*fn){
    m.executable(r,64);U8 code[]={0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xD0,0x48,0x83,0xC4,0x30,0x5E,0xC3};std::memcpy(code+2,&fn,8);std::memcpy(m.b+r+23,code,sizeof(code));
}
int main(int argc,char**argv){try{
    CHECK(argc==4||argc==5);scenario=argv[3];auto game=loadPE(argv[1],false);base=game->b;
    CHECK(parseImage(base,behavior.image)&&resolve(behavior.image,behavior.resolved));
    const auto&z=behavior.resolved;U8 saved[2][15];std::memcpy(saved[0],base+z.interaction,15);std::memcpy(saved[1],base+z.appearance,15);
    put32(base+z.symbols[Sym_AppearStyle],0x4321);put32(base+z.symbols[Sym_DisappearStyle],0x4320);
    for(auto r:{std::pair<U32,U32>{z.interaction,484},{z.appearance,2178},{z.setAppearance,267},{z.symbols[Sym_ResetKeyguide],0x149},{z.symbols[Sym_AddStyle],0x99},{z.symbols[Sym_RemoveStyle],0x70}})game->executable(r.first,r.second);
    typedShowStub(*game,z.symbols[Sym_Show],(void*)mockShow);typedShowStub(*game,z.symbols[Sym_Hide],(void*)mockHide);
    for(U32 r:{z.symbols[Sym_InvalidateStyle],z.symbols[Sym_SortStyles],z.symbols[Sym_ClearEvent],0xCB6990u,0xCC0190u,0xCC2A70u,0xCC2100u,0x3EF80B0u})stub(*game,r,(void*)noOperation);
    auto asi=loadPE(argv[2],true);asiBase=asi->b;linkImports(asi->b);U32 op=u32(asi->b+0x3C)+24;
    asi->executable(u32(asi->b+op+20),u32(asi->b+op+4));dllMain=(int(NATIVE_ABI*)(void*,W32,void*))(asi->b+u32(asi->b+op+16));
    if(scenario=="image")base[0]=0;
    if(scenario=="shape")base[z.appearance+100]^=1;
    CHECK(dllMain(asi->b,1,nullptr)==1&&worker);CHECK(worker(nullptr)==0);CHECK(suspendedCount==0);
    CHECK(fileHandles.empty());
    if(scenario=="helper"||scenario=="duplicate"){
        CHECK(!gameRan&&files.empty()&&virtualAllocations.empty()&&registeredTables.empty());
        CHECK(allocationsCalled==0&&unwindCalled==0);
        CHECK(std::memcmp(saved[0],base+z.interaction,15)==0&&std::memcmp(saved[1],base+z.appearance,15)==0);
        std::cout<<"PASS compiled ASI / "<<scenario<<" / assertions="<<checks<<" / no hooks or diagnostics written\n";return 0;
    }
    if(scenario=="readonly"){
        CHECK(gameRan&&files.empty()&&virtualAllocations.size()==2&&registeredTables.size()==2);
        std::cout<<"PASS compiled ASI / readonly / assertions="<<checks<<" / diagnostics failure did not disable gameplay\n";return 0;
    }
    std::string report=files.at("C:\\TestGame\\ShutUpAndLetMePlay_UpdateReport.json");
    std::string log=files.at("C:\\TestGame\\ShutUpAndLetMePlay.log");
    CHECK(files.size()==2);
    CHECK(report.find("\"version\": \"" SULMP_VERSION "\"")!=std::string::npos);
    CHECK(report.find("\"session_id\": \"20260919T142433.123Z-42\"")!=std::string::npos);
    CHECK(log.find("Session: 20260919T142433.123Z-42")!=std::string::npos);
    CHECK(log.find("Process ID: 42")!=std::string::npos);
    CHECK(report.find("last_view")==std::string::npos&&report.find("last_skip_input")==std::string::npos);
    CHECK(report.find("TestGame")==std::string::npos&&log.find("TestGame")==std::string::npos);
    const bool succeeds=scenario=="success"||scenario=="busy"||scenario=="case"||scenario=="short"||scenario=="write"||scenario=="zero"||scenario=="rename"||scenario=="duplicate_after";
    if(succeeds){
        CHECK(report.find("\"revision\": 2")!=std::string::npos&&log.find("Revision: 2")!=std::string::npos);
        CHECK(log.find("Status: active\n")!=std::string::npos);
        CHECK(log.find("Appearance repairs: 26\n")!=std::string::npos);
        if(scenario=="write"||scenario=="zero"||scenario=="rename")CHECK(report.find("\"previous_write_errors\": 1")!=std::string::npos);
        else CHECK(report.find("\"previous_write_errors\": 0")!=std::string::npos);
        if(scenario=="duplicate_after"){
            auto originalFiles=files;CHECK(worker(nullptr)==0);CHECK(files==originalFiles);CHECK(mutexCalls==2);
        }
        CHECK(gameRan&&report.find("\"status\": \"active\"")!=std::string::npos);CHECK(virtualAllocations.size()==2&&registeredTables.size()==2);
        CHECK(report.find("\"native_style_state_verified\": 26")!=std::string::npos);CHECK(report.find("\"last_appear_after\": 1")!=std::string::npos);CHECK(report.find("\"last_disappear_after\": 0")!=std::string::npos);
        CHECK(report.find("\"safety_gate_blocks\": 0")!=std::string::npos);
    }else{
        CHECK(report.find("\"revision\": 1")!=std::string::npos&&log.find("Revision: 1")!=std::string::npos);
        CHECK(log.find("Status: disabled\n")!=std::string::npos);
        CHECK(!gameRan&&report.find("\"status\": \"disabled\"")!=std::string::npos);CHECK(virtualAllocations.empty()&&registeredTables.empty());
        CHECK(std::memcmp(saved[0],base+z.interaction,15)==0&&std::memcmp(saved[1],base+z.appearance,15)==0);
    }
    if(argc==5){
        const auto out=std::filesystem::path(argv[4])/scenario;std::filesystem::create_directories(out);
        std::ofstream(out/"report.json",std::ios::binary)<<report;
        std::ofstream(out/"report.log",std::ios::binary)<<log;
    }
    std::cout<<"PASS compiled ASI / "<<scenario<<" / assertions="<<checks<<" / all discovered threads resumed / "<<(gameRan?"26 appearance repairs verified":"both original native entries retained")<<"\n";
    std::cout<<"Win32 services and lower-level rendering are mocked. Not a Windows or in-game execution.\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}}
