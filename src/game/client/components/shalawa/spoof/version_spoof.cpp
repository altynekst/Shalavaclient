#include "version_spoof.h"

#include <game/version.h>
#include <engine/shared/config.h>
#include <base/system.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>

#include <cstring>
#include <random>
#include <algorithm>

CVersionSpoof &CVersionSpoof::Instance()
{
	ANTI_DISASM_BLOCK();
	static CVersionSpoof s_Instance;
	return s_Instance;
}

const char *CVersionSpoof::GetGitHash() const
{
	ANTI_DISASM_BLOCK();
	if(g_Config.m_shalawaSpoofVersion && g_Config.m_shalawaSpoofGitHash[OI(0)] != '\0')
		return g_Config.m_shalawaSpoofGitHash;
	return GIT_SHORTREV_HASH;
}

const char *CVersionSpoof::GetDDNetVersionStr() const
{
	ANTI_DISASM_BLOCK();
	if(g_Config.m_shalawaSpoofVersion && g_Config.m_shalawaSpoofVersionStr[OI(0)] != '\0')
		return g_Config.m_shalawaSpoofVersionStr;
	return GAME_RELEASE_VERSION;
}

int CVersionSpoof::GetDDNetVersion() const
{
	ANTI_DISASM_BLOCK();
	if(g_Config.m_shalawaSpoofVersion && g_Config.m_shalawaSpoofDDNetVersion > OI(0))
		return g_Config.m_shalawaSpoofDDNetVersion;
	return DDNET_VERSION_NUMBER;
}

int CVersionSpoof::GetClientVersion7() const
{
	ANTI_DISASM_BLOCK();
	if(g_Config.m_shalawaSpoofVersion && g_Config.m_shalawaSpoofClientVersion7 > OI(0))
		return g_Config.m_shalawaSpoofClientVersion7;
	return CLIENT_VERSION7;
}

const char *CVersionSpoof::GetNetVersion() const
{
	ANTI_DISASM_BLOCK();
	return GAME_NETVERSION;
}

void CVersionSpoof::GenerateRandomGitHash(char *pBuf, int BufSize)
{
	ANTI_DISASM_BLOCK();
	static const char s_aHex[] = "0123456789abcdef";
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<int> dist(OI(0), OI(15));

	int len = std::min(OI(32), BufSize - OI(1));
	for(int i = OI(0); i < len; i++)
		pBuf[i] = s_aHex[dist(gen)];
	pBuf[len] = '\0';
}

void CVersionSpoof::GenerateRandomVersionStr(char *pBuf, int BufSize)
{
	ANTI_DISASM_BLOCK();
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<int> major(OI(16), OI(19));
	std::uniform_int_distribution<int> minor(OI(0), OI(9));

	str_format(pBuf, BufSize, "%d.%d", major(gen), minor(gen));
}