#pragma once
// ABI declarations used here; no SDK or third-party binary dependency.
using HANDLE=void*;using HMODULE=void*;using DWORD=unsigned long;
using BOOL=int;using SIZE_T=unsigned long long;using LONG=long;
struct ThreadEntry {DWORD size,usage,id,owner;LONG priority,delta;DWORD flags;};
struct MemoryInfo {void*base;void*allocationBase;DWORD allocationProtect;unsigned short partition,pad;SIZE_T regionSize;DWORD state,protect,type,pad2;};
struct SystemTime {unsigned short year,month,dayOfWeek,day,hour,minute,second,millisecond;};
static_assert(sizeof(SystemTime)==16,"SYSTEMTIME ABI");
struct RuntimeFunction {unsigned int begin,end,unwind;};
struct alignas(16) ThreadContext {
    unsigned long long home[6];DWORD flags,mxcsr;unsigned short cs,ds,es,fs,gs,ss;DWORD eflags;
    unsigned long long debug[6],registers[16],rip;unsigned char remainder[976];
};
static_assert(sizeof(DWORD)==4&&sizeof(ThreadEntry)==28&&sizeof(MemoryInfo)==48&&sizeof(ThreadContext)==1232,"Windows x64 ABI");
static_assert(__builtin_offsetof(ThreadContext,rip)==248,"CONTEXT.Rip offset");
extern "C" {
__declspec(dllimport) void __stdcall GetSystemTime(SystemTime*);
__declspec(dllimport) HANDLE __stdcall CreateMutexW(void*,BOOL,const wchar_t*);
__declspec(dllimport) BOOL __stdcall DeleteFileW(const wchar_t*);
__declspec(dllimport) void __stdcall Sleep(DWORD);
__declspec(dllimport) HMODULE __stdcall GetModuleHandleW(const wchar_t*);
__declspec(dllimport) BOOL __stdcall GetModuleHandleExW(DWORD,const wchar_t*,HMODULE*);
__declspec(dllimport) DWORD __stdcall GetModuleFileNameW(HMODULE,wchar_t*,DWORD);
__declspec(dllimport) BOOL __stdcall DisableThreadLibraryCalls(HMODULE);
__declspec(dllimport) void* __stdcall VirtualAlloc(void*,SIZE_T,DWORD,DWORD);
__declspec(dllimport) BOOL __stdcall VirtualFree(void*,SIZE_T,DWORD);
__declspec(dllimport) BOOL __stdcall VirtualProtect(void*,SIZE_T,DWORD,DWORD*);
__declspec(dllimport) SIZE_T __stdcall VirtualQuery(const void*,MemoryInfo*,SIZE_T);
__declspec(dllimport) BOOL __stdcall FlushInstructionCache(HANDLE,const void*,SIZE_T);
__declspec(dllimport) HANDLE __stdcall GetCurrentProcess();
__declspec(dllimport) DWORD __stdcall GetCurrentProcessId();
__declspec(dllimport) DWORD __stdcall GetCurrentThreadId();
__declspec(dllimport) DWORD __stdcall GetLastError();
__declspec(dllimport) HANDLE __stdcall CreateThread(void*,SIZE_T,DWORD(__stdcall*)(void*),void*,DWORD,DWORD*);
__declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE);
__declspec(dllimport) HANDLE __stdcall CreateToolhelp32Snapshot(DWORD,DWORD);
__declspec(dllimport) BOOL __stdcall Thread32First(HANDLE,ThreadEntry*);
__declspec(dllimport) BOOL __stdcall Thread32Next(HANDLE,ThreadEntry*);
__declspec(dllimport) HANDLE __stdcall OpenThread(DWORD,BOOL,DWORD);
__declspec(dllimport) DWORD __stdcall SuspendThread(HANDLE);
__declspec(dllimport) DWORD __stdcall ResumeThread(HANDLE);
__declspec(dllimport) BOOL __stdcall GetThreadContext(HANDLE,ThreadContext*);
__declspec(dllimport) BOOL __stdcall GetExitCodeThread(HANDLE,DWORD*);
__declspec(dllimport) unsigned char __stdcall RtlAddFunctionTable(RuntimeFunction*,DWORD,unsigned long long);
__declspec(dllimport) unsigned char __stdcall RtlDeleteFunctionTable(RuntimeFunction*);
__declspec(dllimport) HANDLE __stdcall CreateFileW(const wchar_t*,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
__declspec(dllimport) BOOL __stdcall WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);
__declspec(dllimport) BOOL __stdcall MoveFileExW(const wchar_t*,const wchar_t*,DWORD);
}
