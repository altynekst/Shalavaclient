#pragma once

#include <imgui.h>
#include <vector>
#include <unordered_map>
#include <game/client/component.h>
#include <base/vmath.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>

class CTail : public CComponent
{
public:
	CTail();

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;
	virtual void OnReset() override;
	virtual void OnRender() override;

	enum ETailType
	{
		TAIL_SIMPLE = 0,
		TAIL_HEARTS,
		TAIL_STARS,
		TAIL_WEAPONS,
		TAIL_TILES,
		TAIL_CHAIN,
		TAIL_FIRE,
		TAIL_RAINBOW,
		NUM_TAIL_TYPES
	};

	enum ETileSubtype
	{
		TILESUB_HOOKABLE = 0,
		TILESUB_DEATH,
		TILESUB_UNHOOKABLE,
		TILESUB_FREEZE,
		TILESUB_UNFREEZE,
		TILESUB_DEEP_FREEZE,
		TILESUB_DEEP_UNFREEZE,
		TILESUB_TELE_IN,
		TILESUB_TELE_OUT,
		TILESUB_START,
		TILESUB_FINISH,
		NUM_TILE_SUBTYPES
	};

	enum EWeaponSubtype
	{
		WEAPONSUB_HAMMER = 0,
		WEAPONSUB_GUN,
		WEAPONSUB_SHOTGUN,
		WEAPONSUB_GRENADE,
		WEAPONSUB_LASER,
		WEAPONSUB_NINJA,
		NUM_WEAPON_SUBTYPES
	};

private:
	struct STailSegment
	{
		float m_X, m_Y;
		float m_VelX, m_VelY;
		float m_Angle;
		float m_Size;
		float m_Alpha;
	};

	struct STrailPoint
	{
		vec2 m_Pos;
		vec2 m_Top, m_Bot;
		vec2 m_Normal;
		float m_Width;
		float m_Progress;
		float m_Alpha;
		bool m_Flip;
	};

	struct STailState
	{
		std::vector<STailSegment> m_Segments;
		float m_PrevX, m_PrevY;
		bool m_Initialized;

		STailState() : m_PrevX(0), m_PrevY(0), m_Initialized(false) {}
	};

	std::unordered_map<int, STailState> m_States;

	float m_Time;
	float m_DeltaTime;
	float m_LastTime;
	bool m_Init;

	bool SafetyCheck();
	bool ValidatePlayer(int ClientId);

	void UpdateTail(int ClientId, float worldX, float worldY);

	void RenderSimpleTrail(int ClientId);
	void RenderSegmentedTail(int ClientId, float screenCX, float screenCY, float scale, vec2 camPos);

	int GetTileIndex(int subtype);
	int GetWeaponSpriteId(int subtype);

	void RenderSegmentHeart(ImDrawList *d, float sx, float sy, float size, float alpha, float angle, ImU32 col);
	void RenderSegmentStar(ImDrawList *d, float sx, float sy, float size, float alpha, float angle, ImU32 col);
	void RenderSegmentFire(ImDrawList *d, float sx, float sy, float size, float alpha, float angle, float time, ImU32 col);
	void RenderSegmentRainbow(ImDrawList *d, float sx, float sy, float size, float alpha, int index, float time);

	void DrawGlow(ImDrawList *d, float sx, float sy, float radius, ImU32 col, float glowAlpha);

	ImVec4 HSBtoRGB(float h, float s, float v);
	ImU32 SetAlpha(ImU32 col, float alpha);
	ImU32 LerpColor(ImU32 c1, ImU32 c2, float t);
	ImU32 DarkenColor(ImU32 col, float factor);
	ImU32 BrightenColor(ImU32 col, float factor);
	ImVec2 RotatePoint(float cx, float cy, float px, float py, float angle);
	float Lerp(float a, float b, float t);
};