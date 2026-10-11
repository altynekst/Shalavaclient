#pragma once

#include <windows.h>
#include <cstdint>

namespace HardenedProtection
{
    void Initialize();
    void ErasePEHeaders();
    void ProtectMemory();
    bool CheckCodeIntegrity();
    bool CheckParentProcess();
    bool CheckDebuggerWindows();
    bool CheckDebuggerProcesses();
    bool CheckSyscallHooks();
    bool IsEnvironmentSafe();
    void ScrambleAndDie();
    
    uint32_t GetIntegrityHash();
    void InjectIntegrityCheck(void* targetFunc);
}