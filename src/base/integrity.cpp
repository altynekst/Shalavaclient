#include "integrity.h"

#if defined(CONF_FAMILY_WINDOWS)

#include "antifrida.h"

DWORD CIntegrityCheck::s_OriginalHash = 0;
bool CIntegrityCheck::s_Initialized = false;

DWORD CIntegrityCheck::HashTextSection()
{
    PBYTE base = (PBYTE)GetModuleHandle(NULL);
    if(!base) return 0;

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    if(dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;

    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    if(nt->Signature != IMAGE_NT_SIGNATURE) return 0;

    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt);
    for(WORD i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        if(section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)
        {
            PBYTE secBase = base + section[i].VirtualAddress;
            DWORD secSize = section[i].SizeOfRawData;

            DWORD hash = 0x811C9DC5;
            for(DWORD j = 0; j < secSize; j++)
            {
                hash ^= secBase[j];
                hash *= 0x01000193;
            }
            return hash;
        }
    }
    return 0;
}

void CIntegrityCheck::StoreSectionHash()
{
    s_OriginalHash = HashTextSection();
    s_Initialized = true;
}

bool CIntegrityCheck::VerifySectionHash()
{
    if(!s_Initialized) return true;
    return HashTextSection() == s_OriginalHash;
}

void CIntegrityCheck::CrashIfModified()
{
    if(!s_Initialized) return;
    if(!VerifySectionHash())
        AntiFrida::TriggerAntiUnpack();
}

#endif