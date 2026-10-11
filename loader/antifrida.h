#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

class AntiFrida
{
public:
    static bool IsFridaModuleLoaded();
    static bool IsFridaPipePresent();
    static bool IsDetected();
    static void TriggerAntiUnpack();
    static void PerformChecks();
    static void ToLower(char *str);
};