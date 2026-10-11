#include "shrender.h"
#include "shworld.h"
#include "shrecorder.h"
#include "shpredict.h"

#include <base/color.h>
#include <base/vmath.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/entities/laser.h>
#include <game/client/prediction/entities/projectile.h>
#include <game/client/render.h>
#include <game/collision.h>
#include <game/client/laser_data.h>
#include <game/client/projectile_data.h>

#include <algorithm>

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
		int StartIdx = 0;
		int EndIdx = NumPoints - 1;

		if(g_Config.m_shalawaTasDrawPathSegmented)
		{
			int PlaybackPointIdx = 0;
			// This would need access to player playback index
			StartIdx = std::max(0, NumPoints - 200);
		}

		if(g_Config.m_shalawaTasDrawPathMode == 0)
		{
			Graphics()->QuadsBegin();
			Graphics()->SetColor(PathCol.r, PathCol.g, PathCol.b, PathCol.a);
			const float HalfWidth = PathWidth * 0.5f;
			for(int i = StartIdx; i <= EndIdx; i++)
			{
				if(i % 2 == 0)
					Graphics()->DrawRectExt(s_vFilteredPath[i].x - HalfWidth, s_vFilteredPath[i].y - HalfWidth, PathWidth, PathWidth, 0.0f, IGraphics::CORNER_NONE);
			}
			Graphics()->QuadsEnd();
		}
		else
		{
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

void CTasRecorder::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	
	if(!m_pTasWorld)
		return;

	if(g_Config.m_shalawaRecording && !IsRecording())
	{
		StartRecording();
	}
	
	if(!g_Config.m_shalawaRecording && IsRecording())
	{
		StopRecording();
	}

	if(IsRecording() && m_pTasWorld->IsInitialized() && !g_Config.m_shalawaTasPause)
	{
		RecordTick();
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