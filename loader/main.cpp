#define NOMINMAX
#define SDL_MAIN_HANDLED

#include <windows.h>
#include <winternl.h>
#include <tlhelp32.h>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "HardenedProtection.h"
#include "UI.h"
#include "AntiDebug.h"
#include "antifrida.h"
#include "obfuscate.h"

typedef NTSTATUS(NTAPI* pNtQueryInformationProcess)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

static void ImageSizeIncreaser()
{
    srand(static_cast<unsigned int>(::time(nullptr)));
    constexpr ULONG minIncrease = 0x100000;
    constexpr ULONG maxIncrease = 0x500000;
    ULONG randomIncrease = minIncrease + (rand() % (maxIncrease - minIncrease + 1));

#if _WIN64
    auto pPeb = (PPEB)__readgsqword(0x60);
#else
    auto pPeb = (PPEB)__readfsdword(0x30);
#endif

    auto inLoadOrderModuleList = static_cast<PLIST_ENTRY>(pPeb->Ldr->Reserved2[1]);
    auto tableEntry = CONTAINING_RECORD(inLoadOrderModuleList, LDR_DATA_TABLE_ENTRY, Reserved1[0]);
    auto pEntrySizeOfImage = (PULONG)&tableEntry->Reserved3[1];
    *pEntrySizeOfImage = static_cast<ULONG>((INT_PTR)tableEntry->DllBase + randomIncrease);
}

static void PerformSecurityChecks()
{
    HardenedProtection::Initialize();
    AntiDebug::Init();
    AntiDebug::HideThread();
    
    if (!HardenedProtection::IsEnvironmentSafe())
    {
        HardenedProtection::ScrambleAndDie();
        ExitProcess(0xDEAD);
    }
    
    if (AntiFrida::IsDetected())
    {
        AntiFrida::TriggerAntiUnpack();
        ExitProcess(0xDEAD);
    }
    
    if (!AntiDebug::IsClean())
    {
        AntiFrida::TriggerAntiUnpack();
        ExitProcess(0xDEAD);
    }
    
    AntiDebug::PerformAdvancedAntiDebug();
    HardenedProtection::ErasePEHeaders();
}

int main(int, char**)
{
    ImageSizeIncreaser();
    PerformSecurityChecks();
    
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    
    UI::UIState s;
    if (UI::Init(s)) {
        UI::Run(s);
        UI::Shutdown(s);
    }
    
    CoUninitialize();
    return 0;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return main(__argc, __argv);
}