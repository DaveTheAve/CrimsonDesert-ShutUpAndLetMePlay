// Read-only, fail-closed resolver shared by the ASI and offline tests.
#pragma once
#include "Image.h"
#include "VerifiedSignatures.h"
#include "New3479Signatures.h"
namespace crimson {
inline bool masked(const U8*p,const Signature&s){for(U32 j=0;j<s.size;++j)if(s.mask[j]&&p[j]!=s.bytes[j])return false;return true;}
inline U32 unique(const Image&im,const Signature&s){
    if(!s.size||!s.bytes||!s.mask)return 0;
    // Search the longest fixed-byte run with a bounded Horspool skip table.
    // Full masks still validate every candidate; uniqueness spans ALL executable
    // sections, rather than accepting the first section with a familiar name.
    U32 anchor=0,length=0;
    for(U32 i=0;i<s.size;){if(!s.mask[i]){++i;continue;}U32 first=i;
        while(i<s.size&&s.mask[i])++i;
        if(i-first>length){anchor=first;length=i-first;}}
    if(!length)return 0;
    U32 shift[256];for(U32 i=0;i<256;++i)shift[i]=length;
    for(U32 i=0;i+1<length;++i)shift[s.bytes[anchor+i]]=length-1-i;
    U32 found=0,last=anchor+length-1;
    for(U32 si=0;si<im.sectionCount;++si){const auto&section=im.sections[si];
        if(!section.executable()||s.size>section.size)continue;
        U32 limit=section.size-s.size;
        for(U32 i=0;i<=limit;){const U8*p=im.base+section.rva+i;U8 tail=p[last];
            if(tail==s.bytes[last]&&same(p+anchor,s.bytes+anchor,length)&&masked(p,s)){
                if(found)return 0;found=section.rva+i;}
            U32 step=shift[tail];if(step>limit-i)break;i+=step;
        }
    }
    return found;
}
inline U32 relative(const Image&im,U32 instruction,U32 displacement,U32 next){
    if(displacement>0xFFFFFFFBu||!im.accessible(instruction,displacement+4)||!im.range(instruction,next))return 0;
    I64 t=(I64)instruction+next+(I32)u32(im.base+instruction+displacement);
    return t>0 && t<im.size?(U32)t:0;
}
struct Resolved {
    U32 interaction=0,appearance=0,setAppearance=0,skipSemantic=0;
    U8 profile=0; // 0: original validated layout; 1: refactored cinematic/dialogue layout
    U8 appearanceUnwind[8]{};
    // Populated only after the selected native profile has been validated.
    U32 guideActionOffset=0x1F8,guideFamilyOffset=0x200,guideSubtypeOffset=0x201;
    U32 guideBinding=0,guideRefresh=0;
    U32 symbols[SymbolCount]{}; U8 unwind[16]{};
    const char*failure="not resolved";
};
inline bool references(const Image&im,U32 r,const Signature&s,Resolved&z){
    for(U32 i=0;i<s.refCount;++i){const auto&q=s.refs[i];U32 t=relative(im,r,q.displacement,q.next);
        if(!t)return false; U32&v=z.symbols[q.symbol]; if(v&&v!=t)return false;v=t;}
    return true;
}
inline bool exactString(const Image&im,U32 r,const char*s){U32 i=0;for(;s[i];++i)if(!im.accessible(r+i,1)||im.base[r+i]!=(U8)s[i])return false;return im.accessible(r+i,1)&&im.base[r+i]==0;}
inline bool validateNames(const Image&im,const Resolved&z){
    // Match the real intern-registration call: (global, string, 1, 0x2ffff).
    const U8 lead[]={0x41,0xB9,0xFF,0xFF,0x02,0x00,0x48,0x8D,0x15};
    const U8 mid[]={0x41,0xB8,1,0,0,0,0x48,0x8D,0x0D};
    U32 a=0,d=0,constructor=0;
    for(U32 si=0;si<im.sectionCount;++si){const auto&section=im.sections[si];
      if(!section.executable()||section.size<31)continue;
      for(U32 i=0;i<=section.size-31;++i){U32 r=section.rva+i;const U8*p=im.base+r;
        if(p[0]!=lead[0]||!same(p,lead,9)||!same(p+13,mid,9)||p[26]!=0xE9)continue;
        U32 global=relative(im,r,22,26);if(global!=z.symbols[Sym_AppearStyle]&&global!=z.symbols[Sym_DisappearStyle])continue;
        U32 text=relative(im,r,9,13),ctor=relative(im,r,27,31);
        if(!im.code(ctor,16)||(constructor&&constructor!=ctor))return false;constructor=ctor;
        if(global==z.symbols[Sym_AppearStyle]){if(!exactString(im,text,"cpp-appear-keyguide"))return false;++a;}
        else{if(!exactString(im,text,"cpp-disappear-keyguide"))return false;++d;}
    }}return a==1&&d==1&&z.symbols[Sym_AppearStyle]!=z.symbols[Sym_DisappearStyle];
}
inline bool validateUnwind(const Image&im,U32 r,U32 length,U8*out){
    const U8 expected[]={1,15,6,0,15,0x64,7,0,15,0x34,6,0,15,0x32,11,0x70};
    // Register a matching unwind record for the copied 15-byte prologue.
    U32 lo=0,hi=im.exceptionSize/12;
    while(lo<hi){U32 m=lo+(hi-lo)/2;const U8*e=im.base+im.exceptionRva+12*m;U32 begin=u32(e);
        if(begin<r)lo=m+1;else hi=m;}
    if(lo>=im.exceptionSize/12)return false;const U8*e=im.base+im.exceptionRva+12*lo;U32 uw=u32(e+8);
    if(u32(e)!=r||u32(e+4)!=r+length||!im.accessible(uw,16)||!same(im.base+uw,expected,16))return false;
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
    if(!im.code(z.symbols[Sym_Show],sizeof(show))||!im.code(z.symbols[Sym_Hide],sizeof(hide))||
       !same(im.base+z.symbols[Sym_Show],show,sizeof(show))||!same(im.base+z.symbols[Sym_Hide],hide,sizeof(hide))||!validateNames(im,z)){
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

// Cross-check exact current native function boundaries and unwind operations,
// not a file timestamp or a fixed RVA. Both profiles still use unique masked
// patterns and must agree about the linked native symbols.
inline bool newUnwind(const Image&im,U32 r,U32 length,const U8*bytes,U32 size){
    U32 lo=0,hi=im.exceptionSize/12;
    while(lo<hi){U32 mid=lo+(hi-lo)/2;const U8*entry=im.base+im.exceptionRva+12*mid;
        if(u32(entry)<r)lo=mid+1;else hi=mid;}
    if(lo==im.exceptionSize/12)return false;
    const U8*entry=im.base+im.exceptionRva+12*lo;
    U32 uw=u32(entry+8);
    return u32(entry)==r&&u32(entry+4)==r+length&&im.accessible(uw,size)&&same(im.base+uw,bytes,size);
}

inline bool newCall(const Image&im,U32 r,U32 offset,U32 dest){
    return im.code(r+offset,5)&&im.base[r+offset]==0xE8&&
        relative(im,r+offset,1,5)==dest;
}

// Derive the current keyguide fields from independently scanned, complete
// native binding/cache methods. No fallback probing of arbitrary object slots.
// Structure displacements remain unmasked, so an unsupported layout fails closed.
inline bool resolveGuideLayout3479(const Image&im,Resolved&z){
    z.guideBinding=unique(im,NewGuideBindingSignature);
    z.guideRefresh=unique(im,NewGuideRefreshSignature);
    if(!z.guideBinding||!z.guideRefresh){
        z.failure="updated keyguide binding/cache shape missing or ambiguous";return false;
    }
    const U8 bindingUw[]={0x11,0x14,0x08,0x00,0x14,0x64,0x0B,0x00,0x14,0x54,0x0A,0x00,0x14,0x34,0x09,0x00,0x14,0x52,0x10,0x70};
    const U8 refreshUw[]={0x01,0x21,0x0C,0x00,0x21,0x64,0x0D,0x00,0x21,0x54,0x0B,0x00,0x21,0x34,0x0A,0x00,0x21,0x32,0x1D,0xF0,0x1B,0xE0,0x19,0xD0,0x17,0xC0,0x15,0x70};
    if(!newUnwind(im,z.guideBinding,NewGuideBindingSignature.size,bindingUw,sizeof(bindingUw))||
       !newUnwind(im,z.guideRefresh,NewGuideRefreshSignature.size,refreshUw,sizeof(refreshUw))||
       !newCall(im,z.guideBinding,0x195,z.guideRefresh)){
        z.failure="updated keyguide binding/cache link or unwind changed";return false;
    }
    U32 lookup=relative(im,z.guideBinding,0x102,0x106);
    if(!im.code(lookup,5)||!newCall(im,z.guideRefresh,0xBD,lookup)||
       !newCall(im,z.guideRefresh,0x14F,lookup)){
        z.failure="updated keyguide action lookup references disagree";return false;
    }
    U32 action=u32(im.base+z.guideBinding+0x97);
    U32 family=u32(im.base+z.guideBinding+0x18E);
    U32 subtype=u32(im.base+z.guideBinding+0x187);
    if(action!=0x1E8||family!=0x1F0||subtype!=0x1F1||
       u32(im.base+z.guideRefresh+0xA9)!=action||
       u32(im.base+z.guideRefresh+0x181)!=family||
       u32(im.base+z.guideRefresh+0x18B)!=subtype||
       im.base[z.guideRefresh+0xB9]!=0x2C){
        z.failure="updated keyguide action/cache field layout disagrees";return false;
    }
    z.guideActionOffset=action;z.guideFamilyOffset=family;z.guideSubtypeOffset=subtype;
    return true;
}

inline bool resolve3479(const Image&im,Resolved&z){
    z=Resolved{};z.profile=1;
    z.interaction=unique(im,NewInteractionSignature);
    z.appearance=unique(im,NewAppearanceSignature);
    z.setAppearance=unique(im,NewSetAppearanceSignature);
    U32 input=unique(im,NewInputSignature);
    if(!z.interaction||!z.appearance||!z.setAppearance||!input){
        z.failure="updated native cinematic/input shape missing or ambiguous";return false;
    }
    // Fixed structure: the same state facade is read in both native row paths.
    U32 manager=relative(im,z.interaction,0x1D,0x21);
    U32 manager2=relative(im,z.appearance,0x14,0x18);
    U32 show=relative(im,z.appearance,0x49,0x4D);
    U32 hide=relative(im,z.appearance,0x55,0x59);
    U32 appear=relative(im,z.setAppearance,0xE6,0xEA);
    U32 disappear=relative(im,z.setAppearance,0x19E,0x1A2);
    U32 duration=relative(im,z.appearance,0xDA,0xDE);
    U32 reset=relative(im,z.appearance,0x19C,0x1A0);
    if(!manager||manager!=manager2||!show||!hide||!appear||!disappear||!duration||!reset ||
       appear==disappear||!im.accessible(appear,4)||!im.accessible(disappear,4)||
       !im.code(show,23)||!im.code(hide,23)||!im.code(reset,16)||!im.accessible(manager,8)){
        z.failure="updated UI native references disagree";return false;
    }
    z.symbols[Sym_GameManager]=manager;
    z.symbols[Sym_Show]=show;z.symbols[Sym_Hide]=hide;
    z.symbols[Sym_AppearStyle]=appear;z.symbols[Sym_DisappearStyle]=disappear;
    z.symbols[Sym_ResetKeyguide]=reset;z.symbols[Sym_AppearanceDuration]=duration;
    // Verify the exact native row call order and private style-vector setter.
    if(!newCall(im,z.appearance,0x48,show)||!newCall(im,z.appearance,0x54,hide)||
       !newCall(im,z.appearance,0x6E,z.setAppearance)||
       !newCall(im,z.appearance,0x19B,reset)||
       !newCall(im,z.interaction,0x4D,show)||
       !newCall(im,z.interaction,0x59,hide)){
        z.failure="updated native Show/Hide/style call order disagrees";return false;
    }
    const U8 showBytes[]={0x40,0x56,0x48,0x83,0xEC,0x30,0x80,0xB9,0xBD,0,0,0,0,0x48,0x8B,0xF1,0xC6,0x81,0xBE,0,0,0,0};
    U8 hideBytes[sizeof(showBytes)];copy(hideBytes,showBytes,sizeof(showBytes));hideBytes[8]=0xBE;hideBytes[18]=0xBD;
    if(!same(im.base+show,showBytes,sizeof(showBytes))||
       !same(im.base+hide,hideBytes,sizeof(hideBytes))||!validateNames(im,z)){
        z.failure="updated native widget semantics or keyguide style names differ";return false;
    }
    // Normal and fixed-row Skip are still handled by the native input path.
    z.skipSemantic=input+0x168;
    if(!im.code(z.skipSemantic,42)||!newCall(im,input,0x18D,z.appearance)||
       !newCall(im,input,0x12C,z.appearance)||!newCall(im,input,0x208,z.appearance)||
       im.base[z.skipSemantic]!=0x49||im.base[z.skipSemantic+1]!=0x3B||
       im.base[z.skipSemantic+8]!=0x74||im.base[z.skipSemantic+18]!=0x0F){
        z.failure="updated native Skip input no longer calls validated cinematic appearance";return false;
    }
    const U8 interactionUw[]={1,15,6,0,15,0x64,7,0,15,0x34,6,0,15,0x32,11,0x70};
    const U8 appearanceUw[]={1,6,2,0,6,0x32,2,0x30};
    if(!newUnwind(im,z.interaction,NewInteractionSignature.size,interactionUw,sizeof(interactionUw))||
       !newUnwind(im,z.appearance,NewAppearanceSignature.size,appearanceUw,sizeof(appearanceUw))){
        z.failure="updated cinematic prologue or exception layout no longer supported";return false;
    }
    copy(z.unwind,interactionUw,16);copy(z.appearanceUnwind,appearanceUw,8);
    // The appearance entry's initial JE rel32 requires a special trampoline.
    if(!same(im.base+z.appearance,(const U8*)"\x40\x53\x48\x83\xEC\x20\x48\x8B\xD9\x84\xD2\x0F\x84",13)){
        z.failure="updated appearance conditional-branch prelude changed";return false;
    }
    if(!resolveGuideLayout3479(im,z))return false;
    z.failure=nullptr;return true;
}
}
