#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_DUMMYESCONNECTOR_DUMMY_MANAGER_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_DUMMYESCONNECTOR_DUMMY_MANAGER_H

#include <game/client/component.h>
#include <game/client/ui.h>
#include <base/color.h>
#include <engine/client/enums.h>

struct CDummyConfig
{
	char m_aName[16];
	char m_aClan[12];
	char m_aSkin[24];
	int m_UseCustomColor;
	unsigned m_ColorBody;
	unsigned m_ColorFeet;
	bool m_Connected;
	bool m_CopyMoves;
	char m_aRconPassword[128] = "";
	char m_aRconUsername[64] = "";
	bool m_RconAuthed = false;
	bool m_UseSocks5;
	char m_aSocks5Addr[128];
	char m_aSocks5User[64];
	char m_aSocks5Pass[64];
	bool m_CopyChat;
	bool m_CopyEmotes;
	bool m_LookAtPlayer;
	bool m_FollowPlayer;

	CDummyConfig()
	{
		m_aName[0] = '\0';
		m_aClan[0] = '\0';
		str_copy(m_aSkin, "default");
		m_UseCustomColor = 0;
		m_ColorBody = 65408;
		m_UseSocks5 = false;
		m_aSocks5Addr[0] = '\0';
		m_aSocks5User[0] = '\0';
		m_aSocks5Pass[0] = '\0';
		m_ColorFeet = 65408;
		m_Connected = false;
		m_CopyMoves = false;
		m_CopyChat = false;
		m_CopyEmotes = false;
		m_LookAtPlayer = false;
		m_FollowPlayer = false;
	}
};

class CDummyManager : public CComponent
{
	bool m_Active;
	int m_SelectedDummy;
	int m_ActiveTab;
	CDummyConfig m_aDummies[MAX_EXTRA_DUMMIES];

public:
	CDummyManager();
	virtual int Sizeof() const override { return sizeof(*this); }

	void OnRender() override;
	void OnStateChange(int NewState, int OldState) override;

	void Toggle();
	bool IsActive() const { return m_Active; }
	bool OnInput(const IInput::CEvent &Event) override;

	CDummyConfig *GetDummyConfig(int Index);
	void RenderOverlay();

private:
	void RenderManager(CUIRect MainView);
	int m_LastSyncedDummy;
	void RenderDummyList(CUIRect View);
	void RenderDummyDetails(CUIRect View);
	void RenderDummyPreview(CUIRect View, int Index);
	void RenderSettings(CUIRect View); 
};

#endif