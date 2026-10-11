#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_SPOOF_VERSION_SPOOF_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_SPOOF_VERSION_SPOOF_H

#include <engine/shared/config.h>

class CVersionSpoof
{
public:
	static CVersionSpoof &Instance();

	const char *GetGitHash() const;
	const char *GetDDNetVersionStr() const;
	int GetDDNetVersion() const;
	int GetClientVersion7() const;
	const char *GetNetVersion() const;

	void GenerateRandomGitHash(char *pBuf, int BufSize);
	void GenerateRandomVersionStr(char *pBuf, int BufSize);

private:
	CVersionSpoof() = default;
};

#endif