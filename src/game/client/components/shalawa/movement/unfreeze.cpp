#include "unfreeze.h"

#include <base/math.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>

#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/client/components/controls.h>
#include <game/client/prediction/entities/character.h>
#include <game/collision.h>
#include <game/mapitems.h>
#include <cmath>
#include <algorithm>

static int s_LastFireTick[2] = {-OI(1000), -OI(1000)};
static vec2 s_CachedDir[2] = {vec2(OI(0), OI(0)), vec2(OI(0), OI(0))};
static bool s_DirValid[2] = {false, false};
static int s_CachedFireAtTick[2] = {OI(0), OI(0)};
static bool s_ShotFired[2] = {false, false};

void CUnfreezeBot::Init(CControls *pControls, CGameClient *pGameClient)
{
    ANTI_DISASM_BLOCK();
	m_pControls = pControls;
	m_pGameClient = pGameClient;
}

CCollision *CUnfreezeBot::Collision() const
{
    ANTI_DISASM_BLOCK();
	return m_pGameClient->Collision();
}

bool CUnfreezeBot::HasLaser(int LocalId) const
{
    ANTI_DISASM_BLOCK();
	int ClientId = m_pGameClient->m_aLocalIds[LocalId];
	if(ClientId < OI(0))
		return false;

	CCharacter *pChar = m_pGameClient->m_PredictedWorld.GetCharacterById(ClientId);
	if(pChar)
		return pChar->GetWeaponGot(WEAPON_LASER);

	if(m_pGameClient->m_Snap.m_pLocalCharacter)
		return m_pGameClient->m_Snap.m_pLocalCharacter->m_Weapon == WEAPON_LASER;

	return false;
}

bool CUnfreezeBot::IsLaserEquipped(int LocalId) const
{
    ANTI_DISASM_BLOCK();
	if(!m_pGameClient->m_Snap.m_pLocalCharacter)
		return false;
	return m_pGameClient->m_Snap.m_pLocalCharacter->m_Weapon == WEAPON_LASER;
}

bool CUnfreezeBot::CanShoot(int LocalId) const
{
    ANTI_DISASM_BLOCK();
	if(!IsLaserEquipped(LocalId))
		return false;

	int ClientId = m_pGameClient->m_aLocalIds[LocalId];
	CCharacter *pChar = m_pGameClient->m_PredictedWorld.GetCharacterById(ClientId);
	if(pChar)
		return pChar->GetReloadTimer() <= OI(0);

	return true;
}

int CUnfreezeBot::PredictMovement(int LocalId, vec2 *pPositions, int MaxTicks, int *pEnterFreeze, int *pExitFreeze) const
{
    ANTI_DISASM_BLOCK();
	*pEnterFreeze = -OI(1);
	*pExitFreeze = -OI(1);

	int ClientId = m_pGameClient->m_aLocalIds[LocalId];
	if(ClientId < OI(0) || ClientId >= MAX_CLIENTS)
		return OI(0);

	if(!m_pGameClient->m_Snap.m_aCharacters[ClientId].m_Active)
		return OI(0);

	CNetObj_PlayerInput Input = m_pControls->m_aInputData[LocalId];
	Input.m_Fire = OI(0);
	if(Input.m_TargetX == OI(0) && Input.m_TargetY == OI(0))
		Input.m_TargetY = -OI(1);

	const_cast<CGameClient*>(m_pGameClient)->m_TempWorld.CopyWorld(
		const_cast<CGameWorld*>(&m_pGameClient->m_PredictedWorld));

	CCharacter *pChar = const_cast<CGameClient*>(m_pGameClient)->m_TempWorld.GetCharacterById(ClientId);
	if(!pChar)
		return OI(0);

	pPositions[OI(0)] = pChar->Core()->m_Pos;
	bool WasFrozen = (pChar->m_FreezeTime > OI(0));
	int Count = OI(1);

	for(int Tick = OI(1); Tick < MaxTicks; Tick++)
	{
		const_cast<CGameClient*>(m_pGameClient)->m_TempWorld.m_GameTick++;
		pChar->OnPredictedInput(&Input);
		const_cast<CGameClient*>(m_pGameClient)->m_TempWorld.Tick();

		pPositions[Tick] = pChar->Core()->m_Pos;
		Count = Tick + OI(1);

		bool Frozen = (pChar->m_FreezeTime > OI(0));

		if(Frozen && !WasFrozen && *pEnterFreeze < OI(0))
			*pEnterFreeze = Tick;

		if(!Frozen && WasFrozen && *pEnterFreeze >= OI(0) && *pExitFreeze < OI(0))
			*pExitFreeze = Tick;

		WasFrozen = Frozen;
	}

	if(*pEnterFreeze >= OI(0) && *pExitFreeze < OI(0))
		*pExitFreeze = std::min(*pEnterFreeze + OI(3), MaxTicks - OI(1));

	return Count;
}

bool CUnfreezeBot::SimulateLaserHit(vec2 From, vec2 Dir, vec2 TargetPos, float Radius, int *pTravelTicks) const
{
    ANTI_DISASM_BLOCK();
	if(!Collision())
		return false;

	const CTuningParams *pTuning = m_pGameClient->GetTuning(OI(0));
	float Energy = pTuning->m_LaserReach;
	float BounceCost = pTuning->m_LaserBounceCost;
	int MaxBounces = std::min(g_Config.m_shalawaUnfreezeMaxBounces, (int)pTuning->m_LaserBounceNum);
	float BounceDelay = pTuning->m_LaserBounceDelay;
	int TickSpeed = m_pGameClient->Client()->GameTickSpeed();

	vec2 LaserPos = From;
	vec2 LaserDir = normalize(Dir);
	int Bounces = OI(0);
	bool ZeroDistLast = false;

	for(int b = OI(0); b <= MaxBounces && Energy > OBF_F(0); b++)
	{
		vec2 To = LaserPos + LaserDir * Energy;
		vec2 Coltile, BeforeCol;

		int Res = Collision()->IntersectLineTeleWeapon(LaserPos, To, &Coltile, &BeforeCol);

		vec2 SegEnd = Res ? BeforeCol : To;

		if(Bounces >= OI(1))
		{
			float SegLen = length(SegEnd - LaserPos);
			if(SegLen > OFR(1,10))
			{
				vec2 SegDir = (SegEnd - LaserPos) / SegLen;
				vec2 ToTarget = TargetPos - LaserPos;
				float Proj = std::clamp(dot(ToTarget, SegDir), OBF_F(0), SegLen);
				vec2 Closest = LaserPos + SegDir * Proj;

				if(length(TargetPos - Closest) < Radius)
				{
					if(pTravelTicks)
					{
						int Ticks = (int)((float)Bounces * BounceDelay * (float)TickSpeed / OBF_F(1000));
						*pTravelTicks = std::max(Ticks, OI(1));
					}
					return true;
				}
			}
		}

		if(!Res)
			break;

		float SegDist = distance(LaserPos, BeforeCol);

		if(SegDist == OBF_F(0) && ZeroDistLast)
			break;
		ZeroDistLast = (SegDist == OBF_F(0));

		Energy -= SegDist + BounceCost;
		if(Energy <= OBF_F(0))
			break;

		Bounces++;

		vec2 TempPos = Coltile;
		vec2 TempDir = LaserDir * OBF_F(4);

		int f = OI(0);
		if(Res == -OI(1))
		{
			f = Collision()->GetTile(round_to_int(Coltile.x), round_to_int(Coltile.y));
			Collision()->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), TILE_SOLID);
		}
		Collision()->MovePoint(&TempPos, &TempDir, OBF_F(1), nullptr);
		if(Res == -OI(1))
		{
			Collision()->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), f);
		}

		LaserPos = TempPos;
		LaserDir = normalize(TempDir);
	}

	return false;
}

bool CUnfreezeBot::FindBounceShot(vec2 MyPos, vec2 *pPositions, int NumPositions, int EnterFreeze, int ExitFreeze, vec2 *pDir, int *pBestFireTick) const
{
    ANTI_DISASM_BLOCK();
	const float PlayerRadius = OBF_F(28);
	int AngleStep = g_Config.m_shalawaUnfreezeAngleStep;

	float BestScore = -OBF_F(1e30);
	vec2 BestDir = vec2(OI(0), OI(0));
	int BestFire = OI(0);
	bool Found = false;

	int TargetStart = std::max(OI(1), ExitFreeze - OI(2));
	int TargetEnd = std::min(ExitFreeze + OI(5), NumPositions - OI(1));

	for(int Deg = OI(0); Deg < OI(360); Deg += AngleStep)
	{
		float Angle = Deg * pi / OBF_F(180);
		vec2 Dir = vec2(std::cos(Angle), std::sin(Angle));

		for(int TargetTick = TargetStart; TargetTick <= TargetEnd; TargetTick++)
		{
			int TravelTicks = OI(0);
			if(!SimulateLaserHit(MyPos, Dir, pPositions[TargetTick], PlayerRadius, &TravelTicks))
				continue;

			int FireTick = TargetTick - TravelTicks;

			if(FireTick >= EnterFreeze)
				continue;
			if(FireTick < -OI(2))
				continue;

			float Score = OBF_F(10000);

			int DistToExit = std::abs(TargetTick - ExitFreeze);
			Score -= DistToExit * OBF_F(400);
			Score -= TravelTicks * OBF_F(50);

			if(TargetTick >= ExitFreeze && TargetTick <= ExitFreeze + OI(2))
				Score += OBF_F(1000);

			if(FireTick < OI(0))
				Score -= OBF_F(3000);

			if(Score > BestScore)
			{
				BestScore = Score;
				BestDir = Dir;
				BestFire = FireTick;
				Found = true;
			}
		}
	}

	if(Found && AngleStep > OI(1))
	{
		float BestAngle = std::atan2(BestDir.y, BestDir.x);

		for(int i = -OI(8); i <= OI(8); i++)
		{
			if(i == OI(0))
				continue;

			float Angle = BestAngle + i * OFR(3,10) * pi / OBF_F(180);
			vec2 Dir = vec2(std::cos(Angle), std::sin(Angle));

			for(int TargetTick = TargetStart; TargetTick <= TargetEnd; TargetTick++)
			{
				int TravelTicks = OI(0);
				if(!SimulateLaserHit(MyPos, Dir, pPositions[TargetTick], PlayerRadius, &TravelTicks))
					continue;

				int FireTick = TargetTick - TravelTicks;
				if(FireTick >= EnterFreeze || FireTick < -OI(2))
					continue;

				float Score = OBF_F(10000);
				int DistToExit = std::abs(TargetTick - ExitFreeze);
				Score -= DistToExit * OBF_F(400);
				Score -= TravelTicks * OBF_F(50);
				if(TargetTick >= ExitFreeze && TargetTick <= ExitFreeze + OI(2))
					Score += OBF_F(1000);
				if(FireTick < OI(0))
					Score -= OBF_F(3000);

				if(Score > BestScore)
				{
					BestScore = Score;
					BestDir = Dir;
					BestFire = FireTick;
				}
			}
		}
	}

	if(Found)
	{
		*pDir = BestDir;
		if(pBestFireTick)
			*pBestFireTick = BestFire;
		return true;
	}

	return false;
}

void CUnfreezeBot::Tick(int LocalId)
{
    ANTI_DISASM_BLOCK();
	if(!g_Config.m_shalawaUnfreezeBot)
	{
		s_DirValid[LocalId] = false;
		s_ShotFired[LocalId] = false;
		return;
	}

	if(!m_pGameClient->m_Snap.m_pLocalCharacter)
		return;
	if(!HasLaser(LocalId))
		return;

	int ClientId = m_pGameClient->m_aLocalIds[LocalId];
	if(ClientId < OI(0))
		return;

	int CurrentTick = m_pGameClient->Client()->GameTick(g_Config.m_ClDummy);

	CCharacter *pChar = m_pGameClient->m_PredictedWorld.GetCharacterById(ClientId);
	if(pChar && pChar->Core())
	{
		if(pChar->m_FreezeTime > OI(0))
		{
			s_DirValid[LocalId] = false;
			s_ShotFired[LocalId] = false;
			return;
		}
	}

	if(s_ShotFired[LocalId])
	{
		if(CurrentTick - s_LastFireTick[LocalId] >= OI(12))
			s_ShotFired[LocalId] = false;
		else
			return;
	}

	vec2 Positions[64];
	int EnterFreeze, ExitFreeze;
	int NumPos = PredictMovement(LocalId, Positions, OI(64), &EnterFreeze, &ExitFreeze);

	if(EnterFreeze < OI(0) || NumPos < OI(2))
	{
		s_DirValid[LocalId] = false;
		return;
	}

	vec2 MyPos;
	if(pChar && pChar->Core())
		MyPos = pChar->Core()->m_Pos;
	else
		MyPos = Positions[OI(0)];

	if(!IsLaserEquipped(LocalId))
	{
		m_pControls->m_aInputData[LocalId].m_WantedWeapon = WEAPON_LASER + OI(1);

		vec2 ShootDir;
		int FireAtTick;
		if(FindBounceShot(MyPos, Positions, NumPos, EnterFreeze, ExitFreeze, &ShootDir, &FireAtTick))
		{
			s_CachedDir[LocalId] = ShootDir;
			s_DirValid[LocalId] = true;
			s_CachedFireAtTick[LocalId] = FireAtTick;
		}
		return;
	}

	vec2 ShootDir;
	int FireAtTick = OI(0);
	bool Found = FindBounceShot(MyPos, Positions, NumPos, EnterFreeze, ExitFreeze, &ShootDir, &FireAtTick);

	if(!Found)
	{
		if(s_DirValid[LocalId])
		{
			ShootDir = s_CachedDir[LocalId];
			FireAtTick = s_CachedFireAtTick[LocalId];
		}
		else
			return;
	}
	else
	{
		s_CachedDir[LocalId] = ShootDir;
		s_DirValid[LocalId] = true;
		s_CachedFireAtTick[LocalId] = FireAtTick;
	}

	m_pControls->m_aInputData[LocalId].m_TargetX = (int)(ShootDir.x * OBF_F(100));
	m_pControls->m_aInputData[LocalId].m_TargetY = (int)(ShootDir.y * OBF_F(100));
	m_pControls->m_aMousePos[LocalId] = ShootDir * OBF_F(100);

	bool ShouldFire = false;

	if(FireAtTick <= OI(0))
		ShouldFire = true;
	else if(FireAtTick <= OI(1) && EnterFreeze <= OI(3))
		ShouldFire = true;

	if(ShouldFire && CanShoot(LocalId))
	{
		int Fire = m_pControls->m_aInputData[LocalId].m_Fire;
		if((Fire & OI(1)) == OI(0))
			m_pControls->m_aInputData[LocalId].m_Fire = Fire + OI(1);

		s_LastFireTick[LocalId] = CurrentTick;
		s_DirValid[LocalId] = false;
		s_ShotFired[LocalId] = true;
	}
}
