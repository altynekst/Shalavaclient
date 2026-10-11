#pragma once

#include <game/client/component.h>
#include <imgui.h>
#include <unordered_map>
#include <cmath>

struct SChinaHatState
{
	float m_BobOffset = 0.0f;
	float m_SwayAngle = 0.0f;
	float m_TiltSmooth = 0.0f;
	float m_SquishX = 1.0f;
	float m_SquishY = 1.0f;
	float m_LandBounce = 0.0f;
	float m_SpeedSmoothed = 0.0f;
	float m_PrevVelX = 0.0f;
	float m_PrevVelY = 0.0f;
	float m_BreathAnim = 0.0f;
	float m_IdleTimer = 0.0f;

	float m_CircleStep = 0.0f;
	double m_CircleSmoothY = 0.0;
	double m_CircleSmoothY2 = 0.0;

	float m_CrystalRotation = 0.0f;
	float m_RhombRotation = 0.0f;
};

class CChinaHat : public CComponent
{
public:
	CChinaHat();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;
	virtual void OnReset() override;
	virtual void OnRender() override;

	float *m_pHue[3] = {};
	float *m_pSat[3] = {};
	float *m_pBri[3] = {};

private:
	bool SafetyCheck();
	bool ValidatePlayer(int ClientId);
	void RenderForPlayer(int ClientId);

	void DrawShadow(ImDrawList *d, float x, float y, float rx, float ry, float alpha);
	void DrawCone(ImDrawList *d, float x, float y, float size, float angle,
		float squishX, float squishY, ImU32 col1, ImU32 col2, float alpha);
	void DrawConeOutline(ImDrawList *d, float x, float y, float size, float angle,
		float squishX, ImU32 col1, ImU32 col2, float alpha);
	void DrawConePattern(ImDrawList *d, float x, float y, float size, float angle,
		float squishX, float squishY, ImU32 col1, ImU32 col2, float alpha);
	void DrawShine(ImDrawList *d, float x, float y, float size, float angle, float alpha);

	void DrawGhostAura(ImDrawList *d, float cx, float cy, float radius,
		float alpha, ImU32 col1, ImU32 col2, ImU32 col3);

	void DrawCrystals(ImDrawList *d, float cx, float cy, float radius,
		float rotation, float alpha, ImU32 col1);

	void DrawCircle(ImDrawList *d, float cx, float cy, float radius, float height,
		SChinaHatState &st, float alpha, ImU32 col1, ImU32 col2);

	void DrawRhomb(ImDrawList *d, float cx, float cy, float size,
		float rotation, float alpha, ImU32 col1, ImU32 col2);

	ImVec4 HSBtoRGB(float h, float s, float v);
	ImU32 GetColor(int index);
	float Lerp(float a, float b, float t);
	double LerpD(double a, double b, double t);
	ImU32 LerpColor(ImU32 c1, ImU32 c2, float t);
	ImU32 SetAlpha(ImU32 col, float alpha);
	ImU32 MultAlpha(ImU32 col, float factor);
	ImU32 DarkenColor(ImU32 col, float factor);
	ImU32 LightenColor(ImU32 col, float factor);
	ImVec2 RotatePoint(float cx, float cy, float px, float py, float angle);
	void AddGradientQuad(ImDrawList *d, ImVec2 p1, ImVec2 p2, ImVec2 p3, ImVec2 p4,
		ImU32 c1, ImU32 c2, ImU32 c3, ImU32 c4);
	bool IsFinite(float v);
	float SafeFloat(float v, float fallback = 0.0f);
	double EaseInOutSine(double t);
	double SmoothSinAnimation(double input);

	std::unordered_map<int, SChinaHatState> m_States;
	float m_Time;
	float m_DeltaTime;
	float m_LastTime;
	bool m_Initialized;
};