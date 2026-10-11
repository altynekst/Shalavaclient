#include "hwid.h"
#include <base/system.h>

#ifdef CONF_FAMILY_WINDOWS
#include <windows.h>

bool GetSystemHWID(char *pBuffer, int BufferSize)
{
	pBuffer[0] = '\0';
	
	HKEY hKey;
	if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, 
		"SOFTWARE\\Microsoft\\Cryptography", 
		0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
	{
		return false;
	}
	
	char aGuid[256] = {0};
	DWORD dwSize = sizeof(aGuid);
	DWORD dwType = REG_SZ;
	
	LONG result = RegQueryValueExA(hKey, "MachineGuid", NULL, &dwType, (LPBYTE)aGuid, &dwSize);
	RegCloseKey(hKey);
	
	if(result != ERROR_SUCCESS)
		return false;
	
	str_copy(pBuffer, aGuid, BufferSize);
	return true;
}

#else

bool GetSystemHWID(char *pBuffer, int BufferSize)
{
	str_copy(pBuffer, "UNSUPPORTED_PLATFORM", BufferSize);
	return false;
}

#endif