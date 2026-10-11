#include "particles.h"

#include <base/math.h>

#include <engine/demo.h>
#include <engine/graphics.h>

#include <generated/client_data.h>

#include <game/client/gameclient.h>

CParticles::CParticles()
{
	OnReset();
	m_RenderTrail.m_pParts = this;
	m_RenderTrailExtra.m_pParts = this;
	m_RenderExplosions.m_pParts = this;
	m_RenderExtra.m_pParts = this;
	m_RenderGeneral.m_pParts = this;
}

void CParticles::OnReset()
{
	for(int i = 0; i < MAX_PARTICLES; i++)
	{
		m_aParticles[i].m_PrevPart = i - 1;
		m_aParticles[i].m_NextPart = i + 1;
	}
	m_aParticles[0].m_PrevPart = 0;
	m_aParticles[MAX_PARTICLES - 1].m_NextPart = -1;
	m_FirstFree = 0;

	for(int &First : m_aFirstPart)
		First = -1;
}

void CParticles::Add(int Group, CParticle *pPart, float TimePassed)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		if(DemoPlayer()->BaseInfo()->m_Paused)
			return;
	}
	else
	{
		if(GameClient()->m_Snap.m_pGameInfoObj && GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED)
			return;
	}

	if(m_FirstFree == -1)
		return;

	int Id = m_FirstFree;
	m_FirstFree = m_aParticles[Id].m_NextPart;
	if(m_FirstFree != -1)
		m_aParticles[m_FirstFree].m_PrevPart = -1;

	m_aParticles[Id] = *pPart;
	m_aParticles[Id].m_PrevPart = -1;
	m_aParticles[Id].m_NextPart = m_aFirstPart[Group];
	if(m_aFirstPart[Group] != -1)
		m_aParticles[m_aFirstPart[Group]].m_PrevPart = Id;
	m_aFirstPart[Group] = Id;
	m_aParticles[Id].m_Life = TimePassed;
}

void CParticles::Update(float TimePassed)
{
	if(TimePassed <= 0.0f)
		return;

	const float InvTimePassed = 1.0f / TimePassed;

	m_FrictionFraction += TimePassed;
	if(m_FrictionFraction > 2.0f)
		m_FrictionFraction = 0;

	int FrictionCount = 0;
	while(m_FrictionFraction > 0.05f)
	{
		FrictionCount++;
		m_FrictionFraction -= 0.05f;
	}

	float LastFriction = -1.0f;
	float PrecomputedFriction = 1.0f;

	for(int &FirstPart : m_aFirstPart)
	{
		if(FirstPart == -1)
			continue;

		int i = FirstPart;
		while(i != -1)
		{
			int Next = m_aParticles[i].m_NextPart;
			CParticle &P = m_aParticles[i];

			P.m_Vel.y += P.m_Gravity * TimePassed;

			if(FrictionCount > 0 && P.m_Friction != LastFriction)
			{
				LastFriction = P.m_Friction;
				PrecomputedFriction = std::pow(P.m_Friction, FrictionCount);
			}
			if(FrictionCount > 0)
				P.m_Vel *= PrecomputedFriction;

			vec2 Vel = P.m_Vel * TimePassed;
			if(P.m_Collides)
			{
				Collision()->MovePoint(&P.m_Pos, &Vel, random_float(0.1f, 1.0f), nullptr);
				P.m_Vel = Vel * InvTimePassed;
			}
			else
				P.m_Pos += Vel;

			P.m_Life += TimePassed;
			P.m_Rot += TimePassed * P.m_Rotspeed;

			if(P.m_Life > P.m_LifeSpan)
			{
				if(P.m_PrevPart != -1)
					m_aParticles[P.m_PrevPart].m_NextPart = P.m_NextPart;
				else
					FirstPart = P.m_NextPart;

				if(P.m_NextPart != -1)
					m_aParticles[P.m_NextPart].m_PrevPart = P.m_PrevPart;

				if(m_FirstFree != -1)
					m_aParticles[m_FirstFree].m_PrevPart = i;
				P.m_PrevPart = -1;
				P.m_NextPart = m_FirstFree;
				m_FirstFree = i;
			}

			i = Next;
		}
	}
}

void CParticles::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	set_new_tick();
	int64_t t = time();

	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const auto *pInfo = DemoPlayer()->BaseInfo();
		if(!pInfo->m_Paused)
			Update((float)((t - m_LastRenderTime) / (double)time_freq()) * pInfo->m_Speed);
	}
	else
	{
		if(GameClient()->m_Snap.m_pGameInfoObj && !(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED))
			Update((float)((t - m_LastRenderTime) / (double)time_freq()));
	}

	m_LastRenderTime = t;
}

void CParticles::OnInit()
{
	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	m_ParticleQuadContainerIndex = Graphics()->CreateQuadContainer(false);
	for(int i = 0; i <= (SPRITE_PART9 - SPRITE_PART_SLICE); ++i)
	{
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Graphics()->QuadContainerAddSprite(m_ParticleQuadContainerIndex, 1.f);
	}
	Graphics()->QuadContainerUpload(m_ParticleQuadContainerIndex);

	m_ExtraParticleQuadContainerIndex = Graphics()->CreateQuadContainer(false);
	for(int i = 0; i <= (SPRITE_PART_SPARKLE - SPRITE_PART_SNOWFLAKE); ++i)
	{
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Graphics()->QuadContainerAddSprite(m_ExtraParticleQuadContainerIndex, 1.f);
	}
	Graphics()->QuadContainerUpload(m_ExtraParticleQuadContainerIndex);
}

bool CParticles::ParticleIsVisibleOnScreen(const vec2 &CurPos, float CurSize)
{
	static constexpr float s_SqrtOf2Half = 0.70710678118f;
	float SizeHalf = s_SqrtOf2Half * CurSize;
	return CurPos.x + SizeHalf >= m_ScreenX0 && CurPos.x - SizeHalf <= m_ScreenX1 &&
		CurPos.y + SizeHalf >= m_ScreenY0 && CurPos.y - SizeHalf <= m_ScreenY1;
}

void CParticles::RenderGroup(int Group)
{
	Graphics()->GetScreen(&m_ScreenX0, &m_ScreenY0, &m_ScreenX1, &m_ScreenY1);

	bool IsExtra = (Group == GROUP_EXTRA || Group == GROUP_TRAIL_EXTRA);
	IGraphics::CTextureHandle *aParticles = IsExtra ? GameClient()->m_ExtrasSkin.m_aSpriteParticles : GameClient()->m_ParticlesSkin.m_aSpriteParticles;
	int FirstOffset = IsExtra ? SPRITE_PART_SNOWFLAKE : SPRITE_PART_SLICE;
	int QuadContainer = IsExtra ? m_ExtraParticleQuadContainerIndex : m_ParticleQuadContainerIndex;

	int i = m_aFirstPart[Group];
	if(i == -1)
		return;

	if(Graphics()->IsQuadContainerBufferingEnabled())
	{
		thread_local static IGraphics::SRenderSpriteInfo s_aInfo[MAX_PARTICLES];
		int Count = 0;

		ColorRGBA LastColor;
		int LastQuad = 0;

		{
			const CParticle &P = m_aParticles[i];
			float Alpha = P.m_Color.a;
			if(P.m_UseAlphaFading)
				Alpha = mix(P.m_StartAlpha, P.m_EndAlpha, P.m_Life / P.m_LifeSpan);
			LastColor = ColorRGBA(P.m_Color.r, P.m_Color.g, P.m_Color.b, Alpha);
			Graphics()->SetColor(LastColor);
			LastQuad = P.m_Spr;
		}

		while(i != -1)
		{
			const CParticle &P = m_aParticles[i];
			int Quad = P.m_Spr;
			float a = P.m_Life / P.m_LifeSpan;
			float Size = mix(P.m_StartSize, P.m_EndSize, a);
			float Alpha = P.m_Color.a;
			if(P.m_UseAlphaFading)
				Alpha = mix(P.m_StartAlpha, P.m_EndAlpha, a);

			if(ParticleIsVisibleOnScreen(P.m_Pos, Size))
			{
				bool Flush = Count == (int)gs_GraphicsMaxParticlesRenderCount ||
					LastQuad != Quad ||
					LastColor.r != P.m_Color.r || LastColor.g != P.m_Color.g ||
					LastColor.b != P.m_Color.b || LastColor.a != Alpha;

				if(Flush)
				{
					Graphics()->TextureSet(aParticles[LastQuad - FirstOffset]);
					Graphics()->RenderQuadContainerAsSpriteMultiple(QuadContainer, LastQuad - FirstOffset, Count, s_aInfo);
					Count = 0;
					LastQuad = Quad;
					LastColor = ColorRGBA(P.m_Color.r, P.m_Color.g, P.m_Color.b, Alpha);
					Graphics()->SetColor(LastColor);
				}

				auto &Info = s_aInfo[Count];
				Info.m_Pos[0] = P.m_Pos.x;
				Info.m_Pos[1] = P.m_Pos.y;
				Info.m_Scale = Size;
				Info.m_Rotation = P.m_Rot;
				++Count;
			}

			i = P.m_NextPart;
		}

		Graphics()->TextureSet(aParticles[LastQuad - FirstOffset]);
		Graphics()->RenderQuadContainerAsSpriteMultiple(QuadContainer, LastQuad - FirstOffset, Count, s_aInfo);
	}
	else
	{
		Graphics()->BlendNormal();
		Graphics()->WrapClamp();

		int LastSpr = -1;

		while(i != -1)
		{
			const CParticle &P = m_aParticles[i];
			float a = P.m_Life / P.m_LifeSpan;
			float Size = mix(P.m_StartSize, P.m_EndSize, a);
			float Alpha = P.m_Color.a;
			if(P.m_UseAlphaFading)
				Alpha = mix(P.m_StartAlpha, P.m_EndAlpha, a);

			if(ParticleIsVisibleOnScreen(P.m_Pos, Size))
			{
				int Spr = P.m_Spr - FirstOffset;
				if(Spr != LastSpr)
				{
					if(LastSpr != -1)
						Graphics()->QuadsEnd();
					Graphics()->TextureSet(aParticles[Spr]);
					Graphics()->QuadsBegin();
					LastSpr = Spr;
				}

				Graphics()->QuadsSetRotation(P.m_Rot);
				Graphics()->SetColor(P.m_Color.r, P.m_Color.g, P.m_Color.b, Alpha);
				IGraphics::CQuadItem QuadItem(P.m_Pos.x, P.m_Pos.y, Size, Size);
				Graphics()->QuadsDraw(&QuadItem, 1);
			}

			i = P.m_NextPart;
		}

		if(LastSpr != -1)
			Graphics()->QuadsEnd();

		Graphics()->WrapNormal();
		Graphics()->BlendNormal();
	}
}