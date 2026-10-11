/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_PARTICLES_H
#define GAME_CLIENT_COMPONENTS_PARTICLES_H
#include <base/color.h>
#include <base/vmath.h>

#include <game/client/component.h>

// particles
struct CParticle
{
	void SetDefault()
	{
		m_Pos = vec2(0, 0);
		m_Vel = vec2(0, 0);
		m_LifeSpan = 0;
		m_StartSize = 32;
		m_EndSize = 32;
		m_UseAlphaFading = false;
		m_StartAlpha = 1;
		m_EndAlpha = 1;
		m_Rot = 0;
		m_Rotspeed = 0;
		m_Gravity = 0;
		m_Friction = 0;
		m_FlowAffected = 1.0f;
		m_Color = ColorRGBA(1, 1, 1, 1);
		m_Collides = true;
	}

	// hot data - accessed every frame in Update()
	vec2 m_Pos;
	vec2 m_Vel;
	float m_Life;
	float m_LifeSpan;
	float m_Gravity;
	float m_Friction;
	float m_Rot;
	float m_Rotspeed;
	int m_PrevPart;
	int m_NextPart;

	// warm data - accessed during rendering
	float m_StartSize;
	float m_EndSize;
	float m_StartAlpha;
	float m_EndAlpha;
	ColorRGBA m_Color;
	int m_Spr;

	// cold data - rarely accessed
	float m_FlowAffected;
	bool m_UseAlphaFading;
	bool m_Collides;
};

class CParticles : public CComponent
{
	friend class CGameClient;

public:
	enum
	{
		GROUP_PROJECTILE_TRAIL = 0,
		GROUP_TRAIL_EXTRA,
		GROUP_EXPLOSIONS,
		GROUP_EXTRA,
		GROUP_GENERAL,
		NUM_GROUPS
	};

	CParticles();
	int Sizeof() const override { return sizeof(*this); }

	void Add(int Group, CParticle *pPart, float TimePassed = 0.f);

	void OnReset() override;
	float m_ScreenX0, m_ScreenY0, m_ScreenX1, m_ScreenY1;
	void OnRender() override;
	void OnInit() override;

private:
	int m_ParticleQuadContainerIndex;
	int m_ExtraParticleQuadContainerIndex;

	enum
	{
		MAX_PARTICLES = 1024 * 8,
	};

	CParticle m_aParticles[MAX_PARTICLES];
	int m_FirstFree;
	int m_aFirstPart[NUM_GROUPS];

	float m_FrictionFraction = 0.0f;
	int64_t m_LastRenderTime = 0;

	void RenderGroup(int Group);
	void Update(float TimePassed);

	template<int TGROUP>
	class CRenderGroup : public CComponent
	{
	public:
		CParticles *m_pParts;
		int Sizeof() const override { return sizeof(*this); }
		void OnRender() override { m_pParts->RenderGroup(TGROUP); }
	};

	// behind players
	CRenderGroup<GROUP_PROJECTILE_TRAIL> m_RenderTrail;
	CRenderGroup<GROUP_TRAIL_EXTRA> m_RenderTrailExtra;
	// in front of players
	CRenderGroup<GROUP_EXPLOSIONS> m_RenderExplosions;
	CRenderGroup<GROUP_EXTRA> m_RenderExtra;
	CRenderGroup<GROUP_GENERAL> m_RenderGeneral;

	bool ParticleIsVisibleOnScreen(const vec2 &CurPos, float CurSize);
};
#endif
