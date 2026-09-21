#pragma once
// Bounded PE32+ image parsing. Section characteristics, not names, define code.
namespace crimson {
using U8 = unsigned char; using U16 = unsigned short; using U32 = unsigned int;
using U64 = unsigned long long; using I32 = int; using I64 = long long;
static_assert(sizeof(U32)==4 && sizeof(U64)==8 && sizeof(void*)==8,"x64 required");
inline U16 u16(const U8*p){return (U16)(p[0]|(p[1]<<8));}
inline U32 u32(const U8*p){return (U32)p[0]|((U32)p[1]<<8)|((U32)p[2]<<16)|((U32)p[3]<<24);}
inline void put32(U8*p,U32 n){for(unsigned i=0;i<4;++i)p[i]=(U8)(n>>(i*8));}
inline bool same(const void*a,const void*b,unsigned n){const U8*x=(const U8*)a,*y=(const U8*)b;for(unsigned i=0;i<n;++i)if(x[i]!=y[i])return false;return true;}
inline void copy(void*d,const void*s,unsigned n){for(unsigned i=0;i<n;++i)((U8*)d)[i]=((const U8*)s)[i];}
using ImageReadable=bool(*)(const void*,U32);
struct ImageSection {
    U32 rva=0,size=0,characteristics=0;
    char name[9]{};
    bool executable()const{return (characteristics&0x20000000u)!=0;}
    bool contains(U32 r,U32 n)const{return n&&r>=rva&&r-rva<size&&n<=size-(r-rva);}
};
struct Image {
    static constexpr U32 MaxSections=96,MaxHeaderBytes=65536;
    U8*base=nullptr;
    U32 size=0,timestamp=0,exceptionRva=0,exceptionSize=0;
    U32 machine=0,optionalMagic=0,headerSize=0,sectionCount=0,executableCount=0;
    I32 failedSection=-1;
    ImageSection sections[MaxSections]{};
    ImageReadable readable=nullptr;
    const char*stage="not_started";
    const char*failure="image validation not run";
    bool range(U32 r,U32 n)const{return n&&r<size&&n<=size-r;}
    bool accessible(U32 r,U32 n)const{return range(r,n)&&(!readable||readable(base+r,n));}
    bool code(U32 r,U32 n=1)const{
        for(U32 i=0;i<sectionCount;++i)if(sections[i].executable()&&sections[i].contains(r,n))return true;
        return false;
    }
    bool pointer(const void*p,U32 n)const{U64 a=(U64)p,b=(U64)base;return a>=b&&a-b<=0xFFFFFFFFu&&range((U32)(a-b),n);}
    bool fail(const char*reason){failure=reason;return false;}
};
// For file-backed fixtures the caller supplies a mapped image and readable header
// length. Runtime callers use loadImage below, which checks memory before reads.
inline bool parseImage(U8*b,Image&im,U32 headerBytes=4096){
    im=Image{};im.base=b;im.stage="headers";
    if(!b||headerBytes<64)return im.fail("DOS header unavailable");
    if(u16(b)!=0x5A4D)return im.fail("invalid DOS signature");
    U32 nt=u32(b+0x3C);
    if(nt<64||nt>Image::MaxHeaderBytes-24||nt>headerBytes-24)return im.fail("PE header offset outside readable headers");
    if(u32(b+nt)!=0x4550)return im.fail("invalid PE signature");
    im.machine=u16(b+nt+4);im.timestamp=u32(b+nt+8);
    if(im.machine!=0x8664)return im.fail("executable is not AMD64");
    U32 n=u16(b+nt+6),os=u16(b+nt+20);
    if(!n||n>Image::MaxSections)return im.fail("unsupported PE section count");
    if(os<0xF0||os>headerBytes-nt-24)return im.fail("truncated PE32+ optional header");
    U32 table=nt+24+os;
    if(n>(headerBytes-table)/40)return im.fail("section table outside readable headers");
    const U8*o=b+nt+24;im.optionalMagic=u16(o);
    if(im.optionalMagic!=0x20B)return im.fail("executable is not PE32+");
    im.size=u32(o+56);im.headerSize=u32(o+60);
    if(im.size<4096||im.size>0x7FFFFFFFu)return im.fail("invalid executable image size");
    if(im.headerSize<table+n*40||im.headerSize>im.size)return im.fail("invalid PE header size");
    U32 dirs=u32(o+108);
    if(dirs<4||dirs>(os-112)/8)return im.fail("invalid PE data-directory count");
    im.exceptionRva=u32(o+136);im.exceptionSize=u32(o+140);
    im.stage="sections";
    // Validate all section extents before allowing any scan; executable names
    // may legitimately be .text, .code, .sbss or an arbitrary eight-byte value.
    for(U32 i=0;i<n;++i){
        const U8*s=b+table+40*i;auto&v=im.sections[i];im.sectionCount=i+1;
        copy(v.name,s,8);v.name[8]=0;
        v.rva=u32(s+12);v.size=u32(s+8);if(!v.size)v.size=u32(s+16);
        v.characteristics=u32(s+36);im.failedSection=(I32)i;
        if(!v.size)continue;
        if(v.rva<im.headerSize||!im.range(v.rva,v.size))return im.fail("section extent outside executable image");
        for(U32 j=0;j<i;++j){const auto&q=im.sections[j];
            if(q.size&&v.rva<q.rva+q.size&&q.rva<v.rva+v.size)return im.fail("overlapping PE sections");}
        if(v.executable())++im.executableCount;
    }
    im.failedSection=-1;
    if(!im.executableCount)return im.fail("no executable PE sections");
    im.stage="exception_directory";
    if(!im.range(im.exceptionRva,im.exceptionSize)||im.exceptionSize%12)
        return im.fail("invalid or missing exception directory");
    im.stage="parsed";im.failure=nullptr;return true;
}
inline bool validateImageMemory(Image&im,ImageReadable readable){
    im.readable=readable;im.stage="executable_sections";
    for(U32 i=0;i<im.sectionCount;++i){const auto&s=im.sections[i];
        if(s.executable()&&s.size&&!im.accessible(s.rva,s.size)){
            im.failedSection=(I32)i;return im.fail("executable section is not fully readable");}}
    im.failedSection=-1;im.stage="exception_directory";
    if(!im.accessible(im.exceptionRva,im.exceptionSize))return im.fail("exception directory is not readable");
    U32 previous=0;
    for(U32 i=0;i<im.exceptionSize/12;++i){const U8*e=im.base+im.exceptionRva+i*12;
        U32 begin=u32(e),end=u32(e+4),unwind=u32(e+8);
        if(!begin||begin<=previous||end<=begin||!im.range(begin,end-begin)||!im.range(unwind,4))
            return im.fail("invalid or unsorted runtime-function table");
        previous=begin;
    }
    im.stage="ready";im.failure=nullptr;return true;
}
inline bool loadImage(U8*b,Image&im,ImageReadable readable){
    im=Image{};im.base=b;im.stage="headers";
    if(!b||!readable||!readable(b,64))return im.fail("DOS header is not readable");
    if(u16(b)!=0x5A4D)return im.fail("invalid DOS signature");
    U32 nt=u32(b+0x3C);
    if(nt<64||nt>Image::MaxHeaderBytes-24)return im.fail("PE header offset exceeds bounded header window");
    if(!readable(b,nt+24))return im.fail("PE file header is not readable");
    U32 n=u16(b+nt+6),os=u16(b+nt+20);
    if(!n||n>Image::MaxSections)return im.fail("unsupported PE section count");
    U32 bytes=nt+24+os+n*40;
    if(bytes>Image::MaxHeaderBytes)return im.fail("PE headers exceed bounded header window");
    if(!readable(b,bytes))return im.fail("optional header or section table is not readable");
    if(!parseImage(b,im,bytes))return false;
    return validateImageMemory(im,readable);
}
}
