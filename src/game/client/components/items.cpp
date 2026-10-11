#include "items.h"

#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#include <game/client/components/effects.h>
#include <game/client/gameclient.h>
#include <game/client/laser_data.h>
#include <game/client/pickup_data.h>
#include <game/client/prediction/entities/laser.h>
#include <game/client/prediction/entities/pickup.h>
#include <game/client/prediction/entities/projectile.h>
#include <game/client/projectile_data.h>
#include <game/mapitems.h>

static void GetProjectileTuning(int Weapon, const CTuningParams *pTuning, float &Curvature, float &Speed)
{
	Curvature = 0;
	Speed = 0;
	switch(Weapon)
	{
	case WEAPON_GRENADE:
		Curvature = pTuning->m_GrenadeCurvature;
		Speed = pTuning->m_GrenadeSpeed;
		break;
	case WEAPON_SHOTGUN:
		Curvature = pTuning->m_ShotgunCurvature;
		Speed = pTuning->m_ShotgunSpeed;
		break;
	case WEAPON_GUN:
		Curvature = pTuning->m_GunCurvature;
		Speed = pTuning->m_GunSpeed;
		break;
	}
}

void CItems::RenderProjectile(const CProjectileData *pCurrent, int ItemId)
{
	int CurWeapon = std::clamp(pCurrent->m_Type, 0, NUM_WEAPONS - 1);

	float Curvature, Speed;
	GetProjectileTuning(CurWeapon, GameClient()->GetTuning(pCurrent->m_TuneZone), Curvature, Speed);

	auto *pGC = GameClient();
	int Dummy = g_Config.m_ClDummy;

	bool LocalPlayerInGame = false;
	if(pGC->m_Snap.m_pLocalInfo)
		LocalPlayerInGame = pGC->m_aClients[pGC->m_Snap.m_pLocalInfo->m_ClientId].m_Team != TEAM_SPECTATORS;

	static float s_LastGameTickTime = Client()->GameTickTime(Dummy);
	if(pGC->m_Snap.m_pGameInfoObj && !(pGC->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED))
		s_LastGameTickTime = Client()->GameTickTime(Dummy);

	bool IsOtherTeam = (pCurrent->m_ExtraInfo && pCurrent->m_Owner >= 0 && pGC->IsOtherTeam(pCurrent->m_Owner));
	float TickSpeed = (float)Client()->GameTickSpeed();

	float Ct;
	if(pGC->Predict() && pGC->AntiPingGrenade() && LocalPlayerInGame && !IsOtherTeam)
		Ct = ((float)(Client()->GetPredictionTick() - 1 - pCurrent->m_StartTick) + Client()->PredIntraGameTick(Dummy)) / TickSpeed;
	else
		Ct = (Client()->PrevGameTick(Dummy) - pCurrent->m_StartTick) / TickSpeed + s_LastGameTickTime;

	if(Ct < 0)
	{
		if(Ct > -s_LastGameTickTime / 2)
			Ct = 0;
		else
			return;
	}

	vec2 Pos = CalcPos(pCurrent->m_StartPos, pCurrent->m_StartVel, Curvature, Speed, Ct);
	vec2 PrevPos = CalcPos(pCurrent->m_StartPos, pCurrent->m_StartVel, Curvature, Speed, Ct - 0.001f);
	float Alpha = IsOtherTeam ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.f;
	vec2 Vel = Pos - PrevPos;

	if(CurWeapon == WEAPON_GRENADE)
	{
		pGC->m_Effects.SmokeTrail(Pos, Vel * -1, Alpha, 0.0f);
		static float s_Time = 0.0f;
		static float s_LastLocalTime = LocalTime();

		bool Paused;
		if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		{
			const auto *pInfo = DemoPlayer()->BaseInfo();
			Paused = pInfo->m_Paused;
			if(!Paused) s_Time += (LocalTime() - s_LastLocalTime) * pInfo->m_Speed;
		}
		else
		{
			Paused = pGC->m_Snap.m_pGameInfoObj && (pGC->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED);
			if(!Paused) s_Time += LocalTime() - s_LastLocalTime;
		}

		Graphics()->QuadsSetRotation(s_Time * pi * 2 * 2 + ItemId);
		s_LastLocalTime = LocalTime();
	}
	else
	{
		pGC->m_Effects.BulletTrail(Pos, Alpha, 0.0f);
		Graphics()->QuadsSetRotation(length(Vel) > 0.00001f ? angle(Vel) : 0);
	}

	if(pGC->m_GameSkin.m_aSpriteWeaponProjectiles[CurWeapon].IsValid())
	{
		Graphics()->TextureSet(pGC->m_GameSkin.m_aSpriteWeaponProjectiles[CurWeapon]);
		Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
		Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, m_aProjectileOffset[CurWeapon], Pos.x, Pos.y);
	}
}

void CItems::RenderPickup(const CNetObj_Pickup *pPrev, const CNetObj_Pickup *pCurrent, bool IsPredicted, int Flags)
{
	int CurWeapon = std::clamp(pCurrent->m_Subtype, 0, NUM_WEAPONS - 1);
	int QuadOffset = 2;
	auto *pGC = GameClient();
	int Dummy = g_Config.m_ClDummy;
	float IntraTick = IsPredicted ? Client()->PredIntraGameTick(Dummy) : Client()->IntraGameTick(Dummy);
	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), IntraTick);

	switch(pCurrent->m_Type)
	{
	case POWERUP_HEALTH:
		QuadOffset = m_PickupHealthOffset;
		Graphics()->TextureSet(pGC->m_GameSkin.m_SpritePickupHealth);
		break;
	case POWERUP_ARMOR:
		QuadOffset = m_PickupArmorOffset;
		Graphics()->TextureSet(pGC->m_GameSkin.m_SpritePickupArmor);
		break;
	case POWERUP_WEAPON:
		QuadOffset = m_aPickupWeaponOffset[CurWeapon];
		Graphics()->TextureSet(pGC->m_GameSkin.m_aSpritePickupWeapons[CurWeapon]);
		break;
	case POWERUP_NINJA:
		QuadOffset = m_PickupNinjaOffset;
		if(Flags & PICKUPFLAG_ROTATE)
			pGC->m_Effects.PowerupShine(Pos, vec2(18, 96), 1.0f);
		else
			pGC->m_Effects.PowerupShine(Pos, vec2(96, 18), 1.0f);
		Graphics()->TextureSet(pGC->m_GameSkin.m_SpritePickupNinja);
		break;
	default:
		if(pCurrent->m_Type >= POWERUP_ARMOR_SHOTGUN && pCurrent->m_Type <= POWERUP_ARMOR_LASER)
		{
			int Idx = pCurrent->m_Type - POWERUP_ARMOR_SHOTGUN;
			QuadOffset = m_aPickupWeaponArmorOffset[Idx];
			Graphics()->TextureSet(pGC->m_GameSkin.m_aSpritePickupWeaponArmor[Idx]);
		}
		break;
	}

	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	vec2 Scale(1, 1);
	if(Flags & PICKUPFLAG_XFLIP) Scale.x = -1;
	if(Flags & PICKUPFLAG_YFLIP) Scale.y = -1;

	if(Flags & PICKUPFLAG_ROTATE)
	{
		Graphics()->QuadsSetRotation(90.f * (pi / 180));
		std::swap(Scale.x, Scale.y);
		if(pCurrent->m_Type == POWERUP_NINJA)
			Pos.y += (Flags & PICKUPFLAG_XFLIP) ? 10.0f : -10.0f;
	}
	else
	{
		if(pCurrent->m_Type == POWERUP_NINJA)
			Pos.x += (Flags & PICKUPFLAG_XFLIP) ? 10.0f : -10.0f;
	}

	static float s_Time = 0.0f;
	static float s_LastLocalTime = LocalTime();
	float Offset = (Pos.y + Pos.x) / 32.0f;

	bool Paused;
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const auto *pInfo = DemoPlayer()->BaseInfo();
		Paused = pInfo->m_Paused;
		if(!Paused) s_Time += (LocalTime() - s_LastLocalTime) * pInfo->m_Speed;
	}
	else
	{
		Paused = pGC->m_Snap.m_pGameInfoObj && (pGC->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED);
		if(!Paused) s_Time += LocalTime() - s_LastLocalTime;
	}

	Pos += direction(s_Time * 2.0f + Offset) * 2.5f;
	s_LastLocalTime = LocalTime();

	Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, QuadOffset, Pos.x, Pos.y, Scale.x, Scale.y);
	Graphics()->QuadsSetRotation(0);
}

void CItems::RenderFlags()
{
	for(int i = 0; i < GameClient()->m_Snap.m_NumFlags; ++i)
		RenderFlag(GameClient()->m_Snap.m_apPrevFlags[i], GameClient()->m_Snap.m_apFlags[i],
			GameClient()->m_Snap.m_pPrevGameDataObj, GameClient()->m_Snap.m_pGameDataObj);
}

void CItems::RenderFlag(const CNetObj_Flag *pPrev, const CNetObj_Flag *pCurrent, const CNetObj_GameData *pPrevGameData, const CNetObj_GameData *pCurGameData)
{
	auto *pGC = GameClient();
	int Dummy = g_Config.m_ClDummy;
	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), Client()->IntraGameTick(Dummy));

	if(pCurGameData)
	{
		int Carrier = (pCurrent->m_Team == TEAM_RED) ? pCurGameData->m_FlagCarrierRed : pCurGameData->m_FlagCarrierBlue;
		if(Carrier >= 0 && pGC->m_Snap.m_aCharacters[Carrier].m_Active)
			Pos = pGC->m_aClients[Carrier].m_RenderPos;

		if(pPrevGameData &&
			((pCurrent->m_Team == TEAM_RED && pPrevGameData->m_FlagCarrierRed != pCurGameData->m_FlagCarrierRed) ||
				(pCurrent->m_Team == TEAM_BLUE && pPrevGameData->m_FlagCarrierBlue != pCurGameData->m_FlagCarrierBlue)))
			Pos = vec2(pCurrent->m_X, pCurrent->m_Y);
	}

	int QuadOffset;
	if(pCurrent->m_Team == TEAM_RED)
	{
		Graphics()->TextureSet(pGC->m_GameSkin.m_SpriteFlagRed);
		QuadOffset = m_RedFlagOffset;
	}
	else
	{
		Graphics()->TextureSet(pGC->m_GameSkin.m_SpriteFlagBlue);
		QuadOffset = m_BlueFlagOffset;
	}
	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, QuadOffset, Pos.x, Pos.y - 42.0f * 0.75f);
}

void CItems::RenderLaser(const CLaserData *pCurrent, bool IsPredicted)
{
	int Type = std::clamp(pCurrent->m_Type, -1, NUM_LASERTYPES - 1);
	int ColorIn, ColorOut;

	switch(Type)
	{
	case LASERTYPE_RIFLE:
		ColorOut = g_Config.m_ClLaserRifleOutlineColor;
		ColorIn = g_Config.m_ClLaserRifleInnerColor;
		break;
	case LASERTYPE_SHOTGUN:
		ColorOut = g_Config.m_ClLaserShotgunOutlineColor;
		ColorIn = g_Config.m_ClLaserShotgunInnerColor;
		break;
	case LASERTYPE_DOOR:
		ColorOut = g_Config.m_ClLaserDoorOutlineColor;
		ColorIn = g_Config.m_ClLaserDoorInnerColor;
		break;
	case LASERTYPE_FREEZE:
		ColorOut = g_Config.m_ClLaserFreezeOutlineColor;
		ColorIn = g_Config.m_ClLaserFreezeInnerColor;
		break;
	case LASERTYPE_DRAGGER:
		ColorOut = g_Config.m_ClLaserDraggerOutlineColor;
		ColorIn = g_Config.m_ClLaserDraggerInnerColor;
		break;
	case LASERTYPE_GUN:
	case LASERTYPE_PLASMA:
		if(pCurrent->m_Subtype == LASERGUNTYPE_FREEZE || pCurrent->m_Subtype == LASERGUNTYPE_EXPFREEZE)
		{
			ColorOut = g_Config.m_ClLaserFreezeOutlineColor;
			ColorIn = g_Config.m_ClLaserFreezeInnerColor;
		}
		else
		{
			ColorOut = g_Config.m_ClLaserRifleOutlineColor;
			ColorIn = g_Config.m_ClLaserRifleInnerColor;
		}
		break;
	default:
		ColorOut = g_Config.m_ClLaserRifleOutlineColor;
		ColorIn = g_Config.m_ClLaserRifleInnerColor;
	}

	bool IsOtherTeam = (pCurrent->m_ExtraInfo && pCurrent->m_Owner >= 0 && GameClient()->IsOtherTeam(pCurrent->m_Owner));
	float Alpha = IsOtherTeam ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.f;

	ColorRGBA OuterColor = color_cast<ColorRGBA>(ColorHSLA(ColorOut).WithAlpha(Alpha));
	ColorRGBA InnerColor = color_cast<ColorRGBA>(ColorHSLA(ColorIn).WithAlpha(Alpha));

	int Dummy = g_Config.m_ClDummy;
	float Ticks;
	float TicksHead = (float)Client()->GameTick(Dummy);

	if(Type == LASERTYPE_DOOR)
	{
		Ticks = 1.0f;
	}
	else if(IsPredicted)
	{
		Ticks = (float)(Client()->GetPredictionTick() - pCurrent->m_StartTick) + Client()->PredIntraGameTick(Dummy);
		TicksHead += Client()->PredIntraGameTick(Dummy);
	}
	else
	{
		Ticks = (float)(Client()->GameTick(Dummy) - pCurrent->m_StartTick) + Client()->IntraGameTick(Dummy);
		TicksHead += Client()->IntraGameTick(Dummy);
	}

	if(Type == LASERTYPE_DRAGGER)
	{
		TicksHead *= (((pCurrent->m_Subtype >> 1) % 3) * 4.0f) + 1;
		TicksHead *= (pCurrent->m_Subtype & 1) ? -1 : 1;
	}
	RenderLaser(pCurrent->m_From, pCurrent->m_To, OuterColor, InnerColor, Ticks, TicksHead, Type);
}

void CItems::RenderLaser(vec2 From, vec2 Pos, ColorRGBA OuterColor, ColorRGBA InnerColor, float TicksBody, float TicksHead, int Type) const
{
	float Len = distance(Pos, From);

	if(Len > 0)
	{
		if(Type == LASERTYPE_DRAGGER)
		{
			float Thickness = std::sqrt(Len) / 5.f;
			TicksBody = std::clamp(Thickness, 1.0f, 5.0f);
		}
		vec2 Dir = normalize_pre_length(Pos - From, Len);

		float Ms = TicksBody * 1000.0f / Client()->GameTickSpeed();
		float a;
		if(Type == LASERTYPE_RIFLE || Type == LASERTYPE_SHOTGUN)
		{
			int TuneZone = (Client()->State() == IClient::STATE_ONLINE && GameClient()->m_GameWorld.m_WorldConfig.m_UseTuneZones)
				? Collision()->IsTune(Collision()->GetMapIndex(From)) : 0;
			a = Ms / GameClient()->GetTuning(TuneZone)->m_LaserBounceDelay;
		}
		else
			a = Ms / CTuningParams::DEFAULT.m_LaserBounceDelay;

		a = std::clamp(a, 0.0f, 1.0f);
		float Ia = 1.0f - a;

		vec2 Out = vec2(Dir.y, -Dir.x);

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();

		Graphics()->SetColor(OuterColor);
		vec2 Outer = Out * (7.0f * Ia);
		IGraphics::CFreeformItem Freeform(From - Outer, From + Outer, Pos - Outer, Pos + Outer);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);

		vec2 Inner = Out * (5.0f * Ia);
		vec2 ExtraPos = Dir;
		vec2 ExtraFrom = (Type == LASERTYPE_DOOR) ? vec2(0, 0) : Dir;
		Graphics()->SetColor(InnerColor);
		Freeform = IGraphics::CFreeformItem(From - Inner + ExtraFrom, From + Inner + ExtraFrom, Pos - Inner - ExtraPos, Pos + Inner - ExtraPos);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);

		Graphics()->QuadsEnd();
	}

	auto *pGC = GameClient();
	if(Type == LASERTYPE_DOOR)
	{
		Graphics()->TextureClear();
		Graphics()->QuadsSetRotation(0);
		Graphics()->SetColor(OuterColor);
		Graphics()->RenderQuadContainerEx(m_ItemsQuadContainerIndex, m_DoorHeadOffset, 1, Pos.x - 8.0f, Pos.y - 8.0f);
		Graphics()->SetColor(InnerColor);
		Graphics()->RenderQuadContainerEx(m_ItemsQuadContainerIndex, m_DoorHeadOffset, 1, Pos.x - 6.0f, Pos.y - 6.0f, 6.f / 8.f, 6.f / 8.f);
	}
	else if(Type == LASERTYPE_DRAGGER)
	{
		Graphics()->TextureSet(pGC->m_ExtrasSkin.m_SpritePulley);
		for(int Inner = 0; Inner < 2; ++Inner)
		{
			Graphics()->SetColor(Inner ? InnerColor : OuterColor);
			float Size = Inner ? 4.f / 5.f : 1.f;

			if(Len > 0)
			{
				Graphics()->QuadsSetRotation(0);
				Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, m_PulleyHeadOffset, From.x, From.y, Size, Size);
			}

			float OrbSize = Inner ? 0.75f - 1.f / 5.f : 0.75f;
			for(int Orb = 0; Orb < 3; ++Orb)
			{
				vec2 Offset = rotate(vec2(10.f, 0), Orb * 120 + TicksHead);
				Graphics()->QuadsSetRotation(TicksHead + Orb * pi * 2.f / 3.f);
				Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, m_PulleyHeadOffset, From.x + Offset.x, From.y + Offset.y, OrbSize, OrbSize);
			}
		}
	}
	else if(Type == LASERTYPE_FREEZE)
	{
		float Pulsation = 6.f / 5.f + 1.f / 10.f * std::sin(TicksHead / 2.f);
		float Ang = angle(Pos - From);
		float Scaled = 6.f / 5.f * Pulsation;
		Graphics()->TextureSet(pGC->m_ExtrasSkin.m_SpriteHectagon);
		Graphics()->QuadsSetRotation(Ang);
		Graphics()->SetColor(OuterColor);
		Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, m_FreezeHeadOffset, Pos.x, Pos.y, Scaled, Scaled);
		Graphics()->TextureSet(pGC->m_ExtrasSkin.m_SpriteParticleSnowflake);
		Graphics()->SetColor(ColorRGBA(1.f, 1.f, 1.f));
		Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, m_FreezeHeadOffset, Pos.x, Pos.y, Pulsation, Pulsation);
	}
	else
	{
		int Particle = (int)TicksHead % 3;
		Graphics()->TextureSet(pGC->m_ParticlesSkin.m_aSpriteParticleSplat[Particle]);
		Graphics()->QuadsSetRotation((int)TicksHead);
		Graphics()->SetColor(OuterColor);
		Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, m_aParticleSplatOffset[Particle], Pos.x, Pos.y);
		Graphics()->SetColor(InnerColor);
		Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, m_aParticleSplatOffset[Particle], Pos.x, Pos.y, 20.f / 24.f, 20.f / 24.f);
	}
}

void CItems::OnRender()
{
	auto *pGC = GameClient();
	if(pGC->SnapEntities().empty() && pGC->m_Snap.m_NumFlags == 0)
		return;

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	int Dummy = g_Config.m_ClDummy;
	bool IsSuper = pGC->IsLocalCharSuper();
	int Ticks = Client()->GameTick(Dummy) % Client()->GameTickSpeed();
	bool BlinkingPickup = (Ticks % 22) < 4;
	bool BlinkingGun = (Ticks % 22) < 4;
	bool BlinkingDragger = (Ticks % 22) < 4;
	bool BlinkingProj = (Ticks % 20) < 2;
	bool BlinkingProjEx = (Ticks % 6) < 2;
	bool BlinkingLight = (Ticks % 6) < 2;
	int SwitcherTeam = pGC->SwitchStateTeam();
	int GameTick = Client()->GameTick(Dummy);
	int DraggerStartTick = maximum((GameTick / 7) * 7, GameTick - 4);
	int GunStartTick = (GameTick / 7) * 7;

	bool UsePredicted = pGC->Predict() && pGC->AntiPingGunfire();
	auto &aSwitchers = pGC->Switchers();

	if(UsePredicted)
	{
		for(auto *pProj = (CProjectile *)pGC->m_PrevPredictedWorld.FindFirst(CGameWorld::ENTTYPE_PROJECTILE); pProj; pProj = (CProjectile *)pProj->NextEntity())
		{
			if(!IsSuper && pProj->m_Number > 0 && pProj->m_Number < (int)aSwitchers.size() && !aSwitchers[pProj->m_Number].m_aStatus[SwitcherTeam] && (pProj->m_Explosive ? BlinkingProjEx : BlinkingProj))
				continue;
			CProjectileData Data = pProj->GetData();
			RenderProjectile(&Data, pProj->GetId());
		}
		for(CEntity *pEnt = pGC->m_PrevPredictedWorld.FindFirst(CGameWorld::ENTTYPE_LASER); pEnt; pEnt = pEnt->NextEntity())
		{
			auto *pLaser = dynamic_cast<CLaser *>(pEnt);
			if(!pLaser || pLaser->GetOwner() < 0 || !pGC->m_aClients[pLaser->GetOwner()].m_IsPredictedLocal)
				continue;
			CLaserData Data = pLaser->GetData();
			RenderLaser(&Data, true);
		}
		for(auto *pPickup = (CPickup *)pGC->m_PrevPredictedWorld.FindFirst(CGameWorld::ENTTYPE_PICKUP); pPickup; pPickup = (CPickup *)pPickup->NextEntity())
		{
			if(!IsSuper && pPickup->m_Layer == LAYER_SWITCH && pPickup->m_Number > 0 && pPickup->m_Number < (int)aSwitchers.size() && !aSwitchers[pPickup->m_Number].m_aStatus[SwitcherTeam] && BlinkingPickup)
				continue;
			if(pPickup->InDDNetTile())
			{
				if(auto *pPrev = (CPickup *)pGC->m_PrevPredictedWorld.GetEntity(pPickup->GetId(), CGameWorld::ENTTYPE_PICKUP))
				{
					CNetObj_Pickup Data, Prev;
					pPickup->FillInfo(&Data);
					pPrev->FillInfo(&Prev);
					RenderPickup(&Prev, &Data, true, pPickup->Flags());
				}
			}
		}
	}

	for(const CSnapEntities &Ent : pGC->SnapEntities())
	{
		const auto &Item = Ent.m_Item;
		const void *pData = Item.m_pData;
		const CNetObj_EntityEx *pEntEx = Ent.m_pDataEx;

		if(Item.m_Type == NETOBJTYPE_PROJECTILE || Item.m_Type == NETOBJTYPE_DDRACEPROJECTILE || Item.m_Type == NETOBJTYPE_DDNETPROJECTILE)
		{
			CProjectileData Data = ExtractProjectileInfo(Item.m_Type, pData, &pGC->m_GameWorld, pEntEx);
			bool Inactive = !IsSuper && Data.m_SwitchNumber > 0 && Data.m_SwitchNumber < (int)aSwitchers.size() && !aSwitchers[Data.m_SwitchNumber].m_aStatus[SwitcherTeam];
			if(Inactive && (Data.m_Explosive ? BlinkingProjEx : BlinkingProj))
				continue;
			if(UsePredicted)
			{
				if(auto *pProj = (CProjectile *)pGC->m_GameWorld.FindMatch(Item.m_Id, Item.m_Type, pData))
				{
					bool IsOtherTeam = pGC->IsOtherTeam(pProj->GetOwner());
					if(pProj->m_LastRenderTick <= 0 && (pProj->m_Type != WEAPON_SHOTGUN || (!pProj->m_Freeze && !pProj->m_Explosive))
						&& (pProj->m_Type == WEAPON_SHOTGUN || absolute(length(pProj->m_Direction) - 1.f) < 0.02f)
						&& (pProj->GetOwner() < 0 || !pGC->m_aClients[pProj->GetOwner()].m_IsPredictedLocal || IsOtherTeam)
						&& !Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_Id))
					{
						ReconstructSmokeTrail(&Data, pProj->m_DestroyTick);
					}
					pProj->m_LastRenderTick = GameTick;
					if(!IsOtherTeam)
						continue;
				}
			}
			RenderProjectile(&Data, Item.m_Id);
		}
		else if(Item.m_Type == NETOBJTYPE_PICKUP || Item.m_Type == NETOBJTYPE_DDNETPICKUP)
		{
			CPickupData Data = ExtractPickupInfo(Item.m_Type, pData, pEntEx);
			bool Inactive = !IsSuper && Data.m_SwitchNumber > 0 && Data.m_SwitchNumber < (int)aSwitchers.size() && !aSwitchers[Data.m_SwitchNumber].m_aStatus[SwitcherTeam];
			if(Inactive && BlinkingPickup)
				continue;
			if(UsePredicted)
			{
				auto *pPickup = (CPickup *)pGC->m_GameWorld.FindMatch(Item.m_Id, Item.m_Type, pData);
				if(pPickup && pPickup->InDDNetTile())
					continue;
			}
			const void *pPrev = Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_Id);
			if(pPrev)
				RenderPickup((const CNetObj_Pickup *)pPrev, (const CNetObj_Pickup *)pData, false, Data.m_Flags);
		}
		else if(Item.m_Type == NETOBJTYPE_LASER || Item.m_Type == NETOBJTYPE_DDNETLASER)
		{
			if(UsePredicted)
			{
				auto *pLaser = dynamic_cast<CLaser *>(pGC->m_GameWorld.FindMatch(Item.m_Id, Item.m_Type, pData));
				if(pLaser && pLaser->GetOwner() >= 0 && pGC->m_aClients[pLaser->GetOwner()].m_IsPredictedLocal)
					continue;
			}

			CLaserData Data = ExtractLaserInfo(Item.m_Type, pData, &pGC->m_GameWorld, pEntEx);
			bool Inactive = !IsSuper && Data.m_SwitchNumber > 0 && Data.m_SwitchNumber < (int)aSwitchers.size() && !aSwitchers[Data.m_SwitchNumber].m_aStatus[SwitcherTeam];

			bool IsEntBlink = false;
			int EntStartTick = -1;

			switch(Data.m_Type)
			{
			case LASERTYPE_FREEZE:
				IsEntBlink = BlinkingLight;
				EntStartTick = DraggerStartTick;
				break;
			case LASERTYPE_GUN:
				IsEntBlink = BlinkingGun;
				EntStartTick = GunStartTick;
				break;
			case LASERTYPE_DRAGGER:
				IsEntBlink = BlinkingDragger;
				EntStartTick = DraggerStartTick;
				break;
			case LASERTYPE_DOOR:
				if(Data.m_Predict && (Inactive || IsSuper))
				{
					Data.m_From.x = Data.m_To.x;
					Data.m_From.y = Data.m_To.y;
				}
				EntStartTick = GameTick;
				break;
			default:
				IsEntBlink = BlinkingDragger;
				EntStartTick = GameTick;
				break;
			}

			if(Data.m_Predict && Inactive && IsEntBlink)
				continue;
			if(Data.m_StartTick <= 0 && EntStartTick != -1)
				Data.m_StartTick = EntStartTick;

			RenderLaser(&Data);
		}
	}

	RenderFlags();

	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
}

void CItems::OnInit()
{
	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	m_ItemsQuadContainerIndex = Graphics()->CreateQuadContainer(false);

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	m_RedFlagOffset = Graphics()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, -21.f, -42.f, 42.f, 84.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	m_BlueFlagOffset = Graphics()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, -21.f, -42.f, 42.f, 84.f);

	float ScaleX, ScaleY;
	Graphics()->GetSpriteScale(SPRITE_PICKUP_HEALTH, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	m_PickupHealthOffset = Graphics()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 64.f * ScaleX, 64.f * ScaleY);
	Graphics()->GetSpriteScale(SPRITE_PICKUP_ARMOR, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	m_PickupArmorOffset = Graphics()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 64.f * ScaleX, 64.f * ScaleY);

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		Graphics()->GetSpriteScale(g_pData->m_Weapons.m_aId[i].m_pSpriteBody, ScaleX, ScaleY);
		float VS = g_pData->m_Weapons.m_aId[i].m_VisualSize;
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		m_aPickupWeaponOffset[i] = Graphics()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, VS * ScaleX, VS * ScaleY);
	}

	Graphics()->GetSpriteScale(SPRITE_PICKUP_NINJA, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	m_PickupNinjaOffset = Graphics()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 128.f * ScaleX, 128.f * ScaleY);

	for(int i = 0; i < 4; i++)
	{
		Graphics()->GetSpriteScale(SPRITE_PICKUP_ARMOR_SHOTGUN + i, ScaleX, ScaleY);
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		m_aPickupWeaponArmorOffset[i] = Graphics()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 64.f * ScaleX, 64.f * ScaleY);
	}

	for(int &Off : m_aProjectileOffset)
	{
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Off = Graphics()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 32.f);
	}
	for(int &Off : m_aParticleSplatOffset)
	{
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Off = Graphics()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 24.f);
	}

	Graphics()->GetSpriteScale(SPRITE_PART_PULLEY, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	m_PulleyHeadOffset = Graphics()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 20.f * ScaleX);

	Graphics()->GetSpriteScale(SPRITE_PART_HECTAGON, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	m_FreezeHeadOffset = Graphics()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 20.f * ScaleX);

	IGraphics::CQuadItem Brick(0, 0, 16.0f, 16.0f);
	m_DoorHeadOffset = Graphics()->QuadContainerAddQuads(m_ItemsQuadContainerIndex, &Brick, 1);

	Graphics()->QuadContainerUpload(m_ItemsQuadContainerIndex);
}

void CItems::ReconstructSmokeTrail(const CProjectileData *pCurrent, int DestroyTick)
{
	auto *pGC = GameClient();
	int Dummy = g_Config.m_ClDummy;

	bool LocalPlayerInGame = false;
	if(pGC->m_Snap.m_pLocalInfo)
		LocalPlayerInGame = pGC->m_aClients[pGC->m_Snap.m_pLocalInfo->m_ClientId].m_Team != TEAM_SPECTATORS;
	if(!pGC->AntiPingGunfire() || !LocalPlayerInGame)
		return;

	int PredictionTick = Client()->GetPredictionTick();
	if(PredictionTick == pCurrent->m_StartTick)
		return;

	float Curvature, Speed;
	GetProjectileTuning(pCurrent->m_Type, pGC->GetTuning(pCurrent->m_TuneZone), Curvature, Speed);

	float TickSpeed = (float)Client()->GameTickSpeed();
	float PredIntra = Client()->PredIntraGameTick(Dummy);
	float Pt = ((float)(PredictionTick - pCurrent->m_StartTick) + PredIntra) / TickSpeed;
	if(Pt < 0) return;

	float Gt = (Client()->PrevGameTick(Dummy) - pCurrent->m_StartTick) / TickSpeed + Client()->GameTickTime(Dummy);

	float Alpha = 1.f;
	if(pCurrent->m_ExtraInfo && pCurrent->m_Owner >= 0 && pGC->IsOtherTeam(pCurrent->m_Owner))
		Alpha = g_Config.m_ClShowOthersAlpha / 100.0f;

	float T = Pt;
	if(DestroyTick >= 0)
		T = minimum(Pt, ((float)(DestroyTick - 1 - pCurrent->m_StartTick) + PredIntra) / TickSpeed);

	bool IsGrenade = pCurrent->m_Type == WEAPON_GRENADE;
	float MinTrailSpan = 0.4f * (IsGrenade ? 0.5f : 0.25f);
	float Step = maximum(Client()->FrameTimeAverage(), IsGrenade ? 0.02f : 0.01f);

	for(int i = 1 + (int)(Gt / Step); i < (int)(T / Step); i++)
	{
		float t = Step * (float)i + 0.4f * Step * random_float(-0.5f, 0.5f);
		vec2 Pos = CalcPos(pCurrent->m_StartPos, pCurrent->m_StartVel, Curvature, Speed, t);
		vec2 PrevPos = CalcPos(pCurrent->m_StartPos, pCurrent->m_StartVel, Curvature, Speed, t - 0.001f);
		float TimePassed = Pt - t;
		if(Pt - MinTrailSpan > 0.01f)
			TimePassed = minimum(TimePassed, (TimePassed - MinTrailSpan) / (Pt - MinTrailSpan) * (MinTrailSpan * 0.5f) + MinTrailSpan);
		if(IsGrenade)
			pGC->m_Effects.SmokeTrail(Pos, (Pos - PrevPos) * -1, Alpha, TimePassed);
		else
			pGC->m_Effects.BulletTrail(Pos, Alpha, TimePassed);
	}
}

void CItems::RenderTasProjectile(const CProjectileData *pCurrent, int ItemId, int WorldTick, float Intra)
{
	int CurWeapon = std::clamp(pCurrent->m_Type, 0, NUM_WEAPONS - 1);
	float Curvature, Speed;
	GetProjectileTuning(CurWeapon, GameClient()->GetTuning(pCurrent->m_TuneZone), Curvature, Speed);

	float Ct = (float)(WorldTick - pCurrent->m_StartTick + Intra) / (float)Client()->GameTickSpeed();
	if(Ct < 0) return;

	vec2 Pos = CalcPos(pCurrent->m_StartPos, pCurrent->m_StartVel, Curvature, Speed, Ct);
	vec2 PrevPos = CalcPos(pCurrent->m_StartPos, pCurrent->m_StartVel, Curvature, Speed, Ct - 0.001f);
	vec2 Vel = Pos - PrevPos;

	if(CurWeapon == WEAPON_GRENADE)
		Graphics()->QuadsSetRotation(Ct * pi * 2 * 2 + ItemId);
	else
		Graphics()->QuadsSetRotation(length(Vel) > 0.00001f ? angle(Vel) : 0);

	if(GameClient()->m_GameSkin.m_aSpriteWeaponProjectiles[CurWeapon].IsValid())
	{
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteWeaponProjectiles[CurWeapon]);
		Graphics()->SetColor(1.f, 1.f, 1.f, 0.5f);
		Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, m_aProjectileOffset[CurWeapon], Pos.x, Pos.y);
	}
}

void CItems::RenderTasLaser(const CLaserData *pCurrent, int WorldTick, float Intra)
{
	int Type = pCurrent->m_Type;
	ColorRGBA OuterColor(1.0f, 1.0f, 1.0f, 0.5f);
	ColorRGBA InnerColor(1.0f, 1.0f, 1.0f, 0.5f);

	if(Type == WEAPON_LASER)
	{
		OuterColor = ColorRGBA(0.07f, 0.07f, 0.98f, 0.5f);
		InnerColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f);
	}

	RenderLaser(pCurrent->m_From, pCurrent->m_To, OuterColor, InnerColor, 0.0f, 0.0f, Type);
}