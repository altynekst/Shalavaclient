#include "errorspizdelka.h"

#include <base/system.h>
#include <base/log.h>

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <ctime>
#include <string>
#include <sstream>
#include <exception>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>

#if defined(CONF_FAMILY_WINDOWS)

#include <windows.h>
#include <psapi.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")
#else
#error "CErrorSpizdelka only supports Windows"
#endif

bool CErrorSpizdelka::s_InsideCrashHandler = false;

static LPTOP_LEVEL_EXCEPTION_FILTER s_pPreviousSEH = nullptr;

static std::string GetExceptionCodeString(unsigned long Code)
{
	switch(Code)
	{
	case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION (read/write to invalid memory)";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
	case EXCEPTION_BREAKPOINT: return "BREAKPOINT";
	case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
	case EXCEPTION_FLT_DENORMAL_OPERAND: return "FLT_DENORMAL_OPERAND";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
	case EXCEPTION_FLT_INEXACT_RESULT: return "FLT_INEXACT_RESULT";
	case EXCEPTION_FLT_INVALID_OPERATION: return "FLT_INVALID_OPERATION";
	case EXCEPTION_FLT_OVERFLOW: return "FLT_OVERFLOW";
	case EXCEPTION_FLT_STACK_CHECK: return "FLT_STACK_CHECK";
	case EXCEPTION_FLT_UNDERFLOW: return "FLT_UNDERFLOW";
	case EXCEPTION_GUARD_PAGE: return "GUARD_PAGE";
	case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
	case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
	case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
	case EXCEPTION_INT_OVERFLOW: return "INT_OVERFLOW";
	case EXCEPTION_INVALID_DISPOSITION: return "INVALID_DISPOSITION";
	case EXCEPTION_INVALID_HANDLE: return "INVALID_HANDLE";
	case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
	case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION (privileged instruction)";
	case EXCEPTION_SINGLE_STEP: return "SINGLE_STEP";
	case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
	default:
		char buf[64];
		snprintf(buf, sizeof(buf), "UNKNOWN (0x%08lX)", Code);
		return buf;
	}
}

static LONG WINAPI GlobalExceptionHandler(EXCEPTION_POINTERS *pExceptionInfo);

CErrorSpizdelka::CErrorSpizdelka() { ANTI_DISASM_BLOCK(); }

CErrorSpizdelka &CErrorSpizdelka::Instance()
{
	ANTI_DISASM_BLOCK();
	static CErrorSpizdelka s_Instance;
	return s_Instance;
}


void CErrorSpizdelka::Install()
{
	ANTI_DISASM_BLOCK();
	if(m_Installed)
		return;


	s_pPreviousSEH = SetUnhandledExceptionFilter(GlobalExceptionHandler);

	signal(SIGSEGV, SignalHandler);
	signal(SIGABRT, SignalHandler);
	signal(SIGFPE, SignalHandler);
	signal(SIGILL, SignalHandler);
	signal(SIGTERM, SignalHandler);

	std::set_terminate(TerminateHandler);

	dbg_assert_set_handler([](const char *pMsg) {
		AssertHandler(pMsg);
	});

	m_Installed = true;
	log_info("errorspizdelka", "crash handlers installed");
}

void CErrorSpizdelka::Uninstall()
{
	ANTI_DISASM_BLOCK();
	if(!m_Installed)
		return;


	SetUnhandledExceptionFilter(s_pPreviousSEH);
	s_pPreviousSEH = nullptr;

	signal(SIGSEGV, SIG_DFL);
	signal(SIGABRT, SIG_DFL);
	signal(SIGFPE, SIG_DFL);
	signal(SIGILL, SIG_DFL);
	signal(SIGTERM, SIG_DFL);

	dbg_assert_set_handler(nullptr);

	m_Installed = false;
}

void CErrorSpizdelka::ShowCrashMessage(const char *pTitle, const char *pMessage)
{
	ANTI_DISASM_BLOCK();
	MessageBoxA(nullptr, pMessage, pTitle, MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
}


std::string CErrorSpizdelka::GetSignalString(int Signal)
{
	switch(Signal)
	{
	case SIGSEGV: return "SIGSEGV (Segmentation fault - invalid memory access)";
	case SIGABRT: return "SIGABRT (Abort - program called abort())";
	case SIGFPE: return "SIGFPE (Floating point exception)";
	case SIGILL: return "SIGILL (Illegal instruction)";
	case SIGTERM: return "SIGTERM (Termination request)";
	default:
		char buf[64];
		snprintf(buf, sizeof(buf), "Signal %d (unknown)", Signal);
		return buf;
	}
}

std::string CErrorSpizdelka::CollectSystemInfo()
{
	ANTI_DISASM_BLOCK();
	std::stringstream ss;


	MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	if(GlobalMemoryStatusEx(&memInfo))
	{
		ss << "RAM: " << (memInfo.ullAvailPhys / (1024 * 1024)) << " MB free / "
		   << (memInfo.ullTotalPhys / (1024 * 1024)) << " MB total\n";
		ss << "RAM Usage: " << memInfo.dwMemoryLoad << "%\n";
	}

	ss << "Process ID: " << GetCurrentProcessId() << "\n";
	ss << "Thread ID: " << GetCurrentThreadId() << "\n";

	HANDLE hProcess = GetCurrentProcess();
	PROCESS_MEMORY_COUNTERS_EX pmc;
	if(GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof(pmc)))
	{
		ss << "Process Memory: " << (pmc.WorkingSetSize / (1024 * 1024)) << " MB\n";
		ss << "Peak Memory: " << (pmc.PeakWorkingSetSize / (1024 * 1024)) << " MB\n";
	}

	SYSTEMTIME st;
	GetLocalTime(&st);
	char timeBuf[64];
	snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	ss << "Time: " << timeBuf << "\n";

	HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
	if(hNtdll)
	{
		typedef LONG(WINAPI * RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
		auto pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
		if(pRtlGetVersion)
		{
			RTL_OSVERSIONINFOW rovi;
			rovi.dwOSVersionInfoSize = sizeof(rovi);
			if(pRtlGetVersion(&rovi) == 0)
			{
				ss << "Windows: " << rovi.dwMajorVersion << "." << rovi.dwMinorVersion
				   << " (Build " << rovi.dwBuildNumber << ")\n";
			}
		}
	}

	return ss.str();
}

std::string CErrorSpizdelka::GetCallstackString(void *pContext)
{
	ANTI_DISASM_BLOCK();
	std::stringstream ss;


	HANDLE hProcess = GetCurrentProcess();
	HANDLE hThread = GetCurrentThread();

	if(!SymInitialize(hProcess, nullptr, TRUE))
	{
		ss << "[failed to init symbols, error " << GetLastError() << "]\n";
		return ss.str();
	}

	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

	CONTEXT ctx;
	if(pContext)
		ctx = *((CONTEXT *)pContext);
	else
		RtlCaptureContext(&ctx);

	STACKFRAME64 frame;
	memset(&frame, 0, sizeof(frame));

#if defined(_M_X64) || defined(__x86_64__)
	DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
	frame.AddrPC.Offset = ctx.Rip;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = ctx.Rbp;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Offset = ctx.Rsp;
	frame.AddrStack.Mode = AddrModeFlat;
#elif defined(_M_IX86) || defined(__i386__)
	DWORD machineType = IMAGE_FILE_MACHINE_I386;
	frame.AddrPC.Offset = ctx.Eip;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = ctx.Ebp;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Offset = ctx.Esp;
	frame.AddrStack.Mode = AddrModeFlat;
#else
	ss << "[unsupported architecture for callstack]\n";
	SymCleanup(hProcess);
	return ss.str();
#endif

	for(int i = 0; i < 64; i++)
	{
		if(!StackWalk64(machineType, hProcess, hThread, &frame, &ctx,
			   nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
			break;

		if(frame.AddrPC.Offset == 0)
			break;

		char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
		SYMBOL_INFO *pSymbol = (SYMBOL_INFO *)symbolBuffer;
		pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		pSymbol->MaxNameLen = MAX_SYM_NAME;

		DWORD64 displacement64 = 0;
		DWORD displacement = 0;
		char frameBuf[512];

		if(SymFromAddr(hProcess, frame.AddrPC.Offset, &displacement64, pSymbol))
		{
			IMAGEHLP_LINE64 line;
			line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

			if(SymGetLineFromAddr64(hProcess, frame.AddrPC.Offset, &displacement, &line))
			{
				const char *pFileName = line.FileName;
				const char *pLastSlash = strrchr(pFileName, '\\');
				if(!pLastSlash)
					pLastSlash = strrchr(pFileName, '/');
				if(pLastSlash)
					pFileName = pLastSlash + 1;

				snprintf(frameBuf, sizeof(frameBuf), "#%d  0x%llX  %s + 0x%llX  (%s:%lu)",
					i, (unsigned long long)frame.AddrPC.Offset,
					pSymbol->Name, (unsigned long long)displacement64,
					pFileName, line.LineNumber);
			}
			else
			{
				HMODULE hMod = nullptr;
				GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					(LPCSTR)(uintptr_t)frame.AddrPC.Offset, &hMod);
				char modName[MAX_PATH] = "???";
				if(hMod)
				{
					GetModuleFileNameA(hMod, modName, MAX_PATH);
					const char *pLast = strrchr(modName, '\\');
					if(pLast)
						memmove(modName, pLast + 1, strlen(pLast));
				}
				snprintf(frameBuf, sizeof(frameBuf), "#%d  0x%llX  %s + 0x%llX  [%s]",
					i, (unsigned long long)frame.AddrPC.Offset,
					pSymbol->Name, (unsigned long long)displacement64, modName);
			}
		}
		else
		{
			HMODULE hMod = nullptr;
			GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR)(uintptr_t)frame.AddrPC.Offset, &hMod);
			char modName[MAX_PATH] = "???";
			if(hMod)
			{
				GetModuleFileNameA(hMod, modName, MAX_PATH);
				const char *pLast = strrchr(modName, '\\');
				if(pLast)
					memmove(modName, pLast + 1, strlen(pLast));
			}
			snprintf(frameBuf, sizeof(frameBuf), "#%d  0x%llX  [%s + 0x%llX]",
				i, (unsigned long long)frame.AddrPC.Offset,
				modName, (unsigned long long)(frame.AddrPC.Offset - (hMod ? (uintptr_t)hMod : 0)));
		}

		ss << frameBuf << "\n";
	}

	SymCleanup(hProcess);
	return ss.str();
}

void CErrorSpizdelka::AssertHandler(const char *pMessage)
{
	ANTI_DISASM_BLOCK();
	if(s_InsideCrashHandler)
		return;

	s_InsideCrashHandler = true;

	std::string callstack = GetCallstackString(nullptr);

	std::stringstream ss;
	ss << "=== SHALAWA CRASH REPORT ===\n\n";
	ss << "Type: ASSERT FAILED\n\n";
	ss << "Message:\n" << (pMessage ? pMessage : "(null)") << "\n\n";
	ss << "--- System Info ---\n";
	ss << CollectSystemInfo() << "\n";
	ss << "--- Callstack ---\n";
	ss << callstack << "\n";

	log_error("errorspizdelka", "%s", ss.str().c_str());
	ShowCrashMessage("shalawa - Assert Failed", ss.str().c_str());

	s_InsideCrashHandler = false;
}

void CErrorSpizdelka::SignalHandler(int Signal)
{
	ANTI_DISASM_BLOCK();
	if(s_InsideCrashHandler)
	{
		signal(Signal, SIG_DFL);
		raise(Signal);
		return;
	}

	s_InsideCrashHandler = true;

	signal(Signal, SIG_DFL);

	std::string signalStr = GetSignalString(Signal);
	std::string callstack = GetCallstackString(nullptr);

	std::stringstream ss;
	ss << "=== SHALAWA CRASH REPORT ===\n\n";
	ss << "Type: SIGNAL\n\n";
	ss << "Signal: " << signalStr << "\n\n";

	if(Signal == SIGABRT)
	{
		ss << "Possible causes:\n";
		ss << "  - abort() was called (e.g. from ImGui, std::terminate)\n";
		ss << "  - An unhandled exception triggered abort\n";
		ss << "  - A failed assertion in a library\n";
		ss << "  - Double-free or heap corruption\n\n";
	}
	else if(Signal == SIGSEGV)
	{
		ss << "Possible causes:\n";
		ss << "  - Null pointer dereference\n";
		ss << "  - Use-after-free (dangling pointer)\n";
		ss << "  - Buffer overflow / out-of-bounds access\n";
		ss << "  - Stack overflow\n\n";
	}
	else if(Signal == SIGFPE)
	{
		ss << "Possible causes:\n";
		ss << "  - Division by zero\n";
		ss << "  - Integer overflow in division\n\n";
	}

	ss << "--- System Info ---\n";
	ss << CollectSystemInfo() << "\n";
	ss << "--- Callstack ---\n";
	ss << callstack << "\n";

	log_error("errorspizdelka", "%s", ss.str().c_str());
	ShowCrashMessage("shalawa - Crash (Signal)", ss.str().c_str());

	s_InsideCrashHandler = false;
	raise(Signal);
}

void CErrorSpizdelka::TerminateHandler()
{
	ANTI_DISASM_BLOCK();
	if(s_InsideCrashHandler)
	{
		abort();
		return;
	}

	s_InsideCrashHandler = true;

	std::string callstack = GetCallstackString(nullptr);

	std::stringstream ss;
	ss << "=== SHALAWA CRASH REPORT ===\n\n";
	ss << "Type: std::terminate\n\n";

	std::exception_ptr ep = std::current_exception();
	if(ep)
	{
		try { std::rethrow_exception(ep); }
		catch(const std::exception &e) { ss << "Exception: " << e.what() << "\n\n"; }
		catch(...) { ss << "Exception: (unknown type)\n\n"; }
	}
	else
	{
		ss << "No active exception (terminate called directly).\n\n";
	}

	ss << "--- System Info ---\n";
	ss << CollectSystemInfo() << "\n";
	ss << "--- Callstack ---\n";
	ss << callstack << "\n";

	log_error("errorspizdelka", "%s", ss.str().c_str());
	ShowCrashMessage("shalawa - Unhandled Exception", ss.str().c_str());

	s_InsideCrashHandler = false;
	abort();
}

static LONG WINAPI GlobalExceptionHandler(EXCEPTION_POINTERS *pEP)
{
	ANTI_DISASM_BLOCK();
	if(CErrorSpizdelka::Instance().IsInstalled() == false)
		return EXCEPTION_CONTINUE_SEARCH;


	// access the static flag via a small trick - we make it public-like through the global function
	// We need a different approach since s_InsideCrashHandler is private
	// Use a file-scope static instead
	static bool s_InHandler = false;
	if(s_InHandler)
		return EXCEPTION_CONTINUE_SEARCH;
	s_InHandler = true;

	EXCEPTION_RECORD *pRecord = pEP->ExceptionRecord;
	CONTEXT *pCtx = pEP->ContextRecord;
	unsigned long code = pRecord->ExceptionCode;

	if(code == EXCEPTION_BREAKPOINT || code == EXCEPTION_SINGLE_STEP)
	{
		s_InHandler = false;
		return EXCEPTION_CONTINUE_SEARCH;
	}

	std::string codeStr = GetExceptionCodeString(code);

	std::stringstream ss;
	ss << "=== SHALAWA CRASH REPORT ===\n\n";
	ss << "Type: UNHANDLED EXCEPTION (SEH)\n\n";
	ss << "Exception: " << codeStr << "\n";
	ss << "Address: 0x" << std::hex << (uintptr_t)pRecord->ExceptionAddress << std::dec << "\n";

	if(code == EXCEPTION_ACCESS_VIOLATION && pRecord->NumberParameters >= 2)
	{
		const char *pAction = pRecord->ExceptionInformation[0] == 0 ? "reading" :
				      pRecord->ExceptionInformation[0] == 1 ? "writing" : "executing";
		ss << "Attempted " << pAction << " address: 0x"
		   << std::hex << pRecord->ExceptionInformation[1] << std::dec << "\n";
		if(pRecord->ExceptionInformation[1] < 0x10000)
			ss << "  -> Likely NULL pointer dereference\n";
	}

	HMODULE hMod = nullptr;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)pRecord->ExceptionAddress, &hMod);
	if(hMod)
	{
		char modPath[MAX_PATH] = {};
		GetModuleFileNameA(hMod, modPath, MAX_PATH);
		const char *pName = strrchr(modPath, '\\');
		ss << "Module: " << (pName ? pName + 1 : modPath) << "\n";
		ss << "Offset: 0x" << std::hex << ((uintptr_t)pRecord->ExceptionAddress - (uintptr_t)hMod) << std::dec << "\n";
	}

	ss << "\n";

#if defined(_M_X64) || defined(__x86_64__)
	ss << "--- Registers ---\n";
	char regBuf[512];
	snprintf(regBuf, sizeof(regBuf),
		"RAX=0x%016llX  RBX=0x%016llX\n"
		"RCX=0x%016llX  RDX=0x%016llX\n"
		"RSI=0x%016llX  RDI=0x%016llX\n"
		"RBP=0x%016llX  RSP=0x%016llX\n"
		"R8 =0x%016llX  R9 =0x%016llX\n"
		"R10=0x%016llX  R11=0x%016llX\n"
		"R12=0x%016llX  R13=0x%016llX\n"
		"R14=0x%016llX  R15=0x%016llX\n"
		"RIP=0x%016llX  EFLAGS=0x%08lX\n",
		(unsigned long long)pCtx->Rax, (unsigned long long)pCtx->Rbx,
		(unsigned long long)pCtx->Rcx, (unsigned long long)pCtx->Rdx,
		(unsigned long long)pCtx->Rsi, (unsigned long long)pCtx->Rdi,
		(unsigned long long)pCtx->Rbp, (unsigned long long)pCtx->Rsp,
		(unsigned long long)pCtx->R8, (unsigned long long)pCtx->R9,
		(unsigned long long)pCtx->R10, (unsigned long long)pCtx->R11,
		(unsigned long long)pCtx->R12, (unsigned long long)pCtx->R13,
		(unsigned long long)pCtx->R14, (unsigned long long)pCtx->R15,
		(unsigned long long)pCtx->Rip, (unsigned long)pCtx->EFlags);
	ss << regBuf << "\n";
#elif defined(_M_IX86) || defined(__i386__)
	ss << "--- Registers ---\n";
	char regBuf[256];
	snprintf(regBuf, sizeof(regBuf),
		"EAX=0x%08lX  EBX=0x%08lX  ECX=0x%08lX  EDX=0x%08lX\n"
		"ESI=0x%08lX  EDI=0x%08lX  EBP=0x%08lX  ESP=0x%08lX\n"
		"EIP=0x%08lX  EFLAGS=0x%08lX\n",
		(unsigned long)pCtx->Eax, (unsigned long)pCtx->Ebx,
		(unsigned long)pCtx->Ecx, (unsigned long)pCtx->Edx,
		(unsigned long)pCtx->Esi, (unsigned long)pCtx->Edi,
		(unsigned long)pCtx->Ebp, (unsigned long)pCtx->Esp,
		(unsigned long)pCtx->Eip, (unsigned long)pCtx->EFlags);
	ss << regBuf << "\n";
#endif

	ss << "--- System Info ---\n";
	ss << CErrorSpizdelka::Instance().CollectSystemInfo() << "\n";
	ss << "--- Callstack ---\n";
	ss << CErrorSpizdelka::Instance().GetCallstackString(pCtx) << "\n";
	ss << "Copy this entire message and send it to the developer.\n";

	log_error("errorspizdelka", "%s", ss.str().c_str());
	CErrorSpizdelka::Instance().ShowCrashMessage("shalawa - Crash", ss.str().c_str());

	s_InHandler = false;
	return EXCEPTION_EXECUTE_HANDLER;
}
