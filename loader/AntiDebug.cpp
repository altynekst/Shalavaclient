#include "AntiDebug.h"
#include "antifrida.h"
#include "AdvancedProtection.h"
#include <windows.h>
#include <intrin.h>
#include <tlhelp32.h>
#include "HardenedProtection.h"

namespace AntiDebug
{

static volatile uint32_t g_Score = 0;
static volatile uint32_t g_ExpectedScore = 0;
static volatile bool g_Initialized = false;

static uint32_t CheckPEB()
{
    uint32_t score = 0x11111111;
    
#if defined(_WIN64)
    __try {
        PVOID peb = (PVOID)__readgsqword(0x60);
        BYTE beingDebugged = *((BYTE*)peb + 2);
        DWORD ntGlobalFlag = *((DWORD*)((BYTE*)peb + 0xBC));
        
        if (beingDebugged == 0)
            score ^= 0x12345678;
        
        if ((ntGlobalFlag & 0x70) == 0)
            score ^= 0x87654321;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {}
#else
    __try {
        PVOID peb = (PVOID)__readfsdword(0x30);
        BYTE beingDebugged = *((BYTE*)peb + 2);
        DWORD ntGlobalFlag = *((DWORD*)((BYTE*)peb + 0x68));
        
        if (beingDebugged == 0)
            score ^= 0x12345678;
        
        if ((ntGlobalFlag & 0x70) == 0)
            score ^= 0x87654321;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {}
#endif

    return score;
}

static uint32_t CheckDebuggerAPI()
{
    uint32_t score = 0x22222222;
    
    if (!IsDebuggerPresent())
        score ^= 0xABCDEF01;
    
    BOOL remoteDebugger = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &remoteDebugger);
    if (!remoteDebugger)
        score ^= 0x10FEDCBA;
    
    return score;
}

static uint32_t CheckHardwareBP()
{
    uint32_t score = 0x44444444;
    
    __try {
        CONTEXT ctx = {};
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        
        if (GetThreadContext(GetCurrentThread(), &ctx))
        {
            if (ctx.Dr0 == 0 && ctx.Dr1 == 0 && ctx.Dr2 == 0 && ctx.Dr3 == 0)
                score ^= 0x13579BDF;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {}
    
    return score;
}

static uint32_t CheckTiming()
{
    uint32_t score = 0x55555555;
    
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    
    volatile int x = 0;
    for (int i = 0; i < 100; i++)
        x += i * i;
    
    QueryPerformanceCounter(&end);
    
    double ms = (double)(end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
    if (ms < 50.0)
        score ^= 0x2468ACE0;
    
    return score;
}

static uint32_t CheckHooks()
{
    uint32_t score = 0x66666666;
    
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll)
        return score;
    
    const char* funcNames[] = {
        "NtQueryInformationProcess",
        "NtSetInformationThread",
        "NtQuerySystemInformation"
    };
    
    bool allClean = true;
    
    for (int i = 0; i < 3; i++)
    {
        BYTE* fn = (BYTE*)GetProcAddress(ntdll, funcNames[i]);
        if (fn)
        {
            // Check for common hook signatures
            if (fn[0] == 0xE9 || fn[0] == 0xEB)  // JMP
                allClean = false;
            if (fn[0] == 0xFF && fn[1] == 0x25)  // JMP [addr]
                allClean = false;
        }
    }
    
    if (allClean)
        score ^= 0xDEADBEEF;
    
    return score;
}

static uint32_t CheckMemoryRegions()
{
    uint32_t score = 0x77777777;
    
    MEMORY_BASIC_INFORMATION mbi;
    BYTE* addr = nullptr;
    int suspiciousCount = 0;
    
    while (VirtualQuery(addr, &mbi, sizeof(mbi)))
    {
        if (mbi.Type == MEM_PRIVATE && 
            mbi.State == MEM_COMMIT &&
            (mbi.Protect & PAGE_EXECUTE_READWRITE) &&
            mbi.RegionSize > 0x10000)
        {
            suspiciousCount++;
        }
        
        addr = (BYTE*)mbi.BaseAddress + mbi.RegionSize;
        if ((SIZE_T)addr < (SIZE_T)mbi.BaseAddress)
            break;
    }
    
    if (suspiciousCount < 3)
        score ^= 0xCAFEBABE;
    
    return score;
}

void Init()
{
    if (g_Initialized)
        return;
    
    HardenedProtection::Initialize();
    
    g_ExpectedScore = 0x11111111 ^ 0x12345678 ^ 0x87654321;
    g_ExpectedScore ^= 0x22222222 ^ 0xABCDEF01 ^ 0x10FEDCBA;
    g_ExpectedScore ^= 0x44444444 ^ 0x13579BDF;
    g_ExpectedScore ^= 0x55555555 ^ 0x2468ACE0;
    g_ExpectedScore ^= 0x66666666 ^ 0xDEADBEEF;
    g_ExpectedScore ^= 0x77777777 ^ 0xCAFEBABE;
    
    g_Initialized = true;
    
    HideThread();
    AdvancedProtection::InitializeVectoredExceptionHandling();
    
    HardenedProtection::ProtectMemory();
}

void HideThread()
{
    typedef NTSTATUS(NTAPI* pNtSetInformationThread)(HANDLE, ULONG, PVOID, ULONG);
    
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll)
        return;
    
    auto fn = (pNtSetInformationThread)GetProcAddress(ntdll, "NtSetInformationThread");
    if (fn)
    {
        // ThreadHideFromDebugger = 0x11
        fn(GetCurrentThread(), 0x11, nullptr, 0);
    }
}

uint32_t GetEnvironmentScore()
{
    uint32_t score = 0;
    
    score ^= CheckPEB();
    score ^= CheckDebuggerAPI();
    score ^= CheckHardwareBP();
    score ^= CheckTiming();
    score ^= CheckHooks();
    score ^= CheckMemoryRegions();
    
    if (AntiFrida::IsDetected())
        score ^= 0xFFFFFFFF;
    
    g_Score = score;
    return score;
}

void ContributeToKey(uint8_t* keyBuffer, size_t keyLen)
{
    if (!g_Initialized)
        Init();
    
    uint32_t score = GetEnvironmentScore();
    uint32_t diff = score ^ g_ExpectedScore;
    
    for (size_t i = 0; i < keyLen && i < 16; i++)
    {
        keyBuffer[i] ^= ((uint8_t*)&diff)[i % 4];
    }
    
    if (diff != 0)
    {
        for (size_t i = 0; i < keyLen; i++)
        {
            keyBuffer[i] ^= 0xFF;
        }
    }
}

bool IsClean()
{
    if (!g_Initialized)
        Init();
    
    if (!HardenedProtection::IsEnvironmentSafe())
        return false;
    
    if (AntiFrida::IsDetected())
        return false;
    
    return GetEnvironmentScore() == g_ExpectedScore;
}

bool RunAllChecks()
{
    if (!g_Initialized)
        Init();
    
    return GetEnvironmentScore() == g_ExpectedScore;
}

std::string GetDynamicChallengeSecret()
{
    std::string secret;
    secret.reserve(16);
    
    uint32_t score = GetEnvironmentScore();
    uint32_t timeSeed = (uint32_t)GetTickCount();
    uint32_t processId = GetCurrentProcessId();
    
    for (int i = 0; i < 16; i++)
    {
        uint32_t mix = score ^ timeSeed ^ processId ^ (i * 0x9E3779B9);
        mix = (mix << 13) | (mix >> 19);
        mix ^= (mix << 7) | (mix >> 25);
        
        BYTE byte = (BYTE)(mix & 0xFF);
        if (byte == 0) byte = 0x42;
        secret.push_back((char)byte);
    }
    
    return secret;
}

void PerformAdvancedAntiDebug()
{
    if (!HardenedProtection::CheckCodeIntegrity())
        HardenedProtection::ScrambleAndDie();
    
    if (!HardenedProtection::CheckParentProcess())
        HardenedProtection::ScrambleAndDie();
    
    if (!HardenedProtection::CheckDebuggerProcesses())
        HardenedProtection::ScrambleAndDie();
    
    if (AntiFrida::IsDetected())
        AntiFrida::TriggerAntiUnpack();
    
    if (!AdvancedProtection::PerformAdvancedSecurityChecks())
        AntiFrida::TriggerAntiUnpack();
    
    if (!HardenedProtection::IsEnvironmentSafe())
        HardenedProtection::ScrambleAndDie();
}

}