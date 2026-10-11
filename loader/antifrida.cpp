#include "antifrida.h"
#include "obfuscate.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <winsock2.h>
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <psapi.h>
#include <string>
#include <tlhelp32.h>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "Ws2_32.lib")


bool AntiFrida::IsFridaModuleLoaded()
{
	HMODULE hMods[1024];
	HANDLE hProcess = GetCurrentProcess();
	DWORD cbNeeded;

	if(EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
	{
		for(unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
		{
			CHAR szModName[MAX_PATH];
			if(GetModuleFileNameExA(hProcess, hMods[i], szModName, sizeof(szModName)))
			{
				std::string mod(szModName);
				std::transform(mod.begin(), mod.end(), mod.begin(), ::tolower);

				// Check for frida (obfuscated)
				if(mod.find(AY_OBFUSCATE("frida")) != std::string::npos)
				{
					return true;
				}

				// Check for frida-agent (obfuscated)
				if(mod.find(AY_OBFUSCATE("frida-agent")) != std::string::npos)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool AntiFrida::IsFridaPipePresent()
{
	WIN32_FIND_DATAA fd;
	HANDLE hFind = FindFirstFileA(AY_OBFUSCATE("\\\\.\\pipe\\frida*"), &fd);

	if(hFind != INVALID_HANDLE_VALUE)
	{
		FindClose(hFind);
		return true;
	}
	return false;
}

bool AntiFrida::IsDetected()
{
	return IsFridaModuleLoaded() || IsFridaPipePresent();
}

void AntiFrida::TriggerAntiUnpack()
{
	PBYTE base = (PBYTE)GetModuleHandle(NULL);
	if(!base)
		return;

	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
	if(dos->e_magic != IMAGE_DOS_SIGNATURE)
		return;

	PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)(base + dos->e_lfanew);
	if(nt->Signature != IMAGE_NT_SIGNATURE)
		return;

	DWORD oldProt;
	if(VirtualProtect(nt, sizeof(IMAGE_NT_HEADERS64), PAGE_EXECUTE_READWRITE, &oldProt))
	{
		memset(nt, 0x00, sizeof(IMAGE_NT_HEADERS64));
	}

	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt);
	WORD numSections = nt->FileHeader.NumberOfSections;

	for(WORD i = 0; i < numSections; i++)
	{
		if(section[i].SizeOfRawData > 0 && (section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE))
		{
			DWORD oldProtSection;
			PBYTE sectionBase = base + section[i].VirtualAddress;
			DWORD sectionSize = section[i].SizeOfRawData;

			if(VirtualProtect(sectionBase, sectionSize, PAGE_EXECUTE_READWRITE, &oldProtSection))
			{
				for(DWORD j = 0; j < sectionSize; j++)
				{
					sectionBase[j] = (j % 2 == 0) ? 0xCC : 0x90;
				}
			}
		}
	}

	*((volatile int *)0) = 0xDEADBEEF;
}

void AntiFrida::PerformChecks()
{
	if(IsDetected())
	{
		TriggerAntiUnpack();
	}
}

void AntiFrida::ToLower(char *str)
{
	while(*str)
	{
		*str = (char)tolower(*str);
		str++;
	}
}