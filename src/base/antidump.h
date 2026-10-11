#pragma once

#if defined(CONF_FAMILY_WINDOWS)
#include <windows.h>
#endif

class AntiDump
{
public:
	static void ErasePEHeaders();
	static void SpoofPEValues();
	static void WipeSectionNames();
	static void PerformAll();
};