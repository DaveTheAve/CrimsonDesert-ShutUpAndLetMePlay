// Read-only, fail-closed resolver shared by the ASI and offline tests.
#pragma once
#include "VerifiedSignatures.h"
namespace crimson {
using U8 = unsigned char; using U16 = unsigned short; using U32 = unsigned int;
using U64 = unsigned long long; using I32 = int; using I64 = long long;
static_assert(sizeof(U32)==4 && sizeof(U64)==8 && sizeof(void*)==8,"x64 required");
inline U16 u16(const U8*p){return (U16)(p[0]|(p[1]<<8));}
inline U32 u32(const U8*p){return (U32)p[0]|((U32)p[1]<<8)|((U32)p[2]<<16)|((U32)p[3]<<24);}
inline void put32(U8*p,U32 n){for(unsigned i=0;i<4;++i)p[i]=(U8)(n>>(i*8));}
inline bool same(const void*a,const void*b,unsigned n){const U8*x=(const U8*)a,*y=(const U8*)b;for(unsigned i=0;i<n;++i)if(x[i]!=y[i])return false;return true;}
inline void copy(void*d,const void*s,unsigned n){for(unsigned i=0;i<n;++i)((U8*)d)[i]=((const U8*)s)[i];}
struct Image {
    U8*base=nullptr; U32 size=0,codeRva=0,codeSize=0,timestamp=0,exceptionRva=0,exceptionSize=0;
    bool range(U32 r,U32 n)const{return r<size && n<=size-r;}
    bool code(U32 r,U32 n=1)const{return r>=codeRva && r-codeRva<codeSize && n<=codeSize-(r-codeRva);}
    bool pointer(const void*p,U32 n)const{U64 a=(U64)p,b=(U64)base;return a>=b && a-b<=0xFFFFFFFFu && range((U32)(a-b),n);}
};
// Caller confirms that at least the first header page is readable.
inline bool parseImage(U8*b,Image&im){
    if(!b||u16(b)!=0x5A4D)return false;U32 nt=u32(b+0x3C);
    if(nt>0xC00||u32(b+nt)!=0x4550||u16(b+nt+4)!=0x8664)return false;
    U32 n=u16(b+nt+6),os=u16(b+nt+20); if(os<0xF0||n==0||n>32||nt+24+os+n*40>0x1000)return false;
    const U8*o=b+nt+24; if(u16(o)!=0x20B)return false;
    im.base=b;im.timestamp=u32(b+nt+8);im.size=u32(o+56);
    if(im.size<0x1000||im.size>0x7FFFFFFFu||u32(o+108)<4)return false;
    im.exceptionRva=u32(o+112+24);im.exceptionSize=u32(o+112+28);
    if(!im.range(im.exceptionRva,im.exceptionSize)||im.exceptionSize%12)return false;
    for(U32 i=0;i<n;++i){const U8*s=o+os+40*i;
        const U8 name[]={'.','c','o','d','e',0,0,0};
        if(same(s,name,8)){
            if(im.codeSize)return false;im.codeRva=u32(s+12);im.codeSize=u32(s+8);
            if(!(u32(s+36)&0x20000000u)||!im.range(im.codeRva,im.codeSize))return false;
        }
    }return im.codeSize>0;
}
inline bool masked(const U8*p,const Signature&s){for(U32 j=0;j<s.size;++j)if(s.mask[j]&&p[j]!=s.bytes[j])return false;return true;}
inline U32 unique(const Image&im,const Signature&s){
    U32 found=0;if(s.size>im.codeSize)return 0;
    for(U32 i=0;i<=im.codeSize-s.size;++i){const U8*p=im.base+im.codeRva+i;
        if(p[0]!=s.bytes[0]||p[1]!=s.bytes[1]||p[2]!=s.bytes[2])continue;
        if(masked(p,s)){if(found)return 0;found=im.codeRva+i;}}
    return found;
}
inline U32 relative(const Image&im,U32 instruction,U32 displacement,U32 next){
    if(!im.range(instruction+displacement,4))return 0;
    I64 t=(I64)instruction+next+(I32)u32(im.base+instruction+displacement);
    return t>0 && t<im.size?(U32)t:0;
}
struct Resolved {
    U32 interaction=0,appearance=0,setAppearance=0,skipSemantic=0;
    U32 symbols[SymbolCount]{}; U8 unwind[16]{};
    const char*failure="not resolved";
};
inline bool references(const Image&im,U32 r,const Signature&s,Resolved&z){
    for(U32 i=0;i<s.refCount;++i){const auto&q=s.refs[i];U32 t=relative(im,r,q.displacement,q.next);
        if(!t)return false; U32&v=z.symbols[q.symbol]; if(v&&v!=t)return false;v=t;}
    return true;
}
inline bool exactString(const Image&im,U32 r,const char*s){U32 i=0;for(;s[i];++i)if(!im.range(r+i,1)||im.base[r+i]!=(U8)s[i])return false;return im.range(r+i,1)&&im.base[r+i]==0;}
inline bool validateNames(const Image&im,const Resolved&z){
    // Match the real intern-registration call: (global, string, 1, 0x2ffff).
    const U8 lead[]={0x41,0xB9,0xFF,0xFF,0x02,0x00,0x48,0x8D,0x15};
    const U8 mid[]={0x41,0xB8,1,0,0,0,0x48,0x8D,0x0D};
    U32 a=0,d=0,constructor=0;
    for(U32 i=0;i+31<=im.codeSize;++i){U32 r=im.codeRva+i;const U8*p=im.base+r;
        if(p[0]!=lead[0]||!same(p,lead,9)||!same(p+13,mid,9)||p[26]!=0xE9)continue;
        U32 global=relative(im,r,22,26);if(global!=z.symbols[Sym_AppearStyle]&&global!=z.symbols[Sym_DisappearStyle])continue;
        U32 text=relative(im,r,9,13),ctor=relative(im,r,27,31);
        if(!im.code(ctor,16)||(constructor&&constructor!=ctor))return false;constructor=ctor;
        if(global==z.symbols[Sym_AppearStyle]){if(!exactString(im,text,"cpp-appear-keyguide"))return false;++a;}
        else{if(!exactString(im,text,"cpp-disappear-keyguide"))return false;++d;}
    }return a==1&&d==1&&z.symbols[Sym_AppearStyle]!=z.symbols[Sym_DisappearStyle];
}
inline bool validateUnwind(const Image&im,U32 r,U32 length,U8*out){
    const U8 expected[]={1,15,6,0,15,0x64,7,0,15,0x34,6,0,15,0x32,11,0x70};
    // Register a matching unwind record for the copied 15-byte prologue.
    U32 lo=0,hi=im.exceptionSize/12;
    while(lo<hi){U32 m=lo+(hi-lo)/2;const U8*e=im.base+im.exceptionRva+12*m;U32 begin=u32(e);
        if(begin<r)lo=m+1;else hi=m;}
    if(lo>=im.exceptionSize/12)return false;const U8*e=im.base+im.exceptionRva+12*lo;U32 uw=u32(e+8);
    if(u32(e)!=r||u32(e+4)!=r+length||!im.range(uw,16)||!same(im.base+uw,expected,16))return false;
    copy(out,expected,16);return true;
}
inline bool resolve(const Image&im,Resolved&z){
    z=Resolved{};
    z.interaction=unique(im,InteractionSignature);z.appearance=unique(im,CinemaAppearanceSignature);z.setAppearance=unique(im,SetKeyguideAppearanceSignature);
    if(!z.interaction||!z.appearance||!z.setAppearance){z.failure="native function shape missing or ambiguous; no patches installed";return false;}
    if(!references(im,z.interaction,InteractionSignature,z)||!references(im,z.appearance,CinemaAppearanceSignature,z)||!references(im,z.setAppearance,SetKeyguideAppearanceSignature,z)){
        z.failure="native cross-reference mismatch";return false;}
    for(U32 i=0;i<SymbolCount;++i){bool data=i==Sym_GameManager||i==Sym_DisappearStyle||i==Sym_AppearStyle||i==Sym_AppearanceDuration;
        if(data?!im.range(z.symbols[i],8):!im.code(z.symbols[i],16)){z.failure="native reference outside expected image region";return false;}}
    const U8 show[]={0x40,0x56,0x48,0x83,0xEC,0x30,0x80,0xB9,0xBD,0,0,0,0,0x48,0x8B,0xF1,0xC6,0x81,0xBE,0,0,0,0};
    U8 hide[sizeof(show)];copy(hide,show,sizeof(show));hide[8]=0xBE;hide[18]=0xBD;
    if(!same(im.base+z.symbols[Sym_Show],show,sizeof(show))||!same(im.base+z.symbols[Sym_Hide],hide,sizeof(hide))||!validateNames(im,z)){
        z.failure="Show/Hide type check or literal appearance-name validation failed";return false;}
    const U8 skipBytes[]={0x4D,0x3B,0xB4,0x24,0xF0,2,0,0,0x74,0x0E,0x4D,0x3B,0xB4,0x24,0xC0,2,0,0,0x0F,0x85,0,0,0,0,0x84,0xDB,0x0F,0x85,0,0,0,0,0x33,0xD2,0x49,0x8B,0xCC,0xE8,0,0,0,0};
    U8 mask[sizeof(skipBytes)];for(U32 i=0;i<sizeof(mask);++i)mask[i]=255;
    for(U32 i=20;i<24;++i)mask[i]=0;for(U32 i=28;i<32;++i)mask[i]=0;for(U32 i=38;i<42;++i)mask[i]=0;
    Signature s={skipBytes,mask,sizeof(skipBytes),nullptr,0};z.skipSemantic=unique(im,s);
    if(!z.skipSemantic||relative(im,z.skipSemantic,38,42)!=z.appearance){z.failure="native Skip action does not call the verified cinema appearance routine";return false;}
    U8 uw[16];if(!validateUnwind(im,z.interaction,InteractionSignature.size,z.unwind)||!validateUnwind(im,z.appearance,CinemaAppearanceSignature.size,uw)){
        z.failure="unsupported prologue/unwind layout";return false;}
    z.failure=nullptr;return true;
}
}
