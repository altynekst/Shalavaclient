#include "tas.h"

#include <base/color.h>
#include <base/vmath.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/laser.h>
#include <game/client/prediction/entities/projectile.h>
#include <game/client/render.h>
#include <game/collision.h>
#include <game/client/laser_data.h>
#include <game/client/projectile_data.h>
#include <game/mapitems.h>

#include <algorithm>
#include <fstream>

// ============================================================================
// CTasWorld Implementation
// ============================================================================

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

	// Initialize TAS World if needed (for recording or if explicitly enabled)
	if((g_Config.m_shalawaTasWorld || g_Config.m_shalawaRecording) && !g_Config.m_shalawaTasInit)
	{
		Init();
		if(IsInitialized())
			g_Config.m_shalawaTasInit = 1;
	}
	
	// TAS World ticking is handled in CTasRecorder::OnRender() for timed mode
	// This OnRender is just for initialization
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

// ============================================================================
// CTasRecorder Implementation
// ============================================================================

CTasRecorder::CTasRecorder()
{
	m_pTasWorld = nullptr;
	m_RecordingStartTime = 0;
	m_RecordingStartTick = 0;
	m_JumpCount = 0;
	m_PrevJump = 0;
	mem_zero(m_aLastAttackTick, sizeof(m_aLastAttackTick));
	mem_zero(m_aPrevFireForAttack, sizeof(m_aPrevFireForAttack));
	mem_zero(m_aLastObservedTasAttackTick, sizeof(m_aLastObservedTasAttackTick));
}

void CTasRecorder::OnReset()
{
	Clear();
}

void CTasRecorder::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	
	if(!m_pTasWorld)
		return;

	// If recording is requested but TAS World is not initialized, wait for it
	if(g_Config.m_shalawaRecording && !m_pTasWorld->IsInitialized())
	{
		// TAS World will be initialized in its OnRender, wait for next frame
		return;
	}

	// Start recording if requested and not already recording
	if(g_Config.m_shalawaRecording && !IsRecording())
	{
		StartRecording();
	}
	
	// Stop recording if requested and currently recording
	if(!g_Config.m_shalawaRecording && IsRecording())
	{
		StopRecording();
	}

	// Record tick only when TAS World actually ticks (controlled by TickSpeed in CTasWorld::OnRender)
	// We record after TAS World ticks, so we check if tick was incremented
	static int s_LastRecordedTick = -1;
	if(IsRecording() && m_pTasWorld->IsInitialized() && !g_Config.m_shalawaTasPause)
	{
		int CurrentTick = m_pTasWorld->GameTick();
		if(CurrentTick != s_LastRecordedTick)
		{
			RecordTick();
			s_LastRecordedTick = CurrentTick;
		}
	}
	else
	{
		s_LastRecordedTick = -1;
	}
}

void CTasRecorder::Clear()
{
	m_vTicks.clear();
	m_vActions.clear();
	m_vMouse.clear();
	m_vDummySwitch.clear();
	m_vKill.clear();
	m_vAttackTick.clear();
	m_vPositions.clear();
	m_vChars.clear();

	m_RecordingStartTime = 0;
	m_RecordingStartTick = 0;
	m_JumpCount = 0;
	m_PrevJump = 0;
	mem_zero(m_aLastAttackTick, sizeof(m_aLastAttackTick));
	mem_zero(m_aPrevFireForAttack, sizeof(m_aPrevFireForAttack));
	mem_zero(m_aLastObservedTasAttackTick, sizeof(m_aLastObservedTasAttackTick));
}

bool CTasRecorder::IsRecording() const
{
	return g_Config.m_shalawaRecording != 0;
}

void CTasRecorder::StartRecording()
{
	if(!m_pTasWorld)
		return;

	// First, enable TAS World (required for TAS to work)
	if(!g_Config.m_shalawaTasWorld)
		g_Config.m_shalawaTasWorld = 1;
	
	// TAS Mode is always 1 (timed mode) in original project
	g_Config.m_shalawaTasMode = 1;
	
	// Clear any previous recording
	Clear();

	// Initialize TAS World if not already initialized
	if(!g_Config.m_shalawaTasInit)
	{
		g_Config.m_shalawaTasInit = 0; // Reset flag first
		if(!m_pTasWorld->IsInitialized())
		{
			m_pTasWorld->Init();
		}
	}

	// If initialization failed, wait for next frame
	if(!m_pTasWorld->IsInitialized())
		return;

	// Now start recording
	m_RecordingStartTime = time_get();
	m_RecordingStartTick = Client()->GameTick(g_Config.m_ClDummy);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_pTasWorld->World()->GetCharacterById(i))
		{
			STasCharacterHistory Entry;
			Entry.m_Id = i;
			m_vChars.push_back(Entry);
		}
	}

	// Mark as recording and initialized
	g_Config.m_shalawaRecording = 1;
	g_Config.m_shalawaTasInit = 1;
}

void CTasRecorder::StopRecording()
{
	g_Config.m_shalawaRecording = 0;
	// Don't disable TAS World automatically - let user control it
	// g_Config.m_shalawaTasWorld = 0;
	// g_Config.m_shalawaTasInit = 0;
}

void CTasRecorder::RecordTick()
{
	if(!m_pTasWorld || !m_pTasWorld->World())
		return;

	const int Dummy = g_Config.m_ClDummy;
	CGameWorld *pWorld = m_pTasWorld->World();

	// Only record when TAS World actually ticks (controlled by TickSpeed)
	// This ensures we record at the correct speed
	m_vTicks.push_back(pWorld->m_GameTick);
	m_vActions.push_back(GameClient()->m_Controls.m_aInputData[Dummy]);
	m_vMouse.push_back(GameClient()->m_Controls.m_aMousePos[Dummy]);
	m_vDummySwitch.push_back(Dummy);
	m_vKill.push_back(0);
	m_vAttackTick.push_back(m_aLastAttackTick[Dummy]);

	const int LocalId = GameClient()->m_aLocalIds[Dummy];
	if(LocalId >= 0)
	{
		CCharacter *pChar = pWorld->GetCharacterById(LocalId);
		if(pChar)
			m_vPositions.push_back(pChar->m_Pos);
		else
			m_vPositions.push_back(vec2(0, 0));
	}
	else
	{
		m_vPositions.push_back(vec2(0, 0));
	}

	for(auto &Entry : m_vChars)
	{
		CCharacter *pChar = pWorld->GetCharacterById(Entry.m_Id);
		if(pChar)
		{
			Entry.m_Positions.push_back(pChar->m_Pos);
			Entry.m_PrevPositions.push_back(pChar->m_PrevPos);
			Entry.m_FreezeTime.push_back(pChar->m_FreezeTime);
			Entry.m_ReloadTimer.push_back(pChar->GetReloadTimer());
			Entry.m_Cores.push_back(*pChar->Core());
		}
	}
}

void CTasRecorder::StepBack()
{
	if(!m_pTasWorld || !m_pTasWorld->World() || m_vActions.empty())
		return;

	CGameWorld *pWorld = m_pTasWorld->World();

	const int LastTick = m_vTicks.back();
	while(!m_vTicks.empty() && m_vTicks.back() == LastTick)
	{
		m_vActions.pop_back();
		m_vAttackTick.pop_back();
		m_vMouse.pop_back();
		m_vDummySwitch.pop_back();
		m_vKill.pop_back();
		m_vTicks.pop_back();
		if(!m_vPositions.empty())
			m_vPositions.pop_back();
	}

	for(auto &Entry : m_vChars)
	{
		if(!Entry.m_Positions.empty())
			Entry.m_Positions.pop_back();
		if(!Entry.m_PrevPositions.empty())
			Entry.m_PrevPositions.pop_back();
		if(!Entry.m_FreezeTime.empty())
			Entry.m_FreezeTime.pop_back();
		if(!Entry.m_ReloadTimer.empty())
			Entry.m_ReloadTimer.pop_back();
		if(!Entry.m_Cores.empty())
			Entry.m_Cores.pop_back();
	}

	if(m_vTicks.empty())
		return;

	const int Last = (int)m_vTicks.size() - 1;
	pWorld->m_GameTick = m_vTicks[Last];

	if(g_Config.m_shalawaTasLegitMode)
	{
		for(int DummyIdx = 0; DummyIdx < NUM_DUMMIES; DummyIdx++)
		{
			for(int i = (int)m_vTicks.size() - 1; i >= 0; i--)
			{
				if(m_vDummySwitch[i] == DummyIdx)
				{
					const vec2 LastMouse = m_vMouse[i];
					GameClient()->m_Controls.m_aMousePos[DummyIdx] = LastMouse;
					GameClient()->m_Controls.m_aInputData[DummyIdx] = m_vActions[i];
					GameClient()->m_Controls.m_aInputData[DummyIdx].m_TargetX = (int)LastMouse.x;
					GameClient()->m_Controls.m_aInputData[DummyIdx].m_TargetY = (int)LastMouse.y;
					break;
				}
			}
		}
	}
	GameClient()->m_Controls.ClampMousePos();

	for(const auto &Entry : m_vChars)
	{
		CCharacter *pChar = pWorld->GetCharacterById(Entry.m_Id);
		if(!pChar || Entry.m_Cores.empty())
			continue;

		pChar->SetCore(Entry.m_Cores.back());
		pChar->m_FreezeTime = Entry.m_FreezeTime.back();
		if(!Entry.m_ReloadTimer.empty())
			pChar->SetReloadTimer(Entry.m_ReloadTimer.back());
		pChar->m_Pos = Entry.m_Positions.back();
		pChar->m_PrevPos = Entry.m_PrevPositions.back();

		int Slot = -1;
		if(Entry.m_Id == GameClient()->m_aLocalIds[0])
			Slot = 0;
		else if(Entry.m_Id == GameClient()->m_aLocalIds[1])
			Slot = 1;

		if(Slot >= 0)
		{
			for(int i = (int)m_vTicks.size() - 1; i >= 0; i--)
			{
				if(m_vDummySwitch[i] == Slot)
				{
					m_aLastAttackTick[Slot] = m_vAttackTick[i];
					break;
				}
			}
		}

		if(pChar->GetNumInputs() < 2)
		{
			CNetObj_PlayerInput RestoreInput = GameClient()->m_Controls.m_aInputData[Slot >= 0 ? Slot : g_Config.m_ClDummy];
			pChar->OnDirectInput(&RestoreInput);
			pChar->OnDirectInput(&RestoreInput);
		}
	}
}

void CTasRecorder::DetectRollback()
{
	if(!g_Config.m_shalawaTasWorld || !g_Config.m_shalawaTasInit || !m_pTasWorld || !m_pTasWorld->World())
		return;
	if(m_vActions.size() < 2)
		return;

	const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	CCharacter *pRealChar = GameClient()->m_GameWorld.GetCharacterById(LocalId);
	if(!pRealChar)
		return;

	const STasCharacterHistory *pHistory = FindCharHistory(LocalId);
	if(!pHistory || pHistory->m_Positions.size() < 2)
		return;

	static int s_LastHandledGameTick = -1;
	if(m_pTasWorld->GameTick() == s_LastHandledGameTick)
		return;

	const int LastIndex = (int)pHistory->m_Positions.size() - 1;
	const vec2 LastTasPos = pHistory->m_Positions[LastIndex];
	const vec2 PrevTasPos = pHistory->m_Positions[LastIndex - 1];
	const vec2 RealPos = pRealChar->m_Pos;

	const float Threshold = 4.0f;
	if(length(RealPos - PrevTasPos) < Threshold && length(RealPos - LastTasPos) > Threshold * 1.5f)
	{
		StepBack();
		s_LastHandledGameTick = m_pTasWorld->GameTick();
	}
}

const STasCharacterHistory *CTasRecorder::FindCharHistory(int Id) const
{
	for(const auto &Entry : m_vChars)
		if(Entry.m_Id == Id)
			return &Entry;
	return nullptr;
}

STasCharacterHistory *CTasRecorder::FindCharHistory(int Id)
{
	for(auto &Entry : m_vChars)
		if(Entry.m_Id == Id)
			return &Entry;
	return nullptr;
}

std::string CTasRecorder::GetSavePath(const char *pName) const
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "tas/%s.tas", pName);
	return std::string(aBuf);
}

bool CTasRecorder::Save(const char *pName)
{
	// TODO: реализовать сохранение
	return false;
}

bool CTasRecorder::Load(const char *pName)
{
	// TODO: реализовать загрузку
	return false;
}

// ============================================================================
// CTasPlayer Implementation
// ============================================================================

CTasPlayer::CTasPlayer()
{
	m_pRecorder = nullptr;
	m_Prepared = false;
	m_PlayIndex = 0;
	m_LastGameTick = -1;
	mem_zero(m_aCachedInput, sizeof(m_aCachedInput));
	std::fill(std::begin(m_aCachedMouse), std::end(m_aCachedMouse), vec2(0.0f, 0.0f));
	mem_zero(m_aCachedKill, sizeof(m_aCachedKill));
}

void CTasPlayer::OnReset()
{
	Reset();
}

void CTasPlayer::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	if(!m_pRecorder)
		return;

	if(g_Config.m_shalawaPlaying && !IsPlaying())
	{
		StartPlayback();
	}

	if(!g_Config.m_shalawaPlaying && IsPlaying())
	{
		StopPlayback();
	}

	if(IsPlaying())
	{
		EnsurePrepared();
		PlayTick();
	}
}

void CTasPlayer::Reset()
{
	m_Prepared = false;
	m_PlayIndex = 0;
	m_LastGameTick = -1;
	mem_zero(m_aCachedInput, sizeof(m_aCachedInput));
	std::fill(std::begin(m_aCachedMouse), std::end(m_aCachedMouse), vec2(0.0f, 0.0f));
	mem_zero(m_aCachedKill, sizeof(m_aCachedKill));
}

bool CTasPlayer::IsPlaying() const
{
	return g_Config.m_shalawaPlaying != 0;
}

void CTasPlayer::StartPlayback()
{
	if(!m_pRecorder)
		return;

	Reset();
	g_Config.m_shalawaPlaying = 1;
}

void CTasPlayer::StopPlayback()
{
	g_Config.m_shalawaPlaying = 0;
	Reset();
}

void CTasPlayer::EnsurePrepared()
{
	if(m_Prepared)
		return;

	m_Prepared = true;
	m_PlayIndex = 0;
	m_LastGameTick = -1;
	mem_zero(m_aCachedInput, sizeof(m_aCachedInput));
	std::fill(std::begin(m_aCachedMouse), std::end(m_aCachedMouse), vec2(0.0f, 0.0f));
	mem_zero(m_aCachedKill, sizeof(m_aCachedKill));
}

void CTasPlayer::PlayTick()
{
	if(!m_pRecorder)
		return;

	const int CurrentGameTick = Client()->PredGameTick(0);
	if(CurrentGameTick == m_LastGameTick)
		return;
	m_LastGameTick = CurrentGameTick;

	const size_t Count = std::min({
		m_pRecorder->m_vTicks.size(),
		m_pRecorder->m_vActions.size(),
		m_pRecorder->m_vMouse.size(),
		m_pRecorder->m_vDummySwitch.size(),
		m_pRecorder->m_vKill.size()});

	if(m_PlayIndex >= Count)
	{
		StopPlayback();
		return;
	}

	const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	if(LocalId >= 0 && !m_pRecorder->m_vPositions.empty() && m_PlayIndex < m_pRecorder->m_vPositions.size())
	{
		CCharacter *pRealChar = GameClient()->m_GameWorld.GetCharacterById(LocalId);
		if(pRealChar)
		{
			vec2 RealPos = pRealChar->m_Pos;
			vec2 RecordedPos = m_pRecorder->m_vPositions[m_PlayIndex];
			float Dist = length(RealPos - RecordedPos);
			if(Dist > 8.0f)
			{
				if(m_PlayIndex > 0)
				{
					float PrevDist = length(RealPos - m_pRecorder->m_vPositions[m_PlayIndex - 1]);
					if(PrevDist < Dist)
						return;
				}
			}
		}
	}

	const int RecordedDummy = m_pRecorder->m_vDummySwitch[m_PlayIndex] ? 1 : 0;
	GameClient()->m_Controls.m_aInputData[RecordedDummy] = m_pRecorder->m_vActions[m_PlayIndex];
	GameClient()->m_Controls.m_aMousePos[RecordedDummy] = m_pRecorder->m_vMouse[m_PlayIndex];

	m_PlayIndex++;
}

// ============================================================================
// CTasPredict Implementation
// ============================================================================

CTasPredict::CTasPredict()
{
	m_pTasWorld = nullptr;
}

void CTasPredict::OnReset()
{
}

void CTasPredict::PredictPath(int ClientId, int Ticks, std::vector<vec2> &vPath)
{
	vPath.clear();

	if(!m_pTasWorld || !m_pTasWorld->IsInitialized())
		return;

	CGameWorld *pWorld = m_pTasWorld->World();
	if(!pWorld)
		return;

	CCharacter *pChar = pWorld->GetCharacterById(ClientId);
	if(!pChar)
		return;

	static CGameWorld s_TempWorld;
	s_TempWorld.CopyWorld(pWorld);

	CCharacter *pTempChar = s_TempWorld.GetCharacterById(ClientId);
	if(!pTempChar)
		return;

	const int Dummy = g_Config.m_ClDummy;
	CNetObj_PlayerInput Input = GameClient()->m_Controls.m_aInputData[Dummy];

	for(int t = 0; t < Ticks; t++)
	{
		vPath.push_back(pTempChar->GetPos());
		s_TempWorld.m_GameTick++;
		pTempChar->OnPredictedInput(&Input);
		s_TempWorld.Tick();
	}
	vPath.push_back(pTempChar->GetPos());
}

bool CTasPredict::WillFreeze(int ClientId, int Ticks)
{
	if(!m_pTasWorld || !m_pTasWorld->IsInitialized())
		return false;
	if(!Collision())
		return false;

	CGameWorld *pWorld = m_pTasWorld->World();
	if(!pWorld)
		return false;

	CCharacter *pChar = pWorld->GetCharacterById(ClientId);
	if(!pChar)
		return false;

	static CGameWorld s_TempWorld;
	s_TempWorld.CopyWorld(pWorld);

	CCharacter *pTempChar = s_TempWorld.GetCharacterById(ClientId);
	if(!pTempChar)
		return false;

	const int Dummy = g_Config.m_ClDummy;
	CNetObj_PlayerInput Input = GameClient()->m_Controls.m_aInputData[Dummy];

	for(int t = 0; t < Ticks; t++)
	{
		s_TempWorld.m_GameTick++;
		pTempChar->OnPredictedInput(&Input);
		s_TempWorld.Tick();

		vec2 Pos = pTempChar->GetPos();
		int MapIndex = Collision()->GetPureMapIndex(Pos);
		int TileIndex = Collision()->GetTileIndex(MapIndex);
		int TileFIndex = Collision()->GetFrontTileIndex(MapIndex);

		if(TileIndex == TILE_FREEZE || TileFIndex == TILE_FREEZE ||
		   TileIndex == TILE_DFREEZE || TileFIndex == TILE_DFREEZE ||
		   TileIndex == TILE_LFREEZE || TileFIndex == TILE_LFREEZE)
			return true;

		if(pTempChar->m_FreezeTime > 0)
			return true;
	}

	return false;
}

// ============================================================================
// CTasRender Implementation
// ============================================================================

CTasRender::CTasRender()
{
	m_pTasWorld = nullptr;
	m_pRecorder = nullptr;
	m_pPredict = nullptr;
	m_SmoothIntra = 0.0f;
	std::fill(std::begin(m_aInterpMouse), std::end(m_aInterpMouse), vec2(0.0f, 0.0f));
}

void CTasRender::OnReset()
{
	m_SmoothIntra = 0.0f;
	std::fill(std::begin(m_aInterpMouse), std::end(m_aInterpMouse), vec2(0.0f, 0.0f));
}

void CTasRender::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	RenderStartEndPos();
	RenderReplayPath();
	RenderPredictionPath();
	RenderTasWorld();
}

vec2 CTasRender::GetInterpolatedMouseForRendering(int Dummy) const
{
	if(g_Config.m_shalawaRecording && g_Config.m_shalawaTasMode && g_Config.m_shalawaTasWorld && m_pTasWorld && m_pTasWorld->IsInitialized())
		return m_aInterpMouse[Dummy];
	return GameClient()->m_Controls.m_aMousePos[Dummy];
}

vec2 CTasRender::GetInterpolatedPlayerPosition(int Dummy) const
{
	if(!g_Config.m_shalawaRecording || !g_Config.m_shalawaTasMode || !g_Config.m_shalawaTasWorld)
		return GameClient()->m_LocalCharacterPos;

	if(!m_pTasWorld || !m_pTasWorld->IsInitialized())
		return GameClient()->m_LocalCharacterPos;

	const int LocalId = GameClient()->m_aLocalIds[Dummy];
	if(LocalId < 0)
		return GameClient()->m_LocalCharacterPos;

	CCharacter *pCharCommitted = m_pTasWorld->World()->GetCharacterById(LocalId);
	if(!pCharCommitted)
		return GameClient()->m_LocalCharacterPos;

	CCharacter *pCharPredicted = m_pTasWorld->PredictedWorld() ? m_pTasWorld->PredictedWorld()->GetCharacterById(LocalId) : nullptr;
	if(pCharPredicted)
	{
		const vec2 PrevPos = pCharCommitted->m_Pos;
		const vec2 CurPos = pCharPredicted->m_Pos;
		float Intra = std::clamp(m_SmoothIntra, 0.0f, 1.0f);
		return mix(PrevPos, CurPos, Intra);
	}

	return pCharCommitted->m_Pos;
}

void CTasRender::RenderStartEndPos()
{
	if(!g_Config.m_shalawaTasDrawStartEndPos)
		return;
	if(!m_pRecorder || m_pRecorder->m_vPositions.empty())
		return;
	if(g_Config.m_shalawaTasWorld)
		return;

	for(int DummyIdx = 0; DummyIdx < NUM_DUMMIES; DummyIdx++)
	{
		const int LocalId = GameClient()->m_aLocalIds[DummyIdx];
		if(LocalId < 0)
			continue;

		CTeeRenderInfo TeeInfo = GameClient()->m_aClients[LocalId].m_RenderInfo;
		TeeInfo.m_Size = 64.0f;
		const CAnimState *pIdleState = CAnimState::GetIdle();

		vec2 StartPos = vec2(0, 0);
		bool FoundStart = false;
		for(size_t i = 0; i < m_pRecorder->m_vDummySwitch.size(); i++)
		{
			if(m_pRecorder->m_vDummySwitch[i] == DummyIdx)
			{
				StartPos = m_pRecorder->m_vPositions[i];
				FoundStart = true;
				break;
			}
		}

		if(FoundStart)
		{
			CTeeRenderInfo GreenTeeInfo = TeeInfo;
			GreenTeeInfo.m_ColorBody = ColorRGBA(0.0f, 1.0f, 0.0f, 1.0f);
			GreenTeeInfo.m_ColorFeet = ColorRGBA(0.0f, 1.0f, 0.0f, 1.0f);
			GreenTeeInfo.m_CustomColoredSkin = true;
			GameClient()->RenderTools()->RenderTee(pIdleState, &GreenTeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), StartPos);
		}

		if(m_pRecorder->m_vPositions.size() > 1 && !g_Config.m_shalawaRecording)
		{
			vec2 EndPos = vec2(0, 0);
			bool FoundEnd = false;
			for(int i = (int)m_pRecorder->m_vDummySwitch.size() - 1; i >= 0; i--)
			{
				if(m_pRecorder->m_vDummySwitch[i] == DummyIdx)
				{
					EndPos = m_pRecorder->m_vPositions[i];
					FoundEnd = true;
					break;
				}
			}

			if(FoundEnd)
			{
				CTeeRenderInfo PurpleTeeInfo = TeeInfo;
				PurpleTeeInfo.m_ColorBody = ColorRGBA(1.0f, 0.0f, 1.0f, 1.0f);
				PurpleTeeInfo.m_ColorFeet = ColorRGBA(1.0f, 0.0f, 1.0f, 1.0f);
				PurpleTeeInfo.m_CustomColoredSkin = true;
				GameClient()->RenderTools()->RenderTee(pIdleState, &PurpleTeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), EndPos);
			}
		}
	}
}

void CTasRender::RenderReplayPath()
{
	if(!g_Config.m_shalawaTasDrawPath)
		return;
	if(!m_pRecorder || m_pRecorder->m_vPositions.empty())
		return;
	if(!g_Config.m_shalawaPlaying || g_Config.m_shalawaRecording)
		return;

	Graphics()->TextureClear();
	ColorRGBA PathCol = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_shalawaTasDrawPathColor));
	const float PathWidth = (float)g_Config.m_shalawaTasDrawPathWidth;

	for(int DummyIdx = 0; DummyIdx < NUM_DUMMIES; DummyIdx++)
	{
		const int NumEntries = (int)m_pRecorder->m_vPositions.size();
		static std::vector<vec2> s_vFilteredPath;
		s_vFilteredPath.clear();

		for(int i = 0; i < NumEntries; i++)
		{
			if(m_pRecorder->m_vDummySwitch[i] == DummyIdx)
				s_vFilteredPath.push_back(m_pRecorder->m_vPositions[i]);
		}

		if(s_vFilteredPath.empty())
			continue;

		const int NumPoints = (int)s_vFilteredPath.size();
		const int StartIdx = std::max(0, NumPoints - 200);
		const int EndIdx = NumPoints - 1;

		// Always segmented full line: erase older path behind the player.
		Graphics()->LinesBegin();
		Graphics()->SetColor(PathCol.r, PathCol.g, PathCol.b, PathCol.a);
		for(int i = StartIdx; i < EndIdx; i++)
		{
			vec2 Pos1 = s_vFilteredPath[i];
			vec2 Pos2 = s_vFilteredPath[i + 1];
			IGraphics::CLineItem LineItem(Pos1.x, Pos1.y, Pos2.x, Pos2.y);
			Graphics()->LinesDraw(&LineItem, 1);
		}
		Graphics()->LinesEnd();
	}
}

void CTasRender::RenderPredictionPath()
{
	if(!g_Config.m_shalawaPredictTas)
		return;
	if(!g_Config.m_shalawaRecording || !g_Config.m_shalawaTasWorld)
		return;
	if(!m_pTasWorld || !m_pTasWorld->IsInitialized())
		return;

	for(int DummyIdx = 0; DummyIdx < NUM_DUMMIES; DummyIdx++)
	{
		const int LocalId = GameClient()->m_aLocalIds[DummyIdx];
		if(LocalId < 0)
			continue;

		CCharacter *pTasChar = m_pTasWorld->World()->GetCharacterById(LocalId);
		if(!pTasChar)
			continue;

		std::vector<vec2> Trajectory;
		if(m_pPredict)
			m_pPredict->PredictPath(LocalId, 30, Trajectory);

		if(Trajectory.empty())
			continue;

		Graphics()->TextureClear();
		Graphics()->LinesBegin();
		ColorRGBA PredictCol = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_shalawaTasPredictColor));
		ColorRGBA FrozenCol = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_shalawaTasPredictColorFrozen));

		for(size_t j = 1; j < Trajectory.size(); ++j)
		{
			vec2 Start = Trajectory[j - 1];
			vec2 End = Trajectory[j];
			ColorRGBA LineColor = PredictCol;

			if(Collision())
			{
				int TileStart = Collision()->GetTileIndex(Collision()->GetPureMapIndex(Start));
				if(TileStart == TILE_FREEZE || TileStart == TILE_DFREEZE || TileStart == TILE_LFREEZE)
					LineColor = FrozenCol;
			}

			Graphics()->SetColor(LineColor.r, LineColor.g, LineColor.b, LineColor.a);
			IGraphics::CLineItem LineItem(Start.x, Start.y, End.x, End.y);
			Graphics()->LinesDraw(&LineItem, 1);
		}
		Graphics()->LinesEnd();
	}
}

void CTasRender::RenderTasWorld()
{
	if(!g_Config.m_shalawaTasWorld || !g_Config.m_shalawaTasInit)
		return;
	if(!m_pTasWorld || !m_pTasWorld->IsInitialized())
		return;
	if(!m_pRecorder)
		return;

	float Intra = std::clamp(m_SmoothIntra, 0.0f, 1.0f);
	CGameWorld *pRenderWorld = m_pTasWorld->PredictedWorld() ? m_pTasWorld->PredictedWorld() : m_pTasWorld->World();

	for(const auto &Entry : m_pRecorder->m_vChars)
	{
		const int ClientId = Entry.m_Id;
		if(ClientId < 0 || ClientId >= MAX_CLIENTS)
			continue;

		CCharacter *pCharCommitted = m_pTasWorld->World()->GetCharacterById(ClientId);
		CCharacter *pCharPredicted = m_pTasWorld->PredictedWorld() ? m_pTasWorld->PredictedWorld()->GetCharacterById(ClientId) : nullptr;
		if(!pCharCommitted)
			continue;

		CTeeRenderInfo TeeInfo = GameClient()->m_aClients[ClientId].m_RenderInfo;
		TeeInfo.m_Size = 64.0f;

		const CCharacterCore &PrevCore = *pCharCommitted->Core();
		const CCharacterCore &CurCore = pCharPredicted ? *pCharPredicted->Core() : PrevCore;
		const vec2 PrevPos = pCharCommitted->m_Pos;
		const vec2 CurPos = pCharPredicted ? pCharPredicted->m_Pos : PrevPos;

		vec2 RenderPos = mix(PrevPos, CurPos, Intra);

		const CAnimState *pIdleState = CAnimState::GetIdle();
		vec2 Dir = length(CurCore.m_Vel) > 0.001f ? normalize(CurCore.m_Vel) : vec2(1.0f, 0.0f);
		GameClient()->RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_NORMAL, Dir, RenderPos);
	}
}

