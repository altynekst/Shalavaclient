#pragma once

#if defined(CONF_FAMILY_WINDOWS)
#include <windows.h>
#include <winternl.h>
#endif

class KRXProtection
{
public:
	KRXProtection() = default;
	~KRXProtection() = default;

	static void Initialize();
	static void PostInitialize();
	static void StartMonitorThread();
	static void StopMonitorThread();

private:
	static void ImageSizeIncreaser();
};