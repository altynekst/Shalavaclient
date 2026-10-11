#pragma once

#if defined(CONF_FAMILY_WINDOWS)
#include <windows.h>
#endif

class AntiHook
{
public:
	static bool IsIATHooked();
	static bool IsInlineHooked(void *pFunc);
	static void CheckCriticalAPIs();
	static void PerformAll();
};