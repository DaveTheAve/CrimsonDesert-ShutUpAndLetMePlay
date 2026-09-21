// Synthetic PE/memory tests only: no game file and no game-code execution.
#include <vector>
#include <array>
#include <random>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include "../source/Resolver.h"
using namespace crimson;
static U32 checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error("image check at line " + std::to_string(__LINE__) + ": " #x);}while(0)
static const U8* memoryBase=nullptr;
static U32 memorySize=0,blockedBegin=0,blockedSize=0;
static bool canRead(const void*p,U32 n){
    U64 a=(U64)p,b=(U64)memoryBase;
    if(!n||a<b||a-b>=memorySize||n>memorySize-(a-b))return false;
    U64 offset=a-b;
    return !blockedSize||offset+n<=blockedBegin||offset>=U64(blockedBegin)+blockedSize;
}
struct Fixture {
    std::vector<U8> bytes=std::vector<U8>(65536,0);
    U32 nt=128,op=152,table=392;
    Fixture(){
        bytes[0]='M';bytes[1]='Z';put32(bytes.data()+60,nt);
        put32(bytes.data()+nt,0x4550);put32(bytes.data()+nt+4,(3u<<16)|0x8664);
        bytes[nt+20]=0xF0;bytes[op]=0x0B;bytes[op+1]=2;
        put32(bytes.data()+op+56,(U32)bytes.size());put32(bytes.data()+op+60,4096);
        put32(bytes.data()+op+108,16);put32(bytes.data()+op+136,0x8000);put32(bytes.data()+op+140,24);
        section(0,".code",0x2000,0x1000,0x60000020);
        section(1,".data",0x4000,0x1000,0x40000040);
        section(2,".more",0x6000,0x1000,0xE0000020);
        put32(bytes.data()+0x8000,0x2020);put32(bytes.data()+0x8004,0x2040);put32(bytes.data()+0x8008,0x8100);
        put32(bytes.data()+0x800C,0x6020);put32(bytes.data()+0x8010,0x6040);put32(bytes.data()+0x8014,0x8104);
    }
    void section(U32 i,const char*name,U32 rva,U32 size,U32 flags){auto*p=bytes.data()+table+i*40;std::memset(p,0,40);
        std::memcpy(p,name,std::min(size_t(8),std::strlen(name)));put32(p+8,size);put32(p+12,rva);put32(p+16,size);put32(p+36,flags);}
    void use(){memoryBase=bytes.data();memorySize=(U32)bytes.size();blockedBegin=blockedSize=0;}
    bool load(Image&im){use();return loadImage(bytes.data(),im,canRead);}
};
static U32 naive(const Image&im,const Signature&s){
    if(!s.size||!s.bytes||!s.mask)return 0;bool any=false;
    for(U32 i=0;i<s.size;++i)any|=s.mask[i]!=0;if(!any)return 0;
    U32 found=0;
    for(U32 i=0;i<im.sectionCount;++i){const auto&v=im.sections[i];if(!v.executable()||v.size<s.size)continue;
        for(U32 j=0;j<=v.size-s.size;++j)if(masked(im.base+v.rva+j,s)){if(found)return 0;found=v.rva+j;}}
    return found;
}
int main(){try{
    const U8 pattern[]={0xAB,0xCD,0xEF,0x01},mask[]={255,255,255,255};Signature sig={pattern,mask,4,nullptr,0};
    for(const char*name:{".code",".text",".sbss",".random","abcdefgh",".data"}){
        Fixture f;f.section(0,name,0x2000,0x1000,0x60000020);std::memcpy(f.bytes.data()+0x2100,pattern,4);
        Image im;CHECK(f.load(im));CHECK(im.executableCount==2);CHECK(unique(im,sig)==0x2100);
    }
    {Fixture f;f.section(1,".code",0x4000,0x1000,0x40000040);std::memcpy(f.bytes.data()+0x4100,pattern,4);
        Image im;CHECK(f.load(im));CHECK(unique(im,sig)==0);CHECK(!im.code(0x4100));
        std::memcpy(f.bytes.data()+0x6100,pattern,4);CHECK(unique(im,sig)==0x6100);
        std::memcpy(f.bytes.data()+0x2100,pattern,4);CHECK(unique(im,sig)==0);}
    {Fixture f;f.section(0,".code",0x2000,0x1000,0x40000020);f.section(2,".more",0x6000,0x1000,0x40000020);
        Image im;CHECK(!f.load(im));CHECK(std::string(im.failure)=="no executable PE sections");}
    {Fixture f;f.section(0,".data",0x2000,0x1000,0x60000040);Image im;CHECK(f.load(im));CHECK(im.code(0x2000,0x1000));
        CHECK(!im.code(0x2FFF,2));std::memcpy(f.bytes.data()+0x2FFE,pattern,4);CHECK(unique(im,sig)==0);
        std::memcpy(f.bytes.data()+0x2FFC,pattern,4);CHECK(unique(im,sig)==0x2FFC);}
    {Fixture f;put32(f.bytes.data()+f.table+8,0);Image im;CHECK(f.load(im));CHECK(im.sections[0].size==0x1000);}
    {Fixture f;std::swap_ranges(f.bytes.begin()+f.table,f.bytes.begin()+f.table+40,f.bytes.begin()+f.table+80);
        Image im;CHECK(f.load(im));CHECK(im.code(0x2000));CHECK(im.code(0x6000));}
    {Fixture f;Image im;f.use();blockedBegin=0;blockedSize=64;CHECK(!loadImage(f.bytes.data(),im,canRead));CHECK(std::string(im.failure)=="DOS header is not readable");
        f.use();blockedBegin=f.op;blockedSize=240;CHECK(!loadImage(f.bytes.data(),im,canRead));CHECK(std::string(im.failure)=="optional header or section table is not readable");
        f.use();blockedBegin=0x2000;blockedSize=4096;CHECK(!loadImage(f.bytes.data(),im,canRead));CHECK(im.failedSection==0);
        f.use();blockedBegin=0x6000;blockedSize=4096;CHECK(!loadImage(f.bytes.data(),im,canRead));CHECK(im.failedSection==2);
        f.use();blockedBegin=0x8000;blockedSize=24;CHECK(!loadImage(f.bytes.data(),im,canRead));CHECK(std::string(im.failure)=="exception directory is not readable");
        CHECK(f.load(im));CHECK(im.failedSection==-1&&im.failure==nullptr);}
    {Fixture f;Image im;f.bytes[0]=0;CHECK(!f.load(im));CHECK(std::string(im.failure)=="invalid DOS signature");}
    for(U32 offset:{0u,63u,0xFFFFFFF0u,Image::MaxHeaderBytes-23}){Fixture f;put32(f.bytes.data()+60,offset);Image im;CHECK(!f.load(im));}
    for(U32 count:{0u,97u,65535u}){Fixture f;f.bytes[f.nt+6]=(U8)count;f.bytes[f.nt+7]=(U8)(count>>8);Image im;CHECK(!f.load(im));}
    for(U32 magic:{0u,0x10bu}){Fixture f;f.bytes[f.op]=(U8)magic;f.bytes[f.op+1]=(U8)(magic>>8);Image im;CHECK(!f.load(im));}
    {Fixture f;f.bytes[f.nt+4]=0x4C;f.bytes[f.nt+5]=1;Image im;CHECK(!f.load(im));CHECK(im.machine==0x14C);}
    for(U32 size:{0u,4095u,0x80000000u,0xFFFFFFFFu}){Fixture f;put32(f.bytes.data()+f.op+56,size);Image im;CHECK(!f.load(im));}
    for(U32 size:{0u,100u,65537u}){Fixture f;put32(f.bytes.data()+f.op+60,size);Image im;CHECK(!f.load(im));}
    for(U32 n:{0u,3u,17u,0xFFFFFFFFu}){Fixture f;put32(f.bytes.data()+f.op+108,n);Image im;CHECK(!f.load(im));}
    for(auto setting:{std::pair<U32,U32>{0x2F00,4096},{0xFFFFFFF0u,4096},{0,4096},{0x6000,0xFFFFFFFFu}}){
        Fixture f;f.section(2,".more",setting.first,setting.second,0x60000020);Image im;CHECK(!f.load(im));CHECK(im.failedSection==2);}
    for(U32 size:{0u,23u,0xFFFFFFFFu}){Fixture f;put32(f.bytes.data()+f.op+140,size);Image im;CHECK(!f.load(im));}
    {Fixture f;put32(f.bytes.data()+0x800C,0x2020);Image im;CHECK(!f.load(im));CHECK(std::string(im.failure)=="invalid or unsorted runtime-function table");}
    {Fixture f;put32(f.bytes.data()+0x8008,65535);Image im;CHECK(!f.load(im));}
    {Fixture f;const U32 nt=0x1200;std::memmove(f.bytes.data()+nt,f.bytes.data()+f.nt,240+24+120);
        put32(f.bytes.data()+60,nt);put32(f.bytes.data()+nt+24+60,0x2000);Image im;
        CHECK(f.load(im));CHECK(!parseImage(f.bytes.data(),im,4096));CHECK(f.load(im));}
    {Fixture f;Image im;f.use();CHECK(!parseImage(f.bytes.data(),im,63));CHECK(!parseImage(f.bytes.data(),im,f.table+119));
        CHECK(f.load(im));CHECK(!im.accessible(0xFFFFFFF0u,32));CHECK(relative(im,0xFFFFFFF0u,32,36)==0);
        CHECK(relative(im,0x2000,0xFFFFFFFEu,4)==0);CHECK(relative(im,0x2000,1,0)==0);}
    // Deterministically compare the optimized multi-section scan with a simple
    // exhaustive oracle, including leading/trailing wildcards and duplicates.
    std::mt19937 rng(0x51A9u);U32 scanCases=12000;
    for(U32 t=0;t<scanCases;++t){Fixture f;Image im;CHECK(f.load(im));
        for(auto&v:im.sections){if(!v.size)continue;for(U32 j=0;j<v.size;++j)f.bytes[v.rva+j]=(U8)rng();}
        std::array<U8,64> b{},m{};U32 n=1+rng()%b.size();
        for(U32 j=0;j<n;++j){b[j]=(U8)rng();m[j]=(rng()%3)?255:0;}
        Signature p={b.data(),m.data(),n,nullptr,0};
        if(t%3){U32 r=0x2000+rng()%(4097-n);std::memcpy(f.bytes.data()+r,b.data(),n);
            if(t%7==0)std::memcpy(f.bytes.data()+0x6000,b.data(),n);}
        CHECK(unique(im,p)==naive(im,p));
    }
    std::cout<<"PASS synthetic image/scan assertions: "<<checks<<"; randomized oracle comparisons: "<<scanCases<<"\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
