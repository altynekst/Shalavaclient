#include "damageind.h"

#include <base/color.h>
#include <engine/demo.h>
#include <engine/graphics.h>
#include <generated/client_data.h>
#include <generated/protocol.h>
#include <game/client/gameclient.h>

CDamageInd::CDamageInd() : m_NumItems(0) {}

void CDamageInd::Create(vec2 Pos, vec2 Dir, float Alpha)
{
	if(m_NumItems >= MAX_ITEMS) return;
	CItem &I = m_aItems[m_NumItems++];
	I.m_Pos = Pos;
	I.m_Dir = -Dir;
	I.m_RemainingLife = 0.75f;
	I.m_StartAngle = -random_angle();
	I.m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, Alpha);
}

void CDamageInd::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	static float s_LastLocalTime = LocalTime();
	float Adj = 0.0f;
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const auto *pInfo = DemoPlayer()->BaseInfo();
		if(!pInfo->m_Paused) Adj = (LocalTime() - s_LastLocalTime) * pInfo->m_Speed;
	}
	else
	{
		const auto *pGI = GameClient()->m_Snap.m_pGameInfoObj;
		if(!pGI || !(pGI->m_GameStateFlags & GAMESTATEFLAG_PAUSED))
			Adj = LocalTime() - s_LastLocalTime;
	}
	s_LastLocalTime = LocalTime();

	Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteStars[0]);
	for(int i = 0; i < m_NumItems;)
	{
		CItem &It = m_aItems[i];
		It.m_RemainingLife -= Adj;
		if(It.m_RemainingLife < 0.0f)
		{
			m_aItems[i] = m_aItems[--m_NumItems];
		}
		else
		{
			float T = std::clamp((It.m_RemainingLife - 0.60f) / 0.15f, 0.0f, 1.0f);
			vec2 Pos = mix(It.m_Pos + It.m_Dir * 75.0f, It.m_Pos, T);
			Graphics()->SetColor(It.m_Color.WithMultipliedAlpha(It.m_RemainingLife / 0.1f));
			Graphics()->QuadsSetRotation(It.m_StartAngle + It.m_RemainingLife * 2.0f);
			Graphics()->RenderQuadContainerAsSprite(m_DmgIndQuadContainerIndex, 0, Pos.x, Pos.y);
			i++;
		}
	}

	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
}

void CDamageInd::OnInit()
{
	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	m_DmgIndQuadContainerIndex = Graphics()->CreateQuadContainer(false);
	float ScaleX, ScaleY;
	Graphics()->GetSpriteScale(SPRITE_STAR1, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_DmgIndQuadContainerIndex, 48.f * ScaleX, 48.f * ScaleY);
	Graphics()->QuadContainerUpload(m_DmgIndQuadContainerIndex);
}

void CDamageInd::OnReset() { m_NumItems = 0; }