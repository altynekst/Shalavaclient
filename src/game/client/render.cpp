#include "render.h"

#include "animstate.h"

#include <base/math.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <generated/client_data.h>
#include <generated/client_data7.h>
#include <generated/protocol.h>
#include <generated/protocol7.h>

#include <game/mapitems.h>

#include <cmath>

static constexpr float INV_64 = 1.0f / 64.0f;
static constexpr float INV_32 = 1.0f / 32.0f;
static constexpr float TWO_PI = pi * 2.0f;

CSkinDescriptor::CSkinDescriptor()
{
	Reset();
}

void CSkinDescriptor::Reset()
{
	m_Flags = 0;
	m_aSkinName[0] = '\0';
	for(auto &Sixup : m_aSixup)
		Sixup.Reset();
}

bool CSkinDescriptor::IsValid() const
{
	return (m_Flags & (FLAG_SIX | FLAG_SEVEN)) != 0;
}

bool CSkinDescriptor::operator==(const CSkinDescriptor &Other) const
{
	if(m_Flags != Other.m_Flags)
		return false;

	if((m_Flags & FLAG_SIX) && str_comp(m_aSkinName, Other.m_aSkinName) != 0)
		return false;

	if(m_Flags & FLAG_SEVEN)
	{
		for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
		{
			if(m_aSixup[Dummy] != Other.m_aSixup[Dummy])
				return false;
		}
	}

	return true;
}

void CSkinDescriptor::CSixup::Reset()
{
	for(auto &aSkinPartName : m_aaSkinPartNames)
		aSkinPartName[0] = '\0';
	m_BotDecoration = false;
	m_XmasHat = false;
}

bool CSkinDescriptor::CSixup::operator==(const CSixup &Other) const
{
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		if(str_comp(m_aaSkinPartNames[Part], Other.m_aaSkinPartNames[Part]) != 0)
			return false;
	}
	return m_BotDecoration == Other.m_BotDecoration && m_XmasHat == Other.m_XmasHat;
}

void CRenderTools::Init(IGraphics *pGraphics, ITextRender *pTextRender)
{
	m_pGraphics = pGraphics;
	m_pTextRender = pTextRender;
	m_TeeQuadContainerIndex = Graphics()->CreateQuadContainer(false);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f);

	for(int i = 0; i < 5; i++)
	{
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f * 0.4f);
	}

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, -32.f, -16.f, 64.f, 32.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, -32.f, -16.f, 64.f, 32.f);

	Graphics()->QuadsSetSubsetFree(1, 0, 0, 0, 0, 1, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, -32.f, -16.f, 64.f, 32.f);
	Graphics()->QuadsSetSubsetFree(1, 0, 0, 0, 0, 1, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, -32.f, -16.f, 64.f, 32.f);

	Graphics()->QuadContainerUpload(m_TeeQuadContainerIndex);
}

void CRenderTools::RenderCursor(vec2 Center, float Size) const
{
	Graphics()->WrapClamp();
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	IGraphics::CQuadItem QuadItem(Center.x, Center.y, Size, Size);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
	Graphics()->WrapNormal();
}

void CRenderTools::RenderIcon(int ImageId, int SpriteId, const CUIRect *pRect, const ColorRGBA *pColor) const
{
	Graphics()->TextureSet(g_pData->m_aImages[ImageId].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SelectSprite(SpriteId);
	if(pColor)
		Graphics()->SetColor(pColor->r * pColor->a, pColor->g * pColor->a, pColor->b * pColor->a, pColor->a);
	IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

void CRenderTools::GetRenderTeeAnimScaleAndBaseSize(const CTeeRenderInfo *pInfo, float &AnimScale, float &BaseSize)
{
	BaseSize = pInfo->m_Size;
	AnimScale = BaseSize * INV_64;
}

void CRenderTools::GetRenderTeeBodyScale(float BaseSize, float &BodyScale)
{
	BodyScale = (g_Config.m_ClFatSkins ? BaseSize * 1.3f : BaseSize) * INV_64;
}

void CRenderTools::GetRenderTeeFeetScale(float BaseSize, float &FeetScaleWidth, float &FeetScaleHeight)
{
	FeetScaleWidth = BaseSize * INV_64;
	FeetScaleHeight = BaseSize * 0.5f * INV_32;
}

void CRenderTools::GetRenderTeeBodySize(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, vec2 &BodyOffset, float &Width, float &Height)
{
	float AnimScale, BaseSize;
	GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);

	float BodyScale;
	GetRenderTeeBodyScale(BaseSize, BodyScale);

	const auto &Metrics = pInfo->m_SkinMetrics.m_Body;
	const float Scale64 = 64.0f * BodyScale;
	Width = Metrics.WidthNormalized() * Scale64;
	Height = Metrics.HeightNormalized() * Scale64;
	BodyOffset.x = Metrics.OffsetXNormalized() * Scale64;
	BodyOffset.y = Metrics.OffsetYNormalized() * Scale64;
}

void CRenderTools::GetRenderTeeFeetSize(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, vec2 &FeetOffset, float &Width, float &Height)
{
	float AnimScale, BaseSize;
	GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);

	float FeetScaleWidth, FeetScaleHeight;
	GetRenderTeeFeetScale(BaseSize, FeetScaleWidth, FeetScaleHeight);

	const auto &Metrics = pInfo->m_SkinMetrics.m_Feet;
	Width = Metrics.WidthNormalized() * 64.0f * FeetScaleWidth;
	Height = Metrics.HeightNormalized() * 32.0f * FeetScaleHeight;
	FeetOffset.x = Metrics.OffsetXNormalized() * 64.0f * FeetScaleWidth;
	FeetOffset.y = Metrics.OffsetYNormalized() * 32.0f * FeetScaleHeight;
}

void CRenderTools::GetRenderTeeOffsetToRenderedTee(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, vec2 &TeeOffsetToMid)
{
	if(pInfo->m_aSixup[g_Config.m_ClDummy].PartTexture(protocol7::SKINPART_BODY).IsValid())
	{
		TeeOffsetToMid = vec2(0.0f, pInfo->m_Size * 0.12f);
		return;
	}

	float AnimScale, BaseSize;
	GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);

	const float AssumedScale = BaseSize * INV_64;

	const CAnimKeyframe *pFrontFoot = pAnim->GetFrontFoot();
	const CAnimKeyframe *pBackFoot = pAnim->GetBackFoot();
	float FeetMaxY = maximum(pFrontFoot->m_Y * AnimScale, pBackFoot->m_Y * AnimScale);

	vec2 BodyOffset;
	float BodyWidth, BodyHeight;
	GetRenderTeeBodySize(pAnim, pInfo, BodyOffset, BodyWidth, BodyHeight);

	float MinY = -32.0f * AssumedScale + pAnim->GetBody()->m_Y * AnimScale + BodyOffset.y;

	vec2 FeetOffset;
	float FeetWidth, FeetHeight;
	GetRenderTeeFeetSize(pAnim, pInfo, FeetOffset, FeetWidth, FeetHeight);

	float MaxY = MinY + BodyHeight;
	MaxY = maximum(MaxY, (-16.0f * AssumedScale + FeetMaxY) + FeetOffset.y + FeetHeight);

	float MidOfRendered = MinY + (MaxY - MinY) * 0.5f;

	TeeOffsetToMid.x = 0;
	TeeOffsetToMid.y = -MidOfRendered;
}

void CRenderTools::RenderTee(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha) const
{
	if(pInfo->m_aSixup[g_Config.m_ClDummy].PartTexture(protocol7::SKINPART_BODY).IsValid())
		RenderTee7(pAnim, pInfo, Emote, Dir, Pos, Alpha);
	else
		RenderTee6(pAnim, pInfo, Emote, Dir, Pos, Alpha);

	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	Graphics()->QuadsSetRotation(0);
}

void CRenderTools::RenderTee7(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha) const
{
	const auto &Sixup = pInfo->m_aSixup[g_Config.m_ClDummy];
	const bool IsBot = Sixup.m_BotTexture.IsValid();
	const float AnimScale = pInfo->m_Size * INV_64;
	const float BaseSize = pInfo->m_Size;
	const float BodyAngle = pAnim->GetBody()->m_Angle * TWO_PI;
	const vec2 BodyPos = Pos + vec2(pAnim->GetBody()->m_X, pAnim->GetBody()->m_Y) * AnimScale;
	const IGraphics::CQuadItem BodyItem(BodyPos.x, BodyPos.y, BaseSize, BaseSize);
	const float FootSize = BaseSize / 2.1f;
	const IGraphics::CTextureHandle &BodyTexture = Sixup.PartTexture(protocol7::SKINPART_BODY);
	const IGraphics::CTextureHandle &FeetTexture = Sixup.PartTexture(protocol7::SKINPART_FEET);
	const IGraphics::CTextureHandle &DecorationTexture = Sixup.PartTexture(protocol7::SKINPART_DECORATION);
	const IGraphics::CTextureHandle &MarkingTexture = Sixup.PartTexture(protocol7::SKINPART_MARKING);
	const IGraphics::CTextureHandle &EyesTexture = Sixup.PartTexture(protocol7::SKINPART_EYES);

	const bool HasDecoration = DecorationTexture.IsValid();
	const bool HasMarking = MarkingTexture.IsValid();
	const bool HasHat = Sixup.m_HatTexture.IsValid();

	const bool Indicate = !pInfo->m_GotAirJump && g_Config.m_ClAirjumpindicator;
	const float FeetColorScale = Indicate ? 0.5f : 1.0f;
	const ColorRGBA &FeetColor = Sixup.m_aColors[protocol7::SKINPART_FEET];

	for(int Pass = 0; Pass < 2; Pass++)
	{
		const bool OutLine = (Pass == 0);

		for(int Filling = 0; Filling < 2; Filling++)
		{
			IGraphics::CQuadItem Item;

			if(IsBot && !OutLine)
			{
				const IGraphics::CQuadItem BotItem(BodyPos.x + (2.f / 3.f) * AnimScale, BodyPos.y + (-16 + 2.f / 3.f) * AnimScale, BaseSize, BaseSize);

				Graphics()->TextureSet(Sixup.m_BotTexture);
				Graphics()->QuadsBegin();
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
				Graphics()->SelectSprite7(client_data7::SPRITE_TEE_BOT_BACKGROUND);
				Item = BotItem;
				Graphics()->QuadsDraw(&Item, 1);
				Graphics()->SelectSprite7(client_data7::SPRITE_TEE_BOT_FOREGROUND);
				Item = BotItem;
				Graphics()->QuadsDraw(&Item, 1);
				Graphics()->SetColor(Sixup.m_BotColor.WithAlpha(Alpha));
				Graphics()->SelectSprite7(client_data7::SPRITE_TEE_BOT_GLOW);
				Item = BotItem;
				Graphics()->QuadsDraw(&Item, 1);
				Graphics()->QuadsEnd();
			}

			if(HasDecoration)
			{
				Graphics()->TextureSet(DecorationTexture);
				Graphics()->QuadsBegin();
				Graphics()->QuadsSetRotation(BodyAngle);
				Graphics()->SetColor(Sixup.m_aColors[protocol7::SKINPART_DECORATION].WithAlpha(Alpha));
				Graphics()->SelectSprite7(OutLine ? client_data7::SPRITE_TEE_DECORATION_OUTLINE : client_data7::SPRITE_TEE_DECORATION);
				Item = BodyItem;
				Graphics()->QuadsDraw(&Item, 1);
				Graphics()->QuadsEnd();
			}

			Graphics()->TextureSet(BodyTexture);
			Graphics()->QuadsBegin();
			Graphics()->QuadsSetRotation(BodyAngle);
			if(OutLine)
			{
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
				Graphics()->SelectSprite7(client_data7::SPRITE_TEE_BODY_OUTLINE);
			}
			else
			{
				Graphics()->SetColor(Sixup.m_aColors[protocol7::SKINPART_BODY].WithAlpha(Alpha));
				Graphics()->SelectSprite7(client_data7::SPRITE_TEE_BODY);
			}
			Item = BodyItem;
			Graphics()->QuadsDraw(&Item, 1);
			Graphics()->QuadsEnd();

			if(HasMarking && !OutLine)
			{
				Graphics()->TextureSet(MarkingTexture);
				Graphics()->QuadsBegin();
				Graphics()->QuadsSetRotation(BodyAngle);
				const ColorRGBA &MC = Sixup.m_aColors[protocol7::SKINPART_MARKING];
				Graphics()->SetColor(MC.r * MC.a, MC.g * MC.a, MC.b * MC.a, MC.a * Alpha);
				Graphics()->SelectSprite7(client_data7::SPRITE_TEE_MARKING);
				Item = BodyItem;
				Graphics()->QuadsDraw(&Item, 1);
				Graphics()->QuadsEnd();
			}

			if(!OutLine)
			{
				Graphics()->TextureSet(BodyTexture);
				Graphics()->QuadsBegin();
				Graphics()->QuadsSetRotation(BodyAngle);
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
				Graphics()->SelectSprite7(client_data7::SPRITE_TEE_BODY_SHADOW);
				Item = BodyItem;
				Graphics()->QuadsDraw(&Item, 1);
				Graphics()->SelectSprite7(client_data7::SPRITE_TEE_BODY_UPPER_OUTLINE);
				Item = BodyItem;
				Graphics()->QuadsDraw(&Item, 1);
				Graphics()->QuadsEnd();
			}

			Graphics()->TextureSet(EyesTexture);
			Graphics()->QuadsBegin();
			Graphics()->QuadsSetRotation(BodyAngle);
			if(IsBot)
			{
				Graphics()->SetColor(Sixup.m_BotColor.WithAlpha(Alpha));
				Emote = EMOTE_SURPRISE;
			}
			else
			{
				Graphics()->SetColor(Sixup.m_aColors[protocol7::SKINPART_EYES].WithAlpha(Alpha));
			}
			if(Pass == 1)
			{
				int EyeSprite;
				switch(Emote)
				{
				case EMOTE_PAIN: EyeSprite = client_data7::SPRITE_TEE_EYES_PAIN; break;
				case EMOTE_HAPPY: EyeSprite = client_data7::SPRITE_TEE_EYES_HAPPY; break;
				case EMOTE_SURPRISE: EyeSprite = client_data7::SPRITE_TEE_EYES_SURPRISE; break;
				case EMOTE_ANGRY: EyeSprite = client_data7::SPRITE_TEE_EYES_ANGRY; break;
				default: EyeSprite = client_data7::SPRITE_TEE_EYES_NORMAL; break;
				}
				Graphics()->SelectSprite7(EyeSprite);

				const float EyeScale = BaseSize * 0.60f;
				const float h = Emote == EMOTE_BLINK ? BaseSize * 0.075f : EyeScale * 0.5f;
				const vec2 Offset = vec2(Dir.x * 0.125f, -0.05f + Dir.y * 0.10f) * BaseSize;
				IGraphics::CQuadItem QuadItem(BodyPos.x + Offset.x, BodyPos.y + Offset.y, EyeScale, h);
				Graphics()->QuadsDraw(&QuadItem, 1);
			}
			Graphics()->QuadsEnd();

			if(!OutLine && HasHat)
			{
				Graphics()->TextureSet(Sixup.m_HatTexture);
				Graphics()->QuadsBegin();
				Graphics()->QuadsSetRotation(BodyAngle);
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
				int Flag = Dir.x < 0.0f ? IGraphics::SPRITE_FLAG_FLIP_X : 0;
				static constexpr int s_aHatSprites[] = {
					client_data7::SPRITE_TEE_HATS_TOP1,
					client_data7::SPRITE_TEE_HATS_TOP2,
					client_data7::SPRITE_TEE_HATS_SIDE1,
					client_data7::SPRITE_TEE_HATS_SIDE2};
				Graphics()->SelectSprite7(s_aHatSprites[Sixup.m_HatSpriteIndex], Flag);
				Item = BodyItem;
				Graphics()->QuadsDraw(&Item, 1);
				Graphics()->QuadsEnd();
			}

			Graphics()->TextureSet(FeetTexture);
			Graphics()->QuadsBegin();
			const CAnimKeyframe *pFoot = Filling ? pAnim->GetFrontFoot() : pAnim->GetBackFoot();

			Graphics()->QuadsSetRotation(pFoot->m_Angle * TWO_PI);

			if(OutLine)
			{
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
				Graphics()->SelectSprite7(client_data7::SPRITE_TEE_FOOT_OUTLINE);
			}
			else
			{
				Graphics()->SetColor(
					FeetColor.r * FeetColorScale,
					FeetColor.g * FeetColorScale,
					FeetColor.b * FeetColorScale,
					FeetColor.a * Alpha);
				Graphics()->SelectSprite7(client_data7::SPRITE_TEE_FOOT);
			}

			IGraphics::CQuadItem QuadItem(Pos.x + pFoot->m_X * AnimScale, Pos.y + pFoot->m_Y * AnimScale, FootSize, FootSize);
			Graphics()->QuadsDraw(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}
	}
}

void CRenderTools::RenderTee6(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha) const
{
	const CSkin::CSkinTextures *pSkinTextures = pInfo->m_CustomColoredSkin ? &pInfo->m_ColorableRenderSkin : &pInfo->m_OriginalRenderSkin;

	float AnimScale, BaseSize;
	GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);
	float BodyScale;
	GetRenderTeeBodyScale(BaseSize, BodyScale);
	const float BodyAngle = pAnim->GetBody()->m_Angle * TWO_PI;

	const bool Indicate = !pInfo->m_GotAirJump && g_Config.m_ClAirjumpindicator;
	const float FeetColorScale = Indicate ? 0.5f : 1.0f;
	const vec2 BodyPos = Pos + vec2(pAnim->GetBody()->m_X, pAnim->GetBody()->m_Y) * AnimScale;
	const float w = BaseSize;
	const float h = BaseSize * 0.5f;

	static constexpr int s_aEyeQuadOffset[] = {4, 0, 1, 2, 3, 4};
	static constexpr int s_aTeeEye[] = {
		0,
		SPRITE_TEE_EYE_PAIN - SPRITE_TEE_EYE_NORMAL,
		SPRITE_TEE_EYE_HAPPY - SPRITE_TEE_EYE_NORMAL,
		SPRITE_TEE_EYE_SURPRISE - SPRITE_TEE_EYE_NORMAL,
		SPRITE_TEE_EYE_ANGRY - SPRITE_TEE_EYE_NORMAL,
		0};

	for(int Pass = 0; Pass < 2; Pass++)
	{
		const int OutLine = Pass == 0 ? 1 : 0;

		for(int Filling = 0; Filling < 2; Filling++)
		{
			if(Filling == 1)
			{
				Graphics()->QuadsSetRotation(BodyAngle);
				Graphics()->SetColor(pInfo->m_ColorBody.r, pInfo->m_ColorBody.g, pInfo->m_ColorBody.b, Alpha);
				Graphics()->TextureSet(OutLine == 1 ? pSkinTextures->m_BodyOutline : pSkinTextures->m_Body);
				Graphics()->RenderQuadContainerAsSprite(m_TeeQuadContainerIndex, OutLine, BodyPos.x, BodyPos.y, BodyScale, BodyScale);

				if(Pass == 1)
				{
					int EmoteIdx = (Emote >= 0 && Emote < (int)std::size(s_aEyeQuadOffset)) ? Emote : 0;
					int EyeQuadOffset = s_aEyeQuadOffset[EmoteIdx];
					int TeeEye = s_aTeeEye[EmoteIdx];

					float EyeScale = BaseSize * 0.40f;
					float EyeH = Emote == EMOTE_BLINK ? BaseSize * 0.15f : EyeScale;
					float EyeSeparation = (0.075f - 0.010f * absolute(Dir.x)) * BaseSize;
					vec2 Offset = vec2(Dir.x * 0.125f, -0.05f + Dir.y * 0.10f) * BaseSize;

					const float EyeScaleNorm = EyeScale / (64.f * 0.4f);
					const float EyeHNorm = EyeH / (64.f * 0.4f);

					Graphics()->TextureSet(pSkinTextures->m_aEyes[TeeEye]);
					Graphics()->RenderQuadContainerAsSprite(m_TeeQuadContainerIndex, 2 + EyeQuadOffset, BodyPos.x - EyeSeparation + Offset.x, BodyPos.y + Offset.y, EyeScaleNorm, EyeHNorm);
					Graphics()->RenderQuadContainerAsSprite(m_TeeQuadContainerIndex, 2 + EyeQuadOffset, BodyPos.x + EyeSeparation + Offset.x, BodyPos.y + Offset.y, -EyeScaleNorm, EyeHNorm);
				}
			}

			const CAnimKeyframe *pFoot = Filling ? pAnim->GetFrontFoot() : pAnim->GetBackFoot();

			int QuadOffset = 7;
			if(Dir.x < 0 && pInfo->m_FeetFlipped)
				QuadOffset += 2;

			Graphics()->QuadsSetRotation(pFoot->m_Angle * TWO_PI);

			if(!OutLine)
				++QuadOffset;

			Graphics()->SetColor(pInfo->m_ColorFeet.r * FeetColorScale, pInfo->m_ColorFeet.g * FeetColorScale, pInfo->m_ColorFeet.b * FeetColorScale, Alpha);
			Graphics()->TextureSet(OutLine == 1 ? pSkinTextures->m_FeetOutline : pSkinTextures->m_Feet);
			Graphics()->RenderQuadContainerAsSprite(m_TeeQuadContainerIndex, QuadOffset, Pos.x + pFoot->m_X * AnimScale, Pos.y + pFoot->m_Y * AnimScale, w * INV_64, h * INV_32);
		}
	}
}