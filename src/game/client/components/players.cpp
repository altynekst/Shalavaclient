#include "players.h"

#include <base/color.h>
#include <base/math.h>

#include <engine/client/enums.h>
#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <generated/client_data.h>
#include <generated/client_data7.h>
#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/controls.h>
#include <game/client/components/effects.h>
#include <game/client/components/flow.h>
#include <game/client/components/skins.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

static float CalculateHandAngle(vec2 Dir, float AngleOffset)
{
	float a = angle(Dir);
	return Dir.x < 0.0f ? a - AngleOffset : a + AngleOffset;
}

static vec2 CalculateHandPosition(vec2 CenterPos, vec2 Dir, vec2 PostRotOffset)
{
	vec2 DirY(-Dir.y, Dir.x);
	if(Dir.x < 0.0f)
		DirY = -DirY;
	return CenterPos + Dir + Dir * PostRotOffset.x + DirY * PostRotOffset.y;
}

void CPlayers::RenderHand(const CTeeRenderInfo *pInfo, vec2 CenterPos, vec2 Dir, float AngleOffset, vec2 PostRotOffset, float Alpha)
{
	vec2 HandPos = CalculateHandPosition(CenterPos, Dir, PostRotOffset);
	float HandAngle = CalculateHandAngle(Dir, AngleOffset);
	if(pInfo->m_aSixup[g_Config.m_ClDummy].PartTexture(protocol7::SKINPART_HANDS).IsValid())
		RenderHand7(pInfo, HandPos, HandAngle, Alpha);
	else
		RenderHand6(pInfo, HandPos, HandAngle, Alpha);
}

void CPlayers::RenderHand7(const CTeeRenderInfo *pInfo, vec2 HandPos, float HandAngle, float Alpha)
{
	float BaseSize = 15.0f * (pInfo->m_Size / 64.0f);
	float S = 2.0f * BaseSize;
	IGraphics::CQuadItem Quad(HandPos.x, HandPos.y, S, S);

	const auto &Sixup = pInfo->m_aSixup[g_Config.m_ClDummy];
	Graphics()->TextureSet(Sixup.PartTexture(protocol7::SKINPART_HANDS));
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Sixup.m_aColors[protocol7::SKINPART_HANDS].WithAlpha(Alpha));
	Graphics()->QuadsSetRotation(HandAngle);
	Graphics()->SelectSprite7(client_data7::SPRITE_TEE_HAND_OUTLINE);
	Graphics()->QuadsDraw(&Quad, 1);
	Graphics()->SelectSprite7(client_data7::SPRITE_TEE_HAND);
	Graphics()->QuadsDraw(&Quad, 1);
	Graphics()->QuadsEnd();
}

void CPlayers::RenderHand6(const CTeeRenderInfo *pInfo, vec2 HandPos, float HandAngle, float Alpha)
{
	const CSkin::CSkinTextures *pTex = pInfo->m_CustomColoredSkin ? &pInfo->m_ColorableRenderSkin : &pInfo->m_OriginalRenderSkin;
	int Offset = NUM_WEAPONS * 2;

	Graphics()->SetColor(pInfo->m_ColorBody.WithAlpha(Alpha));
	Graphics()->QuadsSetRotation(HandAngle);
	Graphics()->TextureSet(pTex->m_HandsOutline);
	Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, Offset, HandPos.x, HandPos.y);
	Graphics()->TextureSet(pTex->m_Hands);
	Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, Offset + 1, HandPos.x, HandPos.y);
}

float CPlayers::GetPlayerTargetAngle(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	int ClientId,
	float Intra)
{
	const auto *pGC = GameClient();
	int Dummy = g_Config.m_ClDummy;

	if(pGC->PredictDummy() && pGC->m_aLocalIds[!Dummy] == ClientId)
	{
		const auto &Input = g_Config.m_ClDummyHammer ? pGC->m_HammerInput : pGC->m_DummyInput;
		return angle(vec2(Input.m_TargetX, Input.m_TargetY));
	}

	bool IsLocal = pGC->m_Snap.m_LocalClientId == ClientId;
	bool IsDummyCopy = pGC->PredictDummy() && g_Config.m_ClDummyCopyMoves && pGC->m_aLocalIds[!Dummy] == ClientId;

	if((IsLocal || IsDummyCopy) && !pGC->m_Snap.m_SpecInfo.m_Active && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		vec2 Dir = normalize(vec2((int)pGC->m_Controls.m_aMousePos[Dummy].x, (int)pGC->m_Controls.m_aMousePos[Dummy].y));
		if(Dir == vec2(0.0f, 0.0f))
			Dir = vec2(1.0f, 0.0f);
		return angle(Dir);
	}

	if(ClientId >= 0)
		Intra = Client()->IntraGameTick(Dummy);

	if(ClientId >= 0 && pGC->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo)
	{
		const auto *pExt = &pGC->m_Snap.m_aCharacters[ClientId].m_ExtendedData;
		const auto *pPrevExt = pGC->m_Snap.m_aCharacters[ClientId].m_pPrevExtendedData;
		if(pPrevExt)
			return angle(vec2(mix((float)pPrevExt->m_TargetX, (float)pExt->m_TargetX, Intra),
				mix((float)pPrevExt->m_TargetY, (float)pExt->m_TargetY, Intra)));
		return angle(vec2(pExt->m_TargetX, pExt->m_TargetY));
	}

	float PrevAngle = (float)pPrevChar->m_Angle;
	float CurAngle = (float)pPlayerChar->m_Angle;
	float Threshold = 256.0f * pi;
	float Full = 256.0f * 2.0f * pi;

	if(CurAngle > Threshold && PrevAngle < 0)
		CurAngle -= Full;
	else if(CurAngle < 0 && PrevAngle > Threshold)
		CurAngle += Full;

	return mix(PrevAngle, CurAngle, Intra) / 256.0f;
}

void CPlayers::RenderHookCollLine(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	int ClientId)
{
	if(ClientId != -3)
		dbg_assert(in_range(ClientId, MAX_CLIENTS - 1), "invalid client id (%d)", ClientId);

	auto *pGC = GameClient();
	if(!pGC->m_GameInfo.m_AllowHookColl || ClientId == -3)
		return;

	int Dummy = g_Config.m_ClDummy;
	bool Local = pGC->m_Snap.m_LocalClientId == ClientId;

#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current() && !g_Config.m_ClVideoShowHookCollOther && !Local)
		return;
#endif

	CNetObj_Character Player = *pPlayerChar;
	CNetObj_Character Prev = *pPrevChar;

	bool Aim = (Player.m_PlayerFlags & PLAYERFLAG_AIM);
	if(!Client()->ServerCapAnyPlayerFlag())
	{
		for(int i = 0; i < NUM_DUMMIES; i++)
		{
			if(ClientId == pGC->m_aLocalIds[i])
			{
				Aim = pGC->m_Controls.m_aShowHookColl[i];
				break;
			}
		}
	}

	if(pGC->PredictDummy() && g_Config.m_ClDummyCopyMoves && pGC->m_aLocalIds[!Dummy] == ClientId)
		Aim = false;

	int ShowOwn = g_Config.m_ClShowHookCollOwn;
	int ShowOther = g_Config.m_ClShowHookCollOther;
	int ShowSetting = Local ? ShowOwn : ShowOther;

	bool AlwaysRender = ShowSetting == 2;
	bool RenderPlayer = Aim && ShowSetting > 0;

	if(Local && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		RenderPlayer = pGC->m_Controls.m_aShowHookColl[Dummy] && ShowOwn > 0;

	if(pGC->PredictDummy() && g_Config.m_ClDummyCopyMoves &&
		pGC->m_aLocalIds[!Dummy] == ClientId && pGC->m_Controls.m_aShowHookColl[Dummy] &&
		Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		RenderPlayer = ShowOther > 0;
	}

	if(!AlwaysRender && !RenderPlayer)
		return;

	const auto &ClientData = pGC->m_aClients[ClientId];
	float Intra = ClientData.m_IsPredicted ? Client()->PredIntraGameTick(Dummy) : Client()->IntraGameTick(Dummy);
	float Angle = GetPlayerTargetAngle(&Prev, &Player, ClientId, Intra);

	vec2 Direction = direction(Angle);
	vec2 Position = ClientData.m_RenderPos;

	static constexpr float HOOK_START_DISTANCE = CCharacterCore::PhysicalSize() * 1.5f;
	float HookLength = (float)ClientData.m_Predicted.m_Tuning.m_HookLength;
	float HookFireSpeed = (float)ClientData.m_Predicted.m_Tuning.m_HookFireSpeed;

	if(HookLength < HOOK_START_DISTANCE || HookFireSpeed <= 0.0f)
		return;

	vec2 QuantizedDirection = Direction;
	vec2 StartOffset = Direction * HOOK_START_DISTANCE;
	vec2 BasePos = Position;
	vec2 LineStartPos = BasePos + StartOffset;
	vec2 SegmentStartPos = LineStartPos;

	ColorRGBA HookCollColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorNoColl));
	std::vector<IGraphics::CLineItem> vLineSegments;

	const int MaxHookTicks = 5 * Client()->GameTickSpeed();

	auto AddHookPlayerSegment = [&](const vec2 &Start, const vec2 &End, const vec2 &PlayerPos, const vec2 &HitPos) {
		HookCollColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorTeeColl));
		vec2 aIntersections[2];
		int Num = intersect_line_circle(Start, End, PlayerPos, CCharacterCore::PhysicalSize() * 1.45f / 2.0f, aIntersections);
		if(Num == 2)
		{
			vLineSegments.emplace_back(Start, distance(Position, aIntersections[0]) < distance(Position, aIntersections[1]) ? aIntersections[0] : aIntersections[1]);
		}
		else if(Num == 1)
			vLineSegments.emplace_back(Start, aIntersections[0]);
		else
			vLineSegments.emplace_back(Start, HitPos);
	};

	int HookTick;
	bool HookEnteredTelehook = false;
	for(HookTick = 0; HookTick < MaxHookTicks; ++HookTick)
	{
		int Tele;
		vec2 HitPos, IntersectedPlayerPos;
		vec2 SegmentEndPos = SegmentStartPos + QuantizedDirection * HookFireSpeed;

		if(distance(BasePos, SegmentEndPos) > HookLength)
		{
			if(!HookEnteredTelehook)
			{
				vec2 RetractEnd = BasePos + normalize(SegmentEndPos - BasePos) * HookLength;
				int Hit = Collision()->IntersectLineTeleHook(SegmentStartPos, RetractEnd, &HitPos, nullptr, &Tele);

				if(pGC->IntersectCharacter(SegmentStartPos, HitPos, RetractEnd, ClientId, &IntersectedPlayerPos) != -1)
				{
					AddHookPlayerSegment(LineStartPos, SegmentEndPos, IntersectedPlayerPos, RetractEnd);
					break;
				}
				if(Hit && Hit != TILE_TELEINHOOK)
				{
					vLineSegments.emplace_back(LineStartPos, SegmentStartPos);
					break;
				}
			}
			vLineSegments.emplace_back(LineStartPos, SegmentStartPos);
			break;
		}

		int Hit = Collision()->IntersectLineTeleHook(SegmentStartPos, SegmentEndPos, &HitPos, nullptr, &Tele);

		if(pGC->IntersectCharacter(SegmentStartPos, HitPos, SegmentEndPos, ClientId, &IntersectedPlayerPos) != -1)
		{
			AddHookPlayerSegment(LineStartPos, HitPos, IntersectedPlayerPos, SegmentEndPos);
			break;
		}

		if(!Hit)
		{
			SegmentStartPos = SegmentEndPos;
			SegmentStartPos.x = round_to_int(SegmentStartPos.x);
			SegmentStartPos.y = round_to_int(SegmentStartPos.y);
			if(HookTick == 0)
			{
				QuantizedDirection.x = round_to_int(QuantizedDirection.x * 256.0f) / 256.0f;
				QuantizedDirection.y = round_to_int(QuantizedDirection.y * 256.0f) / 256.0f;
			}
			continue;
		}

		if(Hit != TILE_TELEINHOOK)
		{
			if(Hit != TILE_NOHOOK)
				HookCollColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorHookableColl));
			vLineSegments.emplace_back(LineStartPos, HitPos);
			break;
		}

		vLineSegments.emplace_back(LineStartPos, HitPos);
		HookEnteredTelehook = true;

		const auto &vTeleOuts = Collision()->TeleOuts(Tele - 1);
		if(vTeleOuts.empty())
		{
			HookCollColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorHookableColl));
			break;
		}
		if(vTeleOuts.size() > 1)
		{
			HookCollColor = color_invert(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorTeeColl)));
			break;
		}

		BasePos = vTeleOuts[0];
		LineStartPos = BasePos;
		SegmentStartPos = BasePos + Direction * HOOK_START_DISTANCE;
		SegmentStartPos.x = round_to_int(SegmentStartPos.x);
		SegmentStartPos.y = round_to_int(SegmentStartPos.y);
		if(HookTick == 0)
		{
			QuantizedDirection.x = round_to_int(QuantizedDirection.x * 256.0f) / 256.0f;
			QuantizedDirection.y = round_to_int(QuantizedDirection.y * 256.0f) / 256.0f;
		}
	}

	if(HookTick >= MaxHookTicks && vLineSegments.empty())
	{
		HookCollColor = color_invert(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorTeeColl)));
		vLineSegments.emplace_back(LineStartPos, BasePos + QuantizedDirection * HookLength);
	}

	vLineSegments.emplace_back(Position, Position + StartOffset);

	if(AlwaysRender && RenderPlayer)
		HookCollColor = color_invert(HookCollColor);

	int HookCollSize = Local ? g_Config.m_ClHookCollSize : g_Config.m_ClHookCollSizeOther;
	float Alpha = pGC->IsOtherTeam(ClientId) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;
	Alpha *= g_Config.m_ClHookCollAlpha / 100.0f;
	if(Alpha <= 0.0f)
		return;

	Graphics()->TextureClear();
	if(HookCollSize > 0)
	{
		float LineWidth = 0.5f + (float)(HookCollSize - 1) * 0.25f;
		vec2 Perp = normalize(vec2(Direction.y, -Direction.x)) * pGC->m_Camera.m_Zoom;

		std::vector<IGraphics::CFreeformItem> vQuads;
		vQuads.reserve(vLineSegments.size());
		for(const auto &Seg : vLineSegments)
		{
			vec2 A(Seg.m_X0, Seg.m_Y0), B(Seg.m_X1, Seg.m_Y1);
			vec2 P0 = B + Perp * -LineWidth, P1 = B + Perp * LineWidth;
			vec2 P2 = A + Perp * -LineWidth, P3 = A + Perp * LineWidth;
			vQuads.emplace_back(P0.x, P0.y, P1.x, P1.y, P2.x, P2.y, P3.x, P3.y);
		}
		Graphics()->QuadsBegin();
		Graphics()->SetColor(HookCollColor.WithAlpha(Alpha));
		Graphics()->QuadsDrawFreeform(vQuads.data(), vQuads.size());
		Graphics()->QuadsEnd();
	}
	else
	{
		Graphics()->LinesBegin();
		Graphics()->SetColor(HookCollColor.WithAlpha(Alpha));
		Graphics()->LinesDraw(vLineSegments.data(), vLineSegments.size());
		Graphics()->LinesEnd();
	}
}

void CPlayers::RenderHook(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CTeeRenderInfo *pRenderInfo,
	int ClientId,
	float Intra)
{
	if(pPrevChar->m_HookState <= 0 || pPlayerChar->m_HookState <= 0)
		return;

	auto *pGC = GameClient();
	int Dummy = g_Config.m_ClDummy;
	CNetObj_Character Prev = *pPrevChar;
	CNetObj_Character Player = *pPlayerChar;
	CTeeRenderInfo RenderInfo = *pRenderInfo;

	if(Player.m_HookedPlayer != -1 && !pGC->m_Snap.m_aCharacters[Player.m_HookedPlayer].m_Active)
		return;

	if(ClientId >= 0)
		Intra = pGC->m_aClients[ClientId].m_IsPredicted ? Client()->PredIntraGameTick(Dummy) : Client()->IntraGameTick(Dummy);

	bool OtherTeam = pGC->IsOtherTeam(ClientId);
	float Alpha = (OtherTeam || ClientId < 0) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;
	if(ClientId == -3)
		Alpha = 1.0f;
	else if(ClientId == -2)
		Alpha = g_Config.m_ClRaceGhostAlpha / 100.0f;

	RenderInfo.m_Size = 64.0f;

	vec2 Position;
	if(in_range(ClientId, MAX_CLIENTS - 1))
		Position = pGC->m_aClients[ClientId].m_RenderPos;
	else
		Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), Intra);

	Graphics()->SetColor(1.0f, 1.0f, 1.0f, (ClientId < 0 && ClientId != -3) ? 0.5f : 1.0f);

	vec2 HookPos;
	if(in_range(Player.m_HookedPlayer, MAX_CLIENTS - 1))
		HookPos = pGC->m_aClients[Player.m_HookedPlayer].m_RenderPos;
	else
		HookPos = mix(vec2(Prev.m_HookX, Prev.m_HookY), vec2(Player.m_HookX, Player.m_HookY), Intra);

	float d = distance(Position, HookPos);
	vec2 Dir = normalize(Position - HookPos);

	int QuadOffset = NUM_WEAPONS * 2 + 2;
	Graphics()->TextureSet(pGC->m_GameSkin.m_SpriteHookHead);
	Graphics()->QuadsSetRotation(angle(Dir) + pi);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
	Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, HookPos.x, HookPos.y);

	++QuadOffset;
	static IGraphics::SRenderSpriteInfo s_aChainInfo[1024];
	int Count = 0;
	float RotVal = angle(Dir) + pi;
	for(float f = 24; f < d && Count < 1024; f += 24, ++Count)
	{
		vec2 p = HookPos + Dir * f;
		s_aChainInfo[Count].m_Pos[0] = p.x;
		s_aChainInfo[Count].m_Pos[1] = p.y;
		s_aChainInfo[Count].m_Scale = 1;
		s_aChainInfo[Count].m_Rotation = RotVal;
	}
	Graphics()->TextureSet(pGC->m_GameSkin.m_SpriteHookChain);
	Graphics()->RenderQuadContainerAsSpriteMultiple(m_WeaponEmoteQuadContainerIndex, QuadOffset, Count, s_aChainInfo);

	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);

	RenderHand(&RenderInfo, Position, normalize(HookPos - Position), -pi / 2, vec2(20, 0), Alpha);
}

void CPlayers::RenderPlayer(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CTeeRenderInfo *pRenderInfo,
	int ClientId,
	float Intra)
{
	auto *pGC = GameClient();
	int Dummy = g_Config.m_ClDummy;

	CNetObj_Character Prev = *pPrevChar;
	CNetObj_Character Player = *pPlayerChar;
	CTeeRenderInfo RenderInfo = *pRenderInfo;

	bool Local = pGC->m_Snap.m_LocalClientId == ClientId;
	bool OtherTeam = pGC->IsOtherTeam(ClientId);
	float Alpha = (OtherTeam || ClientId < 0) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;
	if(ClientId == -3)
		Alpha = 1.0f;
	else if(ClientId == -2)
		Alpha = g_Config.m_ClRaceGhostAlpha / 100.0f;

	RenderInfo.m_Size = 64.0f;

	if(ClientId >= 0)
		Intra = pGC->m_aClients[ClientId].m_IsPredicted ? Client()->PredIntraGameTick(Dummy) : Client()->IntraGameTick(Dummy);

	static float s_LastGameTickTime = Client()->GameTickTime(Dummy);
	static float s_LastPredIntraTick = Client()->PredIntraGameTick(Dummy);
	if(pGC->m_Snap.m_pGameInfoObj && !(pGC->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED))
	{
		s_LastGameTickTime = Client()->GameTickTime(Dummy);
		s_LastPredIntraTick = Client()->PredIntraGameTick(Dummy);
	}

	float TickSpeed = (float)Client()->GameTickSpeed();
	int PrevTick = Client()->PrevGameTick(Dummy);
	float TickDelta = (PrevTick - Player.m_AttackTick) / TickSpeed;

	bool PredictLocalWeapons = false;
	float AttackTime = TickDelta + Client()->GameTickTime(Dummy);
	float LastAttackTime = TickDelta + s_LastGameTickTime;

	if(ClientId >= 0 && pGC->m_aClients[ClientId].m_IsPredictedLocal && pGC->AntiPingGunfire())
	{
		PredictLocalWeapons = true;
		int PredTick = Client()->PredGameTick(Dummy);
		float PredIntra = Client()->PredIntraGameTick(Dummy);
		AttackTime = (PredIntra + (PredTick - 1 - Player.m_AttackTick)) / TickSpeed;
		LastAttackTime = (s_LastPredIntraTick + (PredTick - 1 - Player.m_AttackTick)) / TickSpeed;
	}
	float AttackTicksPassed = AttackTime * TickSpeed;

	float Angle = GetPlayerTargetAngle(&Prev, &Player, ClientId, Intra);
	vec2 Direction = direction(Angle);

	vec2 Position;
	if(in_range(ClientId, MAX_CLIENTS - 1))
		Position = pGC->m_aClients[ClientId].m_RenderPos;
	else
		Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), Intra);

	vec2 Vel = mix(vec2(Prev.m_VelX / 256.0f, Prev.m_VelY / 256.0f), vec2(Player.m_VelX / 256.0f, Player.m_VelY / 256.0f), Intra);
	pGC->m_Flow.Add(Position, Vel * 100.0f, 10.0f);

	RenderInfo.m_GotAirJump = !(Player.m_Jumped & 2);
	RenderInfo.m_FeetFlipped = false;

	bool Stationary = Player.m_VelX <= 1 && Player.m_VelX >= -1;
	bool InAir = !Collision()->CheckPoint(Player.m_X, Player.m_Y + 16);
	bool Running = Player.m_VelX >= 5000 || Player.m_VelX <= -5000;
	bool WantOtherDir = (Player.m_Direction == -1 && Vel.x > 0) || (Player.m_Direction == 1 && Vel.x < 0);
	bool Inactive = ClientId >= 0 && (pGC->m_aClients[ClientId].m_Afk || pGC->m_aClients[ClientId].m_Paused);

	float WalkTime = std::fmod(Position.x, 100.0f) / 100.0f;
	float RunTime = std::fmod(Position.x, 200.0f) / 200.0f;
	if(WalkTime < 0.0f) WalkTime += 1.0f;
	if(RunTime < 0.0f) RunTime += 1.0f;

	CAnimState State;
	State.Set(&g_pData->m_aAnimations[ANIM_BASE], 0.0f);

	if(InAir)
		State.Add(&g_pData->m_aAnimations[ANIM_INAIR], 0.0f, 1.0f);
	else if(Stationary)
	{
		if(Inactive)
		{
			State.Add(Direction.x < 0.0f ? &g_pData->m_aAnimations[ANIM_SIT_LEFT] : &g_pData->m_aAnimations[ANIM_SIT_RIGHT], 0.0f, 1.0f);
			RenderInfo.m_FeetFlipped = true;
		}
		else
			State.Add(&g_pData->m_aAnimations[ANIM_IDLE], 0.0f, 1.0f);
	}
	else if(!WantOtherDir)
	{
		if(Running)
			State.Add(Player.m_VelX < 0 ? &g_pData->m_aAnimations[ANIM_RUN_LEFT] : &g_pData->m_aAnimations[ANIM_RUN_RIGHT], RunTime, 1.0f);
		else
			State.Add(&g_pData->m_aAnimations[ANIM_WALK], WalkTime, 1.0f);
	}

	if(Player.m_Weapon == WEAPON_HAMMER)
		State.Add(&g_pData->m_aAnimations[ANIM_HAMMER_SWING], std::clamp(LastAttackTime * 5.0f, 0.0f, 1.0f), 1.0f);
	if(Player.m_Weapon == WEAPON_NINJA)
		State.Add(&g_pData->m_aAnimations[ANIM_NINJA_SWING], std::clamp(LastAttackTime * 2.0f, 0.0f, 1.0f), 1.0f);

	if(!InAir && WantOtherDir && length(Vel * 50) > 500.0f)
		pGC->m_Effects.SkidTrail(Position, Vel, Player.m_Direction, Alpha, 1.0f);

	if(Player.m_Weapon >= 0 && !(RenderInfo.m_TeeRenderFlags & TEE_NO_WEAPON))
	{
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);

		int CurWeapon = std::clamp(Player.m_Weapon, 0, NUM_WEAPONS - 1);
		Graphics()->TextureSet(pGC->m_GameSkin.m_aSpriteWeapons[CurWeapon]);
		int QuadOffset = CurWeapon * 2 + (Direction.x < 0.0f ? 1 : 0);
		bool LeftDir = Direction.x < 0.0f;

		vec2 WeaponPosition;
		bool IsSit = Inactive && !InAir && Stationary;
		const auto &WeaponData = g_pData->m_Weapons.m_aId[CurWeapon];

		if(Player.m_Weapon == WEAPON_HAMMER)
		{
			WeaponPosition = Position + vec2(State.GetAttach()->m_X, State.GetAttach()->m_Y);
			WeaponPosition.y += WeaponData.m_Offsety;
			if(LeftDir)
				WeaponPosition.x -= WeaponData.m_Offsetx;
			if(IsSit)
				WeaponPosition.y += 3.0f;

			if(!Inactive || LastAttackTime < pGC->m_aClients[ClientId].m_Predicted.m_Tuning.GetWeaponFireDelay(Player.m_Weapon))
				Graphics()->QuadsSetRotation(LeftDir ? -pi / 2.0f - State.GetAttach()->m_Angle * pi * 2.0f : -pi / 2.0f + State.GetAttach()->m_Angle * pi * 2.0f);
			else
				Graphics()->QuadsSetRotation(LeftDir ? 100.0f : 500.0f);

			Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, WeaponPosition.x, WeaponPosition.y);
		}
		else if(Player.m_Weapon == WEAPON_NINJA)
		{
			WeaponPosition = Position;
			WeaponPosition.y += WeaponData.m_Offsety;
			if(IsSit)
				WeaponPosition.y += 3.0f;

			float AttachAngle = State.GetAttach()->m_Angle * pi * 2.0f;
			if(LeftDir)
			{
				Graphics()->QuadsSetRotation(-pi / 2 - AttachAngle);
				WeaponPosition.x -= WeaponData.m_Offsetx;
				pGC->m_Effects.PowerupShine(WeaponPosition + vec2(32.0f, 0.0f), vec2(32.0f, 12.0f), Alpha);
			}
			else
			{
				Graphics()->QuadsSetRotation(-pi / 2 + AttachAngle);
				pGC->m_Effects.PowerupShine(WeaponPosition - vec2(32.0f, 0.0f), vec2(32.0f, 12.0f), Alpha);
			}
			Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, WeaponPosition.x, WeaponPosition.y);

			if(AttackTime <= 1.0f / 6.0f && WeaponData.m_NumSpriteMuzzles)
			{
				int IteX = rand() % WeaponData.m_NumSpriteMuzzles;
				static int s_LastIteX = IteX;
				bool Paused = false;
				if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
					Paused = DemoPlayer()->BaseInfo()->m_Paused;
				else
					Paused = pGC->m_Snap.m_pGameInfoObj && (pGC->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED);

				if(Paused) IteX = s_LastIteX; else s_LastIteX = IteX;

				if(WeaponData.m_aSpriteMuzzles[IteX])
				{
					vec2 HadokenDir;
					if(PredictLocalWeapons || ClientId < 0)
						HadokenDir = vec2(pPlayerChar->m_X - pPrevChar->m_X, pPlayerChar->m_Y - pPrevChar->m_Y);
					else
					{
						const auto &Snap = pGC->m_Snap.m_aCharacters[ClientId];
						HadokenDir = vec2(Snap.m_Cur.m_X - Snap.m_Prev.m_X, Snap.m_Cur.m_Y - Snap.m_Prev.m_Y);
					}

					float HadokenAngle = 0.0f;
					if(absolute(HadokenDir.x) > 0.0001f || absolute(HadokenDir.y) > 0.0001f)
					{
						HadokenDir = normalize(HadokenDir);
						HadokenAngle = angle(HadokenDir);
					}
					else
						HadokenDir = vec2(1.0f, 0.0f);

					Graphics()->QuadsSetRotation(HadokenAngle);
					int MuzzleQuad = IteX * 2;
					vec2 MuzzlePos = Position - HadokenDir * WeaponData.m_Muzzleoffsetx;
					Graphics()->TextureSet(pGC->m_GameSkin.m_aaSpriteWeaponsMuzzles[CurWeapon][IteX]);
					Graphics()->RenderQuadContainerAsSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[CurWeapon], MuzzleQuad, MuzzlePos.x, MuzzlePos.y);
				}
			}
		}
		else
		{
			float Recoil = 0.0f;
			float a = AttackTicksPassed / 5.0f;
			if(a < 1.0f)
				Recoil = std::sin(a * pi);
			WeaponPosition = Position + Direction * WeaponData.m_Offsetx - Direction * Recoil * 10.0f;
			WeaponPosition.y += WeaponData.m_Offsety;
			if(IsSit)
				WeaponPosition.y += 3.0f;
			if(Player.m_Weapon == WEAPON_GUN && g_Config.m_ClOldGunPosition)
				WeaponPosition.y -= 8.0f;
			Graphics()->QuadsSetRotation(State.GetAttach()->m_Angle * pi * 2.0f + Angle);
			Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, WeaponPosition.x, WeaponPosition.y);
		}

		if(Player.m_Weapon == WEAPON_GUN || Player.m_Weapon == WEAPON_SHOTGUN)
		{
			if(WeaponData.m_NumSpriteMuzzles)
			{
				float AlphaMuzzle = 0.0f;
				if(AttackTicksPassed < WeaponData.m_Muzzleduration + 3.0f)
				{
					float t = AttackTicksPassed / WeaponData.m_Muzzleduration;
					AlphaMuzzle = mix(2.0f, 0.0f, minimum(1.0f, maximum(0.0f, t)));
				}

				int IteX = rand() % WeaponData.m_NumSpriteMuzzles;
				static int s_LastIteX = IteX;
				bool Paused = false;
				if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
					Paused = DemoPlayer()->BaseInfo()->m_Paused;
				else
					Paused = pGC->m_Snap.m_pGameInfoObj && (pGC->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED);

				if(Paused) IteX = s_LastIteX; else s_LastIteX = IteX;

				if(AlphaMuzzle > 0.0f && WeaponData.m_aSpriteMuzzles[IteX])
				{
					float OffsetY = -WeaponData.m_Muzzleoffsety;
					int MuzzleQuad = IteX * 2 + (LeftDir ? 1 : 0);
					if(LeftDir)
						OffsetY = -OffsetY;

					vec2 DirY(-Direction.y, Direction.x);
					vec2 MuzzlePos = WeaponPosition + Direction * WeaponData.m_Muzzleoffsetx + DirY * OffsetY;
					Graphics()->TextureSet(pGC->m_GameSkin.m_aaSpriteWeaponsMuzzles[CurWeapon][IteX]);
					Graphics()->RenderQuadContainerAsSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[CurWeapon], MuzzleQuad, MuzzlePos.x, MuzzlePos.y);
				}
			}
		}

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0.0f);

		switch(Player.m_Weapon)
		{
		case WEAPON_GUN: RenderHand(&RenderInfo, WeaponPosition, Direction, -3.0f * pi / 4.0f, vec2(-15.0f, 4.0f), Alpha); break;
		case WEAPON_SHOTGUN: RenderHand(&RenderInfo, WeaponPosition, Direction, -pi / 2.0f, vec2(-5.0f, 4.0f), Alpha); break;
		case WEAPON_GRENADE: RenderHand(&RenderInfo, WeaponPosition, Direction, -pi / 2.0f, vec2(-4.0f, 7.0f), Alpha); break;
		}
	}

	if(Local && ((g_Config.m_Debug && g_Config.m_ClUnpredictedShadow >= 0) || g_Config.m_ClUnpredictedShadow == 1))
	{
		vec2 ShadowPos = Position;
		if(ClientId >= 0)
		{
			const auto &Snap = pGC->m_Snap.m_aCharacters[ClientId];
			ShadowPos = mix(vec2(Snap.m_Prev.m_X, Snap.m_Prev.m_Y), vec2(Snap.m_Cur.m_X, Snap.m_Cur.m_Y), Client()->IntraGameTick(Dummy));
		}
		RenderTools()->RenderTee(&State, &RenderInfo, Player.m_Emote, Direction, ShadowPos, 0.5f);
	}

	RenderTools()->RenderTee(&State, &RenderInfo, Player.m_Emote, Direction, Position, Alpha);

	float TeeAnimScale, TeeBaseSize;
	CRenderTools::GetRenderTeeAnimScaleAndBaseSize(&RenderInfo, TeeAnimScale, TeeBaseSize);
	vec2 BodyPos = Position + vec2(State.GetBody()->m_X, State.GetBody()->m_Y) * TeeAnimScale;

	if(RenderInfo.m_TeeRenderFlags & TEE_EFFECT_FROZEN)
		pGC->m_Effects.FreezingFlakes(BodyPos, vec2(32, 32), Alpha);
	if(RenderInfo.m_TeeRenderFlags & TEE_EFFECT_SPARKLE)
		pGC->m_Effects.SparkleTrail(BodyPos, Alpha);

	if(ClientId < 0)
		return;

	int EmoticonBase = NUM_WEAPONS * 2 + 2 + 2;

	if((Player.m_PlayerFlags & PLAYERFLAG_CHATTING) && !pGC->m_aClients[ClientId].m_Afk)
	{
		int Emot = SPRITE_DOTDOT - SPRITE_OOP;
		Graphics()->TextureSet(pGC->m_EmoticonsSkin.m_aSpriteEmoticons[Emot]);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
		Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, EmoticonBase + Emot, Position.x + 24.f, Position.y - 40.f);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}

	if(g_Config.m_ClAfkEmote && pGC->m_aClients[ClientId].m_Afk && ClientId != pGC->m_aLocalIds[!Dummy])
	{
		int Emot = SPRITE_ZZZ - SPRITE_OOP;
		Graphics()->TextureSet(pGC->m_EmoticonsSkin.m_aSpriteEmoticons[Emot]);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
		Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, EmoticonBase + Emot, Position.x + 24.f, Position.y - 40.f);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}

	const auto &ClientInfo = pGC->m_aClients[ClientId];
	if(g_Config.m_ClShowEmotes && !ClientInfo.m_EmoticonIgnore && ClientInfo.m_EmoticonStartTick != -1)
	{
		float GameTickSpd = (float)Client()->GameTickSpeed();
		float SinceStart = (Client()->GameTick(Dummy) - ClientInfo.m_EmoticonStartTick) + (Client()->IntraGameTickSincePrev(Dummy) - ClientInfo.m_EmoticonStartFraction);
		float FromEnd = (2.0f * GameTickSpd) - SinceStart;

		if(SinceStart >= 0 && FromEnd > 0)
		{
			float a = 1.0f;
			if(FromEnd < GameTickSpd / 5.0f)
				a = FromEnd / (GameTickSpd / 5.0f);

			float h = 1.0f;
			if(SinceStart < GameTickSpd / 10.0f)
				h = SinceStart / (GameTickSpd / 10.0f);

			float Wiggle = 0.0f;
			if(SinceStart < GameTickSpd / 5.0f)
				Wiggle = SinceStart / (GameTickSpd / 5.0f);

			Graphics()->QuadsSetRotation(pi / 6.0f * std::sin(5.0f * Wiggle));
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, a * Alpha);
			Graphics()->TextureSet(pGC->m_EmoticonsSkin.m_aSpriteEmoticons[ClientInfo.m_Emoticon]);
			Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, EmoticonBase + ClientInfo.m_Emoticon, Position.x, Position.y - 23.f - 32.f * h, 1.f, (64.f * h) / 64.f);
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			Graphics()->QuadsSetRotation(0);
		}
	}
}

inline bool CPlayers::IsPlayerInfoAvailable(int ClientId) const
{
	const auto &Snap = GameClient()->m_Snap;
	return Snap.m_aCharacters[ClientId].m_Active && Snap.m_apPrevPlayerInfos[ClientId] && Snap.m_apPlayerInfos[ClientId];
}

void CPlayers::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	auto *pGC = GameClient();
	int Dummy = g_Config.m_ClDummy;
	bool IsTeamPlay = pGC->IsTeamPlay();

	CTeeRenderInfo aRenderInfo[MAX_CLIENTS];
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		aRenderInfo[i] = pGC->m_aClients[i].m_RenderInfo;
		aRenderInfo[i].m_TeeRenderFlags = 0;

		bool IsLocalPlayer = (i == pGC->m_aLocalIds[0] || i == pGC->m_aLocalIds[1]);
		bool Frozen;

		if(IsLocalPlayer)
		{
			const auto &Pred = pGC->m_aClients[i].m_Predicted;
			if(Pred.m_FreezeEnd != 0)
				aRenderInfo[i].m_TeeRenderFlags |= TEE_EFFECT_FROZEN | TEE_NO_WEAPON;
			if(Pred.m_LiveFrozen)
				aRenderInfo[i].m_TeeRenderFlags |= TEE_EFFECT_FROZEN;
			if(Pred.m_Invincible)
				aRenderInfo[i].m_TeeRenderFlags |= TEE_EFFECT_SPARKLE;
			Frozen = Pred.m_FreezeEnd != 0;
		}
		else
		{
			const auto &Cl = pGC->m_aClients[i];
			if(Cl.m_FreezeEnd != 0)
				aRenderInfo[i].m_TeeRenderFlags |= TEE_EFFECT_FROZEN | TEE_NO_WEAPON;
			if(Cl.m_LiveFrozen)
				aRenderInfo[i].m_TeeRenderFlags |= TEE_EFFECT_FROZEN;
			if(Cl.m_Invincible)
				aRenderInfo[i].m_TeeRenderFlags |= TEE_EFFECT_SPARKLE;

			const auto &SnapChar = pGC->m_Snap.m_aCharacters[i];
			Frozen = SnapChar.m_HasExtendedData && SnapChar.m_ExtendedData.m_FreezeEnd != 0;
		}

		if((pGC->m_aClients[i].m_RenderCur.m_Weapon == WEAPON_NINJA || (Frozen && !pGC->m_GameInfo.m_NoSkinChangeForFrozen)) && g_Config.m_ClShowNinja)
		{
			aRenderInfo[i].m_aSixup[Dummy].Reset();
			aRenderInfo[i].ApplySkin(NinjaTeeRenderInfo()->TeeRenderInfo());
			aRenderInfo[i].m_CustomColoredSkin = IsTeamPlay;
			if(!IsTeamPlay)
			{
				aRenderInfo[i].m_ColorBody = ColorRGBA(1, 1, 1);
				aRenderInfo[i].m_ColorFeet = ColorRGBA(1, 1, 1);
			}
		}
	}

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	constexpr float Border = 100.0f;
	ScreenX0 -= Border;
	ScreenX1 += Border;
	ScreenY0 -= Border;
	ScreenY1 += Border;

	int LocalId = pGC->m_Snap.m_LocalClientId;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == LocalId || !IsPlayerInfoAvailable(i))
			continue;
		const auto &Cl = pGC->m_aClients[i];
		RenderHook(&Cl.m_RenderPrev, &Cl.m_RenderCur, &aRenderInfo[i], i);
	}
	if(LocalId != -1 && IsPlayerInfoAvailable(LocalId))
	{
		const auto &Cl = pGC->m_aClients[LocalId];
		RenderHook(&Cl.m_RenderPrev, &Cl.m_RenderCur, &aRenderInfo[LocalId], LocalId);
	}

	for(const auto &Cl : pGC->m_aClients)
	{
		if(!Cl.m_SpecCharPresent)
			continue;
		int Cid = Cl.ClientId();
		float a = (pGC->IsOtherTeam(Cid) || Cid < 0) ? g_Config.m_ClShowOthersAlpha / 100.f : 1.f;
		if(Cid == -2)
			a = g_Config.m_ClRaceGhostAlpha / 100.f;
		RenderTools()->RenderTee(CAnimState::GetIdle(), &SpectatorTeeRenderInfo()->TeeRenderInfo(), EMOTE_BLINK, vec2(1, 0), Cl.m_SpecChar, a);
	}

	const auto &Spec = pGC->m_Snap.m_SpecInfo;
	int RenderLastId = (Spec.m_SpectatorId != SPEC_FREEVIEW && Spec.m_Active) ? Spec.m_SpectatorId : LocalId;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == RenderLastId || !IsPlayerInfoAvailable(i))
			continue;

		const auto &Cl = pGC->m_aClients[i];
		RenderHookCollLine(&Cl.m_RenderPrev, &Cl.m_RenderCur, i);

		if(!in_range(Cl.m_RenderPos.x, ScreenX0, ScreenX1) || !in_range(Cl.m_RenderPos.y, ScreenY0, ScreenY1))
			continue;
		RenderPlayer(&Cl.m_RenderPrev, &Cl.m_RenderCur, &aRenderInfo[i], i);
	}

	if(RenderLastId != -1 && IsPlayerInfoAvailable(RenderLastId))
	{
		const auto &Cl = pGC->m_aClients[RenderLastId];
		RenderHookCollLine(&Cl.m_RenderPrev, &Cl.m_RenderCur, RenderLastId);
		RenderPlayer(&Cl.m_RenderPrev, &Cl.m_RenderCur, &aRenderInfo[RenderLastId], RenderLastId);
	}

	pGC->m_Controls.RenderTasWorld();
}

void CPlayers::CreateNinjaTeeRenderInfo()
{
	CTeeRenderInfo Info;
	Info.m_Size = 64.0f;
	CSkinDescriptor Desc;
	Desc.m_Flags |= CSkinDescriptor::FLAG_SIX;
	str_copy(Desc.m_aSkinName, "x_ninja");
	m_pNinjaTeeRenderInfo = GameClient()->CreateManagedTeeRenderInfo(Info, Desc);
}

void CPlayers::CreateSpectatorTeeRenderInfo()
{
	CTeeRenderInfo Info;
	Info.m_Size = 64.0f;
	CSkinDescriptor Desc;
	Desc.m_Flags |= CSkinDescriptor::FLAG_SIX;
	str_copy(Desc.m_aSkinName, "x_spec");
	m_pSpectatorTeeRenderInfo = GameClient()->CreateManagedTeeRenderInfo(Info, Desc);
}

void CPlayers::OnInit()
{
	m_WeaponEmoteQuadContainerIndex = Graphics()->CreateQuadContainer(false);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		float ScaleX, ScaleY;
		Graphics()->GetSpriteScale(g_pData->m_Weapons.m_aId[i].m_pSpriteBody, ScaleX, ScaleY);
		float W = g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleX;
		float H = g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleY;
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Graphics()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, W, H);
		Graphics()->QuadsSetSubset(0, 1, 1, 0);
		Graphics()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, W, H);
	}

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, 20.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, 20.f);

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, -12.f, -8.f, 24.f, 16.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, -12.f, -8.f, 24.f, 16.f);

	for(int i = 0; i < NUM_EMOTICONS; ++i)
	{
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Graphics()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, 64.f);
	}
	Graphics()->QuadContainerUpload(m_WeaponEmoteQuadContainerIndex);

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		m_aWeaponSpriteMuzzleQuadContainerIndex[i] = Graphics()->CreateQuadContainer(false);
		float ScaleX, ScaleY;
		for(int n = 0; n < g_pData->m_Weapons.m_aId[i].m_NumSpriteMuzzles; ++n)
		{
			if(g_pData->m_Weapons.m_aId[i].m_aSpriteMuzzles[n])
			{
				if(i == WEAPON_GUN || i == WEAPON_SHOTGUN)
					Graphics()->GetSpriteScaleImpl(96, 64, ScaleX, ScaleY);
				else
					Graphics()->GetSpriteScale(g_pData->m_Weapons.m_aId[i].m_aSpriteMuzzles[n], ScaleX, ScaleY);
			}

			float SWidth = (g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleX) * (4.0f / 3.0f);
			float SHeight = g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleY;
			bool IsNinja = (i == WEAPON_NINJA);

			Graphics()->QuadsSetSubset(0, 0, 1, 1);
			Graphics()->QuadContainerAddSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[i], IsNinja ? 160.f * ScaleX : SWidth, IsNinja ? 160.f * ScaleY : SHeight);
			Graphics()->QuadsSetSubset(0, 1, 1, 0);
			Graphics()->QuadContainerAddSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[i], IsNinja ? 160.f * ScaleX : SWidth, IsNinja ? 160.f * ScaleY : SHeight);
		}
		Graphics()->QuadContainerUpload(m_aWeaponSpriteMuzzleQuadContainerIndex[i]);
	}

	Graphics()->QuadsSetSubset(0.f, 0.f, 1.f, 1.f);
	Graphics()->QuadsSetRotation(0.f);

	CreateNinjaTeeRenderInfo();
	CreateSpectatorTeeRenderInfo();
}

void CPlayers::RenderTasOverlay(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CTeeRenderInfo *pRenderInfo,
	float Intra)
{
	RenderHook(pPrevChar, pPlayerChar, pRenderInfo, -3, Intra);
	RenderPlayer(pPrevChar, pPlayerChar, pRenderInfo, -3, Intra);
}