#include "antidump.h"

#if defined(CONF_FAMILY_WINDOWS)

#include <intrin.h>
#include <cstdlib>
#include <ctime>

void AntiDump::ErasePEHeaders()
{
	PBYTE base = (PBYTE)GetModuleHandle(NULL);
	if(!base)
		return;

	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
	if(dos->e_magic != IMAGE_DOS_SIGNATURE)
		return;

	PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
	if(nt->Signature != IMAGE_NT_SIGNATURE)
		return;

	DWORD headerSize = nt->OptionalHeader.SizeOfHeaders;
	DWORD oldProt;

	if(VirtualProtect(base, headerSize, PAGE_READWRITE, &oldProt))
	{
		SecureZeroMemory(base, headerSize);
		VirtualProtect(base, headerSize, oldProt, &oldProt);
	}
}

void AntiDump::SpoofPEValues()
{
	PBYTE base = (PBYTE)GetModuleHandle(NULL);
	if(!base)
		return;

	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
	if(dos->e_magic != IMAGE_DOS_SIGNATURE)
		return;

	PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
	if(nt->Signature != IMAGE_NT_SIGNATURE)
		return;

	DWORD oldProt;

	if(VirtualProtect(&nt->OptionalHeader.SizeOfImage, sizeof(DWORD), PAGE_READWRITE, &oldProt))
	{
		nt->OptionalHeader.SizeOfImage = 0;
		VirtualProtect(&nt->OptionalHeader.SizeOfImage, sizeof(DWORD), oldProt, &oldProt);
	}

	if(VirtualProtect(&nt->OptionalHeader.AddressOfEntryPoint, sizeof(DWORD), PAGE_READWRITE, &oldProt))
	{
		nt->OptionalHeader.AddressOfEntryPoint = 0;
		VirtualProtect(&nt->OptionalHeader.AddressOfEntryPoint, sizeof(DWORD), oldProt, &oldProt);
	}

	if(VirtualProtect(&nt->FileHeader.NumberOfSections, sizeof(WORD), PAGE_READWRITE, &oldProt))
	{
		nt->FileHeader.NumberOfSections = 0;
		VirtualProtect(&nt->FileHeader.NumberOfSections, sizeof(WORD), oldProt, &oldProt);
	}
}

void AntiDump::WipeSectionNames()
{
	PBYTE base = (PBYTE)GetModuleHandle(NULL);
	if(!base)
		return;

	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
	if(dos->e_magic != IMAGE_DOS_SIGNATURE)
		return;

	PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
	if(nt->Signature != IMAGE_NT_SIGNATURE)
		return;

	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt);
	WORD numSections = nt->FileHeader.NumberOfSections;

	for(WORD i = 0; i < numSections; i++)
	{
		DWORD oldProt;
		if(VirtualProtect(section[i].Name, sizeof(section[i].Name), PAGE_READWRITE, &oldProt))
		{
			SecureZeroMemory(section[i].Name, sizeof(section[i].Name));
			VirtualProtect(section[i].Name, sizeof(section[i].Name), oldProt, &oldProt);
		}
	}
}

void AntiDump::PerformAll()
{
	WipeSectionNames();
	SpoofPEValues();
	ErasePEHeaders();
}

#else

void AntiDump::ErasePEHeaders() {}
void AntiDump::SpoofPEValues() {}
void AntiDump::WipeSectionNames() {}
void AntiDump::PerformAll() {}

#endif