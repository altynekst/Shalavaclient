#include "freezebars.h"
#include <game/client/gameclient.h>

void CFreezeBars::RenderFreezeBar(const int ClientId)
{
	static constexpr float W = 64.0f, HW = 32.0f, H = 16.0f;
	auto *pCC = &GameClient()->m_aClients[ClientId].m_Predicted;

	if(pCC->m_FreezeEnd <= 0 || pCC->m_FreezeStart == 0 || pCC->m_FreezeEnd <= pCC->m_FreezeStart ||
		!GameClient()->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo ||
		(pCC->m_IsInFreeze && g_Config.m_ClFreezeBarsAlphaInsideFreeze == 0))
		return;

	int Max = pCC->m_FreezeEnd - pCC->m_FreezeStart;
	float Prog = std::clamp(Max - (Client()->GameTick(g_Config.m_ClDummy) - pCC->m_FreezeStart), 0, Max) / (float)Max;
	if(Prog <= 0.0f) return;

	vec2 Pos = GameClient()->m_aClients[ClientId].m_RenderPos;
	Pos.x -= HW;
	Pos.y += 32;

	float Alpha = GameClient()->IsOtherTeam(ClientId) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;
	if(pCC->m_IsInFreeze) Alpha *= g_Config.m_ClFreezeBarsAlphaInsideFreeze / 100.0f;

	RenderFreezeBarPos(Pos.x, Pos.y, W, H, Prog, Alpha);
}

void CFreezeBars::RenderFreezeBarPos(float x, const float y, const float Width, const float Height, float Progress, const float Alpha)
{
	Progress = std::clamp(Progress, 0.0f, 1.0f);
	const float RestPct = 0.5f, ProgPct = 0.5f;
	const float EndW = Height;
	const float MiddleW = Width - EndW * 2.0f;
	const float EndProgW = EndW * ProgPct;
	const float EndRestW = EndW * RestPct;
	const float ProgBarW = Width - EndProgW * 2.0f;
	const float EndProgProp = EndProgW / ProgBarW;
	const float MidProgProp = MiddleW / ProgBarW;

	float BeginProg = Progress <= EndProgProp ? Progress / EndProgProp : 1.0f;
	auto SetCA = [&]() { Graphics()->SetColor(1.f, 1.f, 1.f, Alpha); };
	auto &HS = GameClient()->m_HudSkin;

	Graphics()->WrapClamp();

	Graphics()->TextureSet(HS.m_SpriteHudFreezeBarFullLeft);
	Graphics()->QuadsBegin(); SetCA();
	Graphics()->QuadsSetSubsetFree(0,0, RestPct + ProgPct*BeginProg,0, RestPct + ProgPct*BeginProg,1, 0,1);
	IGraphics::CQuadItem QFB(x, y, EndRestW + EndProgW*BeginProg, Height);
	Graphics()->QuadsDrawTL(&QFB, 1); Graphics()->QuadsEnd();

	if(BeginProg < 1.0f)
	{
		Graphics()->TextureSet(HS.m_SpriteHudFreezeBarEmptyRight);
		Graphics()->QuadsBegin(); SetCA();
		Graphics()->QuadsSetSubsetFree(ProgPct - ProgPct*BeginProg,0, 0,0, 0,1, ProgPct - ProgPct*BeginProg,1);
		IGraphics::CQuadItem QEB(x + EndRestW + EndProgW*BeginProg, y, EndProgW*(1-BeginProg), Height);
		Graphics()->QuadsDrawTL(&QEB, 1); Graphics()->QuadsEnd();
	}

	x += EndW;
	float MidProg = 1.0f;
	if(Progress <= EndProgProp + MidProgProp)
		MidProg = Progress <= EndProgProp ? 0.0f : (Progress - EndProgProp) / MidProgProp;

	float FullMid = MiddleW * MidProg, EmptyMid = MiddleW - FullMid;

	Graphics()->TextureSet(HS.m_SpriteHudFreezeBarFull);
	Graphics()->QuadsBegin(); SetCA();
	if(FullMid <= EndW)
		Graphics()->QuadsSetSubsetFree(0,0, FullMid/EndW,0, FullMid/EndW,1, 0,1);
	else
		Graphics()->QuadsSetSubsetFree(0,0,1,0,1,1,0,1);
	IGraphics::CQuadItem QFm(x, y, FullMid, Height);
	Graphics()->QuadsDrawTL(&QFm, 1); Graphics()->QuadsEnd();

	Graphics()->TextureSet(HS.m_SpriteHudFreezeBarEmpty);
	Graphics()->QuadsBegin(); SetCA();
	if(EmptyMid <= EndW)
		Graphics()->QuadsSetSubsetFree(EmptyMid/EndW,0, 0,0, 0,1, EmptyMid/EndW,1);
	else
		Graphics()->QuadsSetSubsetFree(1,0,0,0,0,1,1,1);
	IGraphics::CQuadItem QEm(x + FullMid, y, EmptyMid, Height);
	Graphics()->QuadsDrawTL(&QEm, 1); Graphics()->QuadsEnd();

	x += MiddleW;
	float EndProg = 1.0f;
	if(Progress <= 1.0f)
	{
		if(Progress <= EndProgProp + MidProgProp) EndProg = 0.0f;
		else EndProg = (Progress - EndProgProp - MidProgProp) / EndProgProp;
	}

	if(EndProg > 0.0f)
	{
		Graphics()->TextureSet(HS.m_SpriteHudFreezeBarFullLeft);
		Graphics()->QuadsBegin(); SetCA();
		Graphics()->QuadsSetSubsetFree(1,0, 1-ProgPct*EndProg,0, 1-ProgPct*EndProg,1, 1,1);
		IGraphics::CQuadItem QFE(x, y, EndProgW*EndProg, Height);
		Graphics()->QuadsDrawTL(&QFE, 1); Graphics()->QuadsEnd();
	}

	Graphics()->TextureSet(HS.m_SpriteHudFreezeBarEmptyRight);
	Graphics()->QuadsBegin(); SetCA();
	Graphics()->QuadsSetSubsetFree(ProgPct - ProgPct*(1-EndProg),0, 1,0, 1,1, ProgPct - ProgPct*(1-EndProg),1);
	IGraphics::CQuadItem QEE(x + EndProgW*EndProg, y, EndProgW*(1-EndProg) + EndRestW, Height);
	Graphics()->QuadsDrawTL(&QEE, 1); Graphics()->QuadsEnd();

	Graphics()->QuadsSetSubset(0,0,1,1);
	Graphics()->SetColor(1.f,1.f,1.f,1.f);
	Graphics()->WrapNormal();
}

inline bool CFreezeBars::IsPlayerInfoAvailable(int ClientId) const
{
	const auto &Snap = GameClient()->m_Snap;
	return Snap.m_aCharacters[ClientId].m_Active && Snap.m_apPrevPlayerInfos[ClientId] && Snap.m_apPlayerInfos[ClientId];
}

void CFreezeBars::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK) return;
	if(!g_Config.m_ClShowFreezeBars) return;

	float X0, Y0, X1, Y1;
	Graphics()->GetScreen(&X0, &Y0, &X1, &Y1);
	constexpr float Border = 120.0f;
	X0 -= Border; X1 += Border; Y0 -= Border; Y1 += Border;

	int LocalId = GameClient()->m_Snap.m_LocalClientId;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == LocalId || !IsPlayerInfoAvailable(i)) continue;
		const vec2 &P = GameClient()->m_aClients[i].m_RenderPos;
		if(P.x < X0 || P.x > X1 || P.y < Y0 || P.y > Y1) continue;
		RenderFreezeBar(i);
	}
	if(LocalId != -1 && IsPlayerInfoAvailable(LocalId))
		RenderFreezeBar(LocalId);

	GameClient()->m_Controls.RenderTasFreezeBars();
}