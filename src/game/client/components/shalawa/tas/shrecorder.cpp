#include "shrecorder.h"
#include "shworld.h"

#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>

#include <fstream>

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

	Clear();

	if(!m_pTasWorld->IsInitialized())
		m_pTasWorld->Init();

	if(!m_pTasWorld->IsInitialized())
		return;

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

	g_Config.m_shalawaRecording = 1;
	g_Config.m_shalawaTasWorld = 1;
	g_Config.m_shalawaTasInit = 1;
}

void CTasRecorder::StopRecording()
{
	g_Config.m_shalawaRecording = 0;
	g_Config.m_shalawaTasWorld = 0;
	g_Config.m_shalawaTasInit = 0;
}

void CTasRecorder::RecordTick()
{
	if(!m_pTasWorld || !m_pTasWorld->World())
		return;

	const int Dummy = g_Config.m_ClDummy;
	CGameWorld *pWorld = m_pTasWorld->World();

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