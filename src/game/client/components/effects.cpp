#include "effects.h"

#include <engine/demo.h>
#include <engine/shared/config.h>

#include <generated/client_data.h>

#include <game/client/components/damageind.h>
#include <game/client/components/flow.h>
#include <game/client/components/particles.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>

CEffects::CEffects()
{
	m_Add5hz = false;
	m_Add50hz = false;
	m_Add100hz = false;
}

static CParticle MakeBaseParticle(int Spr, vec2 Pos, float LifeSpan, float StartSize, float EndSize, float Alpha)
{
	CParticle p;
	p.SetDefault();
	p.m_Spr = Spr;
	p.m_Pos = Pos;
	p.m_LifeSpan = LifeSpan;
	p.m_StartSize = StartSize;
	p.m_EndSize = EndSize;
	p.m_Color.a = Alpha;
	p.m_StartAlpha = Alpha;
	return p;
}

void CEffects::AirJump(vec2 Pos, float Alpha, float Volume)
{
	CParticle p = MakeBaseParticle(SPRITE_PART_AIRJUMP, Pos + vec2(-6.0f, 16.0f), 0.5f, 48.0f, 0.0f, Alpha);
	p.m_Vel = vec2(0.0f, -200.0f);
	p.m_Rot = random_angle();
	p.m_Rotspeed = pi * 2.0f;
	p.m_Gravity = 500.0f;
	p.m_Friction = 0.7f;
	p.m_FlowAffected = 0.0f;
	GameClient()->m_Particles.Add(CParticles::GROUP_GENERAL, &p);

	p.m_Pos = Pos + vec2(6.0f, 16.0f);
	GameClient()->m_Particles.Add(CParticles::GROUP_GENERAL, &p);

	if(g_Config.m_SndGame)
		GameClient()->m_Sounds.PlayAt(CSounds::CHN_WORLD, SOUND_PLAYER_AIRJUMP, Volume, Pos);
}

void CEffects::DamageIndicator(vec2 Pos, vec2 Dir, float Alpha)
{
	GameClient()->m_DamageInd.Create(Pos, Dir, Alpha);
}

void CEffects::PowerupShine(vec2 Pos, vec2 Size, float Alpha)
{
	if(!m_Add50hz)
		return;

	CParticle p = MakeBaseParticle(SPRITE_PART_SLICE, Pos + vec2(random_float(-0.5f, 0.5f), random_float(-0.5f, 0.5f)) * Size, 0.5f, 16.0f, 0.0f, Alpha);
	p.m_Rot = random_angle();
	p.m_Rotspeed = pi * 2.0f;
	p.m_Gravity = 500.0f;
	p.m_Friction = 0.9f;
	p.m_FlowAffected = 0.0f;
	GameClient()->m_Particles.Add(CParticles::GROUP_GENERAL, &p);
}

void CEffects::FreezingFlakes(vec2 Pos, vec2 Size, float Alpha)
{
	if(!m_Add5hz)
		return;

	float StartSize = random_float(0.5f, 1.5f) * 16.0f;
	CParticle p = MakeBaseParticle(SPRITE_PART_SNOWFLAKE, Pos + vec2(random_float(-0.5f, 0.5f), random_float(-0.5f, 0.5f)) * Size, 1.5f, StartSize, StartSize * 0.5f, Alpha);
	p.m_UseAlphaFading = true;
	p.m_StartAlpha = 1.0f;
	p.m_EndAlpha = 0.0f;
	p.m_Rot = random_angle();
	p.m_Rotspeed = pi;
	p.m_Gravity = random_float(250.0f);
	p.m_Friction = 0.9f;
	p.m_FlowAffected = 0.0f;
	p.m_Collides = false;
	p.m_Color.a = Alpha;
	p.m_StartAlpha = Alpha;
	GameClient()->m_Particles.Add(CParticles::GROUP_EXTRA, &p);
}

void CEffects::SparkleTrail(vec2 Pos, float Alpha)
{
	if(!m_Add50hz)
		return;

	CParticle p;
	p.SetDefault();
	p.m_Spr = SPRITE_PART_SPARKLE;
	p.m_Pos = Pos + random_direction() * random_float(40.0f);
	p.m_LifeSpan = 0.5f;
	p.m_StartSize = 0.0f;
	p.m_EndSize = random_float(20.0f, 30.0f);
	p.m_UseAlphaFading = true;
	p.m_StartAlpha = Alpha;
	p.m_EndAlpha = std::min(0.2f, Alpha);
	p.m_Collides = false;
	GameClient()->m_Particles.Add(CParticles::GROUP_TRAIL_EXTRA, &p);
}

void CEffects::SmokeTrail(vec2 Pos, vec2 Vel, float Alpha, float TimePassed)
{
	if(!m_Add50hz && TimePassed < 0.001f)
		return;

	CParticle p = MakeBaseParticle(SPRITE_PART_SMOKE, Pos, random_float(0.5f, 1.0f), random_float(12.0f, 20.0f), 0.0f, Alpha);
	p.m_Vel = Vel + random_direction() * 50.0f;
	p.m_Friction = 0.7f;
	p.m_Gravity = random_float(-500.0f);
	GameClient()->m_Particles.Add(CParticles::GROUP_PROJECTILE_TRAIL, &p, TimePassed);
}

void CEffects::SkidTrail(vec2 Pos, vec2 Vel, int Direction, float Alpha, float Volume)
{
	if(m_Add100hz)
	{
		CParticle p = MakeBaseParticle(SPRITE_PART_SMOKE, Pos + vec2(-Direction * 6.0f, 12.0f), random_float(0.5f, 1.0f), random_float(24.0f, 36.0f), 0.0f, Alpha);
		p.m_Vel = vec2(-Direction * 100.0f * length(Vel), -50.0f) + random_direction() * 50.0f;
		p.m_Friction = 0.7f;
		p.m_Gravity = random_float(-500.0f);
		p.m_Color = ColorRGBA(0.75f, 0.75f, 0.75f, Alpha);
		GameClient()->m_Particles.Add(CParticles::GROUP_GENERAL, &p);
	}
	if(g_Config.m_SndGame)
	{
		int64_t Now = time();
		if(Now - m_SkidSoundTimer > time_freq() / 10)
		{
			m_SkidSoundTimer = Now;
			GameClient()->m_Sounds.PlayAt(CSounds::CHN_WORLD, SOUND_PLAYER_SKID, Volume, Pos);
		}
	}
}

void CEffects::BulletTrail(vec2 Pos, float Alpha, float TimePassed)
{
	if(!m_Add100hz && TimePassed < 0.001f)
		return;

	CParticle p = MakeBaseParticle(SPRITE_PART_BALL, Pos, random_float(0.25f, 0.5f), 8.0f, 0.0f, Alpha);
	p.m_Friction = 0.7f;
	p.m_Color.a *= Alpha;
	GameClient()->m_Particles.Add(CParticles::GROUP_PROJECTILE_TRAIL, &p, TimePassed);
}

void CEffects::PlayerSpawn(vec2 Pos, float Alpha, float Volume)
{
	for(int i = 0; i < 32; i++)
	{
		CParticle p = MakeBaseParticle(SPRITE_PART_SHELL, Pos, random_float(0.3f, 0.6f), random_float(64.0f, 96.0f), 0.0f, Alpha);
		p.m_Vel = random_direction() * (std::pow(random_float(), 3) * 600.0f);
		p.m_Rot = random_angle();
		p.m_Rotspeed = random_float();
		p.m_Gravity = random_float(-400.0f);
		p.m_Friction = 0.7f;
		p.m_Color = ColorRGBA(0xb5 / 255.0f, 0x50 / 255.0f, 0xcb / 255.0f, Alpha);
		GameClient()->m_Particles.Add(CParticles::GROUP_GENERAL, &p);
	}
	if(g_Config.m_SndGame)
		GameClient()->m_Sounds.PlayAt(CSounds::CHN_WORLD, SOUND_PLAYER_SPAWN, Volume, Pos);
}

void CEffects::PlayerDeath(vec2 Pos, int ClientId, float Alpha)
{
	ColorRGBA BloodColor(1.0f, 1.0f, 1.0f);

	if(ClientId >= 0)
	{
		const auto &Info = GameClient()->m_aClients[ClientId].m_RenderInfo;
		int Dummy = g_Config.m_ClDummy;
		if(GameClient()->Client()->IsSixup())
		{
			BloodColor = Info.m_aSixup[Dummy].m_aUseCustomColors[protocol7::SKINPART_BODY]
				? Info.m_aSixup[Dummy].m_aColors[protocol7::SKINPART_BODY]
				: Info.m_aSixup[Dummy].m_BloodColor;
		}
		else
		{
			BloodColor = Info.m_CustomColoredSkin ? Info.m_ColorBody : Info.m_BloodColor;
		}
	}

	for(int i = 0; i < 64; i++)
	{
		CParticle p = MakeBaseParticle(SPRITE_PART_SPLAT01 + (rand() % 3), Pos, random_float(0.3f, 0.6f), random_float(24.0f, 40.0f), 0.0f, Alpha);
		p.m_Vel = random_direction() * (random_float(0.1f, 1.1f) * 900.0f);
		p.m_Rot = random_angle();
		p.m_Rotspeed = random_float(-0.5f, 0.5f) * pi;
		p.m_Gravity = 800.0f;
		p.m_Friction = 0.8f;
		p.m_Color = BloodColor.Multiply(random_float(0.75f, 1.0f)).WithAlpha(0.75f * Alpha);
		GameClient()->m_Particles.Add(CParticles::GROUP_GENERAL, &p);
	}
}

void CEffects::Confetti(vec2 Pos, float Alpha)
{
	static const ColorRGBA aColors[] = {
		{1.0f, 0.4f, 0.4f}, {0.4f, 1.0f, 0.4f}, {0.4f, 0.4f, 1.0f},
		{1.0f, 1.0f, 0.4f}, {0.4f, 1.0f, 1.0f}, {1.0f, 0.4f, 1.0f}
	};

	auto SpawnConfetti = [&](int Count, float DirSpread, float MaxSpeed, float MinLife, float MaxLife) {
		for(int i = 0; i < Count; i++)
		{
			CParticle p = MakeBaseParticle(SPRITE_PART_SPLAT01 + (rand() % 3), Pos, random_float(MinLife, MaxLife), random_float(12.0f, 24.0f), 0.0f, Alpha);
			p.m_Vel = direction(-0.5f * pi + random_float(-DirSpread, DirSpread)) * random_float(0.01f, 1.0f) * MaxSpeed;
			p.m_Rot = random_angle();
			p.m_Rotspeed = random_float(-0.5f, 0.5f) * pi;
			p.m_Gravity = -700.0f;
			p.m_Friction = 0.6f;
			p.m_Color = aColors[rand() % std::size(aColors)].WithMultipliedAlpha(0.75f * Alpha);
			GameClient()->m_Particles.Add(CParticles::GROUP_GENERAL, &p);
		}
	};

	SpawnConfetti(32, 0.2f, 2000.0f, 1.0f, 1.2f);
	SpawnConfetti(32, 0.8f, 1500.0f, 0.8f, 1.0f);
}

void CEffects::Explosion(vec2 Pos, float Alpha)
{
	for(int y = -8; y <= 8; y++)
		for(int x = -8; x <= 8; x++)
		{
			if(x == 0 && y == 0)
				continue;
			vec2 V(x, y);
			float a = 1 - (length(V) / length(vec2(8.0f, 8.0f)));
			GameClient()->m_Flow.Add(Pos + V * 16.0f, normalize(V) * 5000.0f * a, 10.0f);
		}

	CParticle p = MakeBaseParticle(SPRITE_PART_EXPL01, Pos, 0.4f, 150.0f, 0.0f, Alpha);
	p.m_Rot = random_angle();
	GameClient()->m_Particles.Add(CParticles::GROUP_EXPLOSIONS, &p);

	if(Collision()->CheckPoint(Pos))
	{
		vec2 DistToTL = Pos - vec2(round_truncate(Pos.x / 32), round_truncate(Pos.y / 32)) * 32;
		vec2 CheckOff;
		CheckOff.x = (DistToTL.x > 16.0f ? 32.0f : -1.0f) - DistToTL.x;
		CheckOff.y = (DistToTL.y > 16.0f ? 32.0f : -1.0f) - DistToTL.y;

		for(vec2 Mask : {vec2(1.0f, 0.0f), vec2(0.0f, 1.0f), vec2(1.0f, 1.0f)})
		{
			vec2 NewPos = Pos + CheckOff * Mask;
			if(!Collision()->CheckPoint(NewPos))
			{
				Pos = NewPos;
				break;
			}
		}
	}

	for(int i = 0; i < 24; i++)
	{
		CParticle s = MakeBaseParticle(SPRITE_PART_SMOKE, Pos, random_float(0.5f, 0.9f), random_float(32.0f, 40.0f), 0.0f, Alpha);
		s.m_Vel = random_direction() * (random_float(1.0f, 1.2f) * 1000.0f);
		s.m_Gravity = random_float(-800.0f);
		s.m_Friction = 0.4f;
		s.m_Color = ColorRGBA(1.0f, 1.0f, 1.0f).Multiply(random_float(0.5f, 0.75f)).WithAlpha(Alpha);
		s.m_StartAlpha = s.m_Color.a;
		GameClient()->m_Particles.Add(CParticles::GROUP_GENERAL, &s);
	}
}

void CEffects::HammerHit(vec2 Pos, float Alpha, float Volume)
{
	CParticle p = MakeBaseParticle(SPRITE_PART_HIT01, Pos, 0.3f, 120.0f, 0.0f, Alpha);
	p.m_Rot = random_angle();
	GameClient()->m_Particles.Add(CParticles::GROUP_EXPLOSIONS, &p);
	if(g_Config.m_SndGame)
		GameClient()->m_Sounds.PlayAt(CSounds::CHN_WORLD, SOUND_HAMMER_HIT, Volume, Pos);
}

void CEffects::OnRender()
{
	float Speed = 1.0f;
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		Speed = DemoPlayer()->BaseInfo()->m_Speed;

	const int64_t Now = time();
	auto UpdateClock = [&](bool &Add, int64_t &LastUpdate, int Frequency) {
		Add = Now - LastUpdate > time_freq() / ((float)Frequency * Speed);
		if(Add)
			LastUpdate = Now;
	};
	UpdateClock(m_Add5hz, m_LastUpdate5hz, 5);
	UpdateClock(m_Add50hz, m_LastUpdate50hz, 50);
	UpdateClock(m_Add100hz, m_LastUpdate100hz, 100);

	if(m_Add50hz)
		GameClient()->m_Flow.Update();
}