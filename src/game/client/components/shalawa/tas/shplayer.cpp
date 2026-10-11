#include "shplayer.h"
#include "shrecorder.h"

#include <base/vmath.h>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>

#include <algorithm>

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