/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_EMOTICON_H
#define GAME_CLIENT_COMPONENTS_EMOTICON_H
#include <base/vmath.h>

#include <engine/console.h>

#include <game/client/component.h>
#include <game/client/ui.h>

#include <generated/protocol.h>

class CEmoticon : public CComponent
{
	bool m_WasActive;
	bool m_Active;

	vec2 m_SelectorMouse;
	int m_SelectedEmote;
	int m_SelectedEyeEmote;
	int m_LastSelectedEmote;
	int m_LastSelectedEyeEmote;

	CUi::CTouchState m_TouchState;
	bool m_TouchPressedOutside;

	float m_OpenAnimation;
	float m_aEmoteAnimations[NUM_EMOTICONS];
	float m_aEmoteRotations[NUM_EMOTICONS];
	float m_aEmoteBounce[NUM_EMOTICONS];
	float m_aEyeEmoteAnimations[NUM_EMOTES];
	float m_aEyeEmoteBounce[NUM_EMOTES];
	float m_CircleAnimation;
	float m_BackgroundPulse;
	float m_CursorAnimation;
	float m_SelectionRingAngle;

	static void ConKeyEmoticon(IConsole::IResult *pResult, void *pUserData);
	static void ConEmote(IConsole::IResult *pResult, void *pUserData);

public:
	CEmoticon();
	int Sizeof() const override { return sizeof(*this); }

	void OnReset() override;
	void OnConsoleInit() override;
	void OnRender() override;
	void OnRelease() override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	bool OnInput(const IInput::CEvent &Event) override;

	void Emote(int Emoticon);
	void EyeEmote(int EyeEmote);

	bool IsActive() const { return m_Active; }
};

#endif