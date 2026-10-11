#pragma once

#if defined(CONF_FAMILY_WINDOWS)
#include <windows.h>
#endif

class AntiDebug
{
public:
	static bool IsDebuggerDetected();
	static void CrashIfDetected();

private:
#if defined(CONF_FAMILY_WINDOWS)
	static bool CheckIsDebuggerPresent();
	static bool CheckNtGlobalFlag();
	static bool CheckRemoteDebugger();
	static bool CheckHardwareBreakpoints();
	static bool CheckProcessDebugPort();
	static bool CheckDebugObjectHandle();
	static bool CheckRdtscTiming();
#endif
};