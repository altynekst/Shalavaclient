#include "chinahat.h"

#include <game/client/gameclient.h>
#include <engine/shared/config.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <SDL.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>
#include <algorithm>
#include <cmath>

static const int CONE_SEGS = OI(120);
static const float PI = OFR(314159265,100000000);
static const float PI2 = PI * OBF_F(2);

CChinaHat::CChinaHat()
{
    ANTI_DISASM_BLOCK();
	m_Time = OBF_F(0);
	m_DeltaTime = OBF_F(0);
	m_LastTime = OBF_F(0);
	m_Initialized = false;
}

void CChinaHat::OnInit()
{
    ANTI_DISASM_BLOCK();
	m_Initialized = true;
	m_LastTime = (float)SDL_GetTicks() / 1000.0f;
}

void CChinaHat::OnReset()
{
    ANTI_DISASM_BLOCK();
	m_States.clear();
}


bool CChinaHat::IsFinite(float v) { return std::isfinite(v); }
float CChinaHat::SafeFloat(float v, float fallback) { return IsFinite(v) ? v : fallback; }

bool CChinaHat::SafetyCheck()
{
    ANTI_DISASM_BLOCK();
	if(!m_Initialized) return false;

	if(!GameClient()) return false;
	if(!Client()) return false;
	if(Client()->State() != IClient::STATE_ONLINE) return false;
	if(GameClient()->m_Snap.m_LocalClientId < 0) return false;
	if(GameClient()->m_Snap.m_LocalClientId >= MAX_CLIENTS) return false;
	return true;
}

bool CChinaHat::ValidatePlayer(int ClientId)
{
    ANTI_DISASM_BLOCK();
	if(ClientId < 0 || ClientId >= MAX_CLIENTS) return false;

	if(!GameClient()->m_Snap.m_apPlayerInfos[ClientId]) return false;
	if(!GameClient()->m_Snap.m_aCharacters[ClientId].m_Active) return false;
	if(!GameClient()->m_aClients[ClientId].m_Active) return false;
	return true;
}

float CChinaHat::Lerp(float a, float b, float t) { return a + (b - a) * std::clamp(t, 0.0f, 1.0f); }
double CChinaHat::LerpD(double a, double b, double t) { return a + (b - a) * std::clamp(t, 0.0, 1.0); }

double CChinaHat::EaseInOutSine(double t) { return -(cos(PI * t) - 1.0) / 2.0; }
double CChinaHat::SmoothSinAnimation(double input) { return EaseInOutSine((sin(input) + 1.0) / 2.0); }

ImVec4 CChinaHat::HSBtoRGB(float h, float s, float v)
{
	h = fmodf(h, 1.0f);
	if(h < 0) h += 1.0f;
	float c = v * s;
	float x = c * (1.0f - fabsf(fmodf(h * 6.0f, 2.0f) - 1.0f));
	float m = v - c;
	float r, g, b;
	if(h < 1.0f / 6) { r = c; g = x; b = 0; }
	else if(h < 2.0f / 6) { r = x; g = c; b = 0; }
	else if(h < 3.0f / 6) { r = 0; g = c; b = x; }
	else if(h < 4.0f / 6) { r = 0; g = x; b = c; }
	else if(h < 5.0f / 6) { r = x; g = 0; b = c; }
	else { r = c; g = 0; b = x; }
	return ImVec4(r + m, g + m, b + m, 1.0f);
}

ImU32 CChinaHat::GetColor(int index)
{
    ANTI_DISASM_BLOCK();
	index = std::clamp(index, 0, 2);

	if(m_pHue[index] && m_pSat[index] && m_pBri[index])
	{
		ImVec4 c = HSBtoRGB(*m_pHue[index], *m_pSat[index], *m_pBri[index]);
		return IM_COL32((int)(c.x * 255), (int)(c.y * 255), (int)(c.z * 255), 255);
	}
	switch(index)
	{
	case 0: return IM_COL32(g_Config.m_shalawaChinaHatColor1R, g_Config.m_shalawaChinaHatColor1G, g_Config.m_shalawaChinaHatColor1B, 255);
	case 1: return IM_COL32(g_Config.m_shalawaChinaHatColor2R, g_Config.m_shalawaChinaHatColor2G, g_Config.m_shalawaChinaHatColor2B, 255);
	case 2: return IM_COL32(g_Config.m_shalawaChinaHatColor3R, g_Config.m_shalawaChinaHatColor3G, g_Config.m_shalawaChinaHatColor3B, 255);
	}
	return IM_COL32(255, 50, 100, 255);
}

ImU32 CChinaHat::LerpColor(ImU32 c1, ImU32 c2, float t)
{
	t = std::clamp(t, 0.0f, 1.0f);
	ImVec4 a = ImGui::ColorConvertU32ToFloat4(c1);
	ImVec4 b = ImGui::ColorConvertU32ToFloat4(c2);
	return ImGui::ColorConvertFloat4ToU32(ImVec4(
		a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
		a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t));
}

ImU32 CChinaHat::SetAlpha(ImU32 col, float alpha)
{
	ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
	c.w = std::clamp(alpha, 0.0f, 1.0f);
	return ImGui::ColorConvertFloat4ToU32(c);
}

ImU32 CChinaHat::MultAlpha(ImU32 col, float factor)
{
	ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
	c.w = std::clamp(c.w * factor, 0.0f, 1.0f);
	return ImGui::ColorConvertFloat4ToU32(c);
}

ImU32 CChinaHat::DarkenColor(ImU32 col, float factor)
{
	ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
	c.x *= factor; c.y *= factor; c.z *= factor;
	return ImGui::ColorConvertFloat4ToU32(c);
}

ImU32 CChinaHat::LightenColor(ImU32 col, float factor)
{
	ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
	c.x = std::min(c.x * factor, 1.0f);
	c.y = std::min(c.y * factor, 1.0f);
	c.z = std::min(c.z * factor, 1.0f);
	return ImGui::ColorConvertFloat4ToU32(c);
}

ImVec2 CChinaHat::RotatePoint(float cx, float cy, float px, float py, float angle)
{
	float cs = cosf(angle), sn = sinf(angle);
	float dx = px - cx, dy = py - cy;
	return ImVec2(cx + dx * cs - dy * sn, cy + dx * sn + dy * cs);
}

void CChinaHat::AddGradientQuad(ImDrawList *d, ImVec2 p1, ImVec2 p2, ImVec2 p3, ImVec2 p4,
	ImU32 c1, ImU32 c2, ImU32 c3, ImU32 c4)
{
	if(!d || d->CmdBuffer.empty()) return;
	if(!d->_Data) return;
	if(!std::isfinite(p1.x) || !std::isfinite(p1.y)) return;
	if(!std::isfinite(p2.x) || !std::isfinite(p2.y)) return;
	if(!std::isfinite(p3.x) || !std::isfinite(p3.y)) return;
	if(!std::isfinite(p4.x) || !std::isfinite(p4.y)) return;
	float limit = 16384.0f;
	if(fabsf(p1.x) > limit || fabsf(p1.y) > limit) return;
	if(fabsf(p2.x) > limit || fabsf(p2.y) > limit) return;
	if(fabsf(p3.x) > limit || fabsf(p3.y) > limit) return;
	if(fabsf(p4.x) > limit || fabsf(p4.y) > limit) return;
	const ImVec2 uv = d->_Data->TexUvWhitePixel;
	d->PrimReserve(6, 4);
	ImDrawIdx idx = (ImDrawIdx)d->_VtxCurrentIdx;
	d->PrimWriteIdx(idx);
	d->PrimWriteIdx((ImDrawIdx)(idx + 1));
	d->PrimWriteIdx((ImDrawIdx)(idx + 2));
	d->PrimWriteIdx(idx);
	d->PrimWriteIdx((ImDrawIdx)(idx + 2));
	d->PrimWriteIdx((ImDrawIdx)(idx + 3));
	d->PrimWriteVtx(p1, uv, c1);
	d->PrimWriteVtx(p2, uv, c2);
	d->PrimWriteVtx(p3, uv, c3);
	d->PrimWriteVtx(p4, uv, c4);
}

void CChinaHat::DrawShadow(ImDrawList *d, float x, float y, float rx, float ry, float alpha)
{
	if(alpha < 0.01f) return;
	if(!d || !d->_Data) return;
	for(int layer = 3; layer >= 0; layer--)
	{
		float expand = (float)layer * 1.5f;
		float a = alpha * (1.0f - (float)layer / 3.0f) * 0.12f;
		int ai = (int)(std::clamp(a, 0.0f, 1.0f) * 255);
		ImU32 col = IM_COL32(0, 0, 0, ai);
		int segs = 20;
		for(int j = 0; j < segs; j++)
		{
			float a1 = (float)j / segs * PI2;
			float a2 = (float)(j + 1) / segs * PI2;
			d->AddTriangleFilled(ImVec2(x, y),
				ImVec2(x + cosf(a1) * (rx + expand), y + sinf(a1) * (ry + expand * 0.3f)),
				ImVec2(x + cosf(a2) * (rx + expand), y + sinf(a2) * (ry + expand * 0.3f)), col);
		}
	}
}

void CChinaHat::DrawCone(ImDrawList *d, float x, float y, float size, float angle,
	float squishX, float squishY, ImU32 col1, ImU32 col2, float alpha)
{
	if(alpha < 0.01f) return;
	if(!d || !d->_Data) return;

	float width = size * 0.55f * squishX;
	float coneH = size * 0.31f * squishY;
	float baseRY = size * 0.15f;
	float coneAlpha = alpha * (185.0f / 255.0f);

	ImVec2 apex = RotatePoint(x, y, x, y - coneH, angle);

	float animSpeed = 5.0f;
	float timeOffset = fmodf(m_Time * animSpeed, (float)CONE_SEGS);
	if(!std::isfinite(timeOffset)) timeOffset = 0.0f;

	ImVec2 prevPt(0, 0);
	bool hasPrev = false;

	for(int i = 0; i <= CONE_SEGS; i++)
	{
		float a = (float)i / CONE_SEGS * PI2;
		float bx = x + (-sinf(a)) * width;
		float by = y + cosf(a) * baseRY;
		ImVec2 pt = RotatePoint(x, y, bx, by, angle);

		if(hasPrev)
		{
			int adjIdx = ((int)((float)i + timeOffset)) % CONE_SEGS;
			float progress = (float)adjIdx / CONE_SEGS;
			float t = progress < 0.5f ? progress * 2.0f : (1.0f - progress) * 2.0f;

			ImU32 segCol = LerpColor(col1, col2, t);

			float midA = ((float)(i - 1) + 0.5f) / CONE_SEGS * PI2;
			float darkFactor = 0.6f + 0.4f * (cosf(midA) * 0.5f + 0.5f);
			segCol = DarkenColor(segCol, darkFactor);
			segCol = SetAlpha(segCol, coneAlpha);

			d->AddTriangleFilled(apex, prevPt, pt, segCol);
		}
		prevPt = pt;
		hasPrev = true;
	}
}

void CChinaHat::DrawConeOutline(ImDrawList *d, float x, float y, float size, float angle,
	float squishX, ImU32 col1, ImU32 col2, float alpha)
{
	if(alpha < 0.01f) return;
	if(!d || !d->_Data) return;

	float width = size * 0.55f * squishX;
	float baseRY = size * 0.15f;
	int segs = 64;
	float animSpeed = 5.0f;
	float timeOffset = fmodf(m_Time * animSpeed, (float)(segs * 2));

	ImVec2 prev(0, 0);
	for(int i = 0; i <= segs; i++)
	{
		float a = (float)i / segs * PI2;
		float bx = x + (-sinf(a)) * width;
		float by = y + cosf(a) * baseRY;
		ImVec2 pt = RotatePoint(x, y, bx, by, angle);

		if(i > 0)
		{
			int adjIdx = ((int)((float)(i * 2) + timeOffset)) % (segs * 2);
			float progress = (float)adjIdx / (segs * 2);
			float t = progress < 0.5f ? progress * 2.0f : (1.0f - progress) * 2.0f;
			ImU32 col = LerpColor(col1, col2, t);
			col = SetAlpha(col, alpha);
			d->AddLine(prev, pt, col, 1.5f);
		}
		prev = pt;
	}
}

void CChinaHat::DrawConePattern(ImDrawList *d, float x, float y, float size, float angle,
	float squishX, float squishY, ImU32 col1, ImU32 col2, float alpha)
{
	if(alpha < 0.02f) return;

	float coneH = size * 0.31f * squishY;

	for(int band = 0; band < 2; band++)
	{
		float t = 0.35f + band * 0.25f;
		float bandY = y - coneH * t;
		float bandW = size * 0.55f * squishX * (1.0f - t * 0.6f);
		float bandRY = size * 0.15f * (1.0f - t * 0.6f);
		int segs = 48;
		float timeOff = fmodf(m_Time * 5.0f, (float)segs);

		ImVec2 prev(0, 0);
		for(int i = 0; i <= segs; i++)
		{
			float a = (float)i / segs * PI2;
			float bx = x + (-sinf(a)) * bandW;
			float by = bandY + cosf(a) * bandRY;
			ImVec2 pt = RotatePoint(x, y, bx, by, angle);

			if(i > 0)
			{
				int adj = ((int)((float)i + timeOff)) % segs;
				float prog = (float)adj / segs;
				float gt = prog < 0.5f ? prog * 2.0f : (1.0f - prog) * 2.0f;
				ImU32 col = LerpColor(col1, col2, gt);
				col = SetAlpha(col, alpha * 0.3f);
				d->AddLine(prev, pt, col, 0.8f);
			}
			prev = pt;
		}
	}
}

void CChinaHat::DrawShine(ImDrawList *d, float x, float y, float size, float angle, float alpha)
{
	if(alpha < 0.02f) return;
	ImVec2 s1 = RotatePoint(x, y, x - size * 0.12f, y - size * 0.22f, angle);
	d->AddCircleFilled(s1, size * 0.04f, IM_COL32(255, 255, 255, (int)(alpha * 0.25f * 255)), 8);
	ImVec2 s2 = RotatePoint(x, y, x - size * 0.04f, y - size * 0.18f, angle);
	d->AddCircleFilled(s2, size * 0.025f, IM_COL32(255, 255, 255, (int)(alpha * 0.12f * 255)), 6);
}

void CChinaHat::DrawGhostAura(ImDrawList *d, float cx, float cy, float radius,
	float alpha, ImU32 col1, ImU32 col2, ImU32 col3)
{
	if(alpha < 0.01f) return;
	if(!d || !d->_Data) return;

	ImU32 colors[3] = {col1, col2, col3};
	ImU32 nextColors[3] = {col2, col3, col1};

	int particleCount = std::clamp((int)(25.0f * alpha), 4, 25);
	float orbitSpeed = PI2 / 4.2f;
	float spacing = 0.15f;
	float glowR = radius * 0.07f;
	float coreR = radius * 0.03f;
	float ellipseRatio = 0.4f;

	for(int s = 0; s < 3; s++)
	{
		float spiralAngle = (float)s * PI2 / 3.0f;
		float ca = cosf(spiralAngle);
		float sa = sinf(spiralAngle);

		for(int i = 0; i < particleCount; i++)
		{
			float phase = m_Time * orbitSpeed - (float)i * spacing;
			float bx = sinf(phase) * radius;
			float by = cosf(phase) * radius * ellipseRatio;

			float px = cx + bx * ca - by * sa;
			float py = cy + bx * sa + by * ca;

			float t = (float)i / (float)particleCount;
			ImU32 color = LerpColor(colors[s], nextColors[s], t);
			float pAlpha = alpha * (1.0f - t * 0.85f);
			if(pAlpha < 0.01f) continue;

			float sizeFade = 1.0f - t * 0.5f;

			d->AddCircleFilled(ImVec2(px, py), glowR * sizeFade,
				SetAlpha(color, pAlpha * 0.3f), 10);
			d->AddCircleFilled(ImVec2(px, py), coreR * sizeFade,
				SetAlpha(color, pAlpha * 0.9f), 8);
		}
	}
}

void CChinaHat::DrawCrystals(ImDrawList *d, float cx, float cy, float radius,
	float rotation, float alpha, ImU32 col1)
{
	if(alpha < 0.01f) return;
	if(!d || !d->_Data) return;

	int count = 6;
	float orbitRX = radius;
	float orbitRY = radius * 0.4f;

	for(int i = 0; i < count; i++)
	{
		float a = rotation + (float)i / count * PI2;
		float crystalX = cx + cosf(a) * orbitRX;
		float crystalY = cy + sinf(a) * orbitRY;

		float selfRot = m_Time * (0.8f + i * 0.3f) + i * 1.2f;
		float pulse = 1.0f + sinf(m_Time * 2.0f + i * 1.5f) * 0.15f;
		float cs = radius * 0.1f * pulse;
		float hw = cs * 0.5f;
		float hh = cs * 0.8f;

		ImVec2 top = RotatePoint(crystalX, crystalY, crystalX, crystalY - hh, selfRot);
		ImVec2 right = RotatePoint(crystalX, crystalY, crystalX + hw, crystalY, selfRot);
		ImVec2 bottom = RotatePoint(crystalX, crystalY, crystalX, crystalY + hh, selfRot);
		ImVec2 left = RotatePoint(crystalX, crystalY, crystalX - hw, crystalY, selfRot);

		d->AddCircleFilled(ImVec2(crystalX, crystalY), cs * 2.0f,
			SetAlpha(col1, alpha * 0.1f), 12);

		d->AddTriangleFilled(top, right, left,
			SetAlpha(col1, alpha * 0.55f));
		d->AddTriangleFilled(bottom, right, left,
			SetAlpha(DarkenColor(col1, 0.6f), alpha * 0.55f));
		d->AddQuad(top, right, bottom, left,
			SetAlpha(col1, alpha * 0.85f), 1.0f);
	}
}

void CChinaHat::DrawCircle(ImDrawList *d, float cx, float cy, float radius, float height,
	SChinaHatState &st, float alpha, ImU32 col1, ImU32 col2)
{
	if(alpha < 0.01f) return;
	if(!d || !d->_Data) return;

	st.m_CircleStep += 3.6f * m_DeltaTime;

	double targetY = SmoothSinAnimation((double)st.m_CircleStep) * height;
	double targetY2 = SmoothSinAnimation((double)st.m_CircleStep - 0.35) * height;

	double sf = 1.0 - pow(1.0 - 0.12, (double)m_DeltaTime * 60.0);
	double sf2 = 1.0 - pow(1.0 - 0.10, (double)m_DeltaTime * 60.0);
	st.m_CircleSmoothY += (targetY - st.m_CircleSmoothY) * sf;
	st.m_CircleSmoothY2 += (targetY2 - st.m_CircleSmoothY2) * sf2;
	if(!std::isfinite(st.m_CircleSmoothY)) st.m_CircleSmoothY = 0.0;
	if(!std::isfinite(st.m_CircleSmoothY2)) st.m_CircleSmoothY2 = 0.0;

	int segs = 48;
	float rx = radius;
	float ry = radius * 0.35f;

	for(int i = 0; i < segs; i++)
	{
		float t = (float)i / segs;
		float tN = (float)((i + 1) % segs) / segs;

		float gradT = 0.5f - 0.5f * cosf(t * PI2);
		float gradTN = 0.5f - 0.5f * cosf(tN * PI2);

		ImU32 curCol = LerpColor(col1, col2, gradT);
		ImU32 nextCol = LerpColor(col1, col2, gradTN);

		float a1 = t * PI2, a2 = tN * PI2;

		float x1 = cx + cosf(a1) * rx;
		float y1r = cy + sinf(a1) * ry + (float)st.m_CircleSmoothY;
		float y1t = cy + sinf(a1) * ry + (float)st.m_CircleSmoothY2;
		float x2 = cx + cosf(a2) * rx;
		float y2r = cy + sinf(a2) * ry + (float)st.m_CircleSmoothY;
		float y2t = cy + sinf(a2) * ry + (float)st.m_CircleSmoothY2;

		ImU32 brightCur = MultAlpha(curCol, 0.8f * alpha);
		ImU32 brightNext = MultAlpha(nextCol, 0.8f * alpha);
		ImU32 fadeCur = SetAlpha(curCol, 0.0f);
		ImU32 fadeNext = SetAlpha(nextCol, 0.0f);

		AddGradientQuad(d,
			ImVec2(x1, y1r), ImVec2(x2, y2r),
			ImVec2(x2, y2t), ImVec2(x1, y1t),
			brightCur, brightNext, fadeNext, fadeCur);

		AddGradientQuad(d,
			ImVec2(x1, y1t), ImVec2(x2, y2t),
			ImVec2(x2, y2r), ImVec2(x1, y1r),
			fadeCur, fadeNext, brightNext, brightCur);

		d->AddLine(ImVec2(x1, y1r), ImVec2(x1, y1t),
			MultAlpha(curCol, 0.15f * alpha), 1.5f);

		d->AddLine(ImVec2(x1, y1r), ImVec2(x2, y2r),
			MultAlpha(curCol, alpha), 2.0f);
	}
}

void CChinaHat::DrawRhomb(ImDrawList *d, float cx, float cy, float size,
	float rotation, float alpha, ImU32 col1, ImU32 col2)
{
	if(alpha < 0.01f) return;
	if(!d || !d->_Data) return;

	float sinR = sinf(rotation);
	float rhombAngle = sinR * PI;
	float hs = size * 0.5f;

	ImVec2 p1 = RotatePoint(cx, cy, cx - hs, cy - hs, rhombAngle);
	ImVec2 p2 = RotatePoint(cx, cy, cx - hs, cy + hs, rhombAngle);
	ImVec2 p3 = RotatePoint(cx, cy, cx + hs, cy + hs, rhombAngle);
	ImVec2 p4 = RotatePoint(cx, cy, cx + hs, cy - hs, rhombAngle);

	AddGradientQuad(d, p1, p2, p3, p4,
		SetAlpha(col2, alpha), SetAlpha(col1, alpha),
		SetAlpha(col2, alpha), SetAlpha(col1, alpha));

	d->AddQuad(p1, p2, p3, p4,
		SetAlpha(LightenColor(col1, 1.3f), alpha * 0.6f), 1.0f);
}

void CChinaHat::RenderForPlayer(int ClientId)
{
    ANTI_DISASM_BLOCK();
	if(!ImGui::GetCurrentContext()) return;

	if(!ValidatePlayer(ClientId)) return;

	vec2 renderPos = GameClient()->m_aClients[ClientId].m_RenderPos;
	if(!IsFinite(renderPos.x) || !IsFinite(renderPos.y)) return;

	float zoom = GameClient()->m_Camera.m_Zoom;
	if(!IsFinite(zoom) || zoom <= 0.001f) zoom = 1.0f;
	float scale = 1.0f / zoom;

	ImVec2 ds = ImGui::GetIO().DisplaySize;
	if(ds.x < 1 || ds.y < 1) return;

	vec2 cam = GameClient()->m_Camera.m_Center;
	float sx = ds.x / 2.0f + (renderPos.x - cam.x) * scale;
	float sy = ds.y / 2.0f + (renderPos.y - cam.y) * scale;
	if(!IsFinite(sx) || !IsFinite(sy)) return;
	if(sx < -300 || sx > ds.x + 300 || sy < -300 || sy > ds.y + 300) return;

	SChinaHatState &st = m_States[ClientId];

	float velX = SafeFloat(GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_VelX / 256.0f);
	float velY = SafeFloat(GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_VelY / 256.0f);

	float speed = sqrtf(velX * velX + velY * velY);
	st.m_SpeedSmoothed = Lerp(st.m_SpeedSmoothed, speed, m_DeltaTime * 8.0f);
	bool isMoving = st.m_SpeedSmoothed > 0.5f;

	if(isMoving)
		st.m_IdleTimer = 0.0f;
	else
		st.m_IdleTimer += m_DeltaTime;

	float bobSpeed = 2.5f + st.m_SpeedSmoothed * 0.3f;
	float bobAmount = 0.8f + st.m_SpeedSmoothed * 0.15f;
	st.m_BobOffset = sinf(m_Time * bobSpeed) * bobAmount;

	st.m_BreathAnim = sinf(m_Time * 1.2f) * 0.3f;
	if(!isMoving)
		st.m_BobOffset += st.m_BreathAnim;

	float targetSway = std::clamp(-velX * 0.025f, -0.25f, 0.25f);
	st.m_TiltSmooth = Lerp(st.m_TiltSmooth, targetSway, m_DeltaTime * 5.0f);
	st.m_SwayAngle = st.m_TiltSmooth;

	float accelX = velX - st.m_PrevVelX;
	float squishAmt = std::clamp(fabsf(accelX) * 0.04f, 0.0f, 0.12f);
	st.m_SquishX = Lerp(st.m_SquishX, 1.0f + squishAmt, m_DeltaTime * 7.0f);
	st.m_SquishY = Lerp(st.m_SquishY, 1.0f - squishAmt * 0.3f, m_DeltaTime * 7.0f);

	if(st.m_PrevVelY > 2.5f && velY < 0.5f)
		st.m_LandBounce = 1.0f;
	st.m_LandBounce = Lerp(st.m_LandBounce, 0.0f, m_DeltaTime * 5.0f);

	if(g_Config.m_shalawaChinaHatCrystals)
	{
		st.m_CrystalRotation += m_DeltaTime * (0.5f + st.m_SpeedSmoothed * 0.15f);
		if(st.m_CrystalRotation > 10000.0f) st.m_CrystalRotation = 0.0f;
	}

	if(g_Config.m_shalawaChinaHatRhomb)
	{
		st.m_RhombRotation = fmodf(m_Time, PI2);
	}

	st.m_PrevVelX = velX;
	st.m_PrevVelY = velY;

	if(!std::isfinite(st.m_SpeedSmoothed)) st.m_SpeedSmoothed = 0.0f;
	if(!std::isfinite(st.m_BobOffset)) st.m_BobOffset = 0.0f;
	if(!std::isfinite(st.m_TiltSmooth)) st.m_TiltSmooth = 0.0f;
	if(!std::isfinite(st.m_SwayAngle)) st.m_SwayAngle = 0.0f;
	if(!std::isfinite(st.m_SquishX)) st.m_SquishX = 1.0f;
	if(!std::isfinite(st.m_SquishY)) st.m_SquishY = 1.0f;
	if(!std::isfinite(st.m_LandBounce)) st.m_LandBounce = 0.0f;
	if(!std::isfinite(st.m_CrystalRotation)) st.m_CrystalRotation = 0.0f;
	if(!std::isfinite(st.m_BreathAnim)) st.m_BreathAnim = 0.0f;

	float hatSize = std::clamp((float)g_Config.m_shalawaChinaHatSize, 8.0f, 40.0f) * scale;
	float hatX = sx;
	float hatY = sy - 28.0f * scale + st.m_BobOffset * scale - st.m_LandBounce * 4.0f * scale;
	float alpha = std::clamp((float)g_Config.m_shalawaChinaHatAlpha / 100.0f, 0.0f, 1.0f);

	if(GameClient()->IsOtherTeam(ClientId))
		alpha *= std::clamp(g_Config.m_ClShowOthersAlpha / 100.0f, 0.0f, 1.0f);

	if(alpha < 0.01f) return;

	ImDrawList *d = ImGui::GetBackgroundDrawList();
	if(!d || !d->_Data) return;
	if(!d) return;

	ImU32 col1 = GetColor(0);
	ImU32 col2 = GetColor(1);
	ImU32 col3 = GetColor(2);

	float playerCenterY = sy;
	float auraRadius = hatSize * 1.8f;
	float entityHeight = hatSize * 2.0f;

	if(g_Config.m_shalawaChinaHatCircle)
		DrawCircle(d, sx, playerCenterY, auraRadius, entityHeight, st, alpha, col1, col2);

	if(g_Config.m_shalawaChinaHatGhost)
		DrawGhostAura(d, sx, playerCenterY, auraRadius, alpha, col1, col2, col3);

	if(g_Config.m_shalawaChinaHatCrystals)
		DrawCrystals(d, sx, playerCenterY, auraRadius, st.m_CrystalRotation, alpha * 0.8f, col1);

	if(g_Config.m_shalawaChinaHatRhomb)
		DrawRhomb(d, sx, playerCenterY - auraRadius * 0.3f, hatSize * 0.6f,
			st.m_RhombRotation, alpha, col1, col2);

	float shadowRX = hatSize * 0.5f * st.m_SquishX;
	float shadowRY = hatSize * 0.1f;
	DrawShadow(d, hatX, hatY + hatSize * 0.05f, shadowRX, shadowRY, alpha * 0.2f);

	DrawCone(d, hatX, hatY, hatSize, st.m_SwayAngle,
		st.m_SquishX, st.m_SquishY, col1, col2, alpha);

	if(g_Config.m_shalawaChinaHatPattern)
		DrawConePattern(d, hatX, hatY, hatSize, st.m_SwayAngle,
			st.m_SquishX, st.m_SquishY, col1, col2, alpha);

	DrawConeOutline(d, hatX, hatY, hatSize, st.m_SwayAngle,
		st.m_SquishX, col1, col2, alpha);

	DrawShine(d, hatX, hatY, hatSize, st.m_SwayAngle, alpha);
}

void CChinaHat::OnRender()
{
    ANTI_DISASM_BLOCK();
	if(!g_Config.m_shalawaChinaHat) return;

	if(!SafetyCheck()) return;
	if(!ImGui::GetCurrentContext()) return;

	ImDrawList *d = ImGui::GetBackgroundDrawList();
	if(!d) return;

	float currentTime = (float)SDL_GetTicks() / OBF_F(1000);
	m_DeltaTime = std::clamp(currentTime - m_LastTime, 0.0001f, 0.05f);
	m_LastTime = currentTime;
	m_Time += m_DeltaTime;

	if(!std::isfinite(m_Time)) m_Time = 0.0f;
	if(!std::isfinite(m_DeltaTime)) m_DeltaTime = 0.016f;
	if(m_Time > OBF_F(100000)) m_Time = OBF_F(0);
	if(m_States.size() > (size_t)(MAX_CLIENTS * 2)) m_States.clear();

	int target = std::clamp(g_Config.m_shalawaChinaHatTarget, 0, 3);
	int localId = GameClient()->m_Snap.m_LocalClientId;
	if(localId < 0 || localId >= MAX_CLIENTS) localId = -1;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!ValidatePlayer(i)) continue;

		bool isLocal = (i == localId);
		bool isFriend = GameClient()->m_aClients[i].m_Friend;

		switch(target)
		{
		case 0: if(!isLocal) continue; break;
		case 1: break;
		case 2: if(!isLocal && !isFriend) continue; break;
		case 3: if(isLocal) continue; break;
		}

		try
		{
			RenderForPlayer(i);
		}
		catch(...)
		{
			m_States.erase(i);
		}
	}
}
