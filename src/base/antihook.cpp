#include "antihook.h"

#if defined(CONF_FAMILY_WINDOWS)

#include "antifrida.h"
#include "obfuscate.h"

#include <psapi.h>

bool AntiHook::IsInlineHooked(void *pFunc)
{
	if(!pFunc)
		return false;

	BYTE *pBytes = (BYTE *)pFunc;

	if(pBytes[0] == 0xE9)
		return true;

	if(pBytes[0] == 0xFF && pBytes[1] == 0x25)
		return true;

	if(pBytes[0] == 0x68 && pBytes[5] == 0xC3)
		return true;

	if(pBytes[0] == 0x48 && pBytes[1] == 0xB8 && pBytes[10] == 0xFF && pBytes[11] == 0xE0)
		return true;

	return false;
}

bool AntiHook::IsIATHooked()
{
	PBYTE base = (PBYTE)GetModuleHandle(NULL);
	if(!base)
		return false;

	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
	PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);

	DWORD importRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
	if(!importRVA)
		return false;

	PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)(base + importRVA);

	while(importDesc->Name)
	{
		PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)(base + importDesc->FirstThunk);

		while(thunk->u1.Function)
		{
			PVOID funcAddr = (PVOID)thunk->u1.Function;

			MEMORY_BASIC_INFORMATION mbi;
			if(VirtualQuery(funcAddr, &mbi, sizeof(mbi)))
			{
				HMODULE hMod = (HMODULE)mbi.AllocationBase;
				char modName[MAX_PATH];
				if(GetModuleFileNameA(hMod, modName, MAX_PATH))
				{
					std::string name(modName);
					std::transform(name.begin(), name.end(), name.begin(), ::tolower);

					if(name.find(AY_OBFUSCATE("frida")) != std::string::npos ||
						name.find(AY_OBFUSCATE("hook")) != std::string::npos ||
						name.find(AY_OBFUSCATE("detour")) != std::string::npos ||
						name.find(AY_OBFUSCATE("minhook")) != std::string::npos)
					{
						return true;
					}
				}
			}

			thunk++;
		}

		importDesc++;
	}

	return false;
}

void AntiHook::CheckCriticalAPIs()
{
	HMODULE hKernel32 = GetModuleHandleA(AY_OBFUSCATE("kernel32.dll"));
	HMODULE hNtdll = GetModuleHandleA(AY_OBFUSCATE("ntdll.dll"));

	void *criticalFuncs[] = {
		(void *)GetProcAddress(hKernel32, AY_OBFUSCATE("VirtualAlloc")),
		(void *)GetProcAddress(hKernel32, AY_OBFUSCATE("VirtualProtect")),
		(void *)GetProcAddress(hKernel32, AY_OBFUSCATE("LoadLibraryA")),
		(void *)GetProcAddress(hKernel32, AY_OBFUSCATE("CreateFileA")),
		(void *)GetProcAddress(hNtdll, AY_OBFUSCATE("NtReadVirtualMemory")),
		(void *)GetProcAddress(hNtdll, AY_OBFUSCATE("NtWriteVirtualMemory")),
	};

	for(auto func : criticalFuncs)
	{
		if(IsInlineHooked(func))
		{
			AntiFrida::TriggerAntiUnpack();
			return;
		}
	}
}

void AntiHook::PerformAll()
{
	if(IsIATHooked())
	{
		AntiFrida::TriggerAntiUnpack();
		return;
	}
	CheckCriticalAPIs();
}

#else

bool AntiHook::IsIATHooked() { return false; }
bool AntiHook::IsInlineHooked(void *) { return false; }
void AntiHook::CheckCriticalAPIs() {}
void AntiHook::PerformAll() {}

#endif