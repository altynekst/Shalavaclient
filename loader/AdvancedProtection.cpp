#include "AdvancedProtection.h"
#include <windows.h>
#include <intrin.h>

namespace AdvancedProtection
{

static bool g_SecurityBreached = false;

static LONG NTAPI VectoredExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo)
{
    if (!ExceptionInfo || !ExceptionInfo->ExceptionRecord)
        return EXCEPTION_CONTINUE_SEARCH;
        
    DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;
    
    if (code == EXCEPTION_BREAKPOINT || code == EXCEPTION_SINGLE_STEP)
    {
        g_SecurityBreached = true;
    }
    
    return EXCEPTION_CONTINUE_SEARCH;
}

void InitializeVectoredExceptionHandling()
{
    AddVectoredExceptionHandler(1, VectoredExceptionHandler);
}

static bool CheckPEBDebugFlag()
{
#if defined(_WIN64)
    // 64-bit: PEB at gs:[0x60], BeingDebugged at offset 0x2
    BYTE beingDebugged = 0;
    __try {
        PVOID peb = (PVOID)__readgsqword(0x60);
        beingDebugged = *((BYTE*)peb + 2);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return beingDebugged != 0;
#else
    // 32-bit
    BYTE beingDebugged = 0;
    __try {
        PVOID peb = (PVOID)__readfsdword(0x30);
        beingDebugged = *((BYTE*)peb + 2);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return beingDebugged != 0;
#endif
}

static bool CheckNtGlobalFlag()
{
#if defined(_WIN64)
    __try {
        PVOID peb = (PVOID)__readgsqword(0x60);
        DWORD ntGlobalFlag = *((DWORD*)((BYTE*)peb + 0xBC));
        // FLG_HEAP_ENABLE_TAIL_CHECK | FLG_HEAP_ENABLE_FREE_CHECK | FLG_HEAP_VALIDATE_PARAMETERS
        if (ntGlobalFlag & 0x70)
            return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {}
#else
    __try {
        PVOID peb = (PVOID)__readfsdword(0x30);
        DWORD ntGlobalFlag = *((DWORD*)((BYTE*)peb + 0x68));
        if (ntGlobalFlag & 0x70)
            return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {}
#endif
    return false;
}

static bool CheckDebuggerAPI()
{
    if (IsDebuggerPresent())
        return true;
    
    BOOL remoteDebugger = FALSE;
    if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &remoteDebugger))
    {
        if (remoteDebugger)
            return true;
    }
    
    return false;
}

static bool CheckHardwareBreakpoints()
{
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    
    if (GetThreadContext(GetCurrentThread(), &ctx))
    {
        if (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0)
            return true;
    }
    
    return false;
}

bool PerformAdvancedSecurityChecks()
{
    if (g_SecurityBreached)
        return false;
    
    if (CheckDebuggerAPI())
        return false;
    
    if (CheckPEBDebugFlag())
        return false;
    
    if (CheckNtGlobalFlag())
        return false;
    
    if (CheckHardwareBreakpoints())
        return false;
    
    return true;
}

void EraseMemoryTraces()
{
    volatile bool* pBreached = &g_SecurityBreached;
    *pBreached = false;
}

}