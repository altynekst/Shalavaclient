#include "dummy_manager.h"

#include <base/math.h>
#include <base/vmath.h>
#include <base/color.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/animstate.h>
#include <game/client/components/menus.h>
#include <game/client/components/skins.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/localization.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>

CDummyManager::CDummyManager()
{
	ANTI_DISASM_BLOCK();
	m_Active = false;

	m_SelectedDummy = 0;
	int m_ActiveTab = 0;

	for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)
	{
		str_format(m_aDummies[i].m_aName, sizeof(m_aDummies[i].m_aName), "Dummy %d", i + 1);
		str_copy(m_aDummies[i].m_aSkin, "default");
		m_aDummies[i].m_UseCustomColor = 0;
    	m_LastSyncedDummy = -1;
		m_aDummies[i].m_ColorBody = 65408;
		m_aDummies[i].m_ColorFeet = 65408;
		m_aDummies[i].m_Connected = false;
		m_aDummies[i].m_CopyMoves = false;
		m_aDummies[i].m_CopyChat = false;
		m_aDummies[i].m_CopyEmotes = false;
		m_aDummies[i].m_LookAtPlayer = false;
		m_aDummies[i].m_FollowPlayer = false;
	}
}

void CDummyManager::OnStateChange(int NewState, int OldState)
{
	ANTI_DISASM_BLOCK();
	if(NewState != IClient::STATE_ONLINE)

		m_Active = false;
}

void CDummyManager::Toggle()
{
	ANTI_DISASM_BLOCK();
	m_Active = !m_Active;

}

CDummyConfig *CDummyManager::GetDummyConfig(int Index)
{
	if(Index < 0 || Index >= MAX_EXTRA_DUMMIES)
		return nullptr;
	return &m_aDummies[Index];
}

void CDummyManager::OnRender()
{
	ANTI_DISASM_BLOCK();
	for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)

	{
		m_aDummies[i].m_Connected = Client()->ExtraDummyConnected(i);
		if(m_aDummies[i].m_Connected)
		{
			Client()->SetExtraDummyCopyMoves(i, m_aDummies[i].m_CopyMoves);
			Client()->SetExtraDummyCopyChat(i, m_aDummies[i].m_CopyChat);
			Client()->SetExtraDummyCopyEmotes(i, m_aDummies[i].m_CopyEmotes);
			Client()->SetExtraDummyLookAtPlayer(i, m_aDummies[i].m_LookAtPlayer);
			Client()->SetExtraDummyFollowPlayer(i, m_aDummies[i].m_FollowPlayer);
		}
	}
}

bool CDummyManager::OnInput(const IInput::CEvent &Event)
{
	ANTI_DISASM_BLOCK();
	if(!m_Active)

		return false;

	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		m_Active = false;
		return true;
	}

	return false;
}

void CDummyManager::RenderOverlay()
{
	ANTI_DISASM_BLOCK();
	if(!m_Active)

		return;

	CUIRect Screen = *Ui()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

	CUIRect Box;
	Screen.Margin(30.0f, &Box);
	Box.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);

	CUIRect Inner;
	Box.Margin(5.0f, &Inner);

	RenderManager(Inner);
}

void CDummyManager::RenderManager(CUIRect MainView)
{
	ANTI_DISASM_BLOCK();
	CUIRect TitleBar, TabBar, Content;

	MainView.HSplitTop(24.0f, &TitleBar, &MainView);
	
	TitleBar.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_T, 5.0f);
	
	CUIRect CloseBtn;
	TitleBar.VSplitRight(60.0f, &TitleBar, &CloseBtn);
	Ui()->DoLabel(&TitleBar, "Dummy Manager", 16.0f, TEXTALIGN_MC);
	
	static CButtonContainer s_CloseButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_CloseButton, "×", 0, &CloseBtn))
		m_Active = false;

	MainView.HSplitTop(26.0f, &TabBar, &MainView);

	CUIRect MainTabBar;
	TabBar.VSplitLeft(120.0f, &MainTabBar, &TabBar);

	static CButtonContainer s_TabDummies;
	static CButtonContainer s_TabSettings;

	CUIRect Tab1, Tab2;
	MainTabBar.VSplitMid(&Tab1, &Tab2);

	if(GameClient()->m_Menus.DoButton_MenuTab(&s_TabDummies, "Dummies", m_ActiveTab == 0, &Tab1, IGraphics::CORNER_L, nullptr, nullptr, nullptr, nullptr, 4.0f))
		m_ActiveTab = 0;
	if(GameClient()->m_Menus.DoButton_MenuTab(&s_TabSettings, "Settings", m_ActiveTab == 1, &Tab2, IGraphics::CORNER_R, nullptr, nullptr, nullptr, nullptr, 4.0f))
		m_ActiveTab = 1;

	if(m_ActiveTab == 0)
	{
		float DummyTabWidth = TabBar.w / (float)MAX_EXTRA_DUMMIES;
		static CButtonContainer s_aTabButtons[MAX_EXTRA_DUMMIES];
		
		for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)
		{
			CUIRect Tab;
			TabBar.VSplitLeft(DummyTabWidth, &Tab, &TabBar);
			
			char aBuf[32];
			if(m_aDummies[i].m_aName[0])
				str_format(aBuf, sizeof(aBuf), "%s%s", m_aDummies[i].m_aName, m_aDummies[i].m_Connected ? " ●" : "");
			else
				str_format(aBuf, sizeof(aBuf), "Dummy %d%s", i + 1, m_aDummies[i].m_Connected ? " ●" : "");
			
			int Corners = IGraphics::CORNER_NONE;
			if(i == 0)
				Corners = IGraphics::CORNER_NONE;
			if(i == MAX_EXTRA_DUMMIES - 1)
				Corners = IGraphics::CORNER_R;
			
			if(GameClient()->m_Menus.DoButton_MenuTab(&s_aTabButtons[i], aBuf, m_SelectedDummy == i, &Tab, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
				m_SelectedDummy = i;
		}
	}

	MainView.HSplitTop(10.0f, nullptr, &MainView);
	MainView.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_ALL, 5.0f);
	MainView.Margin(10.0f, &Content);
	
	if(m_ActiveTab == 0)
		RenderDummyDetails(Content);
	else
		RenderSettings(Content);
}

void CDummyManager::RenderSettings(CUIRect MainView)
{
	ANTI_DISASM_BLOCK();
	CUIRect Row, Label;

	Ui()->DoLabel_AutoLineSize("Quick Settings (All Dummies)", 20.0f, TEXTALIGN_ML, &MainView, 30.0f);

	MainView.HSplitTop(10.0f, nullptr, &MainView);

	int NumConnected = 0;
	for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)
		if(m_aDummies[i].m_Connected)
			NumConnected++;

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "Connected dummies: %d / %d", NumConnected, MAX_EXTRA_DUMMIES);
	MainView.HSplitTop(16.0f, &Row, &MainView);
	TextRender()->TextColor(0.7f, 0.7f, 0.7f, 1.0f);
	Ui()->DoLabel(&Row, aBuf, 12.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	MainView.HSplitTop(15.0f, nullptr, &MainView);

	CUIRect LeftView, RightView;
	MainView.VSplitMid(&LeftView, &RightView, 20.0f);

	Ui()->DoLabel_AutoLineSize("Behavior (All)", 16.0f, TEXTALIGN_ML, &LeftView, 25.0f);
	LeftView.HSplitTop(5.0f, nullptr, &LeftView);

	static int s_AllCopyMoves = 0;
	static int s_AllCopyChat = 0;
	static int s_AllCopyEmotes = 0;
	static int s_AllLookAtPlayer = 0;
	static int s_AllFollowPlayer = 0;

	LeftView.HSplitTop(20.0f, &Row, &LeftView);
	if(GameClient()->m_Menus.DoButton_CheckBox(&s_AllCopyMoves, "All Copy Moves", s_AllCopyMoves, &Row))
	{
		s_AllCopyMoves = !s_AllCopyMoves;
		for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)
			if(m_aDummies[i].m_Connected)
				m_aDummies[i].m_CopyMoves = s_AllCopyMoves;
	}

	LeftView.HSplitTop(20.0f, &Row, &LeftView);
	if(GameClient()->m_Menus.DoButton_CheckBox(&s_AllCopyChat, "All Copy Chat", s_AllCopyChat, &Row))
	{
		s_AllCopyChat = !s_AllCopyChat;
		for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)
			if(m_aDummies[i].m_Connected)
				m_aDummies[i].m_CopyChat = s_AllCopyChat;
	}

	LeftView.HSplitTop(20.0f, &Row, &LeftView);
	if(GameClient()->m_Menus.DoButton_CheckBox(&s_AllCopyEmotes, "All Copy Emotes", s_AllCopyEmotes, &Row))
	{
		s_AllCopyEmotes = !s_AllCopyEmotes;
		for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)
			if(m_aDummies[i].m_Connected)
				m_aDummies[i].m_CopyEmotes = s_AllCopyEmotes;
	}

	LeftView.HSplitTop(20.0f, &Row, &LeftView);
	if(GameClient()->m_Menus.DoButton_CheckBox(&s_AllLookAtPlayer, "All Look At Player", s_AllLookAtPlayer, &Row))
	{
		s_AllLookAtPlayer = !s_AllLookAtPlayer;
		for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)
			if(m_aDummies[i].m_Connected)
				m_aDummies[i].m_LookAtPlayer = s_AllLookAtPlayer;
	}

	LeftView.HSplitTop(20.0f, &Row, &LeftView);
	if(GameClient()->m_Menus.DoButton_CheckBox(&s_AllFollowPlayer, "All Follow Player", s_AllFollowPlayer, &Row))
	{
		s_AllFollowPlayer = !s_AllFollowPlayer;
		for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)
			if(m_aDummies[i].m_Connected)
				m_aDummies[i].m_FollowPlayer = s_AllFollowPlayer;
	}

	Ui()->DoLabel_AutoLineSize("Rcon (All)", 16.0f, TEXTALIGN_ML, &RightView, 25.0f);
	RightView.HSplitTop(5.0f, nullptr, &RightView);

	static CLineInputBuffered<64> s_AllRconUsername;
	static CLineInputBuffered<128> s_AllRconPassword;

	RightView.HSplitTop(20.0f, &Row, &RightView);
	Row.VSplitLeft(80.0f, &Label, &Row);
	Row.VSplitLeft(200.0f, &Row, nullptr);
	Ui()->DoLabel(&Label, "Username:", 14.0f, TEXTALIGN_ML);
	Ui()->DoEditBox(&s_AllRconUsername, &Row, 14.0f);

	RightView.HSplitTop(5.0f, nullptr, &RightView);

	RightView.HSplitTop(20.0f, &Row, &RightView);
	Row.VSplitLeft(80.0f, &Label, &Row);
	Row.VSplitLeft(200.0f, &Row, nullptr);
	Ui()->DoLabel(&Label, "Password:", 14.0f, TEXTALIGN_ML);
	Ui()->DoEditBox(&s_AllRconPassword, &Row, 14.0f);

	RightView.HSplitTop(10.0f, nullptr, &RightView);

	RightView.HSplitTop(20.0f, &Row, &RightView);
	Row.VSplitLeft(200.0f, &Row, nullptr);
	static CButtonContainer s_AllRconAuthBtn;
	if(GameClient()->m_Menus.DoButton_Menu(&s_AllRconAuthBtn, "Rcon Auth All Dummies", 0, &Row))
	{
		for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)
		{
			if(m_aDummies[i].m_Connected && !Client()->ExtraDummyRconAuthed(i))
			{
				str_copy(m_aDummies[i].m_aRconUsername, s_AllRconUsername.GetString(), sizeof(m_aDummies[i].m_aRconUsername));
				str_copy(m_aDummies[i].m_aRconPassword, s_AllRconPassword.GetString(), sizeof(m_aDummies[i].m_aRconPassword));
				Client()->ExtraDummyRconAuth(i, m_aDummies[i].m_aRconUsername, m_aDummies[i].m_aRconPassword);
			}
		}
	}

	RightView.HSplitTop(10.0f, nullptr, &RightView);

	static CLineInputBuffered<256> s_AllRconCmdInput;
	RightView.HSplitTop(20.0f, &Row, &RightView);
	Row.VSplitLeft(80.0f, &Label, &Row);
	Row.VSplitLeft(200.0f, &Row, nullptr);
	Ui()->DoLabel(&Label, "Command:", 14.0f, TEXTALIGN_ML);

	CUIRect SendBtn;
	Row.VSplitRight(50.0f, &Row, &SendBtn);
	Row.VSplitRight(5.0f, &Row, nullptr);
	Ui()->DoEditBox(&s_AllRconCmdInput, &Row, 14.0f);

	static CButtonContainer s_AllRconSendBtn;
	if(GameClient()->m_Menus.DoButton_Menu(&s_AllRconSendBtn, "Send", 0, &SendBtn))
	{
		if(s_AllRconCmdInput.GetString()[0])
		{
			for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)
			{
				if(m_aDummies[i].m_Connected && Client()->ExtraDummyRconAuthed(i))
					Client()->ExtraDummyRcon(i, s_AllRconCmdInput.GetString());
			}
			s_AllRconCmdInput.Clear();
		}
	}

	RightView.HSplitTop(15.0f, nullptr, &RightView);

	int NumAuthed = 0;
	for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)
		if(m_aDummies[i].m_Connected && Client()->ExtraDummyRconAuthed(i))
			NumAuthed++;

	str_format(aBuf, sizeof(aBuf), "Rcon authed: %d / %d", NumAuthed, NumConnected);
	RightView.HSplitTop(16.0f, &Row, &RightView);
	TextRender()->TextColor(NumAuthed > 0 ? ColorRGBA(0.2f, 1.0f, 0.2f, 1.0f) : ColorRGBA(0.7f, 0.7f, 0.7f, 1.0f));
	Ui()->DoLabel(&Row, aBuf, 12.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	MainView.HSplitBottom(30.0f, &MainView, &Row);
	Row.HSplitTop(5.0f, nullptr, &Row);

	CUIRect ConnAllBtn, DisconnAllBtn;
	Row.VSplitLeft(150.0f, &ConnAllBtn, &Row);
	Row.VSplitLeft(10.0f, nullptr, &Row);
	Row.VSplitLeft(150.0f, &DisconnAllBtn, &Row);

	static CButtonContainer s_ConnectAllBtn;
	if(GameClient()->m_Menus.DoButton_Menu(&s_ConnectAllBtn, "Connect All", 0, &ConnAllBtn))
	{
		for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)
		{
			if(!m_aDummies[i].m_Connected)
			{
				int Slot = Client()->ExtraDummyConnect(
					i,
					m_aDummies[i].m_aName,
					m_aDummies[i].m_aClan,
					m_aDummies[i].m_aSkin,
					m_aDummies[i].m_UseCustomColor,
					m_aDummies[i].m_ColorBody,
					m_aDummies[i].m_ColorFeet,
					m_aDummies[i].m_UseSocks5,
					m_aDummies[i].m_aSocks5Addr,
					m_aDummies[i].m_aSocks5User,
					m_aDummies[i].m_aSocks5Pass);
				if(Slot >= 0)
					m_aDummies[i].m_Connected = true;
			}
		}
	}

	static CButtonContainer s_DisconnectAllBtn;
	if(GameClient()->m_Menus.DoButton_Menu(&s_DisconnectAllBtn, "Disconnect All", 0, &DisconnAllBtn))
	{
		for(int i = 0; i < MAX_EXTRA_DUMMIES; i++)
		{
			if(m_aDummies[i].m_Connected)
			{
				Client()->ExtraDummyDisconnect(i);
				m_aDummies[i].m_Connected = false;
			}
		}
	}
}

void RenderSettings(CUIRect View);

void CDummyManager::RenderDummyList(CUIRect View)
{

}

void CDummyManager::RenderDummyDetails(CUIRect MainView)
{
	ANTI_DISASM_BLOCK();
	if(m_SelectedDummy < 0 || m_SelectedDummy >= MAX_EXTRA_DUMMIES)

		return;

	CDummyConfig *pDummy = &m_aDummies[m_SelectedDummy];

	CUIRect LeftView, RightView;
	MainView.VSplitMid(&LeftView, &RightView, 20.0f);

	CUIRect Label, Button, Row;

	{
		CUIRect PreviewArea;
		LeftView.HSplitTop(90.0f, &PreviewArea, &LeftView);

		CUIRect TeeArea;
		PreviewArea.VSplitLeft(65.0f, &TeeArea, &PreviewArea);

		const CSkin *pSkin = GameClient()->m_Skins.Find(pDummy->m_aSkin[0] ? pDummy->m_aSkin : "default");
		CTeeRenderInfo TeeInfo;
		TeeInfo.Apply(pSkin);
		if(pDummy->m_UseCustomColor)
		{
			TeeInfo.m_CustomColoredSkin = true;
			TeeInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(pDummy->m_ColorBody).UnclampLighting(ColorHSLA::DARKEST_LGT));
			TeeInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(pDummy->m_ColorFeet).UnclampLighting(ColorHSLA::DARKEST_LGT));
		}
		TeeInfo.m_Size = 50.0f;

		vec2 OffsetToMid;
		CRenderTools::GetRenderTeeOffsetToRenderedTee(CAnimState::GetIdle(), &TeeInfo, OffsetToMid);
		vec2 TeePos(TeeArea.x + TeeArea.w / 2.0f, TeeArea.y + TeeArea.h / 2.0f + OffsetToMid.y);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeePos);

		PreviewArea.HSplitTop(20.0f, &Row, &PreviewArea);
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s", pDummy->m_aName[0] ? pDummy->m_aName : "unnamed");
		Ui()->DoLabel(&Row, aBuf, 16.0f, TEXTALIGN_ML);

		PreviewArea.HSplitTop(16.0f, &Row, &PreviewArea);
		str_format(aBuf, sizeof(aBuf), "Skin: %s", pDummy->m_aSkin[0] ? pDummy->m_aSkin : "default");
		TextRender()->TextColor(0.7f, 0.7f, 0.7f, 1.0f);
		Ui()->DoLabel(&Row, aBuf, 12.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		if(pDummy->m_Connected)
		{
			PreviewArea.HSplitTop(16.0f, &Row, &PreviewArea);
			TextRender()->TextColor(0.2f, 1.0f, 0.2f, 1.0f);
			Ui()->DoLabel(&Row, "Connected", 12.0f, TEXTALIGN_ML);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
	}

	LeftView.HSplitTop(5.0f, nullptr, &LeftView);

	Ui()->DoLabel_AutoLineSize(Localize("Identity"), 20.0f, TEXTALIGN_ML, &LeftView, 30.0f);
	LeftView.HSplitTop(5.0f, nullptr, &LeftView);

	static CLineInputBuffered<16> s_NameInput;
	static CLineInputBuffered<12> s_ClanInput;
	static CLineInputBuffered<24> s_SkinInput;
	if(m_LastSyncedDummy != m_SelectedDummy)
	{
		s_NameInput.Set(pDummy->m_aName);
		s_ClanInput.Set(pDummy->m_aClan);
		s_SkinInput.Set(pDummy->m_aSkin);
		m_LastSyncedDummy = m_SelectedDummy;
	}

	LeftView.HSplitTop(20.0f, &Row, &LeftView);
	Row.VSplitLeft(80.0f, &Label, &Row);
	Row.VSplitLeft(200.0f, &Row, nullptr);
	Ui()->DoLabel(&Label, "Name:", 14.0f, TEXTALIGN_ML);
	if(Ui()->DoEditBox(&s_NameInput, &Row, 14.0f))
		str_copy(pDummy->m_aName, s_NameInput.GetString(), sizeof(pDummy->m_aName));

	LeftView.HSplitTop(5.0f, nullptr, &LeftView);

	LeftView.HSplitTop(20.0f, &Row, &LeftView);
	Row.VSplitLeft(80.0f, &Label, &Row);
	Row.VSplitLeft(200.0f, &Row, nullptr);
	Ui()->DoLabel(&Label, "Clan:", 14.0f, TEXTALIGN_ML);
	if(Ui()->DoEditBox(&s_ClanInput, &Row, 14.0f))
		str_copy(pDummy->m_aClan, s_ClanInput.GetString(), sizeof(pDummy->m_aClan));

	LeftView.HSplitTop(5.0f, nullptr, &LeftView);

	LeftView.HSplitTop(20.0f, &Row, &LeftView);
	Row.VSplitLeft(80.0f, &Label, &Row);
	Row.VSplitLeft(200.0f, &Row, nullptr);
	Ui()->DoLabel(&Label, "Skin:", 14.0f, TEXTALIGN_ML);
	if(Ui()->DoEditBox(&s_SkinInput, &Row, 14.0f))
		str_copy(pDummy->m_aSkin, s_SkinInput.GetString(), sizeof(pDummy->m_aSkin));

	LeftView.HSplitTop(10.0f, nullptr, &LeftView);

	LeftView.HSplitTop(20.0f, &Row, &LeftView);
	if(GameClient()->m_Menus.DoButton_CheckBox(&pDummy->m_UseCustomColor, "Custom colors", pDummy->m_UseCustomColor, &Row))
		pDummy->m_UseCustomColor = !pDummy->m_UseCustomColor;

	if(pDummy->m_UseCustomColor)
	{
		LeftView.HSplitTop(5.0f, nullptr, &LeftView);
		CUIRect ColorArea;
		LeftView.HSplitTop(95.0f, &ColorArea, &LeftView);
		CUIRect aRects[2];
		ColorArea.VSplitMid(&aRects[0], &aRects[1], 20.0f);

		unsigned *apColors[] = {&pDummy->m_ColorBody, &pDummy->m_ColorFeet};
		const char *apParts[] = {"Body", "Feet"};

		for(int i = 0; i < 2; i++)
		{
			CUIRect PartLabel;
			aRects[i].HSplitTop(20.0f, &PartLabel, &aRects[i]);
			Ui()->DoLabel(&PartLabel, apParts[i], 14.0f, TEXTALIGN_ML);
			GameClient()->m_Menus.RenderHslaScrollbars(&aRects[i], apColors[i], false, ColorHSLA::DARKEST_LGT);
		}
	}

	LeftView.HSplitTop(10.0f, nullptr, &LeftView);

	LeftView.HSplitTop(20.0f, &Row, &LeftView);
	Row.VSplitLeft(150.0f, &Row, nullptr);
	static CButtonContainer s_ConnectButton;
	if(pDummy->m_Connected)
	{
		if(GameClient()->m_Menus.DoButton_Menu(&s_ConnectButton, "Disconnect", 0, &Row))
		{
			Client()->ExtraDummyDisconnect(m_SelectedDummy);
			pDummy->m_Connected = false;
		}
	}
	else
	{
		if(GameClient()->m_Menus.DoButton_Menu(&s_ConnectButton, "Connect", 0, &Row))
		{
			int Slot = Client()->ExtraDummyConnect(
				m_SelectedDummy,
				pDummy->m_aName,
				pDummy->m_aClan,
				pDummy->m_aSkin,
				pDummy->m_UseCustomColor,
				pDummy->m_ColorBody,
				pDummy->m_ColorFeet,
				pDummy->m_UseSocks5,
				pDummy->m_aSocks5Addr,
				pDummy->m_aSocks5User,
				pDummy->m_aSocks5Pass);
			if(Slot >= 0)
				pDummy->m_Connected = true;
		}
	}

	Ui()->DoLabel_AutoLineSize("Behavior", 20.0f, TEXTALIGN_ML, &RightView, 30.0f);
	RightView.HSplitTop(5.0f, nullptr, &RightView);

	if(pDummy->m_Connected)
	{
		RightView.HSplitTop(20.0f, &Row, &RightView);
		if(GameClient()->m_Menus.DoButton_CheckBox(&pDummy->m_CopyMoves, "Copy Moves", pDummy->m_CopyMoves, &Row))
			pDummy->m_CopyMoves = !pDummy->m_CopyMoves;

		RightView.HSplitTop(20.0f, &Row, &RightView);
		if(GameClient()->m_Menus.DoButton_CheckBox(&pDummy->m_CopyChat, "Copy Chat", pDummy->m_CopyChat, &Row))
			pDummy->m_CopyChat = !pDummy->m_CopyChat;

		RightView.HSplitTop(20.0f, &Row, &RightView);
		if(GameClient()->m_Menus.DoButton_CheckBox(&pDummy->m_CopyEmotes, "Copy Emotes", pDummy->m_CopyEmotes, &Row))
			pDummy->m_CopyEmotes = !pDummy->m_CopyEmotes;

		RightView.HSplitTop(20.0f, &Row, &RightView);
		if(GameClient()->m_Menus.DoButton_CheckBox(&pDummy->m_LookAtPlayer, "Look At Player", pDummy->m_LookAtPlayer, &Row))
			pDummy->m_LookAtPlayer = !pDummy->m_LookAtPlayer;

		RightView.HSplitTop(20.0f, &Row, &RightView);
		if(GameClient()->m_Menus.DoButton_CheckBox(&pDummy->m_FollowPlayer, "Follow Player", pDummy->m_FollowPlayer, &Row))
			pDummy->m_FollowPlayer = !pDummy->m_FollowPlayer;
				RightView.HSplitTop(15.0f, nullptr, &RightView);
		Ui()->DoLabel_AutoLineSize("Rcon", 16.0f, TEXTALIGN_ML, &RightView, 25.0f);
		RightView.HSplitTop(5.0f, nullptr, &RightView);

		static CLineInputBuffered<64> s_RconUsernameInput;
		static CLineInputBuffered<128> s_RconPasswordInput;

		if(m_LastSyncedDummy != m_SelectedDummy || !pDummy->m_Connected)
		{
			s_RconUsernameInput.Set(pDummy->m_aRconUsername);
			s_RconPasswordInput.Set(pDummy->m_aRconPassword);
		}

		if(Client()->ExtraDummyRconAuthed(m_SelectedDummy))
		{
			TextRender()->TextColor(0.2f, 1.0f, 0.2f, 1.0f);
			RightView.HSplitTop(16.0f, &Row, &RightView);
			Ui()->DoLabel(&Row, "Rcon Authed", 12.0f, TEXTALIGN_ML);
			TextRender()->TextColor(TextRender()->DefaultTextColor());

			RightView.HSplitTop(5.0f, nullptr, &RightView);

			static CLineInputBuffered<256> s_RconCmdInput;
			RightView.HSplitTop(20.0f, &Row, &RightView);
			Row.VSplitLeft(80.0f, &Label, &Row);
			Row.VSplitLeft(200.0f, &Row, nullptr);
			Ui()->DoLabel(&Label, "Command:", 14.0f, TEXTALIGN_ML);

			CUIRect SendBtn;
			Row.VSplitRight(50.0f, &Row, &SendBtn);
			Row.VSplitRight(5.0f, &Row, nullptr);

			Ui()->DoEditBox(&s_RconCmdInput, &Row, 14.0f);

			static CButtonContainer s_SendRconBtn;
			if(GameClient()->m_Menus.DoButton_Menu(&s_SendRconBtn, "Send", 0, &SendBtn))
			{
				if(s_RconCmdInput.GetString()[0])
				{
					Client()->ExtraDummyRcon(m_SelectedDummy, s_RconCmdInput.GetString());
					s_RconCmdInput.Clear();
				}
			}
		}
		else
		{
			RightView.HSplitTop(20.0f, &Row, &RightView);
			Row.VSplitLeft(80.0f, &Label, &Row);
			Row.VSplitLeft(200.0f, &Row, nullptr);
			Ui()->DoLabel(&Label, "Username:", 14.0f, TEXTALIGN_ML);
			if(Ui()->DoEditBox(&s_RconUsernameInput, &Row, 14.0f))
				str_copy(pDummy->m_aRconUsername, s_RconUsernameInput.GetString(), sizeof(pDummy->m_aRconUsername));

			RightView.HSplitTop(5.0f, nullptr, &RightView);

			RightView.HSplitTop(20.0f, &Row, &RightView);
			Row.VSplitLeft(80.0f, &Label, &Row);
			Row.VSplitLeft(200.0f, &Row, nullptr);
			Ui()->DoLabel(&Label, "Password:", 14.0f, TEXTALIGN_ML);
			if(Ui()->DoEditBox(&s_RconPasswordInput, &Row, 14.0f))
				str_copy(pDummy->m_aRconPassword, s_RconPasswordInput.GetString(), sizeof(pDummy->m_aRconPassword));

			RightView.HSplitTop(5.0f, nullptr, &RightView);

			RightView.HSplitTop(20.0f, &Row, &RightView);
			Row.VSplitLeft(150.0f, &Row, nullptr);
			static CButtonContainer s_RconAuthBtn;
			if(GameClient()->m_Menus.DoButton_Menu(&s_RconAuthBtn, "Rcon Auth", 0, &Row))
			{
				Client()->ExtraDummyRconAuth(
					m_SelectedDummy,
					pDummy->m_aRconUsername,
					pDummy->m_aRconPassword);
			}
		}
	}
	else
	{
		TextRender()->TextColor(0.5f, 0.5f, 0.5f, 1.0f);
		RightView.HSplitTop(20.0f, &Row, &RightView);
		Ui()->DoLabel(&Row, "Connect the dummy to configure behavior", 12.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
}

void CDummyManager::RenderDummyPreview(CUIRect View, int Index)
{
}
