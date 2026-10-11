/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "emoticon.h"

#include "chat.h"

#include <algorithm>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>

CEmoticon::CEmoticon()
{
	OnReset();
}

void CEmoticon::ConKeyEmoticon(IConsole::IResult *pResult, void *pUserData)
{
	CEmoticon *pSelf = (CEmoticon *)pUserData;

	if(pSelf->GameClient()->m_Scoreboard.IsActive())
		return;

	if(!pSelf->GameClient()->m_Snap.m_SpecInfo.m_Active && pSelf->Client()->State() != IClient::STATE_DEMOPLAYBACK)
		pSelf->m_Active = pResult->GetInteger(0) != 0;
}

void CEmoticon::ConEmote(IConsole::IResult *pResult, void *pUserData)
{
	((CEmoticon *)pUserData)->Emote(pResult->GetInteger(0));
}

void CEmoticon::OnConsoleInit()
{
	Console()->Register("+emote", "", CFGFLAG_CLIENT, ConKeyEmoticon, this, "Open emote selector");
	Console()->Register("emote", "i[emote-id]", CFGFLAG_CLIENT, ConEmote, this, "Use emote");
}

void CEmoticon::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_SelectedEmote = -1;
	m_SelectedEyeEmote = -1;
	m_LastSelectedEmote = -1;
	m_LastSelectedEyeEmote = -1;
	m_TouchPressedOutside = false;
	m_OpenAnimation = 0.0f;
	m_CircleAnimation = 0.0f;
	m_BackgroundPulse = 0.0f;
	m_CursorAnimation = 0.0f;
	m_SelectionRingAngle = 0.0f;
	for(int i = 0; i < NUM_EMOTICONS; i++)
	{
		m_aEmoteAnimations[i] = 0.0f;
		m_aEmoteRotations[i] = 0.0f;
		m_aEmoteBounce[i] = 0.0f;
	}
	for(int i = 0; i < NUM_EMOTES; i++)
	{
		m_aEyeEmoteAnimations[i] = 0.0f;
		m_aEyeEmoteBounce[i] = 0.0f;
	}
}

void CEmoticon::OnRelease()
{
	m_Active = false;
}

bool CEmoticon::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Active)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	m_SelectorMouse += vec2(x, y);
	return true;
}

bool CEmoticon::OnInput(const IInput::CEvent &Event)
{
	if(IsActive() && Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		OnRelease();
		return true;
	}
	return false;
}

void CEmoticon::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const float FrameTime = Client()->RenderFrameTime();
	const float GlobalTime = Client()->LocalTime();

	const float AnimSpeedOpen = 10.0f;
	const float AnimSpeedClose = 8.0f;

	if(m_Active)
		m_OpenAnimation = minimum(m_OpenAnimation + FrameTime * AnimSpeedOpen, 1.0f);
	else
		m_OpenAnimation = maximum(m_OpenAnimation - FrameTime * AnimSpeedClose, 0.0f);

	m_BackgroundPulse = std::fmod(m_BackgroundPulse + FrameTime * 2.0f, 2.0f * pi);
	m_SelectionRingAngle = std::fmod(m_SelectionRingAngle + FrameTime * 1.5f, 2.0f * pi);

	if(m_OpenAnimation <= 0.0f)
	{
		if(m_TouchPressedOutside)
		{
			m_SelectedEmote = -1;
			m_SelectedEyeEmote = -1;
			m_TouchPressedOutside = false;
		}

		if(m_WasActive && m_SelectedEmote != -1)
			Emote(m_SelectedEmote);
		if(m_WasActive && m_SelectedEyeEmote != -1)
			EyeEmote(m_SelectedEyeEmote);
		m_WasActive = false;
		m_CircleAnimation = 0.0f;
		m_CursorAnimation = 0.0f;
		return;
	}

	if(GameClient()->m_Snap.m_SpecInfo.m_Active || !GameClient()->m_Snap.m_pLocalCharacter)
	{
		m_Active = false;
		m_WasActive = false;
		return;
	}

	m_WasActive = true;

	m_CircleAnimation = minimum(m_CircleAnimation + FrameTime * 6.0f, 1.0f);
	m_CursorAnimation = minimum(m_CursorAnimation + FrameTime * 8.0f, 1.0f);

	const CUIRect Screen = *Ui()->Screen();

	const bool WasTouchPressed = m_TouchState.m_AnyPressed;
	Ui()->UpdateTouchState(m_TouchState);
	if(m_TouchState.m_AnyPressed)
	{
		const vec2 TouchPos = (m_TouchState.m_PrimaryPosition - vec2(0.5f, 0.5f)) * Screen.Size();
		const float TouchCenterDistance = length(TouchPos);
		if(TouchCenterDistance <= 170.0f)
		{
			m_SelectorMouse = TouchPos;
		}
		else if(TouchCenterDistance > 190.0f)
		{
			m_TouchPressedOutside = true;
		}
	}
	else if(WasTouchPressed)
	{
		m_Active = false;
		return;
	}

	if(length(m_SelectorMouse) > 170.0f)
		m_SelectorMouse = normalize(m_SelectorMouse) * 170.0f;

	float SelectedAngle = angle(m_SelectorMouse) + 2 * pi / 24;
	if(SelectedAngle < 0)
		SelectedAngle += 2 * pi;

	m_SelectedEmote = -1;
	m_SelectedEyeEmote = -1;
	if(length(m_SelectorMouse) > 110.0f)
		m_SelectedEmote = (int)(SelectedAngle / (2 * pi) * (float)NUM_EMOTICONS);
	else if(length(m_SelectorMouse) > 40.0f)
		m_SelectedEyeEmote = (int)(SelectedAngle / (2 * pi) * (float)NUM_EMOTES);

	if(m_SelectedEmote != m_LastSelectedEmote && m_SelectedEmote != -1)
		m_aEmoteBounce[m_SelectedEmote] = 1.0f;
	m_LastSelectedEmote = m_SelectedEmote;

	if(m_SelectedEyeEmote != m_LastSelectedEyeEmote && m_SelectedEyeEmote != -1)
		m_aEyeEmoteBounce[m_SelectedEyeEmote] = 1.0f;
	m_LastSelectedEyeEmote = m_SelectedEyeEmote;

	for(int i = 0; i < NUM_EMOTICONS; i++)
	{
		const float Target = (m_SelectedEmote == i) ? 1.0f : 0.0f;
		m_aEmoteAnimations[i] += (Target - m_aEmoteAnimations[i]) * minimum(FrameTime * 15.0f, 1.0f);
		m_aEmoteBounce[i] = maximum(m_aEmoteBounce[i] - FrameTime * 4.0f, 0.0f);

		if(m_SelectedEmote == i)
			m_aEmoteRotations[i] += FrameTime * 3.0f;
		else
			m_aEmoteRotations[i] += (0.0f - m_aEmoteRotations[i]) * minimum(FrameTime * 5.0f, 1.0f);
	}

	for(int i = 0; i < NUM_EMOTES; i++)
	{
		const float Target = (m_SelectedEyeEmote == i) ? 1.0f : 0.0f;
		m_aEyeEmoteAnimations[i] += (Target - m_aEyeEmoteAnimations[i]) * minimum(FrameTime * 15.0f, 1.0f);
		m_aEyeEmoteBounce[i] = maximum(m_aEyeEmoteBounce[i] - FrameTime * 4.0f, 0.0f);
	}

	const float EaseOutBack = [](float t) {
		const float c1 = 1.70158f;
		const float c3 = c1 + 1.0f;
		return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
	}(m_OpenAnimation);

	const float EaseCircle = m_CircleAnimation * (2.0f - m_CircleAnimation);
	const float EaseCursor = m_CursorAnimation * (2.0f - m_CursorAnimation);

	const vec2 ScreenCenter = Screen.Center();

	Ui()->MapScreen();

	Graphics()->BlendNormal();

	const float PulseScale = 1.0f + std::sin(m_BackgroundPulse) * 0.02f;
	const float OuterRadius = 190.0f * EaseOutBack * PulseScale;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	const float BgAlpha = 0.3f + std::sin(m_BackgroundPulse) * 0.05f;
	Graphics()->SetColor(0, 0, 0, BgAlpha * m_OpenAnimation);
	Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, OuterRadius, 64);
	Graphics()->QuadsEnd();

	if(m_SelectedEmote != -1)
	{
		float HighlightAngle = 2 * pi * m_SelectedEmote / (float)NUM_EMOTICONS;
		if(HighlightAngle > pi)
			HighlightAngle -= 2 * pi;

		const float HighlightDist = 150.0f * EaseOutBack;
		const vec2 HighlightPos = ScreenCenter + direction(HighlightAngle) * HighlightDist;

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		const float RingPulse = 0.6f + std::sin(GlobalTime * 8.0f) * 0.2f;
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, RingPulse * m_OpenAnimation);
		Graphics()->DrawCircle(HighlightPos.x, HighlightPos.y, 35.0f + m_aEmoteAnimations[m_SelectedEmote] * 10.0f, 32);
		Graphics()->SetColor(0, 0, 0, 0.4f * m_OpenAnimation);
		Graphics()->DrawCircle(HighlightPos.x, HighlightPos.y, 30.0f + m_aEmoteAnimations[m_SelectedEmote] * 8.0f, 32);
		Graphics()->QuadsEnd();
	}

	Graphics()->WrapClamp();
	for(int Emote = 0; Emote < NUM_EMOTICONS; Emote++)
	{
		float Angle = 2 * pi * Emote / (float)NUM_EMOTICONS;
		if(Angle > pi)
			Angle -= 2 * pi;

		const float EmoteDelay = (float)Emote / (float)NUM_EMOTICONS;
		const float DelayedCircle = std::clamp((EaseCircle - EmoteDelay * 0.5f) * 2.0f, 0.0f, 1.0f);
		const float EmoteAppear = DelayedCircle * DelayedCircle * (3.0f - 2.0f * DelayedCircle);

		const float BounceScale = 1.0f + m_aEmoteBounce[Emote] * 0.4f * std::sin(m_aEmoteBounce[Emote] * pi);
		const float HoverScale = 50.0f + 30.0f * m_aEmoteAnimations[Emote];
		const float FloatOffset = std::sin(GlobalTime * 2.0f + Emote * 0.5f) * 3.0f * m_aEmoteAnimations[Emote];
		const float Size = HoverScale * EaseOutBack * EmoteAppear * BounceScale;

		const float NudgeDistance = (150.0f + FloatOffset) * EaseOutBack;
		const vec2 Nudge = direction(Angle) * NudgeDistance;

		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[Emote]);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, m_OpenAnimation * EmoteAppear);

		IGraphics::CQuadItem QuadItem(ScreenCenter.x + Nudge.x, ScreenCenter.y + Nudge.y, Size, Size);
		Graphics()->QuadsSetRotation(m_aEmoteRotations[Emote]);
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Graphics()->QuadsDraw(&QuadItem, 1);
		Graphics()->QuadsSetRotation(0);
		Graphics()->QuadsEnd();
	}
	Graphics()->WrapNormal();

	if(GameClient()->m_GameInfo.m_AllowEyeWheel && g_Config.m_ClEyeWheel && GameClient()->m_aLocalIds[g_Config.m_ClDummy] >= 0)
	{
		const float InnerPulse = 1.0f + std::sin(m_BackgroundPulse + pi) * 0.02f;
		const float InnerRadius = 100.0f * EaseOutBack * InnerPulse;
		
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.3f * m_OpenAnimation);
		Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, InnerRadius, 64);
		Graphics()->QuadsEnd();

		if(m_SelectedEyeEmote != -1)
		{
			float HighlightAngle = 2 * pi * m_SelectedEyeEmote / (float)NUM_EMOTES;
			if(HighlightAngle > pi)
				HighlightAngle -= 2 * pi;

			const float HighlightDist = 70.0f * EaseOutBack;
			const vec2 HighlightPos = ScreenCenter + direction(HighlightAngle) * HighlightDist;

			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			const float RingPulse = 0.5f + std::sin(GlobalTime * 8.0f) * 0.15f;
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, RingPulse * m_OpenAnimation);
			Graphics()->DrawCircle(HighlightPos.x, HighlightPos.y, 28.0f + m_aEyeEmoteAnimations[m_SelectedEyeEmote] * 6.0f, 32);
			Graphics()->SetColor(0, 0, 0, 0.3f * m_OpenAnimation);
			Graphics()->DrawCircle(HighlightPos.x, HighlightPos.y, 24.0f + m_aEyeEmoteAnimations[m_SelectedEyeEmote] * 4.0f, 32);
			Graphics()->QuadsEnd();
		}

		CTeeRenderInfo TeeInfo = GameClient()->m_aClients[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_RenderInfo;

		for(int Emote = 0; Emote < NUM_EMOTES; Emote++)
		{
			float Angle = 2 * pi * Emote / (float)NUM_EMOTES;
			if(Angle > pi)
				Angle -= 2 * pi;

			const float EmoteDelay = (float)Emote / (float)NUM_EMOTES;
			const float DelayedCircle = std::clamp((EaseCircle - EmoteDelay * 0.3f) * 3.0f, 0.0f, 1.0f);
			const float EmoteAppear = DelayedCircle * DelayedCircle * (3.0f - 2.0f * DelayedCircle);

			const float BounceScale = 1.0f + m_aEyeEmoteBounce[Emote] * 0.3f * std::sin(m_aEyeEmoteBounce[Emote] * pi);
			const float HoverScale = 48.0f + 16.0f * m_aEyeEmoteAnimations[Emote];
			const float FloatOffset = std::sin(GlobalTime * 2.5f + Emote * 0.8f) * 2.0f * m_aEyeEmoteAnimations[Emote];
			TeeInfo.m_Size = HoverScale * EaseOutBack * EmoteAppear * BounceScale;

			const float NudgeDistance = (70.0f + FloatOffset) * EaseOutBack;
			const vec2 Nudge = direction(Angle) * NudgeDistance;

			RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, Emote, vec2(-1, 0), ScreenCenter + Nudge, m_OpenAnimation * EmoteAppear);
		}

		const float CenterPulse = 1.0f + std::sin(m_BackgroundPulse * 2.0f) * 0.03f;
		const float CenterRadius = 30.0f * EaseOutBack * CenterPulse;
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0, 0, 0, 0.3f * m_OpenAnimation);
		Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, CenterRadius, 64);
		Graphics()->QuadsEnd();
	}
	else
		m_SelectedEyeEmote = -1;

	const float CursorPulse = 1.0f + std::sin(GlobalTime * 6.0f) * 0.1f;
	const float CursorSize = 24.0f * EaseCursor * CursorPulse;
	const vec2 CursorPos = ScreenCenter + m_SelectorMouse * EaseOutBack;
	RenderTools()->RenderCursor(CursorPos, CursorSize);
}

void CEmoticon::Emote(int Emoticon)
{
	CNetMsg_Cl_Emoticon Msg;
	Msg.m_Emoticon = Emoticon;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);

	if(g_Config.m_ClDummyCopyMoves)
	{
		CMsgPacker MsgDummy(NETMSGTYPE_CL_EMOTICON, false);
		MsgDummy.AddInt(Emoticon);
		Client()->SendMsg(!g_Config.m_ClDummy, &MsgDummy, MSGFLAG_VITAL);
	}

	Client()->SendEmoteExtraDummies(Emoticon);
}

void CEmoticon::EyeEmote(int Emote)
{
	char aBuf[32];
	switch(Emote)
	{
	case EMOTE_NORMAL:
		str_format(aBuf, sizeof(aBuf), "/emote normal %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_PAIN:
		str_format(aBuf, sizeof(aBuf), "/emote pain %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_HAPPY:
		str_format(aBuf, sizeof(aBuf), "/emote happy %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_SURPRISE:
		str_format(aBuf, sizeof(aBuf), "/emote surprise %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_ANGRY:
		str_format(aBuf, sizeof(aBuf), "/emote angry %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_BLINK:
		str_format(aBuf, sizeof(aBuf), "/emote blink %d", g_Config.m_ClEyeDuration);
		break;
	}
	GameClient()->m_Chat.SendChat(0, aBuf);
	Client()->SendEyeEmoteExtraDummies(aBuf);
}