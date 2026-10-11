#pragma once

#if defined(CONF_FAMILY_WINDOWS)
#include <windows.h>

class CIntegrityCheck
{
public:
    static void StoreSectionHash();
    static bool VerifySectionHash();
    static void CrashIfModified();

private:
    static DWORD s_OriginalHash;
    static bool s_Initialized;

    static DWORD HashTextSection();
};

#endif