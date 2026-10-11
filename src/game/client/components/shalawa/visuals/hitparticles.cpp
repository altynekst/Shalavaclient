#include "hitparticles.h"

#include <game/client/gameclient.h>
#include <engine/shared/config.h>
#include <imgui.h>
#include <SDL.h>
#include <random>
#include <base/vmath.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>
#include <algorithm>


CHitParticles::CHitParticles()
{
    ANTI_DISASM_BLOCK();
	m_Time = OBF_F(0);

	m_DeltaTime = 0.0f;
	m_LastTime = 0.0f;
	m_LastHammerHitTick = -1;
	m_Initialized = false;
	m_ScreenFlash = 0.0f;
	m_ScreenShakeX = 0.0f;
	m_ScreenShakeY = 0.0f;
}

void CHitParticles::OnInit()
{
    ANTI_DISASM_BLOCK();
	m_Initialized = true;
	m_LastTime = (float)SDL_GetTicks() / OBF_F(1000);
}


void CHitParticles::OnReset()
{
    ANTI_DISASM_BLOCK();
	m_Particles.clear();

	m_HitEvents.clear();
	m_Afterimages.clear();
	m_LastHammerHitTick = -1;
	m_ScreenFlash = 0.0f;
}

float CHitParticles::Lerp(float a, float b, float t)
{
	return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
}

float CHitParticles::RandFloat(float mn, float mx)
{
	static std::mt19937 gen(std::random_device{}());
	std::uniform_real_distribution<float> dist(mn, mx);
	return dist(gen);
}

ImVec4 CHitParticles::HSVtoRGB(float h, float s, float v)
{
	float c = v * s;
	float x = c * (1.0f - fabsf(fmodf(h * 6.0f, 2.0f) - 1.0f));
	float m = v - c;
	float r, g, b;
	if(h < 1.0f/6.0f) { r=c; g=x; b=0; }
	else if(h < 2.0f/6.0f) { r=x; g=c; b=0; }
	else if(h < 3.0f/6.0f) { r=0; g=c; b=x; }
	else if(h < 4.0f/6.0f) { r=0; g=x; b=c; }
	else if(h < 5.0f/6.0f) { r=x; g=0; b=c; }
	else { r=c; g=0; b=x; }
	return ImVec4(r+m, g+m, b+m, 1.0f);
}

ImU32 CHitParticles::ColorWithAlpha(ImVec4 col, float alpha)
{
	col.w = std::clamp(alpha, 0.0f, 1.0f);
	return ImGui::ColorConvertFloat4ToU32(col);
}

void CHitParticles::ProcessSnapshots()
{
    ANTI_DISASM_BLOCK();
}


void CHitParticles::OnNewSnapshot()
{
    ANTI_DISASM_BLOCK();
    if(!m_Initialized || !g_Config.m_shalawaHitParticles)

        return;
    if(!GameClient() || !Client())
        return;
    if(Client()->State() != IClient::STATE_ONLINE)
        return;

    int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
    for(int i = 0; i < Num; i++)
    {
        const IClient::CSnapItem Item = Client()->SnapGetItem(IClient::SNAP_CURRENT, i);
        if(Item.m_Type == NETEVENTTYPE_HAMMERHIT)
        {
            const CNetEvent_HammerHit *pEvent = (const CNetEvent_HammerHit *)Item.m_pData;
            if(pEvent)
                QueueHammerHit((float)pEvent->m_X, (float)pEvent->m_Y);
        }
    }
}


void CHitParticles::OnHammerHit(float x, float y)
{
    ANTI_DISASM_BLOCK();
	if(!g_Config.m_shalawaHitParticles)

		return;
	if(!std::isfinite(x) || !std::isfinite(y))
		return;
	if(m_HitEvents.size() > 50)
		return;

	SpawnParticles(x, y);

	SHitEvent ev;
	ev.m_X = x;
	ev.m_Y = y;
	ev.m_Time = 0.0f;
	ev.m_RingProgress = 0.0f;
	ev.m_RingAlpha = 1.0f;
	ev.m_ShockwaveRadius = 0.0f;
	ev.m_FlashIntensity = 1.0f;
	ev.m_ScreenShake = 1.0f;
	ev.m_Hue = RandFloat(0.0f, 1.0f);
	ev.m_WaveCount = g_Config.m_shalawaHitParticleShockwave ? std::clamp(g_Config.m_shalawaHitParticleWaveCount, 1, 4) : 0;
	for(int i = 0; i < 4; i++)
		ev.m_WavePhases[i] = (float)i * 0.08f;
	m_HitEvents.push_back(ev);

	if(g_Config.m_shalawaHitParticleFlash)
		m_ScreenFlash = 1.0f;
}

void CHitParticles::SpawnParticles(float x, float y)
{
    ANTI_DISASM_BLOCK();
	if(!std::isfinite(x) || !std::isfinite(y))
		return;

	int mode = g_Config.m_shalawaHitParticleType;
	int count = std::clamp(g_Config.m_shalawaHitParticleCount, OI(0), OI(200));


	if(m_Particles.size() > 2000)
		return;

	for(int i = 0; i < count; i++)
	{
		SHitParticle p;
		float angle = RandFloat(0.0f, 6.28318f);
		float speed = RandFloat(60.0f, 320.0f);
		float speedMul = g_Config.m_shalawaHitParticleSpeed / 100.0f;

		p.m_X = x + RandFloat(-4.0f, 4.0f);
		p.m_Y = y + RandFloat(-4.0f, 4.0f);
		p.m_OrigX = x;
		p.m_OrigY = y;
		p.m_VelX = cosf(angle) * speed * speedMul;
		p.m_VelY = sinf(angle) * speed * speedMul - RandFloat(20.0f, 80.0f);
		p.m_MaxLife = RandFloat(0.6f, 1.5f) * (g_Config.m_shalawaHitParticleLifetime / 100.0f);
		p.m_Life = p.m_MaxLife;
		p.m_Size = RandFloat(4.0f, 14.0f) * (g_Config.m_shalawaHitParticleSize / 100.0f);
		p.m_Rotation = RandFloat(0.0f, 6.28f);
		p.m_RotSpeed = RandFloat(-10.0f, 10.0f);
		p.m_Alpha = 1.0f;
		p.m_Hue = RandFloat(0.0f, 1.0f);
		p.m_Gravity = RandFloat(120.0f, 300.0f) * (g_Config.m_shalawaHitParticleGravity / 100.0f);
		p.m_Bounce = 0.0f;
		p.m_Trail = 0.0f;
		p.m_OrbitRadius = RandFloat(15.0f, 45.0f);
		p.m_OrbitSpeed = RandFloat(3.0f, 8.0f);
		p.m_OrbitPhase = RandFloat(0.0f, 6.28f);
		p.m_PulsePhase = RandFloat(0.0f, 6.28f);
		p.m_FlickerSeed = RandFloat(0.0f, 100.0f);
		p.m_SubType = (int)RandFloat(0, 2.99f);
		p.m_ScaleAnim = 0.0f;
		p.m_Drag = RandFloat(1.5f, 3.0f);
		p.m_SpinAccel = RandFloat(-2.0f, 2.0f);
		p.m_HasAfterimage = g_Config.m_shalawaHitParticleAfterimage && (RandFloat(0, 1) > 0.5f);
		p.m_AfterimageAlpha = 0.0f;

		switch(mode)
		{
		case 0:
			p.m_Type = 0;
			p.m_Hue = RandFloat(0.08f, 0.22f);
			p.m_SubType = (int)RandFloat(0, 1.99f);
			break;
		case 1:
			p.m_Type = 1;
			p.m_Hue = RandFloat(0.92f, 1.0f);
			p.m_Gravity *= 0.7f;
			break;
		case 2:
			p.m_Type = 2;
			p.m_VelX *= 0.2f;
			p.m_VelY *= 0.2f;
			p.m_Gravity *= 0.05f;
			p.m_MaxLife = RandFloat(1.0f, 2.2f);
			p.m_Life = p.m_MaxLife;
			p.m_Drag = 0.5f;
			break;
		case 3:
			p.m_Type = 3;
			p.m_Size *= 0.6f;
			p.m_Trail = 1.0f;
			break;
		case 4:
			p.m_Type = 4;
			p.m_Size *= 1.2f;
			break;
		case 5:
			p.m_Type = 5;
			p.m_Hue = RandFloat(0.0f, 1.0f);
			break;
		case 6:
			p.m_Type = 6;
			p.m_Size *= 0.8f;
			p.m_Gravity *= 0.3f;
			break;
		case 7:
			p.m_Type = 7;
			p.m_Size *= 1.1f;
			break;
		case 8:
			p.m_Type = (int)RandFloat(0, 7.99f);
			break;
		}

		m_Particles.push_back(p);
	}

	if(g_Config.m_shalawaHitParticleShockwave)
	{
		int burstCount = std::clamp(8 + g_Config.m_shalawaHitParticleWaveCount * 2, 2, 40);
		for(int i = 0; i < burstCount; i++)
		{
			SHitParticle ring;
			float a = (float)i / (float)burstCount * 6.28318f;
			float sp = RandFloat(180.0f, 300.0f);
			ring.m_X = x;
			ring.m_Y = y;
			ring.m_OrigX = x;
			ring.m_OrigY = y;
			ring.m_VelX = cosf(a) * sp;
			ring.m_VelY = sinf(a) * sp;
			ring.m_MaxLife = RandFloat(0.3f, 0.5f);
			ring.m_Life = ring.m_MaxLife;
			ring.m_Size = RandFloat(2.0f, 4.0f);
			ring.m_Rotation = 0;
			ring.m_RotSpeed = 0;
			ring.m_Alpha = 1.0f;
			ring.m_Type = 3;
			ring.m_Hue = RandFloat(0.0f, 1.0f);
			ring.m_Gravity = 0;
			ring.m_Bounce = 0;
			ring.m_Trail = 1.0f;
			ring.m_OrbitRadius = 0;
			ring.m_OrbitSpeed = 0;
			ring.m_OrbitPhase = 0;
			ring.m_PulsePhase = RandFloat(0, 6.28f);
			ring.m_FlickerSeed = RandFloat(0, 100);
			ring.m_SubType = 0;
			ring.m_ScaleAnim = 1.0f;
			ring.m_Drag = 4.0f;
			ring.m_SpinAccel = 0;
			ring.m_HasAfterimage = true;
			ring.m_AfterimageAlpha = 0.5f;
			m_Particles.push_back(ring);
		}
	}

	if(g_Config.m_shalawaHitParticlePhysics)
	{
		for(int i = 0; i < 4; i++)
		{
			SHitParticle debris;
			float a = RandFloat(0, 6.28f);
			debris.m_X = x;
			debris.m_Y = y;
			debris.m_OrigX = x;
			debris.m_OrigY = y;
			debris.m_VelX = cosf(a) * RandFloat(40, 120);
			debris.m_VelY = -RandFloat(100, 250);
			debris.m_MaxLife = RandFloat(0.8f, 1.5f);
			debris.m_Life = debris.m_MaxLife;
			debris.m_Size = RandFloat(3, 6) * (g_Config.m_shalawaHitParticleSize / 100.0f);
			debris.m_Rotation = RandFloat(0, 6.28f);
			debris.m_RotSpeed = RandFloat(-15, 15);
			debris.m_Alpha = 1.0f;
			debris.m_Type = 4;
			debris.m_Hue = RandFloat(0, 1);
			debris.m_Gravity = RandFloat(400, 600);
			debris.m_Bounce = 0.6f;
			debris.m_Trail = 0.5f;
			debris.m_OrbitRadius = 0;
			debris.m_OrbitSpeed = 0;
			debris.m_OrbitPhase = 0;
			debris.m_PulsePhase = 0;
			debris.m_FlickerSeed = RandFloat(0, 100);
			debris.m_SubType = 1;
			debris.m_ScaleAnim = 1.0f;
			debris.m_Drag = 1.0f;
			debris.m_SpinAccel = RandFloat(-5, 5);
			debris.m_HasAfterimage = false;
			debris.m_AfterimageAlpha = 0;
			m_Particles.push_back(debris);
		}
	}
}

void CHitParticles::QueueHammerHit(float x, float y)
{
    ANTI_DISASM_BLOCK();
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    if(m_HitQueue.size() < OI(50))
        m_HitQueue.push({x, y});
}


void CHitParticles::ProcessHitQueue()
{
    ANTI_DISASM_BLOCK();
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    while(!m_HitQueue.empty())
    {
        auto [x, y] = m_HitQueue.front();
        m_HitQueue.pop();
        OnHammerHit(x, y);
    }
}


void CHitParticles::UpdateParticles()
{
    ANTI_DISASM_BLOCK();
	if(m_DeltaTime <= OBF_F(0) || !std::isfinite(m_DeltaTime))
		return;

	while(m_Afterimages.size() > OI(1000))

		m_Afterimages.erase(m_Afterimages.begin());

	for(auto it = m_Afterimages.begin(); it != m_Afterimages.end();)
	{
		it->m_Life -= m_DeltaTime;
		it->m_Alpha *= 0.92f;
		if(it->m_Life <= 0 || it->m_Alpha < 0.01f)
			it = m_Afterimages.erase(it);
		else
			++it;
	}

	for(auto it = m_Particles.begin(); it != m_Particles.end();)
	{
		SHitParticle &p = *it;
		p.m_Life -= m_DeltaTime;
		if(p.m_Life <= 0.0f)
		{
			it = m_Particles.erase(it);
			continue;
		}

		p.m_ScaleAnim = Lerp(p.m_ScaleAnim, 1.0f, m_DeltaTime * 12.0f);

		p.m_VelY += p.m_Gravity * m_DeltaTime;
		float dragFactor = 1.0f - m_DeltaTime * p.m_Drag;
		if(dragFactor < 0) dragFactor = 0;
		p.m_VelX *= dragFactor;
		p.m_VelY *= dragFactor;
		p.m_X += p.m_VelX * m_DeltaTime;
		p.m_Y += p.m_VelY * m_DeltaTime;
		p.m_RotSpeed += p.m_SpinAccel * m_DeltaTime;
		p.m_Rotation += p.m_RotSpeed * m_DeltaTime;

		float lifeRatio = (p.m_MaxLife > 0.001f) ? (p.m_Life / p.m_MaxLife) : 0.0f;

		if(lifeRatio < 0.25f)
			p.m_Alpha = lifeRatio / 0.25f;
		else if(lifeRatio > 0.9f)
			p.m_Alpha = (1.0f - lifeRatio) / 0.1f;
		else
			p.m_Alpha = 1.0f;

		float pulse = sinf(m_Time * 6.0f + p.m_PulsePhase) * 0.15f + 1.0f;
		p.m_Alpha *= pulse;

		float flicker = sinf(m_Time * 20.0f + p.m_FlickerSeed) * 0.5f + 0.5f;
		if(p.m_Type == 3)
			p.m_Alpha *= (0.6f + flicker * 0.4f);

		if(p.m_Type == 2)
		{
			float elapsed = p.m_MaxLife - p.m_Life;
			float orbitT = elapsed * p.m_OrbitSpeed + p.m_OrbitPhase;
			float shrinkOrbit = p.m_OrbitRadius * lifeRatio;
			p.m_X = p.m_OrigX + cosf(orbitT) * shrinkOrbit + p.m_VelX * elapsed * 0.3f;
			p.m_Y = p.m_OrigY + sinf(orbitT) * shrinkOrbit + p.m_VelY * elapsed * 0.3f;
		}

		if(p.m_HasAfterimage && p.m_Alpha > 0.2f)
		{
			float velSq = p.m_VelX * p.m_VelX + p.m_VelY * p.m_VelY;
			if(velSq > 2000.0f && m_Afterimages.size() < 500)
			{
				if(((int)(m_Time * 60) + (int)(p.m_FlickerSeed * 10)) % 3 == 0)
				{
					SAfterimage ai;
					ai.m_X = p.m_X;
					ai.m_Y = p.m_Y;
					ai.m_Size = p.m_Size * 0.8f;
					ai.m_Rotation = p.m_Rotation;
					ai.m_Alpha = p.m_Alpha * 0.35f;
					ai.m_Type = p.m_Type;
					ai.m_Hue = p.m_Hue;
					ai.m_Life = 0.15f;
					m_Afterimages.push_back(ai);
				}
			}
		}

		++it;
	}

	for(auto it = m_HitEvents.begin(); it != m_HitEvents.end();)
	{
		it->m_Time += m_DeltaTime;
		it->m_RingProgress = std::clamp(it->m_Time / 0.6f, 0.0f, 1.0f);
		it->m_RingAlpha = 1.0f - it->m_RingProgress;
		it->m_ShockwaveRadius = it->m_RingProgress * 80.0f;
		it->m_FlashIntensity *= (1.0f - m_DeltaTime * 8.0f);
		it->m_ScreenShake *= (1.0f - m_DeltaTime * 10.0f);

		if(it->m_Time > 0.8f)
			it = m_HitEvents.erase(it);
		else
			++it;
	}

	m_ScreenFlash *= (1.0f - m_DeltaTime * 12.0f);
	if(m_ScreenFlash < 0.01f) m_ScreenFlash = 0.0f;

	m_ScreenShakeX = 0;
	m_ScreenShakeY = 0;
	for(auto &ev : m_HitEvents)
	{
		if(ev.m_ScreenShake > 0.05f)
		{
			m_ScreenShakeX += sinf(m_Time * 50.0f) * ev.m_ScreenShake * 2.0f;
			m_ScreenShakeY += cosf(m_Time * 43.0f) * ev.m_ScreenShake * 2.0f;
		}
	}
}

void CHitParticles::RenderStar(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col, int points)
{
    ANTI_DISASM_BLOCK();
	if(!d || points < OI(2) || size < OFR(1,10))

		return;
	if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(size) || !std::isfinite(rotation))
		return;
	float outerR = size;
	float innerR = size * 0.38f;
	int totalPts = points * 2;

	for(int i = 0; i < totalPts; i++)
	{
		float a1 = rotation + (float)i / (float)totalPts * 6.28318f;
		float a2 = rotation + (float)(i + 1) / (float)totalPts * 6.28318f;
		float r1 = (i % 2 == 0) ? outerR : innerR;
		float r2 = ((i + 1) % 2 == 0) ? outerR : innerR;

		d->AddTriangleFilled(ImVec2(x, y),
			ImVec2(x + cosf(a1) * r1, y + sinf(a1) * r1),
			ImVec2(x + cosf(a2) * r2, y + sinf(a2) * r2), col);
	}

	ImU32 coreCol = ColorWithAlpha(ImVec4(1, 1, 1, 1), ((col >> 24) & 0xFF) / 255.0f * 0.6f);
	d->AddCircleFilled(ImVec2(x, y), size * 0.25f, coreCol, 8);
}

void CHitParticles::RenderHeart(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col)
{
    ANTI_DISASM_BLOCK();
	if(!d || size < OFR(1,10))

		return;
	if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(size) || !std::isfinite(rotation))
		return;
	int segments = 24;
	std::vector<ImVec2> pts;
	pts.reserve(segments);

	for(int i = 0; i < segments; i++)
	{
		float t = (float)i / (float)(segments - 1) * 6.28318f;
		float hx = 16.0f * sinf(t) * sinf(t) * sinf(t);
		float hy = -(13.0f * cosf(t) - 5.0f * cosf(2*t) - 2.0f * cosf(3*t) - cosf(4*t));
		float scale = size / 16.0f;

		float rx = hx * cosf(rotation) - hy * sinf(rotation);
		float ry = hx * sinf(rotation) + hy * cosf(rotation);

		pts.push_back(ImVec2(x + rx * scale, y + ry * scale));
	}

	if(pts.size() >= 3)
		d->AddConvexPolyFilled(pts.data(), (int)pts.size(), col);

	float shineX = x - size * 0.3f;
	float shineY = y - size * 0.2f;
	float cs = cosf(rotation), sn = sinf(rotation);
	float rsx = (shineX - x) * cs - (shineY - y) * sn + x;
	float rsy = (shineX - x) * sn + (shineY - y) * cs + y;
	ImU32 shine = ColorWithAlpha(ImVec4(1, 1, 1, 1), ((col >> 24) & 0xFF) / 255.0f * 0.4f);
	d->AddCircleFilled(ImVec2(rsx, rsy), size * 0.15f, shine, 6);
}

void CHitParticles::RenderSparkle(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col)
{
    ANTI_DISASM_BLOCK();
	if(!d || size < OFR(1,10))

		return;
	if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(size) || !std::isfinite(rotation))
		return;
	for(int i = 0; i < 4; i++)
	{
		float angle = rotation + (float)i * 0.785f;
		float len = (i % 2 == 0) ? size : size * 0.6f;
		float thick = size * 0.12f;
		float cs = cosf(angle), sn = sinf(angle);

		ImVec2 tip(x + cs * len, y + sn * len);
		ImVec2 left(x - sn * thick, y + cs * thick);
		ImVec2 right(x + sn * thick, y - cs * thick);
		d->AddTriangleFilled(tip, left, right, col);

		ImVec2 tip2(x - cs * len, y - sn * len);
		d->AddTriangleFilled(tip2, left, right, col);
	}

	ImU32 center = ColorWithAlpha(ImVec4(1, 1, 1, 1), ((col >> 24) & 0xFF) / 255.0f * 0.7f);
	d->AddCircleFilled(ImVec2(x, y), size * 0.2f, center, 8);
}

void CHitParticles::RenderRing(ImDrawList *d, float cx, float cy, float radius, float progress, ImU32 col, float thickness)
{
    ANTI_DISASM_BLOCK();
	if(!d || radius < OFR(1,10) || thickness < OFR(1,100))

		return;
	if(!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(radius) || !std::isfinite(progress))
		return;
	int segments = std::max(8, (int)(progress * 40));
	for(int i = 0; i < segments - 1; i++)
	{
		float a1 = (float)i / (float)segments * 6.28318f * progress;
		float a2 = (float)(i + 1) / (float)segments * 6.28318f * progress;
		float segAlpha = 1.0f - (float)i / (float)segments * 0.5f;
		ImU32 segCol = ColorWithAlpha(
			ImVec4(((col >> 0) & 0xFF) / 255.0f, ((col >> 8) & 0xFF) / 255.0f,
				   ((col >> 16) & 0xFF) / 255.0f, 1.0f),
			((col >> 24) & 0xFF) / 255.0f * segAlpha);
		d->AddLine(
			ImVec2(cx + cosf(a1) * radius, cy + sinf(a1) * radius),
			ImVec2(cx + cosf(a2) * radius, cy + sinf(a2) * radius),
			segCol, thickness);
	}
}

void CHitParticles::RenderCrescent(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col)
{
    ANTI_DISASM_BLOCK();
	if(!d || size < OFR(1,10))

		return;
	if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(size) || !std::isfinite(rotation))
		return;
	int segs = 20;
	float outerR = size;
	float innerR = size * 0.7f;
	float offsetX = size * 0.3f;

	std::vector<ImVec2> pts;
	for(int i = 0; i <= segs; i++)
	{
		float a = (float)i / (float)segs * 6.28318f;
		float px = cosf(a) * outerR;
		float py = sinf(a) * outerR;
		float rx = px * cosf(rotation) - py * sinf(rotation);
		float ry = px * sinf(rotation) + py * cosf(rotation);
		pts.push_back(ImVec2(x + rx, y + ry));
	}
	if(pts.size() >= 3)
		d->AddConvexPolyFilled(pts.data(), (int)pts.size(), col);

	float bgAlpha = ((col >> 24) & 0xFF) / 255.0f;
	ImU32 cutout = ColorWithAlpha(ImVec4(0, 0, 0, 1), bgAlpha * 0.9f);
	float cx = x + cosf(rotation) * offsetX;
	float cy = y + sinf(rotation) * offsetX;
	d->AddCircleFilled(ImVec2(cx, cy), innerR, cutout, 16);
}

void CHitParticles::RenderDiamond(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col, ImU32 innerCol)
{
    ANTI_DISASM_BLOCK();
	if(!d || size < OFR(1,10))

		return;
	if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(size) || !std::isfinite(rotation))
		return;
	float cs = cosf(rotation), sn = sinf(rotation);
	auto rot = [&](float px, float py) -> ImVec2 {
		return ImVec2(x + px * cs - py * sn, y + px * sn + py * cs);
	};

	ImVec2 top = rot(0, -size);
	ImVec2 right = rot(size * 0.6f, 0);
	ImVec2 bottom = rot(0, size * 0.8f);
	ImVec2 left = rot(-size * 0.6f, 0);

	d->AddTriangleFilled(top, right, ImVec2(x, y), col);
	d->AddTriangleFilled(right, bottom, ImVec2(x, y), col);
	d->AddTriangleFilled(bottom, left, ImVec2(x, y), innerCol);
	d->AddTriangleFilled(left, top, ImVec2(x, y), innerCol);

	d->AddLine(top, bottom, ColorWithAlpha(ImVec4(1,1,1,1), ((col>>24)&0xFF)/255.0f*0.2f), 1.0f);
	d->AddLine(left, right, ColorWithAlpha(ImVec4(1,1,1,1), ((col>>24)&0xFF)/255.0f*0.2f), 1.0f);
}

void CHitParticles::RenderCross(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col)
{
    ANTI_DISASM_BLOCK();
	if(!d || size < OFR(1,10))

		return;
	if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(size) || !std::isfinite(rotation))
		return;
	float armW = size * 0.3f;
	float armL = size;
	float cs = cosf(rotation), sn = sinf(rotation);
	auto rot = [&](float px, float py) -> ImVec2 {
		return ImVec2(x + px * cs - py * sn, y + px * sn + py * cs);
	};

	d->AddQuadFilled(rot(-armW, -armL), rot(armW, -armL), rot(armW, armL), rot(-armW, armL), col);
	d->AddQuadFilled(rot(-armL, -armW), rot(armL, -armW), rot(armL, armW), rot(-armL, armW), col);

	ImU32 center = ColorWithAlpha(ImVec4(1,1,1,1), ((col>>24)&0xFF)/255.0f * 0.3f);
	d->AddCircleFilled(ImVec2(x, y), armW, center, 8);
}

void CHitParticles::RenderFlower(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col, int petals)
{
    ANTI_DISASM_BLOCK();
	if(!d || size < OFR(1,10) || petals < OI(1))

		return;
	if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(size) || !std::isfinite(rotation))
		return;
	if(petals > 20)
		petals = 20;
	for(int i = 0; i < petals; i++)
	{
		float angle = rotation + (float)i / (float)petals * 6.28318f;
		float petalLen = size;
		float petalW = size * 0.4f;

		float cs = cosf(angle), sn = sinf(angle);
		ImVec2 tip(x + cs * petalLen, y + sn * petalLen);
		ImVec2 left(x + (-sn) * petalW, y + cs * petalW);
		ImVec2 right(x + sn * petalW, y + (-cs) * petalW);

		d->AddTriangleFilled(tip, left, right, col);
	}

	ImU32 center = ColorWithAlpha(ImVec4(1, 0.9f, 0.3f, 1), ((col>>24)&0xFF)/255.0f * 0.9f);
	d->AddCircleFilled(ImVec2(x, y), size * 0.25f, center, 10);
}

void CHitParticles::RenderSwirl(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col)
{
    ANTI_DISASM_BLOCK();
	if(!d || size < OFR(1,10))

		return;
	if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(size) || !std::isfinite(rotation))
		return;
	int segs = 30;
	float baseAlpha = ((col >> 24) & 0xFF) / 255.0f;

	for(int i = 0; i < segs - 1; i++)
	{
		float t1 = (float)i / (float)segs;
		float t2 = (float)(i + 1) / (float)segs;
		float a1 = rotation + t1 * 10.0f;
		float a2 = rotation + t2 * 10.0f;
		float r1 = size * t1;
		float r2 = size * t2;
		float alpha = baseAlpha * (1.0f - t1 * 0.7f);

		ImU32 c = ColorWithAlpha(ImVec4(((col>>0)&0xFF)/255.0f, ((col>>8)&0xFF)/255.0f,
			((col>>16)&0xFF)/255.0f, 1.0f), alpha);
		d->AddLine(ImVec2(x + cosf(a1)*r1, y + sinf(a1)*r1),
			ImVec2(x + cosf(a2)*r2, y + sinf(a2)*r2), c, 2.0f * (1.0f - t1));
	}
}

void CHitParticles::RenderScreenFlash(ImDrawList *d)
{
    ANTI_DISASM_BLOCK();
	if(!d || m_ScreenFlash < OFR(1,100))

		return;
	if(!ImGui::GetCurrentContext())
		return;

	ImGuiIO &io = ImGui::GetIO();
	float alpha = m_ScreenFlash * 0.15f * (g_Config.m_shalawaHitParticleFlashIntensity / 100.0f);
	ImU32 flashCol = ColorWithAlpha(ImVec4(1, 1, 1, 1), alpha);
	d->AddRectFilled(ImVec2(0, 0), io.DisplaySize, flashCol);
}

void CHitParticles::RenderConnectorLines(ImDrawList *d, float screenCX, float screenCY, float scale, vec2 camPos)
{
    ANTI_DISASM_BLOCK();
	if(!d || !g_Config.m_shalawaHitParticleConnectors || m_Particles.size() < OI(2))

		return;
	if(!std::isfinite(screenCX) || !std::isfinite(screenCY) || !std::isfinite(scale))
		return;

	float maxDist = 40.0f * scale;
	float maxDistSq = maxDist * maxDist;

	for(size_t i = 0; i < m_Particles.size() && i < 60; i++)
	{
		float sx1 = screenCX + (m_Particles[i].m_X - camPos.x) * scale;
		float sy1 = screenCY + (m_Particles[i].m_Y - camPos.y) * scale;

		for(size_t j = i + 1; j < m_Particles.size() && j < 60; j++)
		{
			float sx2 = screenCX + (m_Particles[j].m_X - camPos.x) * scale;
			float sy2 = screenCY + (m_Particles[j].m_Y - camPos.y) * scale;

			float dx = sx2 - sx1, dy = sy2 - sy1;
			float distSq = dx*dx + dy*dy;
			if(distSq < maxDistSq && distSq > 1.0f)
			{
				float dist = sqrtf(distSq);
				float alpha = (1.0f - dist / maxDist) * 0.25f *
					std::min(m_Particles[i].m_Alpha, m_Particles[j].m_Alpha);
				float avgHue = (m_Particles[i].m_Hue + m_Particles[j].m_Hue) * 0.5f;
				ImVec4 lineCol = HSVtoRGB(avgHue, 0.6f, 1.0f);
				d->AddLine(ImVec2(sx1, sy1), ImVec2(sx2, sy2),
					ColorWithAlpha(lineCol, alpha), 1.0f);
			}
		}
	}
}

void CHitParticles::RenderParticles()
{
    ANTI_DISASM_BLOCK();
	if(!ImGui::GetCurrentContext())
		return;

	if(!GameClient())
		return;

	ImDrawList *d = ImGui::GetBackgroundDrawList();
	if(!d)
		return;

	vec2 camPos = GameClient()->m_Camera.m_Center;
	if(!std::isfinite(camPos.x) || !std::isfinite(camPos.y))
		return;

	float screenCX = ImGui::GetIO().DisplaySize.x / 2.0f + m_ScreenShakeX;
	float screenCY = ImGui::GetIO().DisplaySize.y / 2.0f + m_ScreenShakeY;
	float zoom = GameClient()->m_Camera.m_Zoom;
	if(zoom < 0.001f)
		zoom = 1.0f;
	float scale = 1.0f / zoom;
	if(!std::isfinite(scale) || scale < 0.001f || scale > 100.0f)
		scale = 1.0f;
	float globalAlpha = g_Config.m_shalawaHitParticleAlpha / 100.0f;

	RenderScreenFlash(d);

	for(auto &ev : m_HitEvents)
	{
		if(!std::isfinite(ev.m_X) || !std::isfinite(ev.m_Y))
			continue;

		float sx = screenCX + (ev.m_X - camPos.x) * scale;
		float sy = screenCY + (ev.m_Y - camPos.y) * scale;

		if(!std::isfinite(sx) || !std::isfinite(sy))
			continue;

		if(ev.m_WaveCount > 0 && ev.m_RingAlpha > 0.01f)
		{
			for(int w = 0; w < ev.m_WaveCount; w++)
			{
				float waveTime = ev.m_Time - ev.m_WavePhases[w];
				if(waveTime < 0) continue;
				float waveProgress = std::clamp(waveTime / 0.5f, 0.0f, 1.0f);
				float waveAlpha = (1.0f - waveProgress) * 0.7f;
				float waveRadius = waveProgress * (50.0f + w * 15.0f) * scale;
				float hue = fmodf(ev.m_Hue + w * 0.15f, 1.0f);
				ImVec4 ringCol = HSVtoRGB(hue, 0.7f, 1.0f);
				ImU32 rc = ColorWithAlpha(ringCol, waveAlpha * globalAlpha);
				float thickness = (2.5f - waveProgress * 1.5f) * scale;
				d->AddCircle(ImVec2(sx, sy), waveRadius, rc, 32, thickness);

				if(g_Config.m_shalawaHitParticleGlow)
				{
					ImU32 glowRc = ColorWithAlpha(ringCol, waveAlpha * 0.15f * globalAlpha);
					d->AddCircle(ImVec2(sx, sy), waveRadius, glowRc, 32, thickness * 4.0f);
				}
			}
		}

		if(ev.m_FlashIntensity > 0.05f)
		{
			ImVec4 flashCol = HSVtoRGB(ev.m_Hue, 0.3f, 1.0f);
			for(int g = 4; g >= 0; g--)
			{
				float expand = (float)g * 5.0f * scale;
				float a = ev.m_FlashIntensity * (1.0f - (float)g / 4.0f) * 0.3f * globalAlpha;
				d->AddCircleFilled(ImVec2(sx, sy), (10.0f + expand) * scale,
					ColorWithAlpha(flashCol, a), 16);
			}
		}
	}

	for(auto &ai : m_Afterimages)
	{
		if(!std::isfinite(ai.m_X) || !std::isfinite(ai.m_Y))
			continue;

		float sx = screenCX + (ai.m_X - camPos.x) * scale;
		float sy = screenCY + (ai.m_Y - camPos.y) * scale;

		if(!std::isfinite(sx) || !std::isfinite(sy))
			continue;

		float drawSize = ai.m_Size * scale;
		if(drawSize < 0.3f || !std::isfinite(drawSize)) continue;

		ImVec4 col = HSVtoRGB(ai.m_Hue, 0.5f, 0.8f);
		ImU32 c = ColorWithAlpha(col, ai.m_Alpha * globalAlpha);

		switch(ai.m_Type)
		{
		case 0: d->AddCircleFilled(ImVec2(sx, sy), drawSize * 0.6f, c, 5); break;
		case 1: d->AddCircleFilled(ImVec2(sx, sy), drawSize * 0.5f, c, 8); break;
		default: d->AddCircleFilled(ImVec2(sx, sy), drawSize * 0.4f, c, 6); break;
		}
	}

	RenderConnectorLines(d, screenCX, screenCY, scale, camPos);

	for(auto &p : m_Particles)
	{
		if(!std::isfinite(p.m_X) || !std::isfinite(p.m_Y))
			continue;

		float sx = screenCX + (p.m_X - camPos.x) * scale;
		float sy = screenCY + (p.m_Y - camPos.y) * scale;

		if(!std::isfinite(sx) || !std::isfinite(sy))
			continue;

		if(sx < -80 || sx > ImGui::GetIO().DisplaySize.x + 80 ||
		   sy < -80 || sy > ImGui::GetIO().DisplaySize.y + 80)
			continue;

		float drawSize = p.m_Size * scale * p.m_ScaleAnim;
		if(drawSize < 0.3f) continue;

		ImVec4 baseCol;
		switch(p.m_Type)
		{
		case 0: baseCol = HSVtoRGB(p.m_Hue, 0.85f, 1.0f); break;
		case 1: baseCol = HSVtoRGB(p.m_Hue, 0.85f, 1.0f); break;
		case 2: baseCol = HSVtoRGB(fmodf(p.m_Hue + m_Time * 0.5f, 1.0f), 0.6f, 1.0f); break;
		case 3: baseCol = ImVec4(1.0f, 0.95f, 0.8f, 1.0f); break;
		case 4: baseCol = HSVtoRGB(fmodf(p.m_Hue + m_Time * 0.2f, 1.0f), 0.7f, 1.0f); break;
		case 5: baseCol = HSVtoRGB(fmodf(p.m_Hue + m_Time * 0.3f, 1.0f), 0.5f, 1.0f); break;
		case 6: baseCol = HSVtoRGB(p.m_Hue, 0.3f, 1.0f); break;
		case 7: baseCol = HSVtoRGB(fmodf(p.m_Hue + m_Time * 0.15f, 1.0f), 0.9f, 0.95f); break;
		default: baseCol = ImVec4(1, 1, 1, 1);
		}

		ImU32 col = ColorWithAlpha(baseCol, p.m_Alpha * globalAlpha);

		if(p.m_Trail > 0.3f)
		{
			float velLen = sqrtf(p.m_VelX * p.m_VelX + p.m_VelY * p.m_VelY);
			if(velLen > 5.0f)
			{
				float nx = -p.m_VelX / velLen;
				float ny = -p.m_VelY / velLen;
				float trailLen = std::min(velLen * 0.05f, 12.0f) * scale;
				ImU32 trailCol = ColorWithAlpha(baseCol, p.m_Alpha * 0.25f * globalAlpha);
				d->AddLine(ImVec2(sx, sy), ImVec2(sx + nx * trailLen, sy + ny * trailLen), trailCol, 1.5f * scale);

				if(g_Config.m_shalawaHitParticleGlow)
				{
					ImU32 trailGlow = ColorWithAlpha(baseCol, p.m_Alpha * 0.08f * globalAlpha);
					d->AddLine(ImVec2(sx, sy), ImVec2(sx + nx * trailLen, sy + ny * trailLen), trailGlow, 4.0f * scale);
				}
			}
		}

		if(g_Config.m_shalawaHitParticleGlow && drawSize > 1.0f)
		{
			ImU32 glowCol = ColorWithAlpha(baseCol, p.m_Alpha * 0.12f * globalAlpha);
			d->AddCircleFilled(ImVec2(sx, sy), drawSize * 2.5f, glowCol, 10);
			ImU32 glowCol2 = ColorWithAlpha(baseCol, p.m_Alpha * 0.05f * globalAlpha);
			d->AddCircleFilled(ImVec2(sx, sy), drawSize * 4.0f, glowCol2, 8);
		}

		switch(p.m_Type)
		{
		case 0:
		{
			int pts = (p.m_SubType == 0) ? 5 : 4;
			RenderStar(d, sx, sy, drawSize, p.m_Rotation, col, pts);
			break;
		}
		case 1:
			RenderHeart(d, sx, sy, drawSize, p.m_Rotation, col);
			break;
		case 2:
			d->AddCircleFilled(ImVec2(sx, sy), drawSize, col, 14);
			{
				ImU32 orbitRing = ColorWithAlpha(baseCol, p.m_Alpha * 0.35f * globalAlpha);
				d->AddCircle(ImVec2(sx, sy), drawSize * 1.6f, orbitRing, 14, 1.0f * scale);
				d->AddCircle(ImVec2(sx, sy), drawSize * 2.2f, 
					ColorWithAlpha(baseCol, p.m_Alpha * 0.15f * globalAlpha), 14, 0.7f * scale);
			}
			break;
		case 3:
			RenderSparkle(d, sx, sy, drawSize, p.m_Rotation, col);
			break;
		case 4:
		{
			ImU32 innerCol = ColorWithAlpha(ImVec4(1, 1, 1, 1), p.m_Alpha * 0.7f * globalAlpha);
			RenderDiamond(d, sx, sy, drawSize, p.m_Rotation, col, innerCol);
			break;
		}
		case 5:
			RenderCrescent(d, sx, sy, drawSize, p.m_Rotation, col);
			break;
		case 6:
			RenderFlower(d, sx, sy, drawSize, p.m_Rotation, col, 5 + p.m_SubType);
			break;
		case 7:
			RenderSwirl(d, sx, sy, drawSize, p.m_Rotation, col);
			break;
		}
	}
}

void CHitParticles::OnRender()
{
    ANTI_DISASM_BLOCK();
    if(!m_Initialized || !g_Config.m_shalawaHitParticles)

        return;
    if(!GameClient() || !Client())
        return;
    if(Client()->State() != IClient::STATE_ONLINE)
        return;
    if(!ImGui::GetCurrentContext())
        return;

    float currentTime = (float)SDL_GetTicks() / OBF_F(1000);
    m_DeltaTime = std::clamp(currentTime - m_LastTime, 0.0001f, 0.05f);
    m_LastTime = currentTime;
    m_Time += m_DeltaTime;

    ProcessHitQueue();
    UpdateParticles();
    RenderParticles();
}
