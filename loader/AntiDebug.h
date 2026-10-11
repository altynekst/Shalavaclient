#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace AntiDebug
{
    void Init();
    void HideThread();
    uint32_t GetEnvironmentScore();
    void ContributeToKey(uint8_t* keyBuffer, size_t keyLen);
    bool IsClean();
    bool RunAllChecks();
    std::string GetDynamicChallengeSecret();
    void PerformAdvancedAntiDebug();
}