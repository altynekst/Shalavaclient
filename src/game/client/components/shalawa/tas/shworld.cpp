#include "shworld.h"

#include <base/vmath.h>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/collision.h>

CTasWorld::CTasWorld()
{
	m_pWorld = nullptr;
	m_pWorldPredicted = nullptr;
	m_Initialized = false;
}

CTasWorld::~CTasWorld()
{
	Shutdown();
}

void CTasWorld::OnReset()
{
	Shutdown();
}

void CTasWorld::OnInit()
{
}

void CTasWorld::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	if(g_Config.m_shalawaTasWorld && !g_Config.m_shalawaTasInit)
	{
		Init();
		if(IsInitialized())
			g_Config.m_shalawaTasInit = 1;
	}
	if(g_Config.m_shalawaRecording && IsInitialized())
	{
		if(!g_Config.m_shalawaTasPause)
		{
			IncrementGameTick();
		}
	}
}

void CTasWorld::Shutdown()
{
	if(m_pWorld)
	{
		delete m_pWorld;
		m_pWorld = nullptr;
	}
	if(m_pWorldPredicted)
	{
		delete m_pWorldPredicted;
		m_pWorldPredicted = nullptr;
	}
	m_Initialized = false;
}

void CTasWorld::Init()
{
	if(!GameClient() || Client()->State() != IClient::STATE_ONLINE)
		return;

	Shutdown();

	m_pWorld = new CGameWorld();
	m_pWorld->Init(Collision(), GameClient()->TuningList(), &GameClient()->m_MapBugs);
	m_pWorld->CopyWorld(&GameClient()->m_PredictedWorld);
	m_pWorld->m_GameTick = Client()->PredGameTick(g_Config.m_ClDummy);

	m_pWorldPredicted = new CGameWorld();
	m_pWorldPredicted->Init(Collision(), GameClient()->TuningList(), &GameClient()->m_MapBugs);

	m_Initialized = true;
}

void CTasWorld::CopyFromPredicted()
{
	if(!m_pWorld || !m_pWorldPredicted)
		return;
	m_pWorldPredicted->CopyWorld(m_pWorld);
}

int CTasWorld::GameTick() const
{
	return m_pWorld ? m_pWorld->m_GameTick : 0;
}

void CTasWorld::SetGameTick(int Tick)
{
	if(m_pWorld)
		m_pWorld->m_GameTick = Tick;
}

void CTasWorld::IncrementGameTick()
{
	if(m_pWorld)
		m_pWorld->m_GameTick++;
	if(m_pWorldPredicted)
		m_pWorldPredicted->m_GameTick++;
}

void CTasWorld::HandleTeleport(CCharacter *pChar)
{
	if(!pChar || !m_pWorld)
		return;

	CCollision *pCollision = Collision();
	if(!pCollision)
		return;

	CCharacterCore Core = pChar->GetCore();
	vec2 Pos = Core.m_Pos;
	int MapIndex = pCollision->GetPureMapIndex(Pos);

	int TeleNumber = pCollision->IsTeleport(MapIndex);
	if(TeleNumber && !pCollision->TeleOuts(TeleNumber - 1).empty())
	{
		int TeleOut = m_pWorld->m_Core.RandomOr0(pCollision->TeleOuts(TeleNumber - 1).size());
		Core.m_Pos = pCollision->TeleOuts(TeleNumber - 1)[TeleOut];
		Core.m_Vel = vec2(0, 0);
		pChar->SetCore(Core);
		pChar->m_Pos = Core.m_Pos;
		pChar->m_FreezeTime = 0;
		pChar->ResetHook();
		return;
	}

	int EvilTeleNumber = pCollision->IsEvilTeleport(MapIndex);
	if(EvilTeleNumber && !pCollision->TeleOuts(EvilTeleNumber - 1).empty())
	{
		int TeleOut = m_pWorld->m_Core.RandomOr0(pCollision->TeleOuts(EvilTeleNumber - 1).size());
		Core.m_Pos = pCollision->TeleOuts(EvilTeleNumber - 1)[TeleOut];
		pChar->SetCore(Core);
		pChar->m_Pos = Core.m_Pos;
		return;
	}

	if(pCollision->GetTileIndex(MapIndex) == TILE_TELEINEVIL || pCollision->GetFrontTileIndex(MapIndex) == TILE_TELEINEVIL)
	{
		int TeleNumberEvil = pCollision->IsTeleport(MapIndex);
		if(TeleNumberEvil && !pCollision->TeleOuts(TeleNumberEvil - 1).empty())
		{
			int TeleOut = m_pWorld->m_Core.RandomOr0(pCollision->TeleOuts(TeleNumberEvil - 1).size());
			Core.m_Pos = pCollision->TeleOuts(TeleNumberEvil - 1)[TeleOut];
			pChar->SetCore(Core);
			pChar->m_Pos = Core.m_Pos;
			return;
		}
	}
}

void CTasWorld::ApplyDummyHammer(CCharacter *pDummyChar)
{
	if(!pDummyChar || !m_pWorld)
		return;

	vec2 DummyPos = pDummyChar->m_Pos;
	int BestId = -1;
	float BestDist = 1e30f;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(ClientId == pDummyChar->GetId())
			continue;
		CCharacter *pTargetChar = m_pWorld->GetCharacterById(ClientId);
		if(!pTargetChar)
			continue;
		float Dist = length(DummyPos - pTargetChar->m_Pos);
		if(Dist < BestDist)
		{
			BestDist = Dist;
			BestId = ClientId;
		}
	}

	if(BestId >= 0)
	{
		float MinSep = (float)g_Config.m_ClDummyHammerMinSeparation;
		if(MinSep <= 0.0f)
			MinSep = 35.0f;
		if(BestDist <= MinSep * 1.25f)
		{
			CCharacter *pTargetChar = m_pWorld->GetCharacterById(BestId);
			if(pTargetChar)
			{
				vec2 Aim = pTargetChar->m_Pos - DummyPos;
				if(!Aim.x && !Aim.y)
					Aim.x = 1;

				const int OtherDummy = !g_Config.m_ClDummy;
				CNetObj_PlayerInput &Input = GameClient()->m_Controls.m_aInputData[OtherDummy];
				Input.m_TargetX = (int)Aim.x;
				Input.m_TargetY = (int)Aim.y;
				Input.m_WantedWeapon = WEAPON_HAMMER + 1;
				if(!(Input.m_Fire & 1))
					Input.m_Fire++;
				else
					Input.m_Fire += 2;

				pDummyChar->OnDirectInput(&Input);
				pDummyChar->OnPredictedInput(&Input);
			}
		}
	}
}