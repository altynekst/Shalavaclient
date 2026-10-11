#include "render_map.h"

#include <base/math.h>

#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/map.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/mapitems.h>
#include <game/mapitems_ex.h>

#include <chrono>
#include <cmath>

using namespace std::chrono_literals;

int IEnvelopePointAccess::FindPointIndex(CFixedTime Time) const
{
	int Low = 0;
	int High = NumPoints() - 2;
	int FoundIndex = -1;

	while(Low <= High)
	{
		int Mid = Low + (High - Low) / 2;
		const CEnvPoint *pMid = GetPoint(Mid);
		const CEnvPoint *pNext = GetPoint(Mid + 1);
		if(Time >= pMid->m_Time && Time < pNext->m_Time)
		{
			FoundIndex = Mid;
			break;
		}
		else if(Time < pMid->m_Time)
			High = Mid - 1;
		else
			Low = Mid + 1;
	}
	return FoundIndex;
}

CMapBasedEnvelopePointAccess::CMapBasedEnvelopePointAccess(CDataFileReader *pReader)
{
	bool FoundBezierEnvelope = false;
	int EnvelopeStart, EnvelopeNum;
	pReader->GetType(MAPITEMTYPE_ENVELOPE, &EnvelopeStart, &EnvelopeNum);
	for(int EnvelopeIndex = 0; EnvelopeIndex < EnvelopeNum; EnvelopeIndex++)
	{
		CMapItemEnvelope *pEnvelope = static_cast<CMapItemEnvelope *>(pReader->GetItem(EnvelopeStart + EnvelopeIndex));
		if(pEnvelope->m_Version >= CMapItemEnvelope::VERSION_TEEWORLDS_BEZIER)
		{
			FoundBezierEnvelope = true;
			break;
		}
	}

	if(FoundBezierEnvelope)
	{
		m_pPoints = nullptr;
		m_pPointsBezier = nullptr;

		int EnvPointStart, FakeEnvPointNum;
		pReader->GetType(MAPITEMTYPE_ENVPOINTS, &EnvPointStart, &FakeEnvPointNum);
		if(FakeEnvPointNum > 0)
			m_pPointsBezierUpstream = static_cast<CEnvPointBezier_upstream *>(pReader->GetItem(EnvPointStart));
		else
			m_pPointsBezierUpstream = nullptr;

		m_NumPointsMax = pReader->GetItemSize(EnvPointStart) / sizeof(CEnvPointBezier_upstream);
	}
	else
	{
		int EnvPointStart, FakeEnvPointNum;
		pReader->GetType(MAPITEMTYPE_ENVPOINTS, &EnvPointStart, &FakeEnvPointNum);
		if(FakeEnvPointNum > 0)
			m_pPoints = static_cast<CEnvPoint *>(pReader->GetItem(EnvPointStart));
		else
			m_pPoints = nullptr;

		m_NumPointsMax = pReader->GetItemSize(EnvPointStart) / sizeof(CEnvPoint);

		int EnvPointBezierStart, FakeEnvPointBezierNum;
		pReader->GetType(MAPITEMTYPE_ENVPOINTS_BEZIER, &EnvPointBezierStart, &FakeEnvPointBezierNum);
		const int NumPointsBezier = pReader->GetItemSize(EnvPointBezierStart) / sizeof(CEnvPointBezier);
		if(FakeEnvPointBezierNum > 0 && m_NumPointsMax == NumPointsBezier)
			m_pPointsBezier = static_cast<CEnvPointBezier *>(pReader->GetItem(EnvPointBezierStart));
		else
			m_pPointsBezier = nullptr;

		m_pPointsBezierUpstream = nullptr;
	}

	SetPointsRange(0, m_NumPointsMax);
}

CMapBasedEnvelopePointAccess::CMapBasedEnvelopePointAccess(IMap *pMap) :
	CMapBasedEnvelopePointAccess(static_cast<CMap *>(pMap)->GetReader())
{
}

void CMapBasedEnvelopePointAccess::SetPointsRange(int StartPoint, int NumPoints)
{
	m_StartPoint = std::clamp(StartPoint, 0, m_NumPointsMax);
	m_NumPoints = std::clamp(NumPoints, 0, maximum(m_NumPointsMax - StartPoint, 0));
}

int CMapBasedEnvelopePointAccess::StartPoint() const { return m_StartPoint; }
int CMapBasedEnvelopePointAccess::NumPoints() const { return m_NumPoints; }
int CMapBasedEnvelopePointAccess::NumPointsMax() const { return m_NumPointsMax; }

const CEnvPoint *CMapBasedEnvelopePointAccess::GetPoint(int Index) const
{
	if(Index < 0 || Index >= m_NumPoints)
		return nullptr;
	if(m_pPoints != nullptr)
		return &m_pPoints[Index + m_StartPoint];
	if(m_pPointsBezierUpstream != nullptr)
		return &m_pPointsBezierUpstream[Index + m_StartPoint];
	return nullptr;
}

const CEnvPointBezier *CMapBasedEnvelopePointAccess::GetBezier(int Index) const
{
	if(Index < 0 || Index >= m_NumPoints)
		return nullptr;
	if(m_pPointsBezier != nullptr)
		return &m_pPointsBezier[Index + m_StartPoint];
	if(m_pPointsBezierUpstream != nullptr)
		return &m_pPointsBezierUpstream[Index + m_StartPoint].m_Bezier;
	return nullptr;
}

static float SolveBezier(float x, float p0, float p1, float p2, float p3)
{
	const double x3 = -p0 + 3.0 * p1 - 3.0 * p2 + p3;
	const double x2 = 3.0 * p0 - 6.0 * p1 + 3.0 * p2;
	const double x1 = -3.0 * p0 + 3.0 * p1;
	const double x0 = p0 - x;

	if(x3 == 0.0 && x2 == 0.0)
	{
		const double a = x1;
		const double b = x0;
		if(a == 0.0)
			return 0.0f;
		return -b / a;
	}
	else if(x3 == 0.0)
	{
		const double b = x1 / x2;
		const double c = x0 / x2;
		if(c == 0.0)
			return 0.0f;
		const double D = b * b - 4.0 * c;
		const double SqrtD = std::sqrt(D);
		const double t = (-b + SqrtD) / 2.0;
		if(0.0 <= t && t <= 1.0001)
			return t;
		return (-b - SqrtD) / 2.0;
	}
	else
	{
		const double a = x2 / x3;
		const double b = x1 / x3;
		const double c = x0 / x3;
		const double Substitute = a / 3.0;
		const double p = b / 3.0 - a * a / 9.0;
		const double q = (2.0 * a * a * a / 27.0 - a * b / 3.0 + c) / 2.0;
		const double D = q * q + p * p * p;

		if(D > 0.0)
		{
			const double s = std::sqrt(D);
			return std::cbrt(s - q) - std::cbrt(s + q) - Substitute;
		}
		else if(D == 0.0)
		{
			const double s = std::cbrt(-q);
			const double t = 2.0 * s - Substitute;
			if(0.0 <= t && t <= 1.0001)
				return t;
			return (-s - Substitute);
		}
		else
		{
			const double Phi = std::acos(-q / std::sqrt(-(p * p * p))) / 3.0;
			const double s = 2.0 * std::sqrt(-p);
			const double t1 = s * std::cos(Phi) - Substitute;
			if(0.0 <= t1 && t1 <= 1.0001)
				return t1;
			const double t2 = -s * std::cos(Phi + pi / 3.0) - Substitute;
			if(0.0 <= t2 && t2 <= 1.0001)
				return t2;
			return -s * std::cos(Phi - pi / 3.0) - Substitute;
		}
	}
}

void CRenderMap::Init(IGraphics *pGraphics, ITextRender *pTextRender)
{
	m_pGraphics = pGraphics;
	m_pTextRender = pTextRender;
}

void CRenderMap::RenderEvalEnvelope(const IEnvelopePointAccess *pPoints, std::chrono::nanoseconds TimeNanos, ColorRGBA &Result, size_t Channels)
{
	const int NumPoints = pPoints->NumPoints();
	if(NumPoints == 0)
		return;

	if(NumPoints == 1)
	{
		const CEnvPoint *pFirstPoint = pPoints->GetPoint(0);
		for(size_t c = 0; c < Channels; c++)
			Result[c] = fx2f(pFirstPoint->m_aValues[c]);
		return;
	}

	const CEnvPoint *pLastPoint = pPoints->GetPoint(NumPoints - 1);
	const int64_t MaxPointTime = (int64_t)pLastPoint->m_Time.GetInternal() * std::chrono::nanoseconds(1ms).count();
	if(MaxPointTime > 0)
		TimeNanos = std::chrono::nanoseconds(TimeNanos.count() % MaxPointTime);
	else
		TimeNanos = decltype(TimeNanos)::zero();

	const double TimeMillis = TimeNanos.count() / (double)std::chrono::nanoseconds(1ms).count();

	int FoundIndex = pPoints->FindPointIndex(CFixedTime(TimeMillis));
	if(FoundIndex == -1)
	{
		for(size_t c = 0; c < Channels; c++)
			Result[c] = fx2f(pLastPoint->m_aValues[c]);
		return;
	}

	const CEnvPoint *pCurrentPoint = pPoints->GetPoint(FoundIndex);
	const CEnvPoint *pNextPoint = pPoints->GetPoint(FoundIndex + 1);

	const CFixedTime Delta = pNextPoint->m_Time - pCurrentPoint->m_Time;
	if(Delta <= CFixedTime(0))
	{
		for(size_t c = 0; c < Channels; c++)
			Result[c] = fx2f(pCurrentPoint->m_aValues[c]);
		return;
	}

	float a = (float)(TimeMillis - pCurrentPoint->m_Time.GetInternal()) / Delta.GetInternal();

	switch(pCurrentPoint->m_Curvetype)
	{
	case CURVETYPE_STEP:
		a = 0.0f;
		break;
	case CURVETYPE_SLOW:
		a = a * a * a;
		break;
	case CURVETYPE_FAST:
		a = 1.0f - a;
		a = 1.0f - a * a * a;
		break;
	case CURVETYPE_SMOOTH:
		a = -2.0f * a * a * a + 3.0f * a * a;
		break;
	case CURVETYPE_BEZIER:
	{
		const CEnvPointBezier *pCurrentPointBezier = pPoints->GetBezier(FoundIndex);
		const CEnvPointBezier *pNextPointBezier = pPoints->GetBezier(FoundIndex + 1);
		if(pCurrentPointBezier == nullptr || pNextPointBezier == nullptr)
			break;
		for(size_t c = 0; c < Channels; c++)
		{
			const vec2 p0 = vec2(pCurrentPoint->m_Time.GetInternal(), fx2f(pCurrentPoint->m_aValues[c]));
			const vec2 p3 = vec2(pNextPoint->m_Time.GetInternal(), fx2f(pNextPoint->m_aValues[c]));
			const vec2 OutTang = vec2(pCurrentPointBezier->m_aOutTangentDeltaX[c].GetInternal(), fx2f(pCurrentPointBezier->m_aOutTangentDeltaY[c]));
			const vec2 InTang = vec2(pNextPointBezier->m_aInTangentDeltaX[c].GetInternal(), fx2f(pNextPointBezier->m_aInTangentDeltaY[c]));
			vec2 p1 = p0 + OutTang;
			vec2 p2 = p3 + InTang;
			p1.x = std::clamp(p1.x, p0.x, p3.x);
			p2.x = std::clamp(p2.x, p0.x, p3.x);
			a = std::clamp(SolveBezier(TimeMillis, p0.x, p1.x, p2.x, p3.x), 0.0f, 1.0f);
			Result[c] = bezier(p0.y, p1.y, p2.y, p3.y, a);
		}
		return;
	}
	case CURVETYPE_LINEAR: [[fallthrough]];
	default:
		break;
	}

	for(size_t c = 0; c < Channels; c++)
	{
		const float v0 = fx2f(pCurrentPoint->m_aValues[c]);
		const float v1 = fx2f(pNextPoint->m_aValues[c]);
		Result[c] = v0 + (v1 - v0) * a;
	}
}

static void Rotate(const CPoint *pCenter, CPoint *pPoint, float Rotation)
{
	const float c = std::cos(Rotation);
	const float s = std::sin(Rotation);
	const int x = pPoint->x - pCenter->x;
	const int y = pPoint->y - pCenter->y;
	pPoint->x = (int)(x * c - y * s + pCenter->x);
	pPoint->y = (int)(x * s + y * c + pCenter->y);
}

void CRenderMap::ForceRenderQuads(CQuad *pQuads, int NumQuads, int RenderFlags, IEnvelopeEval *pEnvEval, float Alpha)
{
	Graphics()->TrianglesBegin();
	const float Conv = 1 / 255.0f;
	for(int i = 0; i < NumQuads; i++)
	{
		CQuad *pQuad = &pQuads[i];

		ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		if(pQuad->m_ColorEnv >= 0)
			pEnvEval->EnvelopeEval(pQuad->m_ColorEnvOffset, pQuad->m_ColorEnv, Color, 4);

		if(Color.a <= 0.0f)
			continue;

		bool Opaque = false;
		if(Opaque && !(RenderFlags & LAYERRENDERFLAG_OPAQUE))
			continue;
		if(!Opaque && !(RenderFlags & LAYERRENDERFLAG_TRANSPARENT))
			continue;

		Graphics()->QuadsSetSubsetFree(
			fx2f(pQuad->m_aTexcoords[0].x), fx2f(pQuad->m_aTexcoords[0].y),
			fx2f(pQuad->m_aTexcoords[1].x), fx2f(pQuad->m_aTexcoords[1].y),
			fx2f(pQuad->m_aTexcoords[2].x), fx2f(pQuad->m_aTexcoords[2].y),
			fx2f(pQuad->m_aTexcoords[3].x), fx2f(pQuad->m_aTexcoords[3].y));

		ColorRGBA Position = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		if(pQuad->m_PosEnv >= 0)
			pEnvEval->EnvelopeEval(pQuad->m_PosEnvOffset, pQuad->m_PosEnv, Position, 3);
		const vec2 Offset = vec2(Position.r, Position.g);
		const float RotAngle = Position.b / 180.0f * pi;

		IGraphics::CColorVertex Array[4] = {
			IGraphics::CColorVertex(0, pQuad->m_aColors[0].r * Conv * Color.r, pQuad->m_aColors[0].g * Conv * Color.g, pQuad->m_aColors[0].b * Conv * Color.b, pQuad->m_aColors[0].a * Conv * Color.a * Alpha),
			IGraphics::CColorVertex(1, pQuad->m_aColors[1].r * Conv * Color.r, pQuad->m_aColors[1].g * Conv * Color.g, pQuad->m_aColors[1].b * Conv * Color.b, pQuad->m_aColors[1].a * Conv * Color.a * Alpha),
			IGraphics::CColorVertex(2, pQuad->m_aColors[2].r * Conv * Color.r, pQuad->m_aColors[2].g * Conv * Color.g, pQuad->m_aColors[2].b * Conv * Color.b, pQuad->m_aColors[2].a * Conv * Color.a * Alpha),
			IGraphics::CColorVertex(3, pQuad->m_aColors[3].r * Conv * Color.r, pQuad->m_aColors[3].g * Conv * Color.g, pQuad->m_aColors[3].b * Conv * Color.b, pQuad->m_aColors[3].a * Conv * Color.a * Alpha)};
		Graphics()->SetColorVertex(Array, 4);

		CPoint *pPoints = pQuad->m_aPoints;

		CPoint aRotated[4];
		if(RotAngle != 0.0f)
		{
			const float rc = std::cos(RotAngle);
			const float rs = std::sin(RotAngle);
			const CPoint &Center = pQuad->m_aPoints[4];
			for(int p = 0; p < 4; ++p)
			{
				const int dx = pQuad->m_aPoints[p].x - Center.x;
				const int dy = pQuad->m_aPoints[p].y - Center.y;
				aRotated[p].x = (int)(dx * rc - dy * rs + Center.x);
				aRotated[p].y = (int)(dx * rs + dy * rc + Center.y);
			}
			pPoints = aRotated;
		}

		IGraphics::CFreeformItem Freeform(
			fx2f(pPoints[0].x) + Offset.x, fx2f(pPoints[0].y) + Offset.y,
			fx2f(pPoints[1].x) + Offset.x, fx2f(pPoints[1].y) + Offset.y,
			fx2f(pPoints[2].x) + Offset.x, fx2f(pPoints[2].y) + Offset.y,
			fx2f(pPoints[3].x) + Offset.x, fx2f(pPoints[3].y) + Offset.y);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);
	}
	Graphics()->TrianglesEnd();
}

void CRenderMap::RenderTileRectangle(int RectX, int RectY, int RectW, int RectH,
	unsigned char IndexIn, unsigned char IndexOut,
	float Scale, ColorRGBA Color, int RenderFlags)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / (ScreenX1 - ScreenX0) * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	const bool UseTextureArrays = Graphics()->HasTextureArraysSupport();

	if(UseTextureArrays)
		Graphics()->QuadsTex3DBegin();
	else
		Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	constexpr int BATCH_SIZE = 512;
	IGraphics::CQuadItem aQuadBatch[BATCH_SIZE];
	int BatchCount = 0;
	int LastIndex = -1;

	auto FlushBatch = [&]() {
		if(BatchCount > 0)
		{
			if(UseTextureArrays)
				Graphics()->QuadsTex3DDrawTL(aQuadBatch, BatchCount);
			else
				Graphics()->QuadsDrawTL(aQuadBatch, BatchCount);
			BatchCount = 0;
		}
	};

	for(int y = StartY; y < EndY; y++)
	{
		for(int x = StartX; x < EndX; x++)
		{
			unsigned char Index = (x >= RectX && x < RectX + RectW && y >= RectY && y < RectY + RectH) ? IndexIn : IndexOut;
			if(Index)
			{
				if(!(RenderFlags & LAYERRENDERFLAG_TRANSPARENT))
					continue;

				if(Index != LastIndex)
				{
					FlushBatch();

					float cx0, cy0, cx1, cy1, cx2, cy2, cx3, cy3;
					if(UseTextureArrays)
					{
						cx0 = 0; cy0 = 0; cx1 = 1; cy1 = 0;
						cx2 = 1; cy2 = 1; cx3 = 0; cy3 = 1;
						Graphics()->QuadsSetSubsetFree(cx0, cy0, cx1, cy1, cx2, cy2, cx3, cy3, Index);
					}
					else
					{
						int tx = Index % 16;
						int ty = Index / 16;
						int Px0 = tx * (1024 / 16);
						int Py0 = ty * (1024 / 16);
						int Px1 = Px0 + (1024 / 16) - 1;
						int Py1 = Py0 + (1024 / 16) - 1;
						cx0 = Nudge + Px0 / TexSize + Frac;
						cy0 = Nudge + Py0 / TexSize + Frac;
						cx1 = Nudge + Px1 / TexSize - Frac;
						cy1 = Nudge + Py0 / TexSize + Frac;
						cx2 = Nudge + Px1 / TexSize - Frac;
						cy2 = Nudge + Py1 / TexSize - Frac;
						cx3 = Nudge + Px0 / TexSize + Frac;
						cy3 = Nudge + Py1 / TexSize - Frac;
						Graphics()->QuadsSetSubsetFree(cx0, cy0, cx1, cy1, cx2, cy2, cx3, cy3);
					}
					LastIndex = Index;
				}

				aQuadBatch[BatchCount++] = IGraphics::CQuadItem(x * Scale, y * Scale, Scale, Scale);
				if(BatchCount >= BATCH_SIZE)
					FlushBatch();
			}
		}
	}

	FlushBatch();

	if(UseTextureArrays)
		Graphics()->QuadsTex3DEnd();
	else
		Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderMap::RenderTile(int x, int y, unsigned char Index, float Scale, ColorRGBA Color)
{
	const bool UseTextureArrays = Graphics()->HasTextureArraysSupport();

	if(UseTextureArrays)
		Graphics()->QuadsTex3DBegin();
	else
		Graphics()->QuadsBegin();

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	float TilePixelSize = 1024 / Scale;
	float FinalTileSize = Scale / (ScreenX1 - ScreenX0) * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	float x0, y0, x1, y1, x2, y2, x3, y3;

	if(UseTextureArrays)
	{
		x0 = 0; y0 = 0; x1 = 1; y1 = 0;
		x2 = 1; y2 = 1; x3 = 0; y3 = 1;
		Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3, Index);
	}
	else
	{
		int tx = Index % 16;
		int ty = Index / 16;
		int Px0 = tx * (1024 / 16);
		int Py0 = ty * (1024 / 16);
		int Px1 = Px0 + (1024 / 16) - 1;
		int Py1 = Py0 + (1024 / 16) - 1;
		x0 = Nudge + Px0 / TexSize + Frac;
		y0 = Nudge + Py0 / TexSize + Frac;
		x1 = Nudge + Px1 / TexSize - Frac;
		y1 = Nudge + Py0 / TexSize + Frac;
		x2 = Nudge + Px1 / TexSize - Frac;
		y2 = Nudge + Py1 / TexSize - Frac;
		x3 = Nudge + Px0 / TexSize + Frac;
		y3 = Nudge + Py1 / TexSize - Frac;
		Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3);
	}

	IGraphics::CQuadItem QuadItem(x, y, Scale, Scale);
	if(UseTextureArrays)
		Graphics()->QuadsTex3DDrawTL(&QuadItem, 1);
	else
		Graphics()->QuadsDrawTL(&QuadItem, 1);

	if(UseTextureArrays)
		Graphics()->QuadsTex3DEnd();
	else
		Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderMap::RenderTilemap(CTile *pTiles, int w, int h, float Scale, ColorRGBA Color, int RenderFlags)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / (ScreenX1 - ScreenX0) * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	const bool UseTextureArrays = Graphics()->HasTextureArraysSupport();

	if(UseTextureArrays)
		Graphics()->QuadsTex3DBegin();
	else
		Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);
	const bool ColorOpaque = Color.a > 254.0f / 255.0f;

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	constexpr int BATCH_SIZE = 512;
	IGraphics::CQuadItem aQuadBatch[BATCH_SIZE];
	int BatchCount = 0;
	int LastSubsetKey = -1;

	auto FlushBatch = [&]() {
		if(BatchCount > 0)
		{
			if(UseTextureArrays)
				Graphics()->QuadsTex3DDrawTL(aQuadBatch, BatchCount);
			else
				Graphics()->QuadsDrawTL(aQuadBatch, BatchCount);
			BatchCount = 0;
		}
	};

	for(int y = StartY; y < EndY; y++)
	{
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x;
			int my = y;

			if(RenderFlags & TILERENDERFLAG_EXTEND)
			{
				if(mx < 0) mx = 0;
				if(mx >= w) mx = w - 1;
				if(my < 0) my = 0;
				if(my >= h) my = h - 1;
			}
			else
			{
				if(mx < 0 || mx >= w || my < 0 || my >= h)
					continue;
			}

			int c = mx + my * w;
			unsigned char Index = pTiles[c].m_Index;
			if(Index)
			{
				unsigned char Flags = pTiles[c].m_Flags;

				bool Render = false;
				if(ColorOpaque && Flags & TILEFLAG_OPAQUE)
				{
					if(RenderFlags & LAYERRENDERFLAG_OPAQUE)
						Render = true;
				}
				else
				{
					if(RenderFlags & LAYERRENDERFLAG_TRANSPARENT)
						Render = true;
				}

				if(Render)
				{
					float cx0, cy0, cx1, cy1, cx2, cy2, cx3, cy3;

					if(UseTextureArrays)
					{
						cx0 = 0; cy0 = 0; cx1 = 1; cy1 = 0;
						cx2 = 1; cy2 = 1; cx3 = 0; cy3 = 1;
					}
					else
					{
						int tx = Index % 16;
						int ty = Index / 16;
						int Px0 = tx * (1024 / 16);
						int Py0 = ty * (1024 / 16);
						int Px1 = Px0 + (1024 / 16) - 1;
						int Py1 = Py0 + (1024 / 16) - 1;
						cx0 = Nudge + Px0 / TexSize + Frac;
						cy0 = Nudge + Py0 / TexSize + Frac;
						cx1 = Nudge + Px1 / TexSize - Frac;
						cy1 = Nudge + Py0 / TexSize + Frac;
						cx2 = Nudge + Px1 / TexSize - Frac;
						cy2 = Nudge + Py1 / TexSize - Frac;
						cx3 = Nudge + Px0 / TexSize + Frac;
						cy3 = Nudge + Py1 / TexSize - Frac;
					}

					if(Flags & TILEFLAG_XFLIP) { float t = cx0; cx0 = cx1; cx1 = t; t = cx2; cx2 = cx3; cx3 = t; }
					if(Flags & TILEFLAG_YFLIP) { float t = cy0; cy0 = cy3; cy3 = t; t = cy1; cy1 = cy2; cy2 = t; }
					if(Flags & TILEFLAG_ROTATE)
					{
						float t = cx0; cx0 = cx3; cx3 = cx2; cx2 = cx1; cx1 = t;
						t = cy0; cy0 = cy3; cy3 = cy2; cy2 = cy1; cy1 = t;
					}

					int SubsetKey = UseTextureArrays ?
						(Index | ((Flags & (TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE)) << 8)) :
						(Index | ((Flags & (TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE)) << 8));

					if(SubsetKey != LastSubsetKey)
					{
						FlushBatch();
						if(UseTextureArrays)
							Graphics()->QuadsSetSubsetFree(cx0, cy0, cx1, cy1, cx2, cy2, cx3, cy3, Index);
						else
							Graphics()->QuadsSetSubsetFree(cx0, cy0, cx1, cy1, cx2, cy2, cx3, cy3);
						LastSubsetKey = SubsetKey;
					}

					aQuadBatch[BatchCount++] = IGraphics::CQuadItem(x * Scale, y * Scale, Scale, Scale);
					if(BatchCount >= BATCH_SIZE)
						FlushBatch();
				}
			}
			x += pTiles[c].m_Skip;
		}
	}

	FlushBatch();

	if(UseTextureArrays)
		Graphics()->QuadsTex3DEnd();
	else
		Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderMap::RenderTeleOverlay(CTeleTile *pTele, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	if(!(OverlayRenderFlag & OVERLAYRENDERFLAG_TEXT))
		return;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	if(EndX - StartX > Graphics()->ScreenWidth() / g_Config.m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / g_Config.m_GfxTextOverlay)
		return;

	float Size = g_Config.m_ClTextEntitiesSize / 100.f;
	char aBuf[16];

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
	for(int y = StartY; y < EndY; y++)
	{
		for(int x = StartX; x < EndX; x++)
		{
			if(x < 0 || x >= w || y < 0 || y >= h)
				continue;

			int c = x + y * w;
			unsigned char Index = pTele[c].m_Number;
			if(Index && IsTeleTileNumberUsedAny(pTele[c].m_Type))
			{
				str_format(aBuf, sizeof(aBuf), "%d", Index);
				float ScaledWidth = TextRender()->TextWidth(Size * Scale, aBuf, -1);
				float Factor = std::clamp(Scale / ScaledWidth, 0.0f, 1.0f);
				float LocalSize = Size * Factor;
				float ToCenterOffset = (1 - LocalSize) / 2.f;
				TextRender()->Text((x + 0.5f) * Scale - (ScaledWidth * Factor) / 2.0f, (y + ToCenterOffset) * Scale, LocalSize * Scale, aBuf);
			}
		}
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderMap::RenderSpeedupOverlay(CSpeedupTile *pSpeedup, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	if(EndX - StartX > Graphics()->ScreenWidth() / g_Config.m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / g_Config.m_GfxTextOverlay)
		return;

	float Size = g_Config.m_ClTextEntitiesSize / 100.f;
	float ToCenterOffset = (1 - Size) / 2.f;
	char aBuf[16];

	bool ArrowsStarted = false;

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
	for(int y = StartY; y < EndY; y++)
	{
		for(int x = StartX; x < EndX; x++)
		{
			if(x < 0 || x >= w || y < 0 || y >= h)
				continue;

			int c = x + y * w;

			int Force = (int)pSpeedup[c].m_Force;
			int MaxSpeed = (int)pSpeedup[c].m_MaxSpeed;
			int Type = (int)pSpeedup[c].m_Type;
			int Angle = (int)pSpeedup[c].m_Angle;
			if((Force && Type == TILE_SPEED_BOOST_OLD) || ((Force || MaxSpeed) && Type == TILE_SPEED_BOOST) || (OverlayRenderFlag & OVERLAYRENDERFLAG_EDITOR && (Type || Force || MaxSpeed || Angle)))
			{
				if(IsValidSpeedupTile(Type))
				{
					if(!ArrowsStarted)
					{
						Graphics()->TextureSet(g_pData->m_aImages[IMAGE_SPEEDUP_ARROW].m_Id);
						Graphics()->QuadsBegin();
						Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
						Graphics()->SelectSprite(SPRITE_SPEEDUP_ARROW);
						ArrowsStarted = true;
					}

					Graphics()->QuadsSetRotation(pSpeedup[c].m_Angle * (pi / 180.0f));
					Graphics()->DrawSprite(x * Scale + 16, y * Scale + 16, 35.0f);

					if(OverlayRenderFlag & OVERLAYRENDERFLAG_TEXT)
					{
						if(ArrowsStarted)
						{
							Graphics()->QuadsEnd();
							ArrowsStarted = false;
						}
						str_format(aBuf, sizeof(aBuf), "%d", Force);
						TextRender()->Text(x * Scale, (y + 0.5f + ToCenterOffset / 2) * Scale, Size * Scale / 2.f, aBuf);
						if(MaxSpeed)
						{
							str_format(aBuf, sizeof(aBuf), "%d", MaxSpeed);
							TextRender()->Text(x * Scale, (y + ToCenterOffset / 2) * Scale, Size * Scale / 2.f, aBuf);
						}
					}
				}
				else
				{
					if(ArrowsStarted)
					{
						Graphics()->QuadsEnd();
						ArrowsStarted = false;
					}
					if(OverlayRenderFlag & OVERLAYRENDERFLAG_TEXT)
					{
						float LineSpacing = Size * Scale / 3.f;
						float BaseY = (y + ToCenterOffset) * Scale;
						str_format(aBuf, sizeof(aBuf), "%d", Force);
						TextRender()->Text(x * Scale, BaseY, LineSpacing, aBuf);
						str_format(aBuf, sizeof(aBuf), "%d", MaxSpeed);
						TextRender()->Text(x * Scale, BaseY + LineSpacing, LineSpacing, aBuf);
						str_format(aBuf, sizeof(aBuf), "%d", Angle);
						TextRender()->Text(x * Scale, BaseY + 2 * LineSpacing, LineSpacing, aBuf);
					}
				}
			}
		}
	}

	if(ArrowsStarted)
		Graphics()->QuadsEnd();

	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderMap::RenderSwitchOverlay(CSwitchTile *pSwitch, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	if(!(OverlayRenderFlag & OVERLAYRENDERFLAG_TEXT))
		return;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	if(EndX - StartX > Graphics()->ScreenWidth() / g_Config.m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / g_Config.m_GfxTextOverlay)
		return;

	float Size = g_Config.m_ClTextEntitiesSize / 100.f;
	float ToCenterOffset = (1 - Size) / 2.f;
	char aBuf[16];

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
	for(int y = StartY; y < EndY; y++)
	{
		for(int x = StartX; x < EndX; x++)
		{
			if(x < 0 || x >= w || y < 0 || y >= h)
				continue;

			int c = x + y * w;

			unsigned char Index = pSwitch[c].m_Number;
			if(Index && IsSwitchTileNumberUsed(pSwitch[c].m_Type))
			{
				str_format(aBuf, sizeof(aBuf), "%d", Index);
				TextRender()->Text(x * Scale, (y + ToCenterOffset / 2) * Scale, Size * Scale / 2.f, aBuf);
			}

			unsigned char Delay = pSwitch[c].m_Delay;
			if(Delay && IsSwitchTileDelayUsed(pSwitch[c].m_Type))
			{
				str_format(aBuf, sizeof(aBuf), "%d", Delay);
				TextRender()->Text(x * Scale, (y + 0.5f + ToCenterOffset / 2) * Scale, Size * Scale / 2.f, aBuf);
			}
		}
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderMap::RenderTuneOverlay(CTuneTile *pTune, int w, int h, float Scale, int OverlayRenderFlag, float Alpha)
{
	if(!(OverlayRenderFlag & OVERLAYRENDERFLAG_TEXT))
		return;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	if(EndX - StartX > Graphics()->ScreenWidth() / g_Config.m_GfxTextOverlay || EndY - StartY > Graphics()->ScreenHeight() / g_Config.m_GfxTextOverlay)
		return;

	float Size = g_Config.m_ClTextEntitiesSize / 200.f;
	char aBuf[16];

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
	for(int y = StartY; y < EndY; y++)
	{
		for(int x = StartX; x < EndX; x++)
		{
			if(x < 0 || x >= w || y < 0 || y >= h)
				continue;

			int c = x + y * w;
			unsigned char Index = pTune[c].m_Number;
			if(Index)
			{
				str_format(aBuf, sizeof(aBuf), "%d", Index);
				float ScaledWidth = TextRender()->TextWidth(Size * Scale, aBuf, -1);
				float Factor = std::clamp(Scale / ScaledWidth, 0.0f, 1.0f);
				float LocalSize = Size * Factor;
				float ToCenterOffset = (1 - LocalSize) / 2.f;
				TextRender()->Text((x + 0.5f) * Scale - (ScaledWidth * Factor) / 2.0f, (y + ToCenterOffset) * Scale, LocalSize * Scale, aBuf);
			}
		}
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderMap::RenderTelemap(CTeleTile *pTele, int w, int h, float Scale, ColorRGBA Color, int RenderFlags)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / (ScreenX1 - ScreenX0) * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	const bool UseTextureArrays = Graphics()->HasTextureArraysSupport();

	if(UseTextureArrays)
		Graphics()->QuadsTex3DBegin();
	else
		Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	constexpr int BATCH_SIZE = 512;
	IGraphics::CQuadItem aQuadBatch[BATCH_SIZE];
	int BatchCount = 0;
	int LastIndex = -1;

	auto FlushBatch = [&]() {
		if(BatchCount > 0)
		{
			if(UseTextureArrays)
				Graphics()->QuadsTex3DDrawTL(aQuadBatch, BatchCount);
			else
				Graphics()->QuadsDrawTL(aQuadBatch, BatchCount);
			BatchCount = 0;
		}
	};

	for(int y = StartY; y < EndY; y++)
	{
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x, my = y;

			if(RenderFlags & TILERENDERFLAG_EXTEND)
			{
				if(mx < 0) mx = 0;
				if(mx >= w) mx = w - 1;
				if(my < 0) my = 0;
				if(my >= h) my = h - 1;
			}
			else
			{
				if(mx < 0 || mx >= w || my < 0 || my >= h)
					continue;
			}

			int c = mx + my * w;
			unsigned char Index = pTele[c].m_Type;
			if(Index && (RenderFlags & LAYERRENDERFLAG_TRANSPARENT))
			{
				if((int)Index != LastIndex)
				{
					FlushBatch();
					if(UseTextureArrays)
					{
						Graphics()->QuadsSetSubsetFree(0, 0, 1, 0, 1, 1, 0, 1, Index);
					}
					else
					{
						int tx = Index % 16;
						int ty = Index / 16;
						int Px0 = tx * (1024 / 16);
						int Py0 = ty * (1024 / 16);
						int Px1 = Px0 + (1024 / 16) - 1;
						int Py1 = Py0 + (1024 / 16) - 1;
						Graphics()->QuadsSetSubsetFree(
							Nudge + Px0 / TexSize + Frac, Nudge + Py0 / TexSize + Frac,
							Nudge + Px1 / TexSize - Frac, Nudge + Py0 / TexSize + Frac,
							Nudge + Px1 / TexSize - Frac, Nudge + Py1 / TexSize - Frac,
							Nudge + Px0 / TexSize + Frac, Nudge + Py1 / TexSize - Frac);
					}
					LastIndex = (int)Index;
				}

				aQuadBatch[BatchCount++] = IGraphics::CQuadItem(x * Scale, y * Scale, Scale, Scale);
				if(BatchCount >= BATCH_SIZE)
					FlushBatch();
			}
		}
	}

	FlushBatch();

	if(UseTextureArrays)
		Graphics()->QuadsTex3DEnd();
	else
		Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderMap::RenderSwitchmap(CSwitchTile *pSwitchTile, int w, int h, float Scale, ColorRGBA Color, int RenderFlags)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / (ScreenX1 - ScreenX0) * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	const bool UseTextureArrays = Graphics()->HasTextureArraysSupport();

	if(UseTextureArrays)
		Graphics()->QuadsTex3DBegin();
	else
		Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	constexpr int BATCH_SIZE = 512;
	IGraphics::CQuadItem aQuadBatch[BATCH_SIZE];
	int BatchCount = 0;
	int LastSubsetKey = -1;

	auto FlushBatch = [&]() {
		if(BatchCount > 0)
		{
			if(UseTextureArrays)
				Graphics()->QuadsTex3DDrawTL(aQuadBatch, BatchCount);
			else
				Graphics()->QuadsDrawTL(aQuadBatch, BatchCount);
			BatchCount = 0;
		}
	};

	for(int y = StartY; y < EndY; y++)
	{
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x, my = y;

			if(RenderFlags & TILERENDERFLAG_EXTEND)
			{
				if(mx < 0) mx = 0;
				if(mx >= w) mx = w - 1;
				if(my < 0) my = 0;
				if(my >= h) my = h - 1;
			}
			else
			{
				if(mx < 0 || mx >= w || my < 0 || my >= h)
					continue;
			}

			int c = mx + my * w;
			unsigned char Index = pSwitchTile[c].m_Type;
			if(Index)
			{
				if(Index == TILE_SWITCHTIMEDOPEN)
					Index = 8;

				unsigned char Flags = pSwitchTile[c].m_Flags;

				bool Render = false;
				if(Flags & TILEFLAG_OPAQUE)
				{
					if(RenderFlags & LAYERRENDERFLAG_OPAQUE) Render = true;
				}
				else
				{
					if(RenderFlags & LAYERRENDERFLAG_TRANSPARENT) Render = true;
				}

				if(Render)
				{
					float cx0, cy0, cx1, cy1, cx2, cy2, cx3, cy3;

					if(UseTextureArrays)
					{
						cx0 = 0; cy0 = 0; cx1 = 1; cy1 = 0;
						cx2 = 1; cy2 = 1; cx3 = 0; cy3 = 1;
					}
					else
					{
						int tx = Index % 16;
						int ty = Index / 16;
						int Px0 = tx * (1024 / 16);
						int Py0 = ty * (1024 / 16);
						int Px1 = Px0 + (1024 / 16) - 1;
						int Py1 = Py0 + (1024 / 16) - 1;
						cx0 = Nudge + Px0 / TexSize + Frac;
						cy0 = Nudge + Py0 / TexSize + Frac;
						cx1 = Nudge + Px1 / TexSize - Frac;
						cy1 = Nudge + Py0 / TexSize + Frac;
						cx2 = Nudge + Px1 / TexSize - Frac;
						cy2 = Nudge + Py1 / TexSize - Frac;
						cx3 = Nudge + Px0 / TexSize + Frac;
						cy3 = Nudge + Py1 / TexSize - Frac;
					}

					if(Flags & TILEFLAG_XFLIP) { float t = cx0; cx0 = cx1; cx1 = t; t = cx2; cx2 = cx3; cx3 = t; }
					if(Flags & TILEFLAG_YFLIP) { float t = cy0; cy0 = cy3; cy3 = t; t = cy1; cy1 = cy2; cy2 = t; }
					if(Flags & TILEFLAG_ROTATE)
					{
						float t = cx0; cx0 = cx3; cx3 = cx2; cx2 = cx1; cx1 = t;
						t = cy0; cy0 = cy3; cy3 = cy2; cy2 = cy1; cy1 = t;
					}

					int SubsetKey = Index | ((Flags & (TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE)) << 8);

					if(SubsetKey != LastSubsetKey)
					{
						FlushBatch();
						if(UseTextureArrays)
							Graphics()->QuadsSetSubsetFree(cx0, cy0, cx1, cy1, cx2, cy2, cx3, cy3, Index);
						else
							Graphics()->QuadsSetSubsetFree(cx0, cy0, cx1, cy1, cx2, cy2, cx3, cy3);
						LastSubsetKey = SubsetKey;
					}

					aQuadBatch[BatchCount++] = IGraphics::CQuadItem(x * Scale, y * Scale, Scale, Scale);
					if(BatchCount >= BATCH_SIZE)
						FlushBatch();
				}
			}
		}
	}

	FlushBatch();

	if(UseTextureArrays)
		Graphics()->QuadsTex3DEnd();
	else
		Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderMap::RenderTunemap(CTuneTile *pTune, int w, int h, float Scale, ColorRGBA Color, int RenderFlags)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	float TilePixelSize = 1024 / 32.0f;
	float FinalTileSize = Scale / (ScreenX1 - ScreenX0) * Graphics()->ScreenWidth();
	float FinalTilesetScale = FinalTileSize / TilePixelSize;

	const bool UseTextureArrays = Graphics()->HasTextureArraysSupport();

	if(UseTextureArrays)
		Graphics()->QuadsTex3DBegin();
	else
		Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);

	int StartY = (int)(ScreenY0 / Scale) - 1;
	int StartX = (int)(ScreenX0 / Scale) - 1;
	int EndY = (int)(ScreenY1 / Scale) + 1;
	int EndX = (int)(ScreenX1 / Scale) + 1;

	float TexSize = 1024.0f;
	float Frac = (1.25f / TexSize) * (1 / FinalTilesetScale);
	float Nudge = (0.5f / TexSize) * (1 / FinalTilesetScale);

	constexpr int BATCH_SIZE = 512;
	IGraphics::CQuadItem aQuadBatch[BATCH_SIZE];
	int BatchCount = 0;
	int LastIndex = -1;

	auto FlushBatch = [&]() {
		if(BatchCount > 0)
		{
			if(UseTextureArrays)
				Graphics()->QuadsTex3DDrawTL(aQuadBatch, BatchCount);
			else
				Graphics()->QuadsDrawTL(aQuadBatch, BatchCount);
			BatchCount = 0;
		}
	};

	for(int y = StartY; y < EndY; y++)
	{
		for(int x = StartX; x < EndX; x++)
		{
			int mx = x, my = y;

			if(RenderFlags & TILERENDERFLAG_EXTEND)
			{
				if(mx < 0) mx = 0;
				if(mx >= w) mx = w - 1;
				if(my < 0) my = 0;
				if(my >= h) my = h - 1;
			}
			else
			{
				if(mx < 0 || mx >= w || my < 0 || my >= h)
					continue;
			}

			int c = mx + my * w;
			unsigned char Index = pTune[c].m_Type;
			if(Index && (RenderFlags & LAYERRENDERFLAG_TRANSPARENT))
			{
				if((int)Index != LastIndex)
				{
					FlushBatch();
					if(UseTextureArrays)
					{
						Graphics()->QuadsSetSubsetFree(0, 0, 1, 0, 1, 1, 0, 1, Index);
					}
					else
					{
						int tx = Index % 16;
						int ty = Index / 16;
						int Px0 = tx * (1024 / 16);
						int Py0 = ty * (1024 / 16);
						int Px1 = Px0 + (1024 / 16) - 1;
						int Py1 = Py0 + (1024 / 16) - 1;
						Graphics()->QuadsSetSubsetFree(
							Nudge + Px0 / TexSize + Frac, Nudge + Py0 / TexSize + Frac,
							Nudge + Px1 / TexSize - Frac, Nudge + Py0 / TexSize + Frac,
							Nudge + Px1 / TexSize - Frac, Nudge + Py1 / TexSize - Frac,
							Nudge + Px0 / TexSize + Frac, Nudge + Py1 / TexSize - Frac);
					}
					LastIndex = (int)Index;
				}

				aQuadBatch[BatchCount++] = IGraphics::CQuadItem(x * Scale, y * Scale, Scale, Scale);
				if(BatchCount >= BATCH_SIZE)
					FlushBatch();
			}
		}
	}

	FlushBatch();

	if(UseTextureArrays)
		Graphics()->QuadsTex3DEnd();
	else
		Graphics()->QuadsEnd();
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CRenderMap::RenderDebugClip(float ClipX, float ClipY, float ClipW, float ClipH, ColorRGBA Color, float Zoom, const char *pLabel)
{
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(Color);
	IGraphics::CLineItem aLineItems[] = {
		IGraphics::CLineItem(ClipX, ClipY, ClipX, ClipY + ClipH),
		IGraphics::CLineItem(ClipX + ClipW, ClipY, ClipX + ClipW, ClipY + ClipH),
		IGraphics::CLineItem(ClipX, ClipY, ClipX + ClipW, ClipY),
		IGraphics::CLineItem(ClipX, ClipY + ClipH, ClipX + ClipW, ClipY + ClipH),
	};
	Graphics()->LinesDraw(aLineItems, std::size(aLineItems));
	Graphics()->LinesEnd();

	TextRender()->TextColor(Color);
	TextRender()->Text(ClipX, ClipY, std::min(12.0f * Zoom, 20.0f), pLabel, ClipW);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}