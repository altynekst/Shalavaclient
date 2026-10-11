#include "tail.h"

#include <game/client/gameclient.h>
#include <game/client/components/mapimages.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <generated/client_data.h>
#include <game/mapitems.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <SDL.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>
#include <algorithm>
#include <cmath>

static const float PI = 3.14159265f;
static const float PI2 = PI * 2.0f;

CTail::CTail()
{
	ANTI_DISASM_BLOCK();
	m_Time = 0;
	m_DeltaTime = 0;
	m_LastTime = 0;
	m_Init = false;
}

void CTail::OnInit()
{
	ANTI_DISASM_BLOCK();
	m_Init = true;
	m_LastTime = (float)SDL_GetTicks() / 1000.0f;
}

void CTail::OnReset()
{
	ANTI_DISASM_BLOCK();
	m_States.clear();
}

bool CTail::SafetyCheck()
{
	ANTI_DISASM_BLOCK();
	if(!m_Init) return false;
	if(!GameClient()) return false;
	if(!Client()) return false;
	if(Client()->State() != IClient::STATE_ONLINE) return false;
	if(GameClient()->m_Snap.m_LocalClientId < 0) return false;
	if(GameClient()->m_Snap.m_LocalClientId >= MAX_CLIENTS) return false;
	return true;
}

bool CTail::ValidatePlayer(int ClientId)
{
	ANTI_DISASM_BLOCK();
	if(ClientId < 0 || ClientId >= MAX_CLIENTS) return false;
	if(!GameClient()->m_Snap.m_apPlayerInfos[ClientId]) return false;
	if(!GameClient()->m_Snap.m_aCharacters[ClientId].m_Active) return false;
	if(!GameClient()->m_aClients[ClientId].m_Active) return false;
	return true;
}

float CTail::Lerp(float a, float b, float t) { return a + (b - a) * std::clamp(t, 0.0f, 1.0f); }

ImVec4 CTail::HSBtoRGB(float h, float s, float v)
{
	ANTI_DISASM_BLOCK();
	h = fmodf(h, 1.0f);
	if(h < 0) h += 1.0f;
	float c = v * s;
	float x = c * (1.0f - fabsf(fmodf(h * 6.0f, 2.0f) - 1.0f));
	float m = v - c;
	float r, g, b;
	if(h < 1.0f/6) { r=c; g=x; b=0; }
	else if(h < 2.0f/6) { r=x; g=c; b=0; }
	else if(h < 3.0f/6) { r=0; g=c; b=x; }
	else if(h < 4.0f/6) { r=0; g=x; b=c; }
	else if(h < 5.0f/6) { r=x; g=0; b=c; }
	else { r=c; g=0; b=x; }
	return ImVec4(r+m, g+m, b+m, 1.0f);
}

ImU32 CTail::SetAlpha(ImU32 col, float alpha)
{
	ANTI_DISASM_BLOCK();
	ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
	c.w = std::clamp(alpha, 0.0f, 1.0f);
	return ImGui::ColorConvertFloat4ToU32(c);
}

ImU32 CTail::LerpColor(ImU32 c1, ImU32 c2, float t)
{
	ANTI_DISASM_BLOCK();
	t = std::clamp(t, 0.0f, 1.0f);
	ImVec4 a = ImGui::ColorConvertU32ToFloat4(c1);
	ImVec4 b = ImGui::ColorConvertU32ToFloat4(c2);
	return ImGui::ColorConvertFloat4ToU32(ImVec4(
		a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t, a.w+(b.w-a.w)*t));
}

ImU32 CTail::DarkenColor(ImU32 col, float factor)
{
	ANTI_DISASM_BLOCK();
	ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
	c.x *= factor; c.y *= factor; c.z *= factor;
	return ImGui::ColorConvertFloat4ToU32(c);
}

ImU32 CTail::BrightenColor(ImU32 col, float factor)
{
	ANTI_DISASM_BLOCK();
	ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
	c.x = std::min(c.x * factor, 1.0f);
	c.y = std::min(c.y * factor, 1.0f);
	c.z = std::min(c.z * factor, 1.0f);
	return ImGui::ColorConvertFloat4ToU32(c);
}

ImVec2 CTail::RotatePoint(float cx, float cy, float px, float py, float angle)
{
	ANTI_DISASM_BLOCK();
	float cs = cosf(angle), sn = sinf(angle);
	float dx = px - cx, dy = py - cy;
	return ImVec2(cx + dx*cs - dy*sn, cy + dx*sn + dy*cs);
}

void CTail::DrawGlow(ImDrawList *d, float sx, float sy, float radius, ImU32 col, float glowAlpha)
{
	ANTI_DISASM_BLOCK();
	for(int i = 3; i >= 1; i--)
	{
		float r = radius * (1.0f + i * 0.4f);
		float a = glowAlpha / (float)(i * i);
		d->AddCircleFilled(ImVec2(sx, sy), r, SetAlpha(col, a), 12);
	}
}

void CTail::UpdateTail(int ClientId, float worldX, float worldY)
{
	ANTI_DISASM_BLOCK();
	STailState &st = m_States[ClientId];

	int segCount = std::clamp(g_Config.m_shalawaTailLength, 3, 60);
	float segDist = std::clamp((float)g_Config.m_shalawaTailSpacing, 2.0f, 20.0f);
	float stiffness = std::clamp((float)g_Config.m_shalawaTailStiffness / 100.0f, 0.01f, 1.0f);
	float gravity = (float)g_Config.m_shalawaTailGravity / 10.0f;
	float damping = 0.92f;

	if(!st.m_Initialized || (int)st.m_Segments.size() != segCount)
	{
		st.m_Segments.resize(segCount);
		for(int i = 0; i < segCount; i++)
		{
			st.m_Segments[i].m_X = worldX;
			st.m_Segments[i].m_Y = worldY + i * segDist;
			st.m_Segments[i].m_VelX = 0;
			st.m_Segments[i].m_VelY = 0;
			st.m_Segments[i].m_Angle = 0;
			st.m_Segments[i].m_Size = 1.0f;
			st.m_Segments[i].m_Alpha = 1.0f;
		}
		st.m_PrevX = worldX;
		st.m_PrevY = worldY;
		st.m_Initialized = true;
		return;
	}

	float anchorX = worldX;
	float anchorY = worldY + 16.0f;

	st.m_Segments[0].m_X = anchorX;
	st.m_Segments[0].m_Y = anchorY;
	st.m_Segments[0].m_VelX = 0;
	st.m_Segments[0].m_VelY = 0;

	float velX = (worldX - st.m_PrevX) / std::max(m_DeltaTime, 0.001f);

	for(int i = 1; i < segCount; i++)
	{
		STailSegment &seg = st.m_Segments[i];

		float windInfluence = (float)i / (float)segCount * 0.3f;
		seg.m_VelX -= velX * windInfluence * m_DeltaTime;
		seg.m_VelY += gravity * m_DeltaTime * 60.0f;

		seg.m_VelX *= damping;
		seg.m_VelY *= damping;

		seg.m_X += seg.m_VelX * m_DeltaTime;
		seg.m_Y += seg.m_VelY * m_DeltaTime;

		STailSegment &prev = st.m_Segments[i-1];
		float dx = seg.m_X - prev.m_X;
		float dy = seg.m_Y - prev.m_Y;
		float dist = sqrtf(dx*dx + dy*dy);

		if(dist > 0.001f)
		{
			float nx = dx / dist;
			float ny = dy / dist;
			float diff = dist - segDist;
			float correction = diff * stiffness;

			seg.m_X -= nx * correction;
			seg.m_Y -= ny * correction;
			seg.m_VelX -= nx * correction * 2.0f;
			seg.m_VelY -= ny * correction * 2.0f;
		}

		seg.m_Angle = atan2f(dy, dx);

		float t = (float)i / (float)(segCount - 1);
		float sizeMul = g_Config.m_shalawaTailSize / 100.0f;

		if(g_Config.m_shalawaTailTaper)
			seg.m_Size = (1.0f - t * 0.7f) * sizeMul;
		else
			seg.m_Size = sizeMul;

		seg.m_Alpha = 1.0f - t * 0.4f;
	}

	st.m_PrevX = worldX;
	st.m_PrevY = worldY;
}

int CTail::GetTileIndex(int subtype)
{
	ANTI_DISASM_BLOCK();
	switch(subtype)
	{
	case TILESUB_HOOKABLE:     return TILE_SOLID;
	case TILESUB_DEATH:        return TILE_DEATH;
	case TILESUB_UNHOOKABLE:   return TILE_NOHOOK;
	case TILESUB_FREEZE:       return TILE_FREEZE;
	case TILESUB_UNFREEZE:     return TILE_UNFREEZE;
	case TILESUB_DEEP_FREEZE:  return TILE_DFREEZE;
	case TILESUB_DEEP_UNFREEZE:return TILE_DUNFREEZE;
	case TILESUB_TELE_IN:      return TILE_TELEIN;
	case TILESUB_TELE_OUT:     return TILE_TELEOUT;
	case TILESUB_START:        return TILE_START;
	case TILESUB_FINISH:       return TILE_FINISH;
	default:                   return TILE_SOLID;
	}
}

int CTail::GetWeaponSpriteId(int subtype)
{
	ANTI_DISASM_BLOCK();
	switch(subtype)
	{
	case WEAPONSUB_HAMMER:  return SPRITE_WEAPON_HAMMER_BODY;
	case WEAPONSUB_GUN:     return SPRITE_WEAPON_GUN_BODY;
	case WEAPONSUB_SHOTGUN: return SPRITE_WEAPON_SHOTGUN_BODY;
	case WEAPONSUB_GRENADE: return SPRITE_WEAPON_GRENADE_BODY;
	case WEAPONSUB_LASER:   return SPRITE_WEAPON_LASER_BODY;
	case WEAPONSUB_NINJA:   return SPRITE_WEAPON_NINJA_BODY;
	default:                return SPRITE_WEAPON_GUN_BODY;
	}
}

void CTail::RenderSimpleTrail(int ClientId)
{
	ANTI_DISASM_BLOCK();
	STailState &st = m_States[ClientId];
	if(!st.m_Initialized || (int)st.m_Segments.size() < 3) return;

	int segCount = (int)st.m_Segments.size();
	float globalAlpha = std::clamp((float)g_Config.m_shalawaTailAlpha / 100.0f, 0.0f, 1.0f);
	float baseWidth = g_Config.m_shalawaTailSize / 100.0f * 8.0f;

	ColorRGBA col(
		g_Config.m_shalawaTailColorR / 255.0f,
		g_Config.m_shalawaTailColorG / 255.0f,
		g_Config.m_shalawaTailColorB / 255.0f,
		globalAlpha);

	std::vector<STrailPoint> trail;
	trail.resize(segCount);

	for(int i = 0; i < segCount; i++)
	{
		STrailPoint &pt = trail[i];
		pt.m_Pos = vec2(st.m_Segments[i].m_X, st.m_Segments[i].m_Y);
		pt.m_Progress = (float)i / (float)(segCount - 1);
		pt.m_Alpha = st.m_Segments[i].m_Alpha * globalAlpha;
		pt.m_Flip = false;

		if(g_Config.m_shalawaTailTaper)
			pt.m_Width = baseWidth * (1.0f - pt.m_Progress);
		else
			pt.m_Width = baseWidth;
	}

	for(int i = 0; i < segCount; i++)
	{
		STrailPoint &pt = trail[i];
		vec2 PrevPos, Pos = pt.m_Pos, NextPos;

		if(i == 0)
		{
			vec2 Dir = normalize(trail[1].m_Pos - Pos);
			PrevPos = Pos - Dir;
		}
		else
			PrevPos = trail[i-1].m_Pos;

		if(i == segCount - 1)
		{
			vec2 Dir = normalize(Pos - trail[i-1].m_Pos);
			NextPos = Pos + Dir;
		}
		else
			NextPos = trail[i+1].m_Pos;

		vec2 NextDir = normalize(NextPos - Pos);
		vec2 PrevDir = normalize(Pos - PrevPos);

		vec2 Normal = vec2(-PrevDir.y, PrevDir.x);
		pt.m_Normal = Normal;
		vec2 Tangent = normalize(NextDir + PrevDir);
		if(length(Tangent) < 0.001f)
			Tangent = Normal;

		vec2 PerpVec = vec2(-Tangent.y, Tangent.x);
		float Width = pt.m_Width;

		float d = dot(Normal, PerpVec);
		if(fabsf(d) < 0.001f) d = 0.001f;
		float ScaledWidth = Width / d;
		float TopScaled = ScaledWidth;
		float BotScaled = ScaledWidth;

		if(dot(PrevDir, Tangent) > 0.0f)
			TopScaled = std::min(Width * 3.0f, TopScaled);
		else
			BotScaled = std::min(Width * 3.0f, BotScaled);

		vec2 Top = Pos + PerpVec * TopScaled;
		vec2 Bot = Pos - PerpVec * BotScaled;
		pt.m_Top = Top;
		pt.m_Bot = Bot;

		if(dot(PrevDir, NextDir) < -0.25f)
		{
			Top = Pos + Tangent * Width;
			Bot = Pos - Tangent * Width;

			float Det = PrevDir.x * NextDir.y - PrevDir.y * NextDir.x;
			if(Det >= 0.0f)
			{
				pt.m_Top = Top;
				pt.m_Bot = Bot;
			}
			else
			{
				pt.m_Top = Bot;
				pt.m_Bot = Top;
			}
			if(i > 0)
				pt.m_Flip = true;
		}
	}

	Graphics()->TextureClear();

	bool lineMode = (baseWidth < 0.5f);
	if(lineMode)
		Graphics()->LinesBegin();
	else
		Graphics()->QuadsBegin();

	for(int i = 0; i < segCount - 1; i++)
	{
		const STrailPoint &Part = trail[i];
		const STrailPoint &NextPart = trail[i+1];

		float Dist = distance(Part.m_Pos, NextPart.m_Pos);
		if(i > 0)
		{
			float PrevDist = distance(trail[i-1].m_Pos, Part.m_Pos);
			if(fabsf(Dist - PrevDist) > 120.0f)
				continue;
		}
		if(i < segCount - 2)
		{
			float NextDist = distance(NextPart.m_Pos, trail[i+2].m_Pos);
			if(fabsf(Dist - NextDist) > 120.0f)
				continue;
		}

		if(lineMode)
		{
			ColorRGBA lineCol = col;
			lineCol.a = Part.m_Alpha;
			Graphics()->SetColor(lineCol);
			IGraphics::CLineItem LineItem(Part.m_Pos.x, Part.m_Pos.y, NextPart.m_Pos.x, NextPart.m_Pos.y);
			Graphics()->LinesDraw(&LineItem, 1);
		}
		else
		{
			vec2 Top, Bot;
			if(Part.m_Flip)
			{
				Top = Part.m_Bot;
				Bot = Part.m_Top;
			}
			else
			{
				Top = Part.m_Top;
				Bot = Part.m_Bot;
			}

			ColorRGBA col1 = col;
			col1.a = Part.m_Alpha;
			if(g_Config.m_shalawaTailTrail)
				col1.a *= (1.0f - Part.m_Progress);

			ColorRGBA col2 = col;
			col2.a = NextPart.m_Alpha;
			if(g_Config.m_shalawaTailTrail)
				col2.a *= (1.0f - NextPart.m_Progress);

			Graphics()->SetColor4(col2, col2, col1, col1);
			IGraphics::CFreeformItem FreeformItem(NextPart.m_Top, NextPart.m_Bot, Top, Bot);
			Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
		}
	}

	if(lineMode)
		Graphics()->LinesEnd();
	else
		Graphics()->QuadsEnd();
}

void CTail::RenderSegmentedTail(int ClientId, float screenCX, float screenCY, float scale, vec2 camPos)
{
	ANTI_DISASM_BLOCK();
	STailState &st = m_States[ClientId];
	if(!st.m_Initialized || st.m_Segments.empty()) return;

	int type = std::clamp(g_Config.m_shalawaTailType, 0, (int)NUM_TAIL_TYPES - 1);
	float globalAlpha = std::clamp((float)g_Config.m_shalawaTailAlpha / 100.0f, 0.0f, 1.0f);

	int segCount = (int)st.m_Segments.size();

	if(type == TAIL_TILES)
	{
		int tileSubtype = std::clamp(g_Config.m_shalawaTailTileType, 0, (int)NUM_TILE_SUBTYPES - 1);
		int tileIndex = GetTileIndex(tileSubtype);
		float tileSize = g_Config.m_shalawaTailSize / 100.0f * 24.0f;

		IGraphics::CTextureHandle Tex = GameClient()->m_MapImages.GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH);

		Graphics()->TextureSet(Tex);
		Graphics()->QuadsBegin();

		for(int i = segCount - 1; i >= 1; i--)
		{
			STailSegment &seg = st.m_Segments[i];
			float drawAlpha = seg.m_Alpha * globalAlpha;
			if(drawAlpha < 0.01f) continue;

			float s = tileSize * seg.m_Size;

			Graphics()->SetColor(1.0f, 1.0f, 1.0f, drawAlpha);

			int tx = tileIndex % 16;
			int ty = tileIndex / 16;
			float x0 = tx / 16.0f;
			float y0 = ty / 16.0f;
			float x1 = (tx + 1) / 16.0f;
			float y1 = (ty + 1) / 16.0f;

			Graphics()->QuadsSetSubsetFree(x0, y0, x1, y0, x1, y1, x0, y1);

			IGraphics::CQuadItem QuadItem(seg.m_X - s * 0.5f, seg.m_Y - s * 0.5f, s, s);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
		}

		Graphics()->QuadsEnd();
		return;
	}

	if(type == TAIL_WEAPONS)
	{
		int weaponSub = std::clamp(g_Config.m_shalawaTailTileType, 0, (int)NUM_WEAPON_SUBTYPES - 1);
		int spriteId = GetWeaponSpriteId(weaponSub);
		float weaponSize = g_Config.m_shalawaTailSize / 100.0f * 40.0f;

		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
		Graphics()->QuadsBegin();

		for(int i = segCount - 1; i >= 1; i--)
		{
			STailSegment &seg = st.m_Segments[i];
			float drawAlpha = seg.m_Alpha * globalAlpha;
			if(drawAlpha < 0.01f) continue;

			float s = weaponSize * seg.m_Size;

			Graphics()->SetColor(1.0f, 1.0f, 1.0f, drawAlpha);
			Graphics()->QuadsSetRotation(seg.m_Angle);
			Graphics()->SelectSprite(spriteId);
			Graphics()->DrawSprite(seg.m_X, seg.m_Y, s);
		}

		Graphics()->QuadsSetRotation(0);
		Graphics()->QuadsEnd();
		return;
	}

	ImDrawList *d = ImGui::GetBackgroundDrawList();
	if(!d) return;

	ImU32 col1 = IM_COL32(
		g_Config.m_shalawaTailColorR,
		g_Config.m_shalawaTailColorG,
		g_Config.m_shalawaTailColorB, 255);

	if(g_Config.m_shalawaTailTrail && segCount >= 2 && (type == TAIL_HEARTS || type == TAIL_STARS || type == TAIL_FIRE || type == TAIL_RAINBOW))
	{
		for(int i = 0; i < segCount - 1; i++)
		{
			float sx1 = screenCX + (st.m_Segments[i].m_X - camPos.x) * scale;
			float sy1 = screenCY + (st.m_Segments[i].m_Y - camPos.y) * scale;
			float sx2 = screenCX + (st.m_Segments[i+1].m_X - camPos.x) * scale;
			float sy2 = screenCY + (st.m_Segments[i+1].m_Y - camPos.y) * scale;

			float t = (float)i / (float)(segCount - 1);
			float lineAlpha = globalAlpha * (1.0f - t * 0.6f) * 0.4f;
			float lineW = (1.0f - t * 0.5f) * 2.0f * scale * st.m_Segments[i].m_Size;

			ImU32 lineCol;
			if(type == TAIL_RAINBOW)
			{
				float hue = fmodf(m_Time * 0.3f + t * 0.5f, 1.0f);
				ImVec4 rgb = HSBtoRGB(hue, 0.85f, 1.0f);
				lineCol = ImGui::ColorConvertFloat4ToU32(ImVec4(rgb.x, rgb.y, rgb.z, lineAlpha));
			}
			else
				lineCol = SetAlpha(col1, lineAlpha);

			d->AddLine(ImVec2(sx1, sy1), ImVec2(sx2, sy2), lineCol, lineW);
		}
	}

	for(int i = segCount - 1; i >= 1; i--)
	{
		STailSegment &seg = st.m_Segments[i];

		float sx = screenCX + (seg.m_X - camPos.x) * scale;
		float sy = screenCY + (seg.m_Y - camPos.y) * scale;

		if(sx < -150 || sx > ImGui::GetIO().DisplaySize.x + 150 ||
		   sy < -150 || sy > ImGui::GetIO().DisplaySize.y + 150)
			continue;

		float drawSize = seg.m_Size * scale;
		float drawAlpha = seg.m_Alpha * globalAlpha;

		if(drawAlpha < 0.01f || drawSize < 0.1f) continue;

		switch(type)
		{
		case TAIL_HEARTS:
			RenderSegmentHeart(d, sx, sy, drawSize, drawAlpha, seg.m_Angle, col1);
			break;
		case TAIL_STARS:
			RenderSegmentStar(d, sx, sy, drawSize, drawAlpha, seg.m_Angle + m_Time * 2.0f, col1);
			break;
		case TAIL_FIRE:
			RenderSegmentFire(d, sx, sy, drawSize, drawAlpha, seg.m_Angle, m_Time + i * 0.2f, col1);
			break;
		case TAIL_RAINBOW:
			RenderSegmentRainbow(d, sx, sy, drawSize, drawAlpha, i, m_Time);
			break;
		}
	}
}

void CTail::RenderSegmentHeart(ImDrawList *d, float sx, float sy, float size, float alpha, float angle, ImU32 col)
{
	ANTI_DISASM_BLOCK();
	float pulse = sinf(m_Time * 4.0f + sx * 0.05f) * 0.12f + 1.0f;
	float s = 5.0f * size * pulse;
	col = SetAlpha(col, alpha);

	DrawGlow(d, sx, sy, s * 0.8f, col, alpha * 0.15f);

	int segments = 24;
	std::vector<ImVec2> pts;
	pts.reserve(segments);
	for(int i = 0; i < segments; i++)
	{
		float t = (float)i / (float)(segments-1) * PI2;
		float hx = 16.0f * sinf(t) * sinf(t) * sinf(t);
		float hy = -(13.0f*cosf(t) - 5.0f*cosf(2*t) - 2.0f*cosf(3*t) - cosf(4*t));
		float sc = s / 17.0f;
		float rx = hx * cosf(angle) - hy * sinf(angle);
		float ry = hx * sinf(angle) + hy * cosf(angle);
		pts.push_back(ImVec2(sx + rx*sc, sy + ry*sc));
	}
	if(pts.size() >= 3)
	{
		d->AddConvexPolyFilled(pts.data(), (int)pts.size(), col);
		ImU32 highlight = SetAlpha(BrightenColor(col, 1.5f), alpha * 0.35f);
		float hlx = sx - s * 0.2f * cosf(angle) - s * 0.15f * sinf(angle);
		float hly = sy - s * 0.2f * sinf(angle) + s * 0.15f * cosf(angle);
		d->AddCircleFilled(ImVec2(hlx, hly), s * 0.15f, highlight, 8);
	}
}

void CTail::RenderSegmentStar(ImDrawList *d, float sx, float sy, float size, float alpha, float angle, ImU32 col)
{
	ANTI_DISASM_BLOCK();
	col = SetAlpha(col, alpha);
	float outerR = 5.5f * size;
	float innerR = outerR * 0.38f;
	int points = 5;
	int totalPts = points * 2;

	DrawGlow(d, sx, sy, outerR, col, alpha * 0.15f);

	std::vector<ImVec2> starPts;
	starPts.reserve(totalPts);
	for(int i = 0; i < totalPts; i++)
	{
		float a = angle + (float)i / (float)totalPts * PI2 - PI * 0.5f;
		float r = (i % 2 == 0) ? outerR : innerR;
		starPts.push_back(ImVec2(sx + cosf(a)*r, sy + sinf(a)*r));
	}

	for(int i = 0; i < totalPts; i++)
	{
		int next = (i + 1) % totalPts;
		d->AddTriangleFilled(ImVec2(sx, sy), starPts[i], starPts[next], col);
	}

	d->AddCircleFilled(ImVec2(sx, sy), innerR * 0.5f, SetAlpha(BrightenColor(col, 1.6f), alpha * 0.6f), 8);
}

void CTail::RenderSegmentFire(ImDrawList *d, float sx, float sy, float size, float alpha, float angle, float time, ImU32 col)
{
	ANTI_DISASM_BLOCK();
	float flicker1 = sinf(time * 18.0f + sx * 0.12f) * 0.25f + 0.75f;
	float flicker2 = cosf(time * 13.0f + sy * 0.1f) * 0.2f + 0.8f;
	float r = 5.0f * size * flicker1;

	ImVec4 core = ImVec4(1.0f, 0.98f, 0.6f, alpha);
	ImVec4 mid = ImVec4(1.0f, 0.6f, 0.1f, alpha * 0.8f);
	ImVec4 outer = ImVec4(1.0f, 0.2f, 0.02f, alpha * 0.4f);
	ImVec4 smoke = ImVec4(0.3f, 0.3f, 0.3f, alpha * 0.1f);

	d->AddCircleFilled(ImVec2(sx, sy), r * 2.5f, ImGui::ColorConvertFloat4ToU32(
		ImVec4(outer.x, outer.y, outer.z, outer.w * 0.15f)), 10);
	d->AddCircleFilled(ImVec2(sx, sy), r * 1.8f * flicker2, ImGui::ColorConvertFloat4ToU32(outer), 10);
	d->AddCircleFilled(ImVec2(sx, sy), r * 1.1f, ImGui::ColorConvertFloat4ToU32(mid), 10);
	d->AddCircleFilled(ImVec2(sx, sy), r * 0.45f, ImGui::ColorConvertFloat4ToU32(core), 8);

	for(int i = 0; i < 5; i++)
	{
		float a = time * 10.0f + i * 1.7f + sx * 0.04f;
		float ox = sinf(a) * r * 0.8f;
		float oy = cosf(a * 1.4f) * r * 0.5f - r * 0.7f;
		float sparkR = r * 0.18f * (sinf(a * 2.5f) * 0.3f + 0.7f);

		float sparkHue = fmodf(0.05f + sinf(a) * 0.05f, 1.0f);
		ImVec4 sparkRGB = HSBtoRGB(sparkHue, 0.9f, 1.0f);
		ImU32 sparkCol = ImGui::ColorConvertFloat4ToU32(ImVec4(sparkRGB.x, sparkRGB.y, sparkRGB.z, alpha * 0.7f));

		d->AddCircleFilled(ImVec2(sx + ox, sy + oy), sparkR, sparkCol, 5);
	}

	for(int i = 0; i < 2; i++)
	{
		float a = time * 4.0f + i * 3.5f;
		float smokeX = sinf(a) * r * 0.4f;
		float smokeY = -r * 1.5f - i * r * 0.8f + sinf(a * 0.7f) * r * 0.2f;
		float smokeR = r * 0.5f * (0.5f + sinf(a * 0.3f) * 0.3f);
		d->AddCircleFilled(ImVec2(sx + smokeX, sy + smokeY), smokeR, ImGui::ColorConvertFloat4ToU32(smoke), 6);
	}
}

void CTail::RenderSegmentRainbow(ImDrawList *d, float sx, float sy, float size, float alpha, int index, float time)
{
	ANTI_DISASM_BLOCK();
	float hue = fmodf(time * 0.3f + (float)index * 0.08f, 1.0f);
	ImVec4 rgb = HSBtoRGB(hue, 0.9f, 1.0f);
	ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(rgb.x, rgb.y, rgb.z, alpha));

	float r = 4.5f * size;
	float pulse = sinf(time * 5.0f + index * 0.5f) * 0.1f + 1.0f;
	r *= pulse;

	DrawGlow(d, sx, sy, r, col, alpha * 0.2f);
	d->AddCircleFilled(ImVec2(sx, sy), r, col, 12);

	ImVec4 rgb2 = HSBtoRGB(fmodf(hue + 0.5f, 1.0f), 0.9f, 1.0f);
	ImU32 innerCol = ImGui::ColorConvertFloat4ToU32(ImVec4(rgb2.x, rgb2.y, rgb2.z, alpha * 0.3f));
	d->AddCircleFilled(ImVec2(sx, sy), r * 0.4f, innerCol, 8);
}

void CTail::OnRender()
{
	ANTI_DISASM_BLOCK();
	if(!g_Config.m_shalawaTail) return;
	if(!SafetyCheck()) return;

	float currentTime = (float)SDL_GetTicks() / OBF_F(1000);
	m_DeltaTime = std::clamp(currentTime - m_LastTime, 0.0001f, 0.05f);
	m_LastTime = currentTime;
	m_Time += m_DeltaTime;

	if(!std::isfinite(m_Time)) m_Time = 0.0f;
	if(m_Time > OBF_F(100000)) m_Time = OBF_F(0);
	if(m_States.size() > (size_t)(MAX_CLIENTS * 2)) m_States.clear();

	int localId = GameClient()->m_Snap.m_LocalClientId;
	if(localId < 0 || localId >= MAX_CLIENTS) return;
	if(!ValidatePlayer(localId)) return;

	vec2 renderPos = GameClient()->m_aClients[localId].m_RenderPos;
	if(!std::isfinite(renderPos.x) || !std::isfinite(renderPos.y)) return;

	UpdateTail(localId, renderPos.x, renderPos.y);

	int type = std::clamp(g_Config.m_shalawaTailType, 0, (int)NUM_TAIL_TYPES - 1);

	if(type == TAIL_SIMPLE)
	{
		RenderSimpleTrail(localId);
		return;
	}

	if(type == TAIL_TILES || type == TAIL_WEAPONS)
	{
		float zoom = GameClient()->m_Camera.m_Zoom;
		if(!std::isfinite(zoom) || zoom <= 0.001f) zoom = 1.0f;
		float scale = 1.0f / zoom;

		vec2 cam = GameClient()->m_Camera.m_Center;

        float screenX0 = 0, screenY0 = 0, screenW = 0, screenH = 0;
        Graphics()->GetScreen(&screenX0, &screenY0, &screenW, &screenH);

		RenderSegmentedTail(localId, 0, 0, scale, cam);
		return;
	}

	if(!ImGui::GetCurrentContext()) return;
	ImDrawList *d = ImGui::GetBackgroundDrawList();
	if(!d) return;

	float zoom = GameClient()->m_Camera.m_Zoom;
	if(!std::isfinite(zoom) || zoom <= 0.001f) zoom = 1.0f;
	float scale = 1.0f / zoom;

	ImVec2 ds = ImGui::GetIO().DisplaySize;
	if(ds.x < 1 || ds.y < 1) return;

	vec2 cam = GameClient()->m_Camera.m_Center;
	float screenCX = ds.x / 2.0f;
	float screenCY = ds.y / 2.0f;

	RenderSegmentedTail(localId, screenCX, screenCY, scale, cam);
}