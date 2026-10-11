/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "hud.h"

#include "binds.h"
#include "camera.h"
#include "controls.h"
#include "voting.h"

#include <base/color.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/scoreboard.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/layers.h>
#include <game/localization.h>

#include <cmath>

static constexpr ColorRGBA TEXT_WHITE(1.f, 1.f, 1.f, 1.f);
static constexpr ColorRGBA TEXT_OUTLINE(0.f, 0.f, 0.f, 0.3f);

static void RenderTextContainerDefault(ITextRender *pTR, STextContainerIndex &Idx)
{
	if(Idx.Valid())
		pTR->RenderTextContainer(Idx, TEXT_WHITE, TEXT_OUTLINE);
}

CHud::CHud()
{
	m_FPSTextContainerIndex.Reset();
	m_DDRaceEffectsTextContainerIndex.Reset();
	m_PlayerAngleTextContainerIndex.Reset();
	m_PlayerPrevAngle = -INFINITY;
	for(int i = 0; i < 2; i++)
	{
		m_aPlayerSpeedTextContainers[i].Reset();
		m_aPlayerPrevSpeed[i] = -INFINITY;
		m_aPlayerPositionContainers[i].Reset();
		m_aPlayerPrevPosition[i] = -INFINITY;
	}
}

void CHud::ResetHudContainers()
{
	for(auto &SI : m_aScoreInfo)
	{
		TextRender()->DeleteTextContainer(SI.m_OptionalNameTextContainerIndex);
		TextRender()->DeleteTextContainer(SI.m_TextRankContainerIndex);
		TextRender()->DeleteTextContainer(SI.m_TextScoreContainerIndex);
		Graphics()->DeleteQuadContainer(SI.m_RoundRectQuadContainerIndex);
		SI.Reset();
	}
	TextRender()->DeleteTextContainer(m_FPSTextContainerIndex);
	TextRender()->DeleteTextContainer(m_DDRaceEffectsTextContainerIndex);
	TextRender()->DeleteTextContainer(m_PlayerAngleTextContainerIndex);
	m_PlayerPrevAngle = -INFINITY;
	for(int i = 0; i < 2; i++)
	{
		TextRender()->DeleteTextContainer(m_aPlayerSpeedTextContainers[i]);
		m_aPlayerPrevSpeed[i] = -INFINITY;
		TextRender()->DeleteTextContainer(m_aPlayerPositionContainers[i]);
		m_aPlayerPrevPosition[i] = -INFINITY;
	}
}

void CHud::OnWindowResize() { ResetHudContainers(); }

void CHud::OnReset()
{
	m_TimeCpDiff = 0.0f;
	m_DDRaceTime = 0;
	m_FinishTimeLastReceivedTick = 0;
	m_TimeCpLastReceivedTick = 0;
	m_ShowFinishTime = false;
	m_aPlayerRecord[0] = m_aPlayerRecord[1] = -1.0f;
	m_aPlayerSpeed[0] = m_aPlayerSpeed[1] = 0;
	m_aLastPlayerSpeedChange[0] = m_aLastPlayerSpeedChange[1] = ESpeedChange::NONE;
	m_LastSpectatorCountTick = 0;
	ResetHudContainers();
}

void CHud::OnInit()
{
	OnReset();
	Graphics()->SetColor(1.0, 1.0, 1.0, 1.0);
	m_HudQuadContainerIndex = Graphics()->CreateQuadContainer(false);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	PrepareAmmoHealthAndArmorQuads();
	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		float ScaleX, ScaleY;
		Graphics()->GetSpriteScale(g_pData->m_Weapons.m_aId[i].m_pSpriteCursor, ScaleX, ScaleY);
		m_aCursorOffset[i] = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 64.f * ScaleX, 64.f * ScaleY);
	}
	m_FlagOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 8.f, 16.f);
	PreparePlayerStateQuads();
	Graphics()->QuadContainerUpload(m_HudQuadContainerIndex);
}

void CHud::RenderGameTimer()
{
	const auto *pGI = GameClient()->m_Snap.m_pGameInfoObj;
	if(pGI->m_GameStateFlags & GAMESTATEFLAG_SUDDENDEATH)
		return;

	int Dummy = g_Config.m_ClDummy;
	int GameTick = Client()->GameTick(Dummy);
	int TickSpeed = Client()->GameTickSpeed();

	int Time = 0;
	if(pGI->m_TimeLimit && pGI->m_WarmupTimer <= 0)
	{
		Time = pGI->m_TimeLimit * 60 - (GameTick - pGI->m_RoundStartTick) / TickSpeed;
		if(pGI->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER) Time = 0;
	}
	else if(pGI->m_GameStateFlags & GAMESTATEFLAG_RACETIME)
		Time = (GameTick + pGI->m_WarmupTimer) / TickSpeed;
	else
		Time = (GameTick - pGI->m_RoundStartTick) / TickSpeed;

	char aBuf[32];
	str_time((int64_t)Time * 100, TIME_DAYS, aBuf, sizeof(aBuf));

	const float FS = 10.0f;
	static float s_WM = TextRender()->TextWidth(FS, "00:00");
	static float s_WH = TextRender()->TextWidth(FS, "00:00:00");
	static float s_W0D = TextRender()->TextWidth(FS, "0d 00:00:00");
	static float s_W00D = TextRender()->TextWidth(FS, "00d 00:00:00");
	static float s_W000D = TextRender()->TextWidth(FS, "000d 00:00:00");

	float w = Time >= 3600*24*100 ? s_W000D : Time >= 3600*24*10 ? s_W00D : Time >= 3600*24 ? s_W0D : Time >= 3600 ? s_WH : s_WM;

	if(pGI->m_TimeLimit && Time <= 60 && pGI->m_WarmupTimer <= 0)
	{
		float a = (Time <= 10 && (2 * time() / time_freq()) % 2) ? 0.5f : 1.0f;
		TextRender()->TextColor(1.0f, 0.25f, 0.25f, a);
	}
	TextRender()->Text(m_Width / 2.0f - w / 2, 2, FS, aBuf);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CHud::RenderPauseNotification()
{
	const auto *pGI = GameClient()->m_Snap.m_pGameInfoObj;
	if(!(pGI->m_GameStateFlags & GAMESTATEFLAG_PAUSED) || (pGI->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
		return;
	const char *pText = Localize("Game paused");
	float FS = 20.0f;
	TextRender()->Text(150.0f * Graphics()->ScreenAspect() - TextRender()->TextWidth(FS, pText) / 2.0f, 50.0f, FS, pText);
}

void CHud::RenderSuddenDeath()
{
	if(!(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_SUDDENDEATH))
		return;
	const char *pText = Localize("Sudden Death");
	float FS = 12.0f;
	TextRender()->Text(m_Width / 2.0f - TextRender()->TextWidth(FS, pText) / 2, 2, FS, pText);
}

void CHud::RenderScoreHud()
{
	auto *pGC = GameClient();
	if(pGC->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER)
		return;

	float StartY = 229.0f;
	const float BoxH = 18.0f;
	bool ForceInit = !m_aScoreInfo[0].m_Initialized || !m_aScoreInfo[1].m_Initialized;
	m_aScoreInfo[0].m_Initialized = m_aScoreInfo[1].m_Initialized = true;

	if(pGC->IsTeamPlay() && pGC->m_Snap.m_pGameDataObj)
	{
		const auto *pGD = pGC->m_Snap.m_pGameDataObj;
		char aScore[2][16];
		str_format(aScore[TEAM_RED], sizeof(aScore[0]), "%d", pGD->m_TeamscoreRed);
		str_format(aScore[TEAM_BLUE], sizeof(aScore[1]), "%d", pGD->m_TeamscoreBlue);

		bool aRecreate[2] = {
			str_comp(aScore[0], m_aScoreInfo[0].m_aScoreText) != 0,
			str_comp(aScore[1], m_aScoreInfo[1].m_aScoreText) != 0
		};
		const int aCarrier[2] = {pGD->m_FlagCarrierRed, pGD->m_FlagCarrierBlue};

		bool RecreateRect = ForceInit;
		for(int t = 0; t < 2; t++)
		{
			if(aRecreate[t])
			{
				const char *pS = aScore[t == 0 ? TEAM_RED : TEAM_BLUE];
				m_aScoreInfo[t].m_ScoreTextWidth = TextRender()->TextWidth(14.0f, pS);
				str_copy(m_aScoreInfo[t].m_aScoreText, pS);
				RecreateRect = true;
			}
		}

		static float s_W100 = TextRender()->TextWidth(14.0f, "100");
		float ScoreWMax = maximum(maximum(m_aScoreInfo[0].m_ScoreTextWidth, m_aScoreInfo[1].m_ScoreTextWidth), s_W100);
		float Split = 3.0f;
		float ImgSz = (pGC->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS) ? 16.0f : Split;

		for(int t = 0; t < 2; t++)
		{
			if(RecreateRect)
			{
				Graphics()->DeleteQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex);
				Graphics()->SetColor(t == 0 ? ColorRGBA(0.975f, 0.17f, 0.17f, 0.3f) : ColorRGBA(0.17f, 0.46f, 0.975f, 0.3f));
				m_aScoreInfo[t].m_RoundRectQuadContainerIndex = Graphics()->CreateRectQuadContainer(m_Width - ScoreWMax - ImgSz - 2*Split, StartY + t*20, ScoreWMax + ImgSz + 2*Split, BoxH, 5.0f, IGraphics::CORNER_L);
			}
			Graphics()->TextureClear();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			if(m_aScoreInfo[t].m_RoundRectQuadContainerIndex != -1)
				Graphics()->RenderQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex, -1);

			if(aRecreate[t])
			{
				const char *pS = aScore[t == 0 ? TEAM_RED : TEAM_BLUE];
				CTextCursor Cursor;
				Cursor.SetPosition(vec2(m_Width - ScoreWMax + (ScoreWMax - m_aScoreInfo[t].m_ScoreTextWidth) / 2 - Split, StartY + t*20 + (18.f-14.f)/2.f));
				Cursor.m_FontSize = 14.0f;
				TextRender()->RecreateTextContainer(m_aScoreInfo[t].m_TextScoreContainerIndex, &Cursor, pS);
			}
			RenderTextContainerDefault(TextRender(), m_aScoreInfo[t].m_TextScoreContainerIndex);

			if(pGC->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS)
			{
				int BlinkTimer = (pGC->m_aFlagDropTick[t] != 0 && (Client()->GameTick(g_Config.m_ClDummy) - pGC->m_aFlagDropTick[t]) / Client()->GameTickSpeed() >= 25) ? 10 : 20;
				if(aCarrier[t] == FLAG_ATSTAND || (aCarrier[t] == FLAG_TAKEN && ((Client()->GameTick(g_Config.m_ClDummy) / BlinkTimer) & 1)))
				{
					Graphics()->TextureSet(t == 0 ? pGC->m_GameSkin.m_SpriteFlagRed : pGC->m_GameSkin.m_SpriteFlagBlue);
					Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
					Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_FlagOffset, m_Width - ScoreWMax - ImgSz, StartY + 1.0f + t*20);
				}
				else if(aCarrier[t] >= 0)
				{
					int Id = aCarrier[t] % MAX_CLIENTS;
					const char *pName = pGC->m_aClients[Id].m_aName;
					if(str_comp(pName, m_aScoreInfo[t].m_aPlayerNameText) != 0 || RecreateRect)
					{
						str_copy(m_aScoreInfo[t].m_aPlayerNameText, pName);
						float w = TextRender()->TextWidth(8.0f, pName);
						CTextCursor Cursor;
						Cursor.SetPosition(vec2(minimum(m_Width - w - 1.0f, m_Width - ScoreWMax - ImgSz - 2*Split), StartY + (t+1)*20.0f - 2.0f));
						Cursor.m_FontSize = 8.0f;
						TextRender()->RecreateTextContainer(m_aScoreInfo[t].m_OptionalNameTextContainerIndex, &Cursor, pName);
					}
					RenderTextContainerDefault(TextRender(), m_aScoreInfo[t].m_OptionalNameTextContainerIndex);

					CTeeRenderInfo TeeInfo = pGC->m_aClients[Id].m_RenderInfo;
					TeeInfo.m_Size = BoxH;
					const CAnimState *pIdle = CAnimState::GetIdle();
					vec2 Off;
					CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdle, &TeeInfo, Off);
					RenderTools()->RenderTee(pIdle, &TeeInfo, EMOTE_NORMAL, vec2(1,0), vec2(m_Width - ScoreWMax - TeeInfo.m_Size/2 - Split, StartY + t*20 + BoxH/2.0f + Off.y));
				}
			}
			StartY += 8.0f;
		}
	}
	else
	{
		int Local = -1;
		int aPos[2] = {1, 2};
		const CNetObj_PlayerInfo *apInfo[2] = {};
		int i = 0;
		for(int t = 0; t < 2 && i < MAX_CLIENTS && pGC->m_Snap.m_apInfoByScore[i]; ++i)
		{
			if(pGC->m_Snap.m_apInfoByScore[i]->m_Team != TEAM_SPECTATORS)
			{
				apInfo[t] = pGC->m_Snap.m_apInfoByScore[i];
				if(apInfo[t]->m_ClientId == pGC->m_Snap.m_LocalClientId) Local = t;
				++t;
			}
		}
		if(Local == -1 && pGC->m_Snap.m_pLocalInfo && pGC->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS)
		{
			for(; i < MAX_CLIENTS && pGC->m_Snap.m_apInfoByScore[i]; ++i)
			{
				if(pGC->m_Snap.m_apInfoByScore[i]->m_Team != TEAM_SPECTATORS) ++aPos[1];
				if(pGC->m_Snap.m_apInfoByScore[i]->m_ClientId == pGC->m_Snap.m_LocalClientId)
				{ apInfo[1] = pGC->m_Snap.m_apInfoByScore[i]; Local = 1; break; }
			}
		}

		char aScore[2][16];
		for(int t = 0; t < 2; ++t)
		{
			if(!apInfo[t]) { aScore[t][0] = 0; continue; }
			if(Client()->IsSixup() && pGC->m_Snap.m_pGameInfoObj->m_GameFlags & protocol7::GAMEFLAG_RACE)
				str_time((int64_t)absolute(apInfo[t]->m_Score) / 10, TIME_MINS_CENTISECS, aScore[t], sizeof(aScore[t]));
			else if(pGC->m_GameInfo.m_TimeScore)
			{
				if(apInfo[t]->m_Score != FinishTime::NOT_FINISHED_TIMESCORE)
					str_time((int64_t)absolute(apInfo[t]->m_Score) * 100, TIME_HOURS, aScore[t], sizeof(aScore[t]));
				else aScore[t][0] = 0;
			}
			else str_format(aScore[t], sizeof(aScore[t]), "%d", apInfo[t]->m_Score);
		}

		bool RecreateScores = str_comp(aScore[0], m_aScoreInfo[0].m_aScoreText) != 0 ||
			str_comp(aScore[1], m_aScoreInfo[1].m_aScoreText) != 0 ||
			m_LastLocalClientId != pGC->m_Snap.m_LocalClientId;
		m_LastLocalClientId = pGC->m_Snap.m_LocalClientId;

		bool RecreateRect = ForceInit;
		for(int t = 0; t < 2; t++)
		{
			if(RecreateScores) { m_aScoreInfo[t].m_ScoreTextWidth = TextRender()->TextWidth(14.0f, aScore[t]); str_copy(m_aScoreInfo[t].m_aScoreText, aScore[t]); RecreateRect = true; }
			if(apInfo[t]) { int Id = apInfo[t]->m_ClientId; if(Id >= 0 && Id < MAX_CLIENTS && str_comp(pGC->m_aClients[Id].m_aName, m_aScoreInfo[t].m_aPlayerNameText) != 0) RecreateRect = true; }
			else if(m_aScoreInfo[t].m_aPlayerNameText[0]) RecreateRect = true;
			char aBuf[16]; str_format(aBuf, sizeof(aBuf), "%d.", aPos[t]);
			if(str_comp(aBuf, m_aScoreInfo[t].m_aRankText) != 0) RecreateRect = true;
		}

		static float s_W10 = TextRender()->TextWidth(14.0f, "10");
		float ScoreWMax = maximum(maximum(m_aScoreInfo[0].m_ScoreTextWidth, m_aScoreInfo[1].m_ScoreTextWidth), s_W10);
		float Split = 3.0f, ImgSz = 16.0f, PosSize = 16.0f;

		for(int t = 0; t < 2; t++)
		{
			if(RecreateRect)
			{
				Graphics()->DeleteQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex);
				Graphics()->SetColor(t == Local ? ColorRGBA(1,1,1,0.25f) : ColorRGBA(0,0,0,0.25f));
				m_aScoreInfo[t].m_RoundRectQuadContainerIndex = Graphics()->CreateRectQuadContainer(m_Width - ScoreWMax - ImgSz - 2*Split - PosSize, StartY + t*20, ScoreWMax + ImgSz + 2*Split + PosSize, BoxH, 5.0f, IGraphics::CORNER_L);
			}
			Graphics()->TextureClear();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			if(m_aScoreInfo[t].m_RoundRectQuadContainerIndex != -1)
				Graphics()->RenderQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex, -1);

			if(RecreateScores)
			{
				CTextCursor Cursor;
				Cursor.SetPosition(vec2(m_Width - ScoreWMax + (ScoreWMax - m_aScoreInfo[t].m_ScoreTextWidth) - Split, StartY + t*20 + (18.f-14.f)/2.f));
				Cursor.m_FontSize = 14.0f;
				TextRender()->RecreateTextContainer(m_aScoreInfo[t].m_TextScoreContainerIndex, &Cursor, aScore[t]);
			}
			RenderTextContainerDefault(TextRender(), m_aScoreInfo[t].m_TextScoreContainerIndex);

			if(apInfo[t])
			{
				int Id = apInfo[t]->m_ClientId;
				if(Id >= 0 && Id < MAX_CLIENTS)
				{
					const char *pName = pGC->m_aClients[Id].m_aName;
					if(RecreateRect)
					{
						str_copy(m_aScoreInfo[t].m_aPlayerNameText, pName);
						CTextCursor Cursor;
						Cursor.SetPosition(vec2(minimum(m_Width - TextRender()->TextWidth(8.0f, pName) - 1.0f, m_Width - ScoreWMax - ImgSz - 2*Split - PosSize), StartY + (t+1)*20.0f - 2.0f));
						Cursor.m_FontSize = 8.0f;
						TextRender()->RecreateTextContainer(m_aScoreInfo[t].m_OptionalNameTextContainerIndex, &Cursor, pName);
					}
					RenderTextContainerDefault(TextRender(), m_aScoreInfo[t].m_OptionalNameTextContainerIndex);

					CTeeRenderInfo TeeInfo = pGC->m_aClients[Id].m_RenderInfo;
					TeeInfo.m_Size = BoxH;
					const CAnimState *pIdle = CAnimState::GetIdle();
					vec2 Off;
					CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdle, &TeeInfo, Off);
					RenderTools()->RenderTee(pIdle, &TeeInfo, EMOTE_NORMAL, vec2(1,0), vec2(m_Width - ScoreWMax - TeeInfo.m_Size/2 - Split, StartY + t*20 + BoxH/2.0f + Off.y));
				}
			}
			else m_aScoreInfo[t].m_aPlayerNameText[0] = 0;

			char aBuf[16]; str_format(aBuf, sizeof(aBuf), "%d.", aPos[t]);
			if(RecreateRect)
			{
				str_copy(m_aScoreInfo[t].m_aRankText, aBuf);
				CTextCursor Cursor;
				Cursor.SetPosition(vec2(m_Width - ScoreWMax - ImgSz - Split - PosSize, StartY + t*20 + (18.f-10.f)/2.f));
				Cursor.m_FontSize = 10.0f;
				TextRender()->RecreateTextContainer(m_aScoreInfo[t].m_TextRankContainerIndex, &Cursor, aBuf);
			}
			RenderTextContainerDefault(TextRender(), m_aScoreInfo[t].m_TextRankContainerIndex);
			StartY += 8.0f;
		}
	}
}

void CHud::RenderWarmupTimer()
{
	const auto *pGI = GameClient()->m_Snap.m_pGameInfoObj;
	if(pGI->m_WarmupTimer <= 0 || (pGI->m_GameStateFlags & GAMESTATEFLAG_RACETIME))
		return;

	float FS = 20.0f;
	float AspX = 150 * Graphics()->ScreenAspect();
	TextRender()->Text(AspX - TextRender()->TextWidth(FS, Localize("Warmup")) / 2, 50, FS, Localize("Warmup"));

	char aBuf[256];
	int Sec = pGI->m_WarmupTimer / Client()->GameTickSpeed();
	if(Sec < 5)
		str_format(aBuf, sizeof(aBuf), "%d.%d", Sec, (pGI->m_WarmupTimer * 10 / Client()->GameTickSpeed()) % 10);
	else
		str_format(aBuf, sizeof(aBuf), "%d", Sec);
	TextRender()->Text(AspX - TextRender()->TextWidth(FS, aBuf) / 2, 75, FS, aBuf);
}

void CHud::RenderTextInfo()
{
	int ShowFPS = g_Config.m_ClShowfps;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current()) ShowFPS = 0;
#endif
	if(ShowFPS)
	{
		char aBuf[16];
		int FPS = round_to_int(1.0f / Client()->FrameTimeAverage());
		str_format(aBuf, sizeof(aBuf), "%d", FPS);

		static float s_aW[5] = {
			TextRender()->TextWidth(12.f, "0"),
			TextRender()->TextWidth(12.f, "00"),
			TextRender()->TextWidth(12.f, "000"),
			TextRender()->TextWidth(12.f, "0000"),
			TextRender()->TextWidth(12.f, "00000")
		};

		int Di = GetDigitsIndex(FPS, 4);
		CTextCursor Cursor;
		Cursor.SetPosition(vec2(m_Width - 10 - s_aW[Di], 5));
		Cursor.m_FontSize = 12.0f;
		auto OldFlags = TextRender()->GetRenderFlags();
		TextRender()->SetRenderFlags(OldFlags | TEXT_RENDER_FLAG_ONE_TIME_USE);
		if(m_FPSTextContainerIndex.Valid())
			TextRender()->RecreateTextContainerSoft(m_FPSTextContainerIndex, &Cursor, aBuf);
		else
			TextRender()->CreateTextContainer(m_FPSTextContainerIndex, &Cursor, "0");
		TextRender()->SetRenderFlags(OldFlags);
		if(m_FPSTextContainerIndex.Valid())
			TextRender()->RenderTextContainer(m_FPSTextContainerIndex, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor());
	}
	if(g_Config.m_ClShowpred && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%d", Client()->GetPredictionTime());
		TextRender()->Text(m_Width - 10 - TextRender()->TextWidth(12, aBuf), ShowFPS ? 20 : 5, 12, aBuf);
	}
}

void CHud::RenderConnectionWarning()
{
	if(!Client()->ConnectionProblems())
		return;
	const char *pText = Localize("I DAED WIH UR INET");
	TextRender()->Text(150 * Graphics()->ScreenAspect() - TextRender()->TextWidth(24, pText) / 2, 50, 24, pText);
}

void CHud::RenderTeambalanceWarning()
{
	auto *pGC = GameClient();
	if(!pGC->IsTeamPlay())
		return;

	int Diff = pGC->m_Snap.m_aTeamSize[TEAM_RED] - pGC->m_Snap.m_aTeamSize[TEAM_BLUE];
	if(!g_Config.m_ClWarningTeambalance || (Diff < 2 && Diff > -2))
		return;

	bool Flash = time() / (time_freq() / 2) % 2 == 0;
	TextRender()->TextColor(Flash ? ColorRGBA(1, 1, 0.5f, 1) : ColorRGBA(0.7f, 0.7f, 0.2f, 1));
	TextRender()->Text(5, 50, 6, Localize("Please balance teams!"));
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CHud::RenderCursor()
{
	int CurWeapon = 0;
	vec2 TargetPos;
	float Alpha = 1.0f;

	auto *pGC = GameClient();
	const vec2 Center = pGC->m_Camera.m_Center;
	float aPoints[4];
	Graphics()->MapScreenToWorld(Center.x, Center.y, 100.f, 100.f, 100.f, 0, 0, Graphics()->ScreenAspect(), 1.f, aPoints);
	Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);

	if(Client()->State() != IClient::STATE_DEMOPLAYBACK && pGC->m_Snap.m_pLocalCharacter)
	{
		CurWeapon = maximum(0, pGC->m_Snap.m_pLocalCharacter->m_Weapon % NUM_WEAPONS);
		TargetPos = pGC->m_Controls.m_aTargetPos[g_Config.m_ClDummy];
	}
	else
	{
		if(!g_Config.m_ClSpecCursor || !pGC->m_CursorInfo.IsAvailable())
			return;
		bool Render = (pGC->m_Snap.m_SpecInfo.m_Active && pGC->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW) || Client()->State() == IClient::STATE_DEMOPLAYBACK;
		if(!Render) return;

		const vec2 Half(Center.x - aPoints[0], Center.y - aPoints[1]);
		const vec2 SP = (pGC->m_CursorInfo.WorldTarget() - Center) / pGC->m_Camera.m_Zoom;
		float Clamp = maximum(1.0f, absolute(SP.x / Half.x), absolute(SP.y / Half.y));
		CurWeapon = maximum(0, pGC->m_CursorInfo.Weapon() % NUM_WEAPONS);
		TargetPos = SP / Clamp + Center;
		if(Clamp != 1.0f) Alpha /= 2.0f;
	}

	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
	Graphics()->TextureSet(pGC->m_GameSkin.m_aSpriteWeaponCursors[CurWeapon]);
	Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_aCursorOffset[CurWeapon], TargetPos.x, TargetPos.y);
}

void CHud::PrepareAmmoHealthAndArmorQuads()
{
	float x = 5, y = 5;
	IGraphics::CQuadItem Array[10];

	auto FillArray = [&](float X, float Y, float W, float H) {
		for(int n = 0; n < 10; n++) Array[n] = IGraphics::CQuadItem(X + n * 12, Y, W, H);
	};

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		FillArray(x, y, 10, 10);
		m_aAmmoOffset[i] = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);
		if(i == WEAPON_GRENADE)
			FillArray(1 + x, y, 10, 10);
		else
			FillArray(x, y, 12, 12);
		Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);
	}

	FillArray(x, y, 10, 10);
	m_HealthOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);
	FillArray(x, y, 12, 12);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	FillArray(x, y, 10, 10);
	m_EmptyHealthOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);
	FillArray(x, y, 12, 12);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	FillArray(x, y + 12, 10, 10);
	m_ArmorOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);
	FillArray(x, y + 12, 12, 12);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	FillArray(x, y + 12, 10, 10);
	m_EmptyArmorOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);
	FillArray(x, y + 12, 12, 12);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);
}

void CHud::RenderAmmoHealthAndArmor(const CNetObj_Character *pChar)
{
	if(!pChar) return;

	auto *pGC = GameClient();
	bool Sixup = pGC->m_GameSkin.IsSixup();
	int SixupOff = Sixup ? 10 : 0;

	if(pGC->m_GameInfo.m_HudAmmo)
	{
		float AmmoY = pGC->m_GameInfo.m_HudHealthArmor ? 24 : 0;
		int W = pChar->m_Weapon % NUM_WEAPONS;
		if(W == WEAPON_NINJA)
		{
			if(!pGC->m_GameInfo.m_HudDDRace && Client()->IsSixup())
			{
				int Max = g_pData->m_Weapons.m_Ninja.m_Duration * Client()->GameTickSpeed() / 1000;
				float Prog = std::clamp(pChar->m_AmmoCount - Client()->GameTick(g_Config.m_ClDummy), 0, Max) / (float)Max;
				RenderNinjaBarPos(5 + 10*12, 5, 6.f, 24.f, Prog);
			}
		}
		else if(W >= 0 && pGC->m_GameSkin.m_aSpriteWeaponProjectiles[W].IsValid())
		{
			Graphics()->TextureSet(pGC->m_GameSkin.m_aSpriteWeaponProjectiles[W]);
			int Count = std::clamp(pChar->m_AmmoCount, 0, 10);
			if(AmmoY > 0)
				Graphics()->RenderQuadContainerEx(m_HudQuadContainerIndex, m_aAmmoOffset[W] + SixupOff, Count, 0, AmmoY);
			else
				Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_aAmmoOffset[W] + SixupOff, Count);
		}
	}

	if(pGC->m_GameInfo.m_HudHealthArmor)
	{
		int H = minimum(pChar->m_Health, 10);
		int A = minimum(pChar->m_Armor, 10);
		Graphics()->TextureSet(pGC->m_GameSkin.m_SpriteHealthFull);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_HealthOffset + SixupOff, H);
		Graphics()->TextureSet(pGC->m_GameSkin.m_SpriteHealthEmpty);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_EmptyHealthOffset + SixupOff + H, 10 - H);
		Graphics()->TextureSet(pGC->m_GameSkin.m_SpriteArmorFull);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_ArmorOffset + SixupOff, A);
		Graphics()->TextureSet(pGC->m_GameSkin.m_SpriteArmorEmpty);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_ArmorOffset + SixupOff + A, 10 - A);
	}
}

void CHud::PreparePlayerStateQuads()
{
	float x = 5, y = 5 + 24;
	IGraphics::CQuadItem Array[10];
	for(int i = 0; i < 10; ++i) Array[i] = IGraphics::CQuadItem(x + i*12, y, 12, 12);
	m_AirjumpOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);
	m_AirjumpEmptyOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	for(int W = 0; W < NUM_WEAPONS; ++W)
	{
		float ScaleX, ScaleY;
		Graphics()->GetSpriteScale(g_pData->m_Weapons.m_aId[W].m_pSpriteBody, ScaleX, ScaleY);
		constexpr float Scale = 0.25f;
		m_aWeaponOffset[W] = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex,
			g_pData->m_Weapons.m_aId[W].m_VisualSize * ScaleX * Scale,
			g_pData->m_Weapons.m_aId[W].m_VisualSize * ScaleY * Scale);
	}

	auto AddIcon = [&](int &Off) { Off = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f); };
	AddIcon(m_EndlessJumpOffset); AddIcon(m_EndlessHookOffset); AddIcon(m_JetpackOffset);
	AddIcon(m_TeleportGrenadeOffset); AddIcon(m_TeleportGunOffset); AddIcon(m_TeleportLaserOffset);
	AddIcon(m_SoloOffset); AddIcon(m_CollisionDisabledOffset); AddIcon(m_HookHitDisabledOffset);
	AddIcon(m_HammerHitDisabledOffset); AddIcon(m_GunHitDisabledOffset); AddIcon(m_ShotgunHitDisabledOffset);
	AddIcon(m_GrenadeHitDisabledOffset); AddIcon(m_LaserHitDisabledOffset);
	AddIcon(m_DeepFrozenOffset); AddIcon(m_LiveFrozenOffset);
	AddIcon(m_DummyHammerOffset); AddIcon(m_DummyCopyOffset);
	AddIcon(m_PracticeModeOffset); AddIcon(m_LockModeOffset); AddIcon(m_Team0ModeOffset);
}

void CHud::RenderPlayerState(const int ClientId)
{
	auto *pGC = GameClient();
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	CCharacterCore *pCC = &pGC->m_aClients[ClientId].m_Predicted;
	CNetObj_Character *pPlayer = &pGC->m_aClients[ClientId].m_RenderCur;

	int TotalJumps = 0;
	if(g_Config.m_ClShowhudJumpsIndicator)
	{
		int Available;
		if(pGC->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo)
		{
			bool Grounded = Collision()->CheckPoint(pPlayer->m_X + CCharacterCore::PhysicalSize()/2, pPlayer->m_Y + CCharacterCore::PhysicalSize()/2 + 5) ||
				Collision()->CheckPoint(pPlayer->m_X - CCharacterCore::PhysicalSize()/2, pPlayer->m_Y + CCharacterCore::PhysicalSize()/2 + 5);

			int Used = pCC->m_JumpedTotal;
			if(pCC->m_Jumps > 1) Used += !Grounded;
			else if(pCC->m_Jumps == 1) Used = pPlayer->m_Jumped & 2;
			else if(pCC->m_Jumps == -1) Used = !Grounded;

			if(pCC->m_EndlessJump && Used >= absolute(pCC->m_Jumps))
				Used = absolute(pCC->m_Jumps) - 1;

			int Unused = absolute(pCC->m_Jumps) - Used;
			if(!(pPlayer->m_Jumped & 2) && Unused <= 0) Unused = 1;

			TotalJumps = std::clamp(absolute(pCC->m_Jumps), 0, 10);
			Available = std::clamp(Unused, 0, TotalJumps);
		}
		else
			TotalJumps = Available = absolute(pGC->m_Snap.m_aCharacters[ClientId].m_ExtendedData.m_Jumps);

		int JumpY = (pGC->m_GameInfo.m_HudHealthArmor && g_Config.m_ClShowhudHealthAmmo ? 24 : 0) +
			(pGC->m_GameInfo.m_HudAmmo && g_Config.m_ClShowhudHealthAmmo ? 12 : 0);

		if(JumpY > 0)
		{
			Graphics()->TextureSet(pGC->m_HudSkin.m_SpriteHudAirjump);
			Graphics()->RenderQuadContainerEx(m_HudQuadContainerIndex, m_AirjumpOffset, Available, 0, JumpY);
			Graphics()->TextureSet(pGC->m_HudSkin.m_SpriteHudAirjumpEmpty);
			Graphics()->RenderQuadContainerEx(m_HudQuadContainerIndex, m_AirjumpEmptyOffset + Available, TotalJumps - Available, 0, JumpY);
		}
		else
		{
			Graphics()->TextureSet(pGC->m_HudSkin.m_SpriteHudAirjump);
			Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_AirjumpOffset, Available);
			Graphics()->TextureSet(pGC->m_HudSkin.m_SpriteHudAirjumpEmpty);
			Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_AirjumpEmptyOffset + Available, TotalJumps - Available);
		}
	}

	float x = 5 + 12;
	float y = 5 + 12 + (pGC->m_GameInfo.m_HudHealthArmor && g_Config.m_ClShowhudHealthAmmo ? 24 : 0) +
		(pGC->m_GameInfo.m_HudAmmo && g_Config.m_ClShowhudHealthAmmo ? 12 : 0);

	{
		static constexpr float aWeaponW[] = {16, 12, 12, 12, 12, 12};
		static constexpr float aWeaponOff[] = {-3, -4, -1, -1, -2, -4};
		bool InitOff = false;
		for(int W = 0; W < NUM_WEAPONS; ++W)
		{
			if(!pCC->m_aWeapons[W].m_Got) continue;
			if(!InitOff) { x += aWeaponOff[W]; InitOff = true; }
			if(pPlayer->m_Weapon != W) Graphics()->SetColor(1,1,1,0.4f);
			Graphics()->QuadsSetRotation(pi * 7 / 4);
			Graphics()->TextureSet(pGC->m_GameSkin.m_aSpritePickupWeapons[W]);
			Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_aWeaponOffset[W], x, y);
			Graphics()->QuadsSetRotation(0);
			Graphics()->SetColor(1,1,1,1);
			x += aWeaponW[W];
		}
		if(pCC->m_aWeapons[WEAPON_NINJA].m_Got)
		{
			int Max = g_pData->m_Weapons.m_Ninja.m_Duration * Client()->GameTickSpeed() / 1000;
			float Prog = std::clamp(pCC->m_Ninja.m_ActivationTick + Max - Client()->GameTick(g_Config.m_ClDummy), 0, Max) / (float)Max;
			if(Prog > 0.0f && pGC->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo)
				RenderNinjaBarPos(x, y - 12, 6.f, 24.f, Prog);
		}
	}

	x = 5;
	y += 12;
	if(TotalJumps > 0) y += 12;

	auto RenderIcon = [&](IGraphics::CTextureHandle Tex, int Offset) {
		Graphics()->TextureSet(Tex);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, Offset, x, y);
		x += 12;
	};

	auto &HS = pGC->m_HudSkin;
	bool HasCap = false;
	if(pCC->m_EndlessJump)    { HasCap = true; RenderIcon(HS.m_SpriteHudEndlessJump, m_EndlessJumpOffset); }
	if(pCC->m_EndlessHook)    { HasCap = true; RenderIcon(HS.m_SpriteHudEndlessHook, m_EndlessHookOffset); }
	if(pCC->m_Jetpack)        { HasCap = true; RenderIcon(HS.m_SpriteHudJetpack, m_JetpackOffset); }
	if(pCC->m_HasTelegunGun && pCC->m_aWeapons[WEAPON_GUN].m_Got)     { HasCap = true; RenderIcon(HS.m_SpriteHudTeleportGun, m_TeleportGunOffset); }
	if(pCC->m_HasTelegunGrenade && pCC->m_aWeapons[WEAPON_GRENADE].m_Got) { HasCap = true; RenderIcon(HS.m_SpriteHudTeleportGrenade, m_TeleportGrenadeOffset); }
	if(pCC->m_HasTelegunLaser && pCC->m_aWeapons[WEAPON_LASER].m_Got) { HasCap = true; RenderIcon(HS.m_SpriteHudTeleportLaser, m_TeleportLaserOffset); }

	x = 5;
	if(HasCap) y += 12;
	bool HasProhib = false;
	if(pCC->m_Solo)             { HasProhib = true; RenderIcon(HS.m_SpriteHudSolo, m_SoloOffset); }
	if(pCC->m_CollisionDisabled) { HasProhib = true; RenderIcon(HS.m_SpriteHudCollisionDisabled, m_CollisionDisabledOffset); }
	if(pCC->m_HookHitDisabled)  { HasProhib = true; RenderIcon(HS.m_SpriteHudHookHitDisabled, m_HookHitDisabledOffset); }
	if(pCC->m_HammerHitDisabled) { HasProhib = true; RenderIcon(HS.m_SpriteHudHammerHitDisabled, m_HammerHitDisabledOffset); }
	if(pCC->m_GrenadeHitDisabled && pCC->m_HasTelegunGun && pCC->m_aWeapons[WEAPON_GUN].m_Got) { HasProhib = true; RenderIcon(HS.m_SpriteHudGunHitDisabled, m_LaserHitDisabledOffset); }
	if(pCC->m_ShotgunHitDisabled && pCC->m_aWeapons[WEAPON_SHOTGUN].m_Got) { HasProhib = true; RenderIcon(HS.m_SpriteHudShotgunHitDisabled, m_ShotgunHitDisabledOffset); }
	if(pCC->m_GrenadeHitDisabled && pCC->m_aWeapons[WEAPON_GRENADE].m_Got) { HasProhib = true; RenderIcon(HS.m_SpriteHudGrenadeHitDisabled, m_GrenadeHitDisabledOffset); }
	if(pCC->m_LaserHitDisabled && pCC->m_aWeapons[WEAPON_LASER].m_Got) { HasProhib = true; RenderIcon(HS.m_SpriteHudLaserHitDisabled, m_LaserHitDisabledOffset); }

	x = 5;
	if(HasProhib) y += 12;

	const auto &SC = pGC->m_Snap.m_aCharacters[ClientId];
	if(SC.m_HasExtendedDisplayInfo)
	{
		if(SC.m_ExtendedData.m_Flags & CHARACTERFLAG_LOCK_MODE)     RenderIcon(HS.m_SpriteHudLockMode, m_LockModeOffset);
		if(SC.m_ExtendedData.m_Flags & CHARACTERFLAG_PRACTICE_MODE) RenderIcon(HS.m_SpriteHudPracticeMode, m_PracticeModeOffset);
		if(SC.m_ExtendedData.m_Flags & CHARACTERFLAG_TEAM0_MODE)    RenderIcon(HS.m_SpriteHudTeam0Mode, m_Team0ModeOffset);
	}
	if(pCC->m_DeepFrozen) RenderIcon(HS.m_SpriteHudDeepFrozen, m_DeepFrozenOffset);
	if(pCC->m_LiveFrozen) RenderIcon(HS.m_SpriteHudLiveFrozen, m_LiveFrozenOffset);
}

void CHud::RenderNinjaBarPos(const float x, float y, const float Width, const float Height, float Progress, const float Alpha)
{
	Progress = std::clamp(Progress, 0.0f, 1.0f);
	const float RestPct = 0.5f, ProgPct = 0.5f;
	const float EndH = Width;
	const float MiddleH = Height - EndH * 2.0f;
	const float EndProgH = EndH * ProgPct;
	const float EndRestH = EndH * RestPct;
	const float ProgBarH = Height - EndProgH * 2.0f;
	const float EndProgProp = EndProgH / ProgBarH;
	const float MidProgProp = MiddleH / ProgBarH;

	float BeginProg = 1.0f;
	if(Progress <= 1.0f)
	{
		if(Progress <= EndProgProp + MidProgProp) BeginProg = 0.0f;
		else BeginProg = (Progress - EndProgProp - MidProgProp) / EndProgProp;
	}

	auto SetColorAlpha = [&]() { Graphics()->SetColor(1.f, 1.f, 1.f, Alpha); };
	auto &HS = GameClient()->m_HudSkin;
	Graphics()->WrapClamp();

	Graphics()->TextureSet(HS.m_SpriteHudNinjaBarEmptyRight);
	Graphics()->QuadsBegin(); SetColorAlpha();
	Graphics()->QuadsSetSubsetFree(1, 1, 1, 0, ProgPct - ProgPct*(1-BeginProg), 0, ProgPct - ProgPct*(1-BeginProg), 1);
	IGraphics::CQuadItem QEB(x, y, Width, EndRestH + EndProgH*(1-BeginProg));
	Graphics()->QuadsDrawTL(&QEB, 1); Graphics()->QuadsEnd();

	if(BeginProg > 0.0f)
	{
		Graphics()->TextureSet(HS.m_SpriteHudNinjaBarFullLeft);
		Graphics()->QuadsBegin(); SetColorAlpha();
		Graphics()->QuadsSetSubsetFree(RestPct + ProgPct*(1-BeginProg), 1, RestPct + ProgPct*(1-BeginProg), 0, 1, 0, 1, 1);
		IGraphics::CQuadItem QFB(x, y + EndRestH + EndProgH*(1-BeginProg), Width, EndProgH*BeginProg);
		Graphics()->QuadsDrawTL(&QFB, 1); Graphics()->QuadsEnd();
	}

	float CurY = y + EndH;
	float MidProg = 1.0f;
	if(Progress <= EndProgProp + MidProgProp)
		MidProg = Progress <= EndProgProp ? 0.0f : (Progress - EndProgProp) / MidProgProp;

	float FullMid = MiddleH * MidProg, EmptyMid = MiddleH - FullMid;

	if(EmptyMid > 0.0f)
	{
		Graphics()->TextureSet(HS.m_SpriteHudNinjaBarEmpty);
		Graphics()->QuadsBegin(); SetColorAlpha();
		if(EmptyMid <= EndH)
			Graphics()->QuadsSetSubsetFree(1,1,1,0, 1-(EmptyMid/EndH),0, 1-(EmptyMid/EndH),1);
		else
			Graphics()->QuadsSetSubsetFree(1,1,1,0,0,0,0,1);
		IGraphics::CQuadItem QEm(x, CurY, Width, EmptyMid);
		Graphics()->QuadsDrawTL(&QEm, 1); Graphics()->QuadsEnd();
	}

	Graphics()->TextureSet(HS.m_SpriteHudNinjaBarFull);
	Graphics()->QuadsBegin(); SetColorAlpha();
	if(FullMid <= EndH)
		Graphics()->QuadsSetSubsetFree(1-(FullMid/EndH),1, 1-(FullMid/EndH),0,1,0,1,1);
	else
		Graphics()->QuadsSetSubsetFree(0,1,0,0,1,0,1,1);
	IGraphics::CQuadItem QFm(x, CurY + EmptyMid, Width, FullMid);
	Graphics()->QuadsDrawTL(&QFm, 1); Graphics()->QuadsEnd();

	CurY += MiddleH;
	float EndProg = 1.0f;
	if(Progress <= EndProgProp) EndProg = Progress / EndProgProp;

	if(EndProg < 1.0f)
	{
		Graphics()->TextureSet(HS.m_SpriteHudNinjaBarEmptyRight);
		Graphics()->QuadsBegin(); SetColorAlpha();
		Graphics()->QuadsSetSubsetFree(0,1,0,0, ProgPct - ProgPct*EndProg,0, ProgPct - ProgPct*EndProg,1);
		IGraphics::CQuadItem QEE(x, CurY, Width, EndProgH*(1-EndProg));
		Graphics()->QuadsDrawTL(&QEE, 1); Graphics()->QuadsEnd();
	}

	Graphics()->TextureSet(HS.m_SpriteHudNinjaBarFullLeft);
	Graphics()->QuadsBegin(); SetColorAlpha();
	Graphics()->QuadsSetSubsetFree(RestPct + ProgPct*EndProg,1, RestPct + ProgPct*EndProg,0,0,0,0,1);
	IGraphics::CQuadItem QFE(x, CurY + EndProgH*(1-EndProg), Width, EndRestH + EndProgH*EndProg);
	Graphics()->QuadsDrawTL(&QFE, 1); Graphics()->QuadsEnd();

	Graphics()->QuadsSetSubset(0,0,1,1);
	Graphics()->SetColor(1.f,1.f,1.f,1.f);
	Graphics()->WrapNormal();
}

void CHud::RenderSpectatorCount()
{
	if(!g_Config.m_ClShowhudSpectatorCount) return;

	auto *pGC = GameClient();
	int Dummy = g_Config.m_ClDummy;
	int GameTick = Client()->GameTick(Dummy);
	int Count = 0;

	if(Client()->IsSixup())
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(i == pGC->m_aLocalIds[0] || (pGC->Client()->DummyConnected() && i == pGC->m_aLocalIds[1]))
				continue;
			if(Client()->m_TranslationContext.m_aClients[i].m_PlayerFlags7 & protocol7::PLAYERFLAG_WATCHING)
				Count++;
		}
	}
	else
	{
		const auto *pSC = pGC->m_Snap.m_pSpectatorCount;
		if(!pSC) { m_LastSpectatorCountTick = GameTick; return; }
		Count = pSC->m_NumSpectators;
	}

	if(Count == 0) { m_LastSpectatorCountTick = GameTick; return; }
	if(GameTick < m_LastSpectatorCountTick + Client()->GameTickSpeed()) return;

	char aBuf[16];
	str_format(aBuf, sizeof(aBuf), "%d", Count);
	const float FS = 6.0f;
	const float BoxH = 14.f, BoxW = 13.f + TextRender()->TextWidth(FS, aBuf);
	float SX = m_Width - BoxW;
	float SY = 285.0f - BoxH - 4;
	if(g_Config.m_ClShowhudPlayerPosition || g_Config.m_ClShowhudPlayerSpeed || g_Config.m_ClShowhudPlayerAngle) SY -= 4;
	SY -= GetMovementInformationBoxHeight();
	if(g_Config.m_ClShowhudScore) SY -= 56;
	if(g_Config.m_ClShowhudDummyActions && !(pGC->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER) && Client()->DummyConnected())
		SY -= 33.0f;

	Graphics()->DrawRect(SX, SY, BoxW, BoxH, ColorRGBA(0,0,0,0.4f), IGraphics::CORNER_L, 5.0f);
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->Text(SX + 2, SY + BoxH/3, FS, FontIcons::FONT_ICON_EYE);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	TextRender()->Text(SX + 2 + FS + 3.f, SY + BoxH/3, FS, aBuf);
}

void CHud::RenderDummyActions()
{
	if(!g_Config.m_ClShowhudDummyActions || (GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER) || !Client()->DummyConnected())
		return;

	const float BoxH = 29.0f, BoxW = 16.0f;
	float SX = m_Width - BoxW;
	float SY = 285.0f - BoxH - 4;
	if(g_Config.m_ClShowhudPlayerPosition || g_Config.m_ClShowhudPlayerSpeed || g_Config.m_ClShowhudPlayerAngle) SY -= 4;
	SY -= GetMovementInformationBoxHeight();
	if(g_Config.m_ClShowhudScore) SY -= 56;

	Graphics()->DrawRect(SX, SY, BoxW, BoxH, ColorRGBA(0,0,0,0.4f), IGraphics::CORNER_L, 5.0f);
	float y = SY + 2, x = SX + 2;

	auto &HS = GameClient()->m_HudSkin;
	Graphics()->SetColor(1,1,1, g_Config.m_ClDummyHammer ? 1.0f : 0.4f);
	Graphics()->TextureSet(HS.m_SpriteHudDummyHammer);
	Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_DummyHammerOffset, x, y);
	y += 13;
	Graphics()->SetColor(1,1,1, g_Config.m_ClDummyCopyMoves ? 1.0f : 0.4f);
	Graphics()->TextureSet(HS.m_SpriteHudDummyCopy);
	Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_DummyCopyOffset, x, y);
}

inline int CHud::GetDigitsIndex(int Value, int Max)
{
	if(Value < 0) Value = -Value;
	return std::clamp((int)std::log10(Value ? Value : 1), 0, Max);
}

inline float CHud::GetMovementInformationBoxHeight()
{
	auto *pGC = GameClient();
	const auto &Spec = pGC->m_Snap.m_SpecInfo;
	if(Spec.m_Active && (Spec.m_SpectatorId == SPEC_FREEVIEW || pGC->m_aClients[Spec.m_SpectatorId].m_SpecCharPresent))
		return g_Config.m_ClShowhudPlayerPosition ? 3.0f * MOVEMENT_INFORMATION_LINE_HEIGHT + 2.0f : 0.0f;
	float H = 3.0f * MOVEMENT_INFORMATION_LINE_HEIGHT * (g_Config.m_ClShowhudPlayerPosition + g_Config.m_ClShowhudPlayerSpeed)
		+ 2.0f * MOVEMENT_INFORMATION_LINE_HEIGHT * g_Config.m_ClShowhudPlayerAngle;
	if(g_Config.m_ClShowhudPlayerPosition || g_Config.m_ClShowhudPlayerSpeed || g_Config.m_ClShowhudPlayerAngle)
		H += 2.0f;
	return H;
}

void CHud::UpdateMovementInformationTextContainer(STextContainerIndex &Idx, float FS, float Value, float &Prev)
{
	Value = std::round(Value * 100.0f) / 100.0f;
	if(Idx.Valid() && Prev == Value) return;
	Prev = Value;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%.2f", Value);
	CTextCursor Cursor;
	Cursor.m_FontSize = FS;
	TextRender()->RecreateTextContainer(Idx, &Cursor, aBuf);
}

void CHud::RenderMovementInformationTextContainer(STextContainerIndex &Idx, const ColorRGBA &Color, float X, float Y)
{
	if(Idx.Valid())
		TextRender()->RenderTextContainer(Idx, Color, TextRender()->DefaultTextOutlineColor(), X - TextRender()->GetBoundingBoxTextContainer(Idx).m_W, Y);
}

CHud::CMovementInformation CHud::GetMovementInformation(int ClientId, int Conn) const
{
	auto *pGC = GameClient();
	CMovementInformation Out;
	if(ClientId == SPEC_FREEVIEW)
	{
		Out.m_Pos = pGC->m_Camera.m_Center / 32.0f;
	}
	else if(pGC->m_aClients[ClientId].m_SpecCharPresent)
	{
		Out.m_Pos = pGC->m_aClients[ClientId].m_SpecChar / 32.0f;
	}
	else
	{
		const auto *pPrev = &pGC->m_Snap.m_aCharacters[ClientId].m_Prev;
		const auto *pCur  = &pGC->m_Snap.m_aCharacters[ClientId].m_Cur;
		float Intra = Client()->IntraGameTick(Conn);
		Out.m_Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCur->m_X, pCur->m_Y), Intra) / 32.0f;

		vec2 Vel = mix(vec2(pPrev->m_VelX, pPrev->m_VelY), vec2(pCur->m_VelX, pCur->m_VelY), Intra);
		float TS = (float)Client()->GameTickSpeed();
		float VX = (Vel.x >= -1.f && Vel.x <= 1.f) ? 0.f : Vel.x / 256.0f * TS;
		float VY = (Vel.y >= -128.f && Vel.y <= 128.f) ? 0.f : Vel.y / 256.0f * TS;
		float VLen = length(vec2(Vel.x, Vel.y) / 256.0f) * TS;
		float Ramp = VelocityRamp(VLen, pGC->m_aTuning[Conn].m_VelrampStart, pGC->m_aTuning[Conn].m_VelrampRange, pGC->m_aTuning[Conn].m_VelrampCurvature);
		Out.m_Speed.x = VX / 32.0f * Ramp;
		Out.m_Speed.y = VY / 32.0f;

		float Ang = pGC->m_Players.GetPlayerTargetAngle(pPrev, pCur, ClientId, Intra);
		if(Ang < 0.0f) Ang += 2.0f * pi;
		Out.m_Angle = Ang * 180.0f / pi;
	}
	return Out;
}

void CHud::RenderMovementInformation()
{
	auto *pGC = GameClient();
	int ClientId = pGC->m_Snap.m_SpecInfo.m_Active ? pGC->m_Snap.m_SpecInfo.m_SpectatorId : pGC->m_Snap.m_LocalClientId;
	bool PosOnly = ClientId == SPEC_FREEVIEW || (ClientId >= 0 && pGC->m_aClients[ClientId].m_SpecCharPresent);

	if(!g_Config.m_ClShowhudPlayerPosition && (PosOnly || (!g_Config.m_ClShowhudPlayerSpeed && !g_Config.m_ClShowhudPlayerAngle)))
		return;

	float BoxH = GetMovementInformationBoxHeight();
	const float BoxW = 62.0f;
	float SX = m_Width - BoxW;
	float SY = 285.0f - BoxH - 4.0f;
	if(g_Config.m_ClShowhudScore) SY -= 56.0f;

	Graphics()->DrawRect(SX, SY, BoxW, BoxH, ColorRGBA(0,0,0,0.4f), IGraphics::CORNER_L, 5.0f);

	const CMovementInformation Info = GetMovementInformation(ClientId, g_Config.m_ClDummy);
	float y = SY + 2.0f;
	const float LeftX = SX + 2.0f, RightX = m_Width - 2.0f;
	const float FS = 6.0f;

	if(g_Config.m_ClShowhudPlayerPosition)
	{
		TextRender()->Text(LeftX, y, FS, Localize("Position:")); y += MOVEMENT_INFORMATION_LINE_HEIGHT;
		TextRender()->Text(LeftX, y, FS, "X:");
		UpdateMovementInformationTextContainer(m_aPlayerPositionContainers[0], FS, Info.m_Pos.x, m_aPlayerPrevPosition[0]);
		RenderMovementInformationTextContainer(m_aPlayerPositionContainers[0], TextRender()->DefaultTextColor(), RightX, y);
		y += MOVEMENT_INFORMATION_LINE_HEIGHT;
		TextRender()->Text(LeftX, y, FS, "Y:");
		UpdateMovementInformationTextContainer(m_aPlayerPositionContainers[1], FS, Info.m_Pos.y, m_aPlayerPrevPosition[1]);
		RenderMovementInformationTextContainer(m_aPlayerPositionContainers[1], TextRender()->DefaultTextColor(), RightX, y);
		y += MOVEMENT_INFORMATION_LINE_HEIGHT;
	}
	if(PosOnly) return;

	if(g_Config.m_ClShowhudPlayerSpeed)
	{
		TextRender()->Text(LeftX, y, FS, Localize("Speed:")); y += MOVEMENT_INFORMATION_LINE_HEIGHT;
		static const char *aCoords[] = {"X:", "Y:"};
		for(int i = 0; i < 2; i++)
		{
			ColorRGBA C(1,1,1,1);
			if(m_aLastPlayerSpeedChange[i] == ESpeedChange::INCREASE) C = ColorRGBA(0,1,0,1);
			if(m_aLastPlayerSpeedChange[i] == ESpeedChange::DECREASE) C = ColorRGBA(1,0.5f,0.5f,1);
			TextRender()->Text(LeftX, y, FS, aCoords[i]);
			UpdateMovementInformationTextContainer(m_aPlayerSpeedTextContainers[i], FS, i == 0 ? Info.m_Speed.x : Info.m_Speed.y, m_aPlayerPrevSpeed[i]);
			RenderMovementInformationTextContainer(m_aPlayerSpeedTextContainers[i], C, RightX, y);
			y += MOVEMENT_INFORMATION_LINE_HEIGHT;
		}
		TextRender()->TextColor(1,1,1,1);
	}

	if(g_Config.m_ClShowhudPlayerAngle)
	{
		TextRender()->Text(LeftX, y, FS, Localize("Angle:")); y += MOVEMENT_INFORMATION_LINE_HEIGHT;
		UpdateMovementInformationTextContainer(m_PlayerAngleTextContainerIndex, FS, Info.m_Angle, m_PlayerPrevAngle);
		RenderMovementInformationTextContainer(m_PlayerAngleTextContainerIndex, TextRender()->DefaultTextColor(), RightX, y);
	}
}

void CHud::RenderSpectatorHud()
{
	if(!g_Config.m_ClShowhudSpectator) return;

	auto *pGC = GameClient();
	Graphics()->DrawRect(m_Width - 180.0f, m_Height - 15.0f, 180.0f, 15.0f, ColorRGBA(0,0,0,0.4f), IGraphics::CORNER_TL, 5.0f);

	char aBuf[128];
	if(pGC->m_MultiViewActivated)
		str_copy(aBuf, Localize("Multi-View"));
	else if(pGC->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW)
	{
		const auto &P = pGC->m_aClients[pGC->m_Snap.m_SpecInfo.m_SpectatorId];
		if(g_Config.m_ClShowIds)
			str_format(aBuf, sizeof(aBuf), Localize("Following %d: %s", "Spectating"), P.ClientId(), P.m_aName);
		else
			str_format(aBuf, sizeof(aBuf), Localize("Following %s", "Spectating"), P.m_aName);
	}
	else str_copy(aBuf, Localize("Free-View"));
	TextRender()->Text(m_Width - 174.0f, m_Height - 15.0f + (15.f-8.f)/2.f, 8.0f, aBuf);

	if(Client()->State() != IClient::STATE_DEMOPLAYBACK && pGC->m_Camera.SpectatingPlayer() && pGC->m_Camera.CanUseAutoSpecCamera() && g_Config.m_ClSpecAutoSync)
	{
		bool Auto = pGC->m_Camera.m_AutoSpecCamera;
		const char *pLabel = Localize("AUTO", "Spectating Camera Mode Icon");
		float TW = TextRender()->TextWidth(6.0f, pLabel);
		constexpr float RM = 4.f, IW = 6.f, Pad = 3.f;
		float TagW = IW + TW + Pad * 3.0f;
		float TagX = m_Width - RM - TagW;
		Graphics()->DrawRect(TagX, m_Height - 12.0f, TagW, 10.0f, ColorRGBA(1,1,1, Auto ? 0.5f : 0.1f), IGraphics::CORNER_ALL, 2.5f);
		TextRender()->TextColor(1, 1, 1, Auto ? 1.0f : 0.65f);
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->Text(TagX + Pad, m_Height - 10.0f, 6.0f, FontIcons::FONT_ICON_CAMERA);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		TextRender()->Text(TagX + Pad + IW + Pad, m_Height - 10.0f, 6.0f, pLabel);
		TextRender()->TextColor(1, 1, 1, 1);
	}
}

void CHud::RenderLocalTime(float x)
{
	if(!g_Config.m_ClShowLocalTimeAlways && !GameClient()->m_Scoreboard.IsActive())
		return;
	Graphics()->DrawRect(x - 30.0f, 0.0f, 25.0f, 12.5f, ColorRGBA(0,0,0,0.4f), IGraphics::CORNER_B, 3.75f);
	char aTime[6];
	str_timestamp_format(aTime, sizeof(aTime), "%H:%M");
	TextRender()->Text(x - 25.0f, (12.5f - 5.f) / 2.f, 5.0f, aTime);
}

void CHud::OnNewSnapshot()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK) return;
	auto *pGC = GameClient();
	if(!pGC->m_Snap.m_pGameInfoObj) return;

	int ClientId = -1;
	if(pGC->m_Snap.m_pLocalCharacter && !pGC->m_Snap.m_SpecInfo.m_Active && !(pGC->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
		ClientId = pGC->m_Snap.m_LocalClientId;
	else if(pGC->m_Snap.m_SpecInfo.m_Active)
		ClientId = pGC->m_Snap.m_SpecInfo.m_SpectatorId;
	if(ClientId == -1) return;

	int Dummy = g_Config.m_ClDummy;
	const auto *pPrev = &pGC->m_Snap.m_aCharacters[ClientId].m_Prev;
	const auto *pCur  = &pGC->m_Snap.m_aCharacters[ClientId].m_Cur;
	ivec2 Vel = mix(ivec2(pPrev->m_VelX, pPrev->m_VelY), ivec2(pCur->m_VelX, pCur->m_VelY), Client()->IntraGameTick(Dummy));

	CCharacter *pChar = pGC->m_PredictedWorld.GetCharacterById(ClientId);
	if(pChar && pChar->IsGrounded()) Vel.y = 0;

	int aV[2] = {Vel.x, Vel.y};
	for(int i = 0; i < 2; i++)
	{
		int Abs = abs(aV[i]);
		m_aLastPlayerSpeedChange[i] = Abs < 2 ? ESpeedChange::NONE : (Abs > m_aPlayerSpeed[i] ? ESpeedChange::INCREASE : Abs < m_aPlayerSpeed[i] ? ESpeedChange::DECREASE : m_aLastPlayerSpeedChange[i]);
		m_aPlayerSpeed[i] = Abs;
	}
}

void CHud::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK) return;
	auto *pGC = GameClient();
	if(!pGC->m_Snap.m_pGameInfoObj) return;

	m_Width = 300.0f * Graphics()->ScreenAspect();
	m_Height = 300.0f;
	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);

#if defined(CONF_VIDEORECORDER)
	bool ShowHud = IVideo::Current() ? g_Config.m_ClVideoShowhud : g_Config.m_ClShowhud;
#else
	bool ShowHud = g_Config.m_ClShowhud;
#endif
	if(ShowHud)
	{
		const auto &GI = pGC->m_Snap.m_pGameInfoObj->m_GameStateFlags;
		int LocalId = pGC->m_Snap.m_LocalClientId;

		if(pGC->m_Snap.m_pLocalCharacter && !pGC->m_Snap.m_SpecInfo.m_Active && !(GI & GAMESTATEFLAG_GAMEOVER))
		{
			if(g_Config.m_ClShowhudHealthAmmo) RenderAmmoHealthAndArmor(pGC->m_Snap.m_pLocalCharacter);
			if(pGC->m_Snap.m_aCharacters[LocalId].m_HasExtendedData && g_Config.m_ClShowhudDDRace && pGC->m_GameInfo.m_HudDDRace)
				RenderPlayerState(LocalId);
			RenderSpectatorCount();
			RenderMovementInformation();
			RenderDDRaceEffects();
		}
		else if(pGC->m_Snap.m_SpecInfo.m_Active)
		{
			int SpecId = pGC->m_Snap.m_SpecInfo.m_SpectatorId;
			if(SpecId != SPEC_FREEVIEW && g_Config.m_ClShowhudHealthAmmo)
				RenderAmmoHealthAndArmor(&pGC->m_Snap.m_aCharacters[SpecId].m_Cur);
			if(SpecId != SPEC_FREEVIEW && pGC->m_Snap.m_aCharacters[SpecId].m_HasExtendedData && g_Config.m_ClShowhudDDRace &&
				(!pGC->m_MultiViewActivated || pGC->m_MultiViewShowHud) && pGC->m_GameInfo.m_HudDDRace)
				RenderPlayerState(SpecId);
			RenderMovementInformation();
			RenderSpectatorHud();
		}

		if(g_Config.m_ClShowhudTimer) RenderGameTimer();
		RenderPauseNotification();
		RenderSuddenDeath();
		if(g_Config.m_ClShowhudScore) RenderScoreHud();
		RenderDummyActions();
		RenderWarmupTimer();
		RenderTasHud();
		RenderTextInfo();
		RenderLocalTime((m_Width / 7) * 3);
		if(Client()->State() != IClient::STATE_DEMOPLAYBACK) RenderConnectionWarning();
		RenderTeambalanceWarning();
		pGC->m_Voting.Render();
		if(g_Config.m_ClShowRecord) RenderRecord();
	}
	RenderCursor();
}

void CHud::OnMessage(int MsgType, void *pRawMsg)
{
	int Dummy = g_Config.m_ClDummy;
	if(MsgType == NETMSGTYPE_SV_DDRACETIME || MsgType == NETMSGTYPE_SV_DDRACETIMELEGACY)
	{
		auto *pMsg = (CNetMsg_Sv_DDRaceTime *)pRawMsg;
		m_DDRaceTime = pMsg->m_Time;
		m_ShowFinishTime = pMsg->m_Finish != 0;
		if(!m_ShowFinishTime)
		{
			m_TimeCpDiff = pMsg->m_Check / 100.0f;
			m_TimeCpLastReceivedTick = Client()->GameTick(Dummy);
		}
		else
		{
			m_FinishTimeDiff = pMsg->m_Check / 100.0f;
			m_FinishTimeLastReceivedTick = Client()->GameTick(Dummy);
		}
	}
	else if(MsgType == NETMSGTYPE_SV_RECORD || MsgType == NETMSGTYPE_SV_RECORDLEGACY)
	{
		auto *pMsg = (CNetMsg_Sv_Record *)pRawMsg;
		auto *pGC = GameClient();
		if(MsgType == NETMSGTYPE_SV_RECORDLEGACY && pGC->m_GameInfo.m_DDRaceRecordMessage)
		{
			m_DDRaceTime = pMsg->m_ServerTimeBest;
			m_FinishTimeLastReceivedTick = Client()->GameTick(Dummy);
			if(pMsg->m_PlayerTimeBest)
			{
				m_TimeCpDiff = pMsg->m_PlayerTimeBest / 100.0f;
				m_TimeCpLastReceivedTick = Client()->GameTick(Dummy);
			}
		}
		else if(MsgType == NETMSGTYPE_SV_RECORD || pGC->m_GameInfo.m_RaceRecordMessage)
			m_aPlayerRecord[Dummy] = pMsg->m_PlayerTimeBest / 100.0f;
	}
}

void CHud::RenderDDRaceEffects()
{
	if(!m_DDRaceTime) return;

	auto *pGC = GameClient();
	int Dummy = g_Config.m_ClDummy;
	int GameTick = Client()->GameTick(Dummy);
	int TickSpeed = Client()->GameTickSpeed();
	float AspX = 150 * Graphics()->ScreenAspect();

	auto CalcAlpha = [&](int BaseTick, int FadeTick, int TotalTick) -> float {
		if(GameTick < BaseTick + FadeTick * TickSpeed) return 1.0f;
		if(GameTick < BaseTick + TotalTick * TickSpeed)
			return (float)(BaseTick + TotalTick * TickSpeed - GameTick) / (float)(TickSpeed * (TotalTick - FadeTick));
		return -1.0f;
	};

	char aBuf[64], aTime[32];
	if(m_ShowFinishTime && m_FinishTimeLastReceivedTick + TickSpeed * 6 > GameTick)
	{
		float Alpha = CalcAlpha(m_FinishTimeLastReceivedTick, 4, 6);
		if(Alpha < 0) return;

		str_time(m_DDRaceTime, TIME_HOURS_CENTISECS, aTime, sizeof(aTime));
		str_format(aBuf, sizeof(aBuf), "Finish time: %s", aTime);

		TextRender()->TextColor(1, 1, 1, Alpha);
		CTextCursor Cursor;
		Cursor.SetPosition(vec2(AspX - TextRender()->TextWidth(12, aBuf) / 2, 20));
		Cursor.m_FontSize = 12.0f;
		TextRender()->RecreateTextContainer(m_DDRaceEffectsTextContainerIndex, &Cursor, aBuf);

		if(m_FinishTimeDiff != 0.0f && m_DDRaceEffectsTextContainerIndex.Valid())
		{
			bool Neg = m_FinishTimeDiff < 0;
			str_time_float(Neg ? -m_FinishTimeDiff : m_FinishTimeDiff, TIME_HOURS_CENTISECS, aTime, sizeof(aTime));
			str_format(aBuf, sizeof(aBuf), "%s%s", Neg ? "-" : "+", aTime);
			TextRender()->TextColor(Neg ? ColorRGBA(0.5f, 1, 0.5f, Alpha) : ColorRGBA(1, 0.5f, 0.5f, Alpha));
			CTextCursor DC;
			DC.SetPosition(vec2(AspX - TextRender()->TextWidth(10, aBuf) / 2, 34));
			DC.m_FontSize = 10.0f;
			TextRender()->AppendTextContainer(m_DDRaceEffectsTextContainerIndex, &DC, aBuf);
		}

		if(m_DDRaceEffectsTextContainerIndex.Valid())
		{
			auto OC = TextRender()->DefaultTextOutlineColor();
			OC.a *= Alpha;
			TextRender()->RenderTextContainer(m_DDRaceEffectsTextContainerIndex, TextRender()->DefaultTextColor(), OC);
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	else if(g_Config.m_ClShowhudTimeCpDiff && !m_ShowFinishTime && m_TimeCpLastReceivedTick + TickSpeed * 6 > GameTick)
	{
		float Alpha = CalcAlpha(m_TimeCpLastReceivedTick, 4, 6);
		if(Alpha < 0) return;

		bool Neg = m_TimeCpDiff < 0;
		str_time_float(Neg ? -m_TimeCpDiff : m_TimeCpDiff, TIME_HOURS_CENTISECS, aTime, sizeof(aTime));
		str_format(aBuf, sizeof(aBuf), "%s%s", Neg ? "-" : "+", aTime);

		if(m_TimeCpDiff > 0) TextRender()->TextColor(1.f, 0.5f, 0.5f, Alpha);
		else if(m_TimeCpDiff < 0) TextRender()->TextColor(0.5f, 1.f, 0.5f, Alpha);
		else TextRender()->TextColor(1, 1, 1, Alpha);

		CTextCursor Cursor;
		Cursor.SetPosition(vec2(AspX - TextRender()->TextWidth(10, aBuf) / 2, 20));
		Cursor.m_FontSize = 10.0f;
		TextRender()->RecreateTextContainer(m_DDRaceEffectsTextContainerIndex, &Cursor, aBuf);

		if(m_DDRaceEffectsTextContainerIndex.Valid())
		{
			auto OC = TextRender()->DefaultTextOutlineColor();
			OC.a *= Alpha;
			TextRender()->RenderTextContainer(m_DDRaceEffectsTextContainerIndex, TextRender()->DefaultTextColor(), OC);
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
}

void CHud::RenderTasHud() {}

void CHud::RenderRecord()
{
	auto *pGC = GameClient();
	int Dummy = g_Config.m_ClDummy;

	if(pGC->m_MapBestTimeSeconds != FinishTime::UNSET && pGC->m_MapBestTimeSeconds != FinishTime::NOT_FINISHED_MILLIS)
	{
		TextRender()->Text(5, 75, 6, Localize("Server best:"));
		char aTime[32], aBuf[64];
		int64_t TC = (int64_t)pGC->m_MapBestTimeSeconds * 100 + (int64_t)pGC->m_MapBestTimeMillis / 10;
		str_time(TC, TIME_HOURS_CENTISECS, aTime, sizeof(aTime));
		str_format(aBuf, sizeof(aBuf), "%s%s", pGC->m_MapBestTimeSeconds > 3600 ? "" : "   ", aTime);
		TextRender()->Text(53, 75, 6, aBuf);
	}

	if(pGC->m_ReceivedDDNetPlayerFinishTimes)
	{
		int PSec = pGC->m_aClients[pGC->m_aLocalIds[Dummy]].m_FinishTimeSeconds;
		if(PSec != FinishTime::NOT_FINISHED_MILLIS)
		{
			TextRender()->Text(5, 82, 6, Localize("Personal best:"));
			char aTime[32], aBuf[64];
			int PMilli = pGC->m_aClients[pGC->m_aLocalIds[Dummy]].m_FinishTimeMillis;
			int64_t TC = (int64_t)PSec * 100 + (int64_t)PMilli / 10;
			str_time(TC, TIME_HOURS_CENTISECS, aTime, sizeof(aTime));
			str_format(aBuf, sizeof(aBuf), "%s%s", PSec > 3600 ? "" : "   ", aTime);
			TextRender()->Text(53, 82, 6, aBuf);
		}
	}
	else
	{
		float PR = m_aPlayerRecord[Dummy];
		if(PR > 0.0f)
		{
			TextRender()->Text(5, 82, 6, Localize("Personal best:"));
			char aTime[32], aBuf[64];
			str_time_float(PR, TIME_HOURS_CENTISECS, aTime, sizeof(aTime));
			str_format(aBuf, sizeof(aBuf), "%s%s", PR > 3600 ? "" : "   ", aTime);
			TextRender()->Text(53, 82, 6, aBuf);
		}
	}
}