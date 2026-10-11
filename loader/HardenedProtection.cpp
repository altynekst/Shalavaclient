#include "HardenedProtection.h"
#include "antifrida.h"
#include "obfuscate.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <intrin.h>

#pragma comment(lib, "psapi.lib")

namespace HardenedProtection
{

static volatile uint32_t g_CodeCRC = 0;
static volatile bool g_HeadersErased = false;
static volatile DWORD g_OriginalEntryPoint = 0;
static volatile DWORD g_StartTick = 0;

static uint32_t CalculateCRC32(const BYTE* data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    static uint32_t table[256] = {0};
    static bool tableInit = false;
    
    if (!tableInit)
    {
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int j = 0; j < 8; j++)
                c = (c >> 1) ^ (0xEDB88320 & (-(int)(c & 1)));
            table[i] = c;
        }
        tableInit = true;
    }
    
    for (size_t i = 0; i < length; i++)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    
    return crc ^ 0xFFFFFFFF;
}

void Initialize()
{
    g_StartTick = GetTickCount();
    
    BYTE* base = (BYTE*)GetModuleHandle(NULL);
    if (!base) return;
    
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;
    
    g_OriginalEntryPoint = nt->OptionalHeader.AddressOfEntryPoint;
    
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        if (section[i].Characteristics & IMAGE_SCN_CNT_CODE)
        {
            BYTE* codeStart = base + section[i].VirtualAddress;
            DWORD codeSize = section[i].Misc.VirtualSize;
            g_CodeCRC ^= CalculateCRC32(codeStart, codeSize);
        }
    }
}

void ErasePEHeaders()
{
    if (g_HeadersErased) return;
    
    BYTE* base = (BYTE*)GetModuleHandle(NULL);
    if (!base) return;
    
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    
    DWORD oldProt;
    SIZE_T headerSize = nt->OptionalHeader.SizeOfHeaders;
    
    if (VirtualProtect(base, headerSize, PAGE_READWRITE, &oldProt))
    {
        SecureZeroMemory(base, headerSize);
        VirtualProtect(base, headerSize, oldProt, &oldProt);
        g_HeadersErased = true;
    }
}

void ProtectMemory()
{
    BYTE* base = (BYTE*)GetModuleHandle(NULL);
    if (!base || g_HeadersErased) return;
    
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt);
    
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        if (section[i].Characteristics & IMAGE_SCN_CNT_CODE)
        {
            BYTE* codeStart = base + section[i].VirtualAddress;
            DWORD codeSize = section[i].Misc.VirtualSize;
            DWORD oldProt;
            VirtualProtect(codeStart, codeSize, PAGE_EXECUTE_READ, &oldProt);
        }
    }
}

bool CheckCodeIntegrity()
{
    if (g_CodeCRC == 0) return true;
    if (g_HeadersErased) return true;
    
    BYTE* base = (BYTE*)GetModuleHandle(NULL);
    if (!base) return false;
    
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt);
    
    uint32_t currentCRC = 0;
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        if (section[i].Characteristics & IMAGE_SCN_CNT_CODE)
        {
            BYTE* codeStart = base + section[i].VirtualAddress;
            DWORD codeSize = section[i].Misc.VirtualSize;
            currentCRC ^= CalculateCRC32(codeStart, codeSize);
        }
    }
    
    return currentCRC == g_CodeCRC;
}

static bool CheckDebuggerWindowClasses()
{
    const char* debuggerClasses[] = {
        "WinDbgFrameClass",
        "ID",
        "OLLYDBG",
        "X64DBG",
        "X32DBG", 
        "PROCMON_WINDOW_CLASS",
        "ProcessHacker",
        "gdkWindowToplevel",
        "Qt5QWindowIcon",
        "SunAwtFrame"
    };
    
    for (int i = 0; i < 10; i++)
    {
        HWND hwnd = FindWindowA(debuggerClasses[i], NULL);
        if (hwnd != NULL)
            return true;
    }
    
    return false;
}

static bool CheckDebuggerModules()
{
    HMODULE hMods[1024];
    DWORD cbNeeded;
    
    if (!EnumProcessModules(GetCurrentProcess(), hMods, sizeof(hMods), &cbNeeded))
        return false;
    
    const char* badDlls[] = {
        "dbghelp.dll",
        "api_ms_win_core_debug",
        "vehdebug",
        "titanhide",
        "scyllahide",
        "sharpod",
        "phantomhide"
    };
    
    for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
    {
        char modName[MAX_PATH];
        if (GetModuleFileNameExA(GetCurrentProcess(), hMods[i], modName, sizeof(modName)))
        {
            for (int j = 0; modName[j]; j++)
                if (modName[j] >= 'A' && modName[j] <= 'Z')
                    modName[j] += 32;
            
            for (int k = 0; k < 7; k++)
            {
                if (strstr(modName, badDlls[k]))
                    return true;
            }
        }
    }
    
    return false;
}

static bool CheckTimingAttack()
{
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    
    volatile int x = 0;
    for (int i = 0; i < 1000; i++)
    {
        x += i;
        x ^= i;
    }
    
    QueryPerformanceCounter(&end);
    
    double ms = (double)(end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
    
    return ms > 100.0;
}

static bool CheckInt2D()
{
    __try
    {
#ifdef _WIN64
        __debugbreak();
#else
        __asm {
            int 0x2D
            nop
        }
#endif
    }
    __except(GetExceptionCode() == EXCEPTION_BREAKPOINT ? 
             EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        return false;
    }
    
    return true;
}

static bool CheckSelfDebugging()
{
    HANDLE hDebugObject = NULL;
    
    typedef NTSTATUS(NTAPI* pNtQueryInformationProcess)(
        HANDLE, ULONG, PVOID, ULONG, PULONG);
    
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) return false;
    
    auto NtQIP = (pNtQueryInformationProcess)
        GetProcAddress(ntdll, "NtQueryInformationProcess");
    if (!NtQIP) return false;
    
    NTSTATUS status = NtQIP(GetCurrentProcess(), 0x1E, 
        &hDebugObject, sizeof(hDebugObject), NULL);
    
    if (status == 0 && hDebugObject != NULL)
        return true;
    
    return false;
}

static bool CheckCloseHandle()
{
    __try
    {
        CloseHandle((HANDLE)(ULONG_PTR)0xBAADF00D);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return true;
    }
    
    return false;
}

static bool CheckNtClose()
{
    typedef NTSTATUS(NTAPI* pNtClose)(HANDLE);
    
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) return false;
    
    auto NtClose = (pNtClose)GetProcAddress(ntdll, "NtClose");
    if (!NtClose) return false;
    
    __try
    {
        NtClose((HANDLE)(ULONG_PTR)0xBAADF00D);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return true;
    }
    
    return false;
}

static bool CheckStartupTime()
{
    if (g_StartTick == 0) return false;
    
    DWORD elapsed = GetTickCount() - g_StartTick;
    
    if (elapsed > 30000)
        return true;
    
    return false;
}

static bool CheckParentProcessInternal()
{
    typedef NTSTATUS(NTAPI* pNtQueryInformationProcess)(
        HANDLE, ULONG, PVOID, ULONG, PULONG);
    
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) return false;
    
    auto NtQIP = (pNtQueryInformationProcess)
        GetProcAddress(ntdll, "NtQueryInformationProcess");
    if (!NtQIP) return false;
    
    struct PROCESS_BASIC_INFORMATION {
        PVOID Reserved1;
        PVOID PebBaseAddress;
        PVOID Reserved2[2];
        ULONG_PTR UniqueProcessId;
        ULONG_PTR InheritedFromUniqueProcessId;
    } pbi = {};
    
    NTSTATUS status = NtQIP(GetCurrentProcess(), 0, &pbi, sizeof(pbi), NULL);
    if (status != 0) return false;
    
    DWORD parentPid = (DWORD)pbi.InheritedFromUniqueProcessId;
    
    HANDLE hParent = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parentPid);
    if (!hParent) return true;
    
    wchar_t parentPath[MAX_PATH] = {};
    DWORD pathLen = MAX_PATH;
    QueryFullProcessImageNameW(hParent, 0, parentPath, &pathLen);
    CloseHandle(hParent);
    
    for (int i = 0; parentPath[i]; i++)
        if (parentPath[i] >= 'A' && parentPath[i] <= 'Z')
            parentPath[i] += 32;
    
    if (wcsstr(parentPath, L"explorer.exe")) return false;
    if (wcsstr(parentPath, L"cmd.exe")) return false;
    if (wcsstr(parentPath, L"powershell.exe")) return false;
    if (wcsstr(parentPath, L"windowsterminal.exe")) return false;
    if (wcsstr(parentPath, L"steam.exe")) return false;
    
    if (wcsstr(parentPath, L"dbg")) return true;
    if (wcsstr(parentPath, L"ida")) return true;
    if (wcsstr(parentPath, L"x64")) return true;
    if (wcsstr(parentPath, L"x32")) return true;
    if (wcsstr(parentPath, L"olly")) return true;
    if (wcsstr(parentPath, L"ghidra")) return true;
    if (wcsstr(parentPath, L"procmon")) return true;
    if (wcsstr(parentPath, L"processhacker")) return true;
    
    return false;
}

bool CheckParentProcess()
{
    return CheckParentProcessInternal();
}

bool CheckDebuggerWindows()
{
    return CheckDebuggerWindowClasses();
}

bool CheckDebuggerProcesses()
{
    return CheckDebuggerModules();
}

bool CheckSyscallHooks()
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) return false;
    
    const char* funcs[] = {
        "NtQueryInformationProcess",
        "NtSetInformationThread",
        "NtQuerySystemInformation",
        "NtClose",
        "NtOpenProcess"
    };
    
    for (int i = 0; i < 5; i++)
    {
        BYTE* fn = (BYTE*)GetProcAddress(ntdll, funcs[i]);
        if (!fn) continue;
        
        if (fn[0] == 0xE9) return true;
        if (fn[0] == 0xEB) return true;
        if (fn[0] == 0xFF && fn[1] == 0x25) return true;
        if (fn[0] == 0xCC) return true;
        
#ifdef _WIN64
        if (fn[0] != 0x4C || fn[1] != 0x8B || fn[2] != 0xD1)
            return true;
#endif
    }
    
    return false;
}

bool IsEnvironmentSafe()
{
    if (!CheckCodeIntegrity())
        return false;
    
    if (CheckDebuggerWindowClasses())
        return false;
    
    if (CheckDebuggerModules())
        return false;
    
    if (CheckSyscallHooks())
        return false;
    
    if (CheckTimingAttack())
        return false;
    
    if (CheckSelfDebugging())
        return false;
    
    if (CheckCloseHandle())
        return false;
    
    if (CheckParentProcessInternal())
        return false;
    
    return true;
}

void ScrambleAndDie()
{
    AntiFrida::TriggerAntiUnpack();
}

uint32_t GetIntegrityHash()
{
    return g_CodeCRC;
}

void InjectIntegrityCheck(void* targetFunc)
{
    (void)targetFunc;
}

}