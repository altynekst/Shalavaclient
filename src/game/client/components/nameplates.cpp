#include "nameplates.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol7.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>

#include <memory>
#include <vector>

enum class EHookStrongWeakState { WEAK, NEUTRAL, STRONG };

class CNamePlateData
{
public:
	bool m_InGame;
	ColorRGBA m_Color;
	bool m_ShowName;
	char m_aName[std::max<size_t>(MAX_NAME_LENGTH, protocol7::MAX_NAME_ARRAY_SIZE)];
	bool m_ShowFriendMark;
	bool m_ShowClientId;
	int m_ClientId;
	float m_FontSizeClientId;
	bool m_ClientIdSeparateLine;
	float m_FontSize;
	bool m_ShowClan;
	char m_aClan[std::max<size_t>(MAX_CLAN_LENGTH, protocol7::MAX_CLAN_ARRAY_SIZE)];
	float m_FontSizeClan;
	bool m_ShowDirection;
	bool m_DirLeft, m_DirJump, m_DirRight;
	float m_FontSizeDirection;
	bool m_ShowHookStrongWeak;
	EHookStrongWeakState m_HookStrongWeakState;
	bool m_ShowHookStrongWeakId;
	int m_HookStrongWeakId;
	float m_FontSizeHookStrongWeak;
};

static constexpr float DEFAULT_PADDING = 5.0f;
static constexpr ColorRGBA s_OutlineColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f);

class CNamePlatePart
{
protected:
	vec2 m_Size = vec2(0.0f, 0.0f);
	vec2 m_Padding = vec2(DEFAULT_PADDING, DEFAULT_PADDING);
	bool m_NewLine = false;
	bool m_Visible = true;
	bool m_ShiftOnInvis = false;
	CNamePlatePart(CGameClient &) {}

public:
	virtual void Update(CGameClient &, const CNamePlateData &) {}
	virtual void Reset(CGameClient &) {}
	virtual void Render(CGameClient &, vec2) const {}
	vec2 Size() const { return m_Size; }
	vec2 Padding() const { return m_Padding; }
	bool NewLine() const { return m_NewLine; }
	bool Visible() const { return m_Visible; }
	bool ShiftOnInvis() const { return m_ShiftOnInvis; }
	CNamePlatePart() = delete;
	virtual ~CNamePlatePart() = default;
};

using PartsVector = std::vector<std::unique_ptr<CNamePlatePart>>;

class CNamePlatePartText : public CNamePlatePart
{
protected:
	STextContainerIndex m_TextContainerIndex;
	ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);

	virtual bool UpdateNeeded(CGameClient &, const CNamePlateData &) = 0;
	virtual void UpdateText(CGameClient &, const CNamePlateData &) = 0;

	CNamePlatePartText(CGameClient &This) : CNamePlatePart(This) { Reset(This); }

public:
	void Update(CGameClient &This, const CNamePlateData &Data) override
	{
		if(!UpdateNeeded(This, Data) && m_TextContainerIndex.Valid())
			return;

		unsigned Flags = ETextRenderFlags::TEXT_RENDER_FLAG_NO_FIRST_CHARACTER_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_LAST_CHARACTER_ADVANCE;
		if(Data.m_InGame)
			Flags |= ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT;
		This.TextRender()->SetRenderFlags(Flags);

		if(Data.m_InGame)
		{
			float X0, Y0, X1, Y1;
			This.Graphics()->GetScreen(&X0, &Y0, &X1, &Y1);
			This.Graphics()->MapScreenToInterface(This.m_Camera.m_Center.x, This.m_Camera.m_Center.y);
			This.TextRender()->DeleteTextContainer(m_TextContainerIndex);
			UpdateText(This, Data);
			This.Graphics()->MapScreen(X0, Y0, X1, Y1);
		}
		else
			UpdateText(This, Data);

		This.TextRender()->SetRenderFlags(0);

		if(!m_TextContainerIndex.Valid())
		{
			m_Visible = false;
			return;
		}

		const STextBoundingBox Box = This.TextRender()->GetBoundingBoxTextContainer(m_TextContainerIndex);
		m_Size = vec2(Box.m_W, Box.m_H);
	}

	void Reset(CGameClient &This) override
	{
		This.TextRender()->DeleteTextContainer(m_TextContainerIndex);
	}

	void Render(CGameClient &This, vec2 Pos) const override
	{
		if(!m_TextContainerIndex.Valid())
			return;
		This.TextRender()->RenderTextContainer(m_TextContainerIndex,
			m_Color,
			s_OutlineColor.WithMultipliedAlpha(m_Color.a),
			Pos.x - m_Size.x / 2.0f,
			Pos.y - m_Size.y / 2.0f);
	}
};

class CNamePlatePartIcon : public CNamePlatePart
{
protected:
	IGraphics::CTextureHandle m_Texture;
	float m_Rotation = 0.0f;
	ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	CNamePlatePartIcon(CGameClient &This) : CNamePlatePart(This) {}

public:
	void Render(CGameClient &This, vec2 Pos) const override
	{
		IGraphics::CQuadItem Q(Pos.x - m_Size.x / 2.0f, Pos.y - m_Size.y / 2.0f, m_Size.x, m_Size.y);
		This.Graphics()->TextureSet(m_Texture);
		This.Graphics()->QuadsBegin();
		This.Graphics()->SetColor(m_Color);
		This.Graphics()->QuadsSetRotation(m_Rotation);
		This.Graphics()->QuadsDrawTL(&Q, 1);
		This.Graphics()->QuadsEnd();
		This.Graphics()->QuadsSetRotation(0.0f);
	}
};

class CNamePlatePartSprite : public CNamePlatePart
{
protected:
	IGraphics::CTextureHandle m_Texture;
	int m_Sprite = -1;
	int m_SpriteFlags = 0;
	float m_Rotation = 0.0f;
	ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	CNamePlatePartSprite(CGameClient &This) : CNamePlatePart(This) {}

public:
	void Render(CGameClient &This, vec2 Pos) const override
	{
		This.Graphics()->TextureSet(m_Texture);
		This.Graphics()->QuadsSetRotation(m_Rotation);
		This.Graphics()->QuadsBegin();
		This.Graphics()->SetColor(m_Color);
		This.Graphics()->SelectSprite(m_Sprite, m_SpriteFlags);
		This.Graphics()->DrawSprite(Pos.x, Pos.y, m_Size.x, m_Size.y);
		This.Graphics()->QuadsEnd();
		This.Graphics()->QuadsSetRotation(0.0f);
	}
};

class CNamePlatePartNewLine : public CNamePlatePart
{
public:
	CNamePlatePartNewLine(CGameClient &This) : CNamePlatePart(This) { m_NewLine = true; }
};

enum Direction { DIRECTION_LEFT, DIRECTION_UP, DIRECTION_RIGHT };

class CNamePlatePartDirection : public CNamePlatePartIcon
{
	int m_Direction;
public:
	CNamePlatePartDirection(CGameClient &This, Direction Dir) : CNamePlatePartIcon(This), m_Direction(Dir)
	{
		m_Texture = g_pData->m_aImages[IMAGE_ARROW].m_Id;
		static const float aRots[] = {pi, pi / -2.0f, 0.0f};
		m_Rotation = aRots[Dir];
	}

	void Update(CGameClient &This, const CNamePlateData &Data) override
	{
		if(!Data.m_ShowDirection) { m_ShiftOnInvis = false; m_Visible = false; return; }
		m_ShiftOnInvis = true;
		m_Size = vec2(Data.m_FontSizeDirection, Data.m_FontSizeDirection);
		m_Padding.y = m_Size.y / 2.0f;
		switch(m_Direction)
		{
		case DIRECTION_LEFT:  m_Visible = Data.m_DirLeft;  break;
		case DIRECTION_UP:    m_Visible = Data.m_DirJump;  break;
		case DIRECTION_RIGHT: m_Visible = Data.m_DirRight; break;
		}
		m_Color.a = Data.m_Color.a;
	}
};

class CNamePlatePartClientId : public CNamePlatePartText
{
	int m_ClientId = -1;
	static_assert(MAX_CLIENTS <= 999);
	char m_aText[5] = "";
	float m_FontSize = -INFINITY;
	bool m_ClientIdSeparateLine;

protected:
	bool UpdateNeeded(CGameClient &, const CNamePlateData &Data) override
	{
		m_Visible = Data.m_ShowClientId && (Data.m_ClientIdSeparateLine == m_ClientIdSeparateLine);
		if(!m_Visible) return false;
		m_Color = Data.m_Color;
		return m_FontSize != Data.m_FontSizeClientId || m_ClientId != Data.m_ClientId;
	}
	void UpdateText(CGameClient &This, const CNamePlateData &Data) override
	{
		m_FontSize = Data.m_FontSizeClientId;
		m_ClientId = Data.m_ClientId;
		str_format(m_aText, sizeof(m_aText), m_ClientIdSeparateLine ? "%d" : "%d:", m_ClientId);
		CTextCursor Cursor;
		Cursor.m_FontSize = m_FontSize;
		This.TextRender()->CreateOrAppendTextContainer(m_TextContainerIndex, &Cursor, m_aText);
	}

public:
	CNamePlatePartClientId(CGameClient &This, bool Sep) : CNamePlatePartText(This), m_ClientIdSeparateLine(Sep) {}
};

class CNamePlatePartFriendMark : public CNamePlatePartText
{
	float m_FontSize = -INFINITY;
protected:
	bool UpdateNeeded(CGameClient &, const CNamePlateData &Data) override
	{
		m_Visible = Data.m_ShowFriendMark;
		if(!m_Visible) return false;
		m_Color.a = Data.m_Color.a;
		return m_FontSize != Data.m_FontSize;
	}
	void UpdateText(CGameClient &This, const CNamePlateData &Data) override
	{
		m_FontSize = Data.m_FontSize;
		CTextCursor Cursor;
		This.TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		Cursor.m_FontSize = m_FontSize;
		This.TextRender()->CreateOrAppendTextContainer(m_TextContainerIndex, &Cursor, FontIcons::FONT_ICON_HEART);
		This.TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	}
public:
	CNamePlatePartFriendMark(CGameClient &This) : CNamePlatePartText(This) { m_Color = ColorRGBA(1.0f, 0.0f, 0.0f); }
};

class CNamePlatePartVerifiedBadge : public CNamePlatePartIcon
{
	bool m_WasVerified = false;
	char m_aLastName[64] = "";
public:
	CNamePlatePartVerifiedBadge(CGameClient &This) : CNamePlatePartIcon(This) { m_Padding = vec2(2.0f, 2.0f); }

	void Update(CGameClient &This, const CNamePlateData &Data) override
	{
		if(!This.m_BadgeManager.IsLoaded()) { m_Visible = false; return; }

		bool IsVerified = false;
		if(Data.m_InGame)
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(str_comp(This.m_aClients[i].m_aName, Data.m_aName) == 0)
				{
					IsVerified = (i == This.m_Snap.m_LocalClientId)
						? This.m_BadgeManager.IsLocalVerified()
						: This.m_BadgeManager.IsOnlinePlayerVerified(Data.m_aName);
					break;
				}
			}
		}

		bool NameChanged = str_comp(m_aLastName, Data.m_aName) != 0;
		if(IsVerified != m_WasVerified || NameChanged)
		{
			m_WasVerified = IsVerified;
			str_copy(m_aLastName, Data.m_aName);
		}

		if(!IsVerified) { m_Visible = false; return; }

		const CBadgeDef *pBadge = This.m_BadgeManager.GetVerifiedBadge();
		if(!pBadge || !pBadge->m_TextureLoaded) { m_Visible = false; return; }

		m_Texture = This.m_BadgeManager.GetCurrentBadgeTexture(pBadge);
		m_Size = vec2(Data.m_FontSize, Data.m_FontSize);
		m_Color.a = Data.m_Color.a;
		m_Visible = true;
	}
};

class CNamePlatePartName : public CNamePlatePartText
{
	char m_aText[std::max<size_t>(MAX_NAME_LENGTH, protocol7::MAX_NAME_ARRAY_SIZE)] = "";
	float m_FontSize = -INFINITY;
protected:
	bool UpdateNeeded(CGameClient &, const CNamePlateData &Data) override
	{
		m_Visible = Data.m_ShowName;
		if(!m_Visible) return false;
		m_Color = Data.m_Color;
		return m_FontSize != Data.m_FontSize || str_comp(m_aText, Data.m_aName) != 0;
	}
	void UpdateText(CGameClient &This, const CNamePlateData &Data) override
	{
		m_FontSize = Data.m_FontSize;
		str_copy(m_aText, Data.m_aName);
		CTextCursor Cursor;
		Cursor.m_FontSize = m_FontSize;
		This.TextRender()->CreateOrAppendTextContainer(m_TextContainerIndex, &Cursor, m_aText);
	}
public:
	CNamePlatePartName(CGameClient &This) : CNamePlatePartText(This) {}
};

class CNamePlatePartClan : public CNamePlatePartText
{
	char m_aText[std::max<size_t>(MAX_CLAN_LENGTH, protocol7::MAX_CLAN_ARRAY_SIZE)] = "";
	float m_FontSize = -INFINITY;
protected:
	bool UpdateNeeded(CGameClient &, const CNamePlateData &Data) override
	{
		m_Visible = Data.m_ShowClan;
		if(!m_Visible && Data.m_aClan[0] != '\0') return false;
		m_Color = Data.m_Color;
		return m_FontSize != Data.m_FontSizeClan || str_comp(m_aText, Data.m_aClan) != 0;
	}
	void UpdateText(CGameClient &This, const CNamePlateData &Data) override
	{
		m_FontSize = Data.m_FontSizeClan;
		str_copy(m_aText, Data.m_aClan);
		CTextCursor Cursor;
		Cursor.m_FontSize = m_FontSize;
		This.TextRender()->CreateOrAppendTextContainer(m_TextContainerIndex, &Cursor, m_aText);
	}
public:
	CNamePlatePartClan(CGameClient &This) : CNamePlatePartText(This) {}
};

static ColorRGBA StrongWeakColor(EHookStrongWeakState State)
{
	switch(State)
	{
	case EHookStrongWeakState::STRONG:  return color_cast<ColorRGBA>(ColorHSLA(6401973));
	case EHookStrongWeakState::WEAK:    return color_cast<ColorRGBA>(ColorHSLA(41131));
	default:                            return ColorRGBA(1.0f, 1.0f, 1.0f);
	}
}

class CNamePlatePartHookStrongWeak : public CNamePlatePartSprite
{
	void Update(CGameClient &, const CNamePlateData &Data) override
	{
		m_Visible = Data.m_ShowHookStrongWeak;
		if(!m_Visible) return;
		m_Size = vec2(Data.m_FontSizeHookStrongWeak + DEFAULT_PADDING, Data.m_FontSizeHookStrongWeak + DEFAULT_PADDING);
		switch(Data.m_HookStrongWeakState)
		{
		case EHookStrongWeakState::STRONG: m_Sprite = SPRITE_HOOK_STRONG; break;
		case EHookStrongWeakState::NEUTRAL: m_Sprite = SPRITE_HOOK_ICON;  break;
		case EHookStrongWeakState::WEAK:   m_Sprite = SPRITE_HOOK_WEAK;   break;
		}
		m_Color = StrongWeakColor(Data.m_HookStrongWeakState);
		m_Color.a = Data.m_Color.a;
	}
public:
	CNamePlatePartHookStrongWeak(CGameClient &This) : CNamePlatePartSprite(This)
	{
		m_Texture = g_pData->m_aImages[IMAGE_STRONGWEAK].m_Id;
		m_Padding = vec2(0.0f, 0.0f);
	}
};

class CNamePlatePartHookStrongWeakId : public CNamePlatePartText
{
	int m_StrongWeakId = -1;
	static_assert(MAX_CLIENTS <= 999);
	char m_aText[4] = "";
	float m_FontSize = -INFINITY;
protected:
	bool UpdateNeeded(CGameClient &, const CNamePlateData &Data) override
	{
		m_Visible = Data.m_ShowHookStrongWeakId;
		if(!m_Visible) return false;
		m_Color = StrongWeakColor(Data.m_HookStrongWeakState);
		m_Color.a = Data.m_Color.a;
		return m_FontSize != Data.m_FontSizeHookStrongWeak || m_StrongWeakId != Data.m_HookStrongWeakId;
	}
	void UpdateText(CGameClient &This, const CNamePlateData &Data) override
	{
		m_FontSize = Data.m_FontSizeHookStrongWeak;
		m_StrongWeakId = Data.m_HookStrongWeakId;
		str_format(m_aText, sizeof(m_aText), "%d", m_StrongWeakId);
		CTextCursor Cursor;
		Cursor.m_FontSize = m_FontSize;
		This.TextRender()->CreateOrAppendTextContainer(m_TextContainerIndex, &Cursor, m_aText);
	}
public:
	CNamePlatePartHookStrongWeakId(CGameClient &This) : CNamePlatePartText(This) {}
};

class CNamePlate
{
	bool m_Inited = false;
	PartsVector m_vpParts;

	void RenderLine(CGameClient &This, vec2 Pos, vec2 Size, PartsVector::iterator Start, PartsVector::iterator End)
	{
		Pos.x -= Size.x / 2.0f;
		for(auto It = Start; It != End; ++It)
		{
			const CNamePlatePart &Part = **It;
			if(Part.Visible())
				Part.Render(This, vec2(
					Pos.x + (Part.Padding().x + Part.Size().x) / 2.0f,
					Pos.y - std::max(Size.y, Part.Padding().y + Part.Size().y) / 2.0f));
			if(Part.Visible() || Part.ShiftOnInvis())
				Pos.x += Part.Size().x + Part.Padding().x;
		}
	}

	template<typename T, typename... A>
	void AddPart(CGameClient &This, A &&...Args)
	{
		m_vpParts.push_back(std::make_unique<T>(This, std::forward<A>(Args)...));
	}

	void Init(CGameClient &This)
	{
		if(m_Inited) return;
		m_Inited = true;
		AddPart<CNamePlatePartDirection>(This, DIRECTION_LEFT);
		AddPart<CNamePlatePartDirection>(This, DIRECTION_UP);
		AddPart<CNamePlatePartDirection>(This, DIRECTION_RIGHT);
		AddPart<CNamePlatePartNewLine>(This);
		AddPart<CNamePlatePartFriendMark>(This);
		AddPart<CNamePlatePartVerifiedBadge>(This);
		AddPart<CNamePlatePartClientId>(This, false);
		AddPart<CNamePlatePartName>(This);
		AddPart<CNamePlatePartNewLine>(This);
		AddPart<CNamePlatePartClan>(This);
		AddPart<CNamePlatePartNewLine>(This);
		AddPart<CNamePlatePartClientId>(This, true);
		AddPart<CNamePlatePartNewLine>(This);
		AddPart<CNamePlatePartHookStrongWeak>(This);
		AddPart<CNamePlatePartHookStrongWeakId>(This);
	}

	struct LineMetrics { vec2 Size; bool Empty; };

	LineMetrics AccumulateLine(PartsVector::iterator Start, PartsVector::iterator End) const
	{
		vec2 S(0, 0);
		bool Empty = true;
		for(auto It = Start; It != End; ++It)
		{
			const CNamePlatePart &P = **It;
			if(!P.NewLine() && (P.Visible() || P.ShiftOnInvis()))
			{
				Empty = false;
				S.x += P.Size().x + P.Padding().x;
				S.y = std::max(S.y, P.Size().y + P.Padding().y);
			}
		}
		return {S, Empty};
	}

public:
	CNamePlate() = default;
	CNamePlate(CGameClient &This, const CNamePlateData &Data) { Update(This, Data); }

	void Reset(CGameClient &This)
	{
		for(auto &P : m_vpParts) P->Reset(This);
	}

	void Update(CGameClient &This, const CNamePlateData &Data)
	{
		Init(This);
		for(auto &P : m_vpParts) P->Update(This, Data);
	}

	void Render(CGameClient &This, const vec2 &BottomMid)
	{
		dbg_assert(m_Inited, "Tried to render uninited nameplate");
		vec2 Pos = BottomMid;
		vec2 LineSize(0, 0);
		bool Empty = true;
		auto Start = m_vpParts.begin();

		for(auto It = m_vpParts.begin(); It != m_vpParts.end(); ++It)
		{
			CNamePlatePart &P = **It;
			if(P.NewLine())
			{
				if(!Empty)
				{
					RenderLine(This, Pos, LineSize, Start, std::next(It));
					Pos.y -= LineSize.y;
				}
				Start = std::next(It);
				LineSize = vec2(0, 0);
				Empty = true;
			}
			else if(P.Visible() || P.ShiftOnInvis())
			{
				Empty = false;
				LineSize.x += P.Size().x + P.Padding().x;
				LineSize.y = std::max(LineSize.y, P.Size().y + P.Padding().y);
			}
		}
		RenderLine(This, Pos, LineSize, Start, m_vpParts.end());
		This.Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	vec2 Size() const
	{
		dbg_assert(m_Inited, "Tried to get size of uninited nameplate");
		vec2 LineSize(0, 0);
		float WMax = 0, HTotal = 0;
		bool Empty = true;

		for(const auto &pP : m_vpParts)
		{
			const CNamePlatePart &P = *pP;
			if(P.NewLine())
			{
				if(!Empty) { WMax = std::max(WMax, LineSize.x); HTotal += LineSize.y; }
				LineSize = vec2(0, 0);
				Empty = true;
			}
			else if(P.Visible() || P.ShiftOnInvis())
			{
				Empty = false;
				LineSize.x += P.Size().x + P.Padding().x;
				LineSize.y = std::max(LineSize.y, P.Size().y + P.Padding().y);
			}
		}
		WMax = std::max(WMax, LineSize.x);
		HTotal += LineSize.y;
		return vec2(WMax, HTotal);
	}
};

class CNamePlates::CNamePlatesData
{
public:
	CNamePlate m_aNamePlates[MAX_CLIENTS];
};

void CNamePlates::RenderNamePlateGame(vec2 Position, const CNetObj_PlayerInfo *pInfo, float Alpha)
{
	float X0, Y0, X1, Y1;
	Graphics()->GetScreen(&X0, &Y0, &X1, &Y1);
	if(!in_range(Position.x, X0 - 400.0f, X1 + 400.0f) || !in_range(Position.y, Y0, Y1 + 800.0f))
		return;

	auto *pGC = GameClient();
	int Dummy = g_Config.m_ClDummy;
	int ClientId = pInfo->m_ClientId;
	const auto &CD = pGC->m_aClients[ClientId];
	bool OtherTeam = pGC->IsOtherTeam(ClientId);

	CNamePlateData Data;
	Data.m_InGame = true;

	Data.m_ShowName = pInfo->m_Local ? g_Config.m_ClNamePlatesOwn : g_Config.m_ClNamePlates;
	str_copy(Data.m_aName, CD.m_aName);
	Data.m_ShowFriendMark = Data.m_ShowName && g_Config.m_ClNamePlatesFriendMark && CD.m_Friend;
	Data.m_ShowClientId = Data.m_ShowName && (g_Config.m_Debug || g_Config.m_ClNamePlatesIds);
	Data.m_FontSize = 18.0f + 20.0f * g_Config.m_ClNamePlatesSize / 100.0f;
	Data.m_ClientId = ClientId;
	Data.m_ClientIdSeparateLine = g_Config.m_ClNamePlatesIdsSeparateLine;
	Data.m_FontSizeClientId = Data.m_ClientIdSeparateLine ? 18.0f + 20.0f * g_Config.m_ClNamePlatesIdsSize / 100.0f : Data.m_FontSize;
	Data.m_ShowClan = Data.m_ShowName && g_Config.m_ClNamePlatesClan;
	str_copy(Data.m_aClan, CD.m_aClan);
	Data.m_FontSizeClan = 18.0f + 20.0f * g_Config.m_ClNamePlatesClanSize / 100.0f;
	Data.m_FontSizeHookStrongWeak = 18.0f + 20.0f * g_Config.m_ClNamePlatesStrongSize / 100.0f;
	Data.m_FontSizeDirection = 18.0f + 20.0f * g_Config.m_ClDirectionSize / 100.0f;

	if(!g_Config.m_ClNamePlatesAlways)
		Alpha *= std::clamp(1.0f - std::pow(distance(pGC->m_Controls.m_aTargetPos[Dummy], Position) / 200.0f, 16.0f), 0.0f, 1.0f);
	if(OtherTeam)
		Alpha *= g_Config.m_ClShowOthersAlpha / 100.0f;

	Data.m_Color = ColorRGBA(1.0f, 1.0f, 1.0f);
	if(pGC->m_CmdsLikeWar.IsWarPlayer(ClientId))
		Data.m_Color = ColorRGBA(1.0f, 0.2f, 0.2f);
	else if(pGC->m_CmdsLikeWar.IsTeamPlayer(ClientId))
		Data.m_Color = ColorRGBA(0.2f, 1.0f, 0.2f);
	else if(g_Config.m_ClNamePlatesTeamcolors)
	{
		if(pGC->IsTeamPlay())
		{
			if(CD.m_Team == TEAM_RED) Data.m_Color = ColorRGBA(1.0f, 0.5f, 0.5f);
			else if(CD.m_Team == TEAM_BLUE) Data.m_Color = ColorRGBA(0.7f, 0.7f, 1.0f);
		}
		else
		{
			int Team = pGC->m_Teams.Team(ClientId);
			if(Team) Data.m_Color = pGC->GetDDTeamColor(Team, 0.75f);
		}
	}
	Data.m_Color.a = Alpha;

	int ShowDirCfg = g_Config.m_ClShowDirection;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current()) ShowDirCfg = g_Config.m_ClVideoShowDirection;
#endif
	Data.m_DirLeft = Data.m_DirJump = Data.m_DirRight = false;
	switch(ShowDirCfg)
	{
	case 0: Data.m_ShowDirection = false; break;
	case 1: Data.m_ShowDirection = !pInfo->m_Local; break;
	case 2: Data.m_ShowDirection = true; break;
	case 3: Data.m_ShowDirection = pInfo->m_Local; break;
	default: dbg_assert_failed("ShowDirectionConfig invalid");
	}

	if(Data.m_ShowDirection)
	{
		const bool IsOtherDummy = Client()->State() != IClient::STATE_DEMOPLAYBACK && ClientId == pGC->m_aLocalIds[!Dummy];
		const bool IsLocalNotDemo = Client()->State() != IClient::STATE_DEMOPLAYBACK && pInfo->m_Local;

		if(IsOtherDummy)
		{
			const auto &In = pGC->m_Controls.m_aInputData[!Dummy];
			Data.m_DirLeft = In.m_Direction == -1;
			Data.m_DirJump = In.m_Jump == 1;
			Data.m_DirRight = In.m_Direction == 1;
		}
		else if(IsLocalNotDemo)
		{
			const auto &In = pGC->m_Controls.m_aInputData[Dummy];
			Data.m_DirLeft = In.m_Direction == -1;
			Data.m_DirJump = In.m_Jump == 1;
			Data.m_DirRight = In.m_Direction == 1;
		}
		else
		{
			const auto &Char = pGC->m_Snap.m_aCharacters[ClientId];
			Data.m_DirLeft = Char.m_Cur.m_Direction == -1;
			Data.m_DirJump = Char.m_Cur.m_Jumped & 1;
			Data.m_DirRight = Char.m_Cur.m_Direction == 1;
		}
	}

	Data.m_ShowHookStrongWeak = false;
	Data.m_HookStrongWeakState = EHookStrongWeakState::NEUTRAL;
	Data.m_ShowHookStrongWeakId = false;
	Data.m_HookStrongWeakId = 0;

	const bool Following = pGC->m_Snap.m_SpecInfo.m_Active && !pGC->m_MultiViewActivated && pGC->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW;
	if(pGC->m_Snap.m_LocalClientId != -1 || Following)
	{
		int SelId = Following ? pGC->m_Snap.m_SpecInfo.m_SpectatorId : pGC->m_Snap.m_LocalClientId;
		const auto &Sel = pGC->m_Snap.m_aCharacters[SelId];
		const auto &Other = pGC->m_Snap.m_aCharacters[ClientId];

		if((Sel.m_HasExtendedData || pGC->m_aClients[SelId].m_SpecCharPresent) && Other.m_HasExtendedData)
		{
			int SelSWId = Sel.m_HasExtendedData ? Sel.m_ExtendedData.m_StrongWeakId : 0;
			Data.m_HookStrongWeakId = Other.m_ExtendedData.m_StrongWeakId;
			Data.m_ShowHookStrongWeakId = g_Config.m_Debug || g_Config.m_ClNamePlatesStrong == 2;
			if(SelId == ClientId)
				Data.m_ShowHookStrongWeak = Data.m_ShowHookStrongWeakId;
			else
			{
				Data.m_HookStrongWeakState = SelSWId > Other.m_ExtendedData.m_StrongWeakId ? EHookStrongWeakState::STRONG : EHookStrongWeakState::WEAK;
				Data.m_ShowHookStrongWeak = g_Config.m_Debug || g_Config.m_ClNamePlatesStrong > 0;
			}
		}
	}

	CNamePlate &NP = m_pData->m_aNamePlates[ClientId];
	NP.Update(*pGC, Data);
	NP.Render(*pGC, Position - vec2(0.0f, (float)g_Config.m_ClNamePlatesOffset));
}

void CNamePlates::RenderNamePlatePreview(vec2 Position, int Dummy)
{
	auto *pGC = GameClient();
	float FS = 18.0f + 20.0f * g_Config.m_ClNamePlatesSize / 100.0f;
	float FSClan = 18.0f + 20.0f * g_Config.m_ClNamePlatesClanSize / 100.0f;
	float FSDir = 18.0f + 20.0f * g_Config.m_ClDirectionSize / 100.0f;
	float FSHSW = 18.0f + 20.0f * g_Config.m_ClNamePlatesStrongSize / 100.0f;

	CNamePlateData Data;
	Data.m_InGame = false;
	Data.m_Color = g_Config.m_ClNamePlatesTeamcolors ? pGC->GetDDTeamColor(13, 0.75f) : TextRender()->DefaultTextColor();
	Data.m_Color.a = 1.0f;

	Data.m_ShowName = g_Config.m_ClNamePlates || g_Config.m_ClNamePlatesOwn;
	const char *pName = Dummy == 0 ? Client()->PlayerName() : Client()->DummyName();
	str_copy(Data.m_aName, str_utf8_skip_whitespaces(pName));
	str_utf8_trim_right(Data.m_aName);
	Data.m_FontSize = FS;
	Data.m_ShowFriendMark = Data.m_ShowName && g_Config.m_ClNamePlatesFriendMark;
	Data.m_ShowClientId = Data.m_ShowName && (g_Config.m_Debug || g_Config.m_ClNamePlatesIds);
	Data.m_ClientId = Dummy;
	Data.m_ClientIdSeparateLine = g_Config.m_ClNamePlatesIdsSeparateLine;
	Data.m_FontSizeClientId = Data.m_ClientIdSeparateLine ? 18.0f + 20.0f * g_Config.m_ClNamePlatesIdsSize / 100.0f : FS;
	Data.m_ShowClan = Data.m_ShowName && g_Config.m_ClNamePlatesClan;

	const char *pClan = Dummy == 0 ? g_Config.m_PlayerClan : g_Config.m_ClDummyClan;
	str_copy(Data.m_aClan, str_utf8_skip_whitespaces(pClan));
	str_utf8_trim_right(Data.m_aClan);
	if(Data.m_aClan[0] == '\0')
		str_copy(Data.m_aClan, "Clan Name");
	Data.m_FontSizeClan = FSClan;
	Data.m_ShowDirection = g_Config.m_ClShowDirection != 0;
	Data.m_DirLeft = Data.m_DirJump = Data.m_DirRight = true;
	Data.m_FontSizeDirection = FSDir;
	Data.m_FontSizeHookStrongWeak = FSHSW;
	Data.m_HookStrongWeakId = Data.m_ClientId;
	Data.m_ShowHookStrongWeakId = g_Config.m_ClNamePlatesStrong == 2;

	if(Dummy == g_Config.m_ClDummy)
	{
		Data.m_HookStrongWeakState = EHookStrongWeakState::NEUTRAL;
		Data.m_ShowHookStrongWeak = Data.m_ShowHookStrongWeakId;
	}
	else
	{
		Data.m_HookStrongWeakState = Data.m_HookStrongWeakId == 2 ? EHookStrongWeakState::STRONG : EHookStrongWeakState::WEAK;
		Data.m_ShowHookStrongWeak = g_Config.m_ClNamePlatesStrong > 0;
	}

	CTeeRenderInfo TeeInfo;
	if(Dummy == 0)
	{
		TeeInfo.Apply(pGC->m_Skins.Find(g_Config.m_ClPlayerSkin));
		TeeInfo.ApplyColors(g_Config.m_ClPlayerUseCustomColor, g_Config.m_ClPlayerColorBody, g_Config.m_ClPlayerColorFeet);
	}
	else
	{
		TeeInfo.Apply(pGC->m_Skins.Find(g_Config.m_ClDummySkin));
		TeeInfo.ApplyColors(g_Config.m_ClDummyUseCustomColor, g_Config.m_ClDummyColorBody, g_Config.m_ClDummyColorFeet);
	}
	TeeInfo.m_Size = 64.0f;

	CNamePlate NP(*pGC, Data);
	Position.y += NP.Size().y / 2.0f + (float)g_Config.m_ClNamePlatesOffset / 2.0f;

	const vec2 Delta = Ui()->MousePos() - Position;
	const float Dist = length(Delta);
	const float InteractDist = 20.0f;
	const vec2 Dir = Dist < InteractDist ? normalize(vec2(Delta.x, maximum(Delta.y, 0.5f))) : normalize(Delta);
	int Emote = Dist < InteractDist ? EMOTE_HAPPY : (Dummy ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes);

	RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, Emote, Dir, Position);
	Position.y -= (float)g_Config.m_ClNamePlatesOffset;
	NP.Render(*pGC, Position);
	NP.Reset(*pGC);
}

void CNamePlates::ResetNamePlates()
{
	for(auto &NP : m_pData->m_aNamePlates)
		NP.Reset(*GameClient());
}

void CNamePlates::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	int ShowDir = g_Config.m_ClShowDirection;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current()) ShowDir = g_Config.m_ClVideoShowDirection;
#endif
	if(!g_Config.m_ClNamePlates && ShowDir == 0)
		return;

	auto *pGC = GameClient();
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CNetObj_PlayerInfo *pInfo = pGC->m_Snap.m_apPlayerInfos[i];
		if(!pInfo) continue;

		if(pGC->m_aClients[i].m_SpecCharPresent)
			RenderNamePlateGame(pGC->m_aClients[i].m_SpecChar, pInfo, 0.4f);
		if(pGC->m_Snap.m_aCharacters[i].m_Active)
			RenderNamePlateGame(pGC->m_aClients[i].m_RenderPos, pInfo, 1.0f);
	}
}

void CNamePlates::OnWindowResize() { ResetNamePlates(); }

CNamePlates::CNamePlates() : m_pData(new CNamePlates::CNamePlatesData()) {}
CNamePlates::~CNamePlates() { delete m_pData; }