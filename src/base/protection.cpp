#include "protection.h"

#if defined(CONF_FAMILY_WINDOWS)

#include "antidebug.h"
#include "integrity.h"
#include "antidump.h"
#include "antifrida.h"
#include "obfuscate.h"
#include "instr.h"

#include <atomic>
#include <cstdlib>
#include <ctime>
#include <thread>

using namespace obf;

#if _WIN64
#define GET_PEB() ((PPEB)__readgsqword(0x60))
#else
#define GET_PEB() ((PPEB)__readfsdword(0x30))
#endif

#define CONTAINING_RECORD_CUSTOM(address, type, field) \
	((type *)((PCHAR)(address) - (ULONG_PTR)(&((type *)0)->field)))

static std::atomic<bool> s_MonitorRunning{false};
static std::thread s_MonitorThread;

static void MonitorThreadFunc()
{
    srand((unsigned int)time(nullptr) ^ GetCurrentThreadId());

    while(s_MonitorRunning.load(std::memory_order_relaxed))
    {
        AntiDebug::CrashIfDetected();
        AntiFrida::PerformChecks();
        CIntegrityCheck::CrashIfModified();

        int sleepMs = 2000 + (rand() % 5000);
        for(int i = 0; i < sleepMs / 100 && s_MonitorRunning.load(std::memory_order_relaxed); i++)
            Sleep(100);
    }
}

void KRXProtection::ImageSizeIncreaser()
{
	OBF_BEGIN
	srand(static_cast<unsigned int>(::time(nullptr)));

	constexpr ULONG minIncrease = 0x100000;
	constexpr ULONG maxIncrease = 0x500000;
	ULONG randomIncrease = minIncrease + (rand() % (maxIncrease - minIncrease + 1));

	PPEB pPeb = GET_PEB();
	IF(!pPeb || !pPeb->Ldr)
		RETURN(0);
	ENDIF

	PLIST_ENTRY inLoadOrderModuleList = &pPeb->Ldr->InLoadOrderModuleList;
	IF(!inLoadOrderModuleList)
		RETURN(0);
	ENDIF

	PLIST_ENTRY currentEntry = inLoadOrderModuleList->Flink;
	IF(!currentEntry || currentEntry == inLoadOrderModuleList)
		RETURN(0);
	ENDIF

	PLDR_DATA_TABLE_ENTRY tableEntry = CONTAINING_RECORD_CUSTOM(currentEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
	IF(!tableEntry || !tableEntry->DllBase)
		RETURN(0);
	ENDIF

	tableEntry->SizeOfImage = static_cast<ULONG>((INT_PTR)tableEntry->DllBase + randomIncrease);
	RETURN(0);
	OBF_END
}

void KRXProtection::Initialize()
{
    OBF_BEGIN

#ifndef CONF_DEBUG
    AntiDebug::CrashIfDetected();
#endif

    IF(AntiFrida::IsDetected())
        AntiFrida::TriggerAntiUnpack();
    ENDIF

    ImageSizeIncreaser();

#ifndef CONF_DEBUG
    CIntegrityCheck::StoreSectionHash();
#endif

    RETURN(0);
    OBF_END
}

void KRXProtection::PostInitialize()
{
#ifndef CONF_DEBUG
	AntiDump::PerformAll();
	StartMonitorThread();
#endif
}

void KRXProtection::StartMonitorThread()
{
	if(s_MonitorRunning.load())
		return;

	s_MonitorRunning.store(true);
	s_MonitorThread = std::thread(MonitorThreadFunc);
	s_MonitorThread.detach();
}

void KRXProtection::StopMonitorThread()
{
	s_MonitorRunning.store(false);
}

#else

void KRXProtection::ImageSizeIncreaser() {}
void KRXProtection::Initialize() {}
void KRXProtection::PostInitialize() {}
void KRXProtection::StartMonitorThread() {}
void KRXProtection::StopMonitorThread() {}

#endif