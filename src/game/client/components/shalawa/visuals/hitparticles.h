#pragma once

#include <game/client/component.h>
#include <vector>
#include <cmath>
#include <imgui.h>
#include <mutex>
#include <queue>

struct SHitParticle
{
	float m_X, m_Y;
	float m_VelX, m_VelY;
	float m_Life;
	float m_MaxLife;
	float m_Size;
	float m_Rotation;
	float m_RotSpeed;
	float m_Alpha;
	int m_Type;
	float m_Hue;
	float m_Gravity;
	float m_Bounce;
	float m_Trail;
	float m_OrigX, m_OrigY;
	float m_OrbitRadius;
	float m_OrbitSpeed;
	float m_OrbitPhase;
	float m_PulsePhase;
	float m_FlickerSeed;
	int m_SubType;
	float m_ScaleAnim;
	float m_Drag;
	float m_SpinAccel;
	bool m_HasAfterimage;
	float m_AfterimageAlpha;
};

struct SHitEvent
{
	float m_X, m_Y;
	float m_Time;
	float m_RingProgress;
	float m_RingAlpha;
	float m_ShockwaveRadius;
	float m_FlashIntensity;
	float m_ScreenShake;
	float m_Hue;
	int m_WaveCount;
	float m_WavePhases[4];
};

class CHitParticles : public CComponent
{
public:
	CHitParticles();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;
	virtual void OnReset() override;
	virtual void OnRender() override;
	void ProcessSnapshots();
	virtual void OnNewSnapshot() override;

	void OnHammerHit(float x, float y);
    void QueueHammerHit(float x, float y);

private:
    std::mutex m_QueueMutex;
    std::queue<std::pair<float, float>> m_HitQueue;
    void ProcessHitQueue();
	void SpawnParticles(float x, float y);
	void UpdateParticles();
	void RenderParticles();
	void RenderStar(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col, int points);
	void RenderHeart(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col);
	void RenderSparkle(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col);
	void RenderRing(ImDrawList *d, float cx, float cy, float radius, float progress, ImU32 col, float thickness);
	void RenderCrescent(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col);
	void RenderDiamond(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col, ImU32 innerCol);
	void RenderCross(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col);
	void RenderFlower(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col, int petals);
	void RenderSwirl(ImDrawList *d, float x, float y, float size, float rotation, ImU32 col);
	void RenderScreenFlash(ImDrawList *d);
	void RenderConnectorLines(ImDrawList *d, float screenCX, float screenCY, float scale, vec2 camPos);

	ImVec4 HSVtoRGB(float h, float s, float v);
	ImU32 ColorWithAlpha(ImVec4 col, float alpha);
	float Lerp(float a, float b, float t);
	float RandFloat(float mn, float mx);

	struct SAfterimage {
		float m_X, m_Y, m_Size, m_Rotation, m_Alpha;
		int m_Type;
		float m_Hue;
		float m_Life;
	};
	std::vector<SAfterimage> m_Afterimages;
	float m_ScreenFlash;
	float m_ScreenShakeX, m_ScreenShakeY;



	std::vector<SHitParticle> m_Particles;
	std::vector<SHitEvent> m_HitEvents;

	float m_Time;
	float m_DeltaTime;
	float m_LastTime;
	int m_LastHammerHitTick;
	bool m_Initialized;
};