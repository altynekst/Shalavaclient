#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_MISC_SERVER_NUKER_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_MISC_SERVER_NUKER_H

#include <game/client/component.h>
#include <engine/console.h>
#include <vector>
#include <string>
#include <random>

class CServerNuker : public CComponent
{
public:
	CServerNuker();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnConsoleInit() override;
	virtual void OnRender() override;
	virtual void OnReset() override;

	void Start();
	void Stop();
	bool IsActive() const { return m_Active; }

private:
	bool m_Active;
	int64_t m_LastCommandTime;
	int64_t m_LastDummyConnectTime;
	int64_t m_LastDummySpamTime;
	int m_CommandIndex;
	int m_CycleCount;
	int m_Phase; // 0=prepare, 1=nuke, 2=dummy_flood
	bool m_PasswordSaved;
	char m_aSavedRconPassword[128];
	char m_aSavedRconUsername[64];

	std::vector<std::string> m_vCommands;
	std::mt19937 m_Rng;

	void BuildCommandList();
	void SendNextCommand();
	void DummyFloodConnect();
	void DummySpamAll();
	std::string RandomMessage();
	std::string RandomBroadcast();

	static void ConNuke(IConsole::IResult *pResult, void *pUserData);
	static void ConNukeStop(IConsole::IResult *pResult, void *pUserData);
};

#endif