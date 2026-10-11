#include "antidebug.h"

#if defined(CONF_FAMILY_WINDOWS)

#include "antifrida.h"
#include "obfuscate.h"

#include <intrin.h>
#include <winternl.h>

typedef NTSTATUS(NTAPI *pfnNtQueryInformationProcess)(
	HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

static pfnNtQueryInformationProcess GetNtQueryInformationProcess()
{
	static pfnNtQueryInformationProcess fn = nullptr;
	if(!fn)
	{
		HMODULE hNtdll = GetModuleHandleA(AY_OBFUSCATE("ntdll.dll"));
		if(hNtdll)
			fn = (pfnNtQueryInformationProcess)GetProcAddress(hNtdll, AY_OBFUSCATE("NtQueryInformationProcess"));
	}
	return fn;
}

bool AntiDebug::CheckIsDebuggerPresent()
{
	return IsDebuggerPresent() != 0;
}

bool AntiDebug::CheckNtGlobalFlag()
{
#if _WIN64
	PBYTE pPeb = (PBYTE)__readgsqword(0x60);
	DWORD ntGlobalFlag = *(DWORD *)(pPeb + 0xBC);
#else
	PBYTE pPeb = (PBYTE)__readfsdword(0x30);
	DWORD ntGlobalFlag = *(DWORD *)(pPeb + 0x68);
#endif
	return (ntGlobalFlag & 0x70) != 0;
}

bool AntiDebug::CheckRemoteDebugger()
{
	BOOL bDebuggerPresent = FALSE;
	CheckRemoteDebuggerPresent(GetCurrentProcess(), &bDebuggerPresent);
	return bDebuggerPresent != 0;
}

bool AntiDebug::CheckHardwareBreakpoints()
{
	CONTEXT ctx = {};
	ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	if(GetThreadContext(GetCurrentThread(), &ctx))
	{
		if(ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3)
			return true;
	}
	return false;
}

bool AntiDebug::CheckProcessDebugPort()
{
	auto fn = GetNtQueryInformationProcess();
	if(!fn)
		return false;

	DWORD_PTR debugPort = 0;
	NTSTATUS status = fn(
		GetCurrentProcess(),
		(PROCESSINFOCLASS)7, // ProcessDebugPort
		&debugPort,
		sizeof(debugPort),
		nullptr);

	return status == 0 && debugPort != 0;
}

bool AntiDebug::CheckDebugObjectHandle()
{
	auto fn = GetNtQueryInformationProcess();
	if(!fn)
		return false;

	HANDLE hDebugObject = nullptr;
	NTSTATUS status = fn(
		GetCurrentProcess(),
		(PROCESSINFOCLASS)0x1E, // ProcessDebugObjectHandle
		&hDebugObject,
		sizeof(hDebugObject),
		nullptr);

	if(status == 0 && hDebugObject)
	{
		CloseHandle(hDebugObject);
		return true;
	}
	return false;
}

bool AntiDebug::CheckRdtscTiming()
{
	unsigned long long t1 = __rdtsc();

	volatile int dummy = 0;
	for(int i = 0; i < 1000; i++)
		dummy += i;

	unsigned long long t2 = __rdtsc();
	unsigned long long delta = t2 - t1;

	return delta > 10000000;
}

bool AntiDebug::IsDebuggerDetected()
{
	return CheckIsDebuggerPresent() ||
	       CheckNtGlobalFlag() ||
	       CheckRemoteDebugger() ||
	       CheckHardwareBreakpoints() ||
	       CheckProcessDebugPort() ||
	       CheckDebugObjectHandle();
}

void AntiDebug::CrashIfDetected()
{
	if(IsDebuggerDetected())
		AntiFrida::TriggerAntiUnpack();
	if(CheckRdtscTiming())
		AntiFrida::TriggerAntiUnpack();
}

#else

bool AntiDebug::IsDebuggerDetected() { return false; }
void AntiDebug::CrashIfDetected() {}

#endif