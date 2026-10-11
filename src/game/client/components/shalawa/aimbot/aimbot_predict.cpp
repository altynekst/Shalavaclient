#include "aimbot.h"

#include <base/math.h>
#include <base/vmath.h>
#include <algorithm>
#include <cmath>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/client/components/controls.h>
#include <game/collision.h>
#include <game/gamecore.h>

vec2 CAimbot::PredictTargetPos(int TargetId, int Ticks)
{
	if(!m_pClient || TargetId < 0 || !m_pClient->m_aClients[TargetId].m_Active)
		return vec2(0, 0);

	vec2 Pos = m_pClient->m_aClients[TargetId].m_Predicted.m_Pos;
	vec2 Vel = m_pClient->m_aClients[TargetId].m_Predicted.m_Vel;

	const CTuningParams *pTuning = GetCurrentTuning();
	if(!pTuning)
		return Pos;

	float Gravity = pTuning->m_Gravity;
	float AirFriction = pTuning->m_AirFriction;

	CCollision *pCol = m_pClient->Collision();

	for(int i = 0; i < Ticks; i++)
	{
		Vel.y += Gravity;
		vec2 NewPos = Pos + Vel;

		if(pCol)
		{
			if(pCol->CheckPoint(NewPos.x, NewPos.y))
			{
				if(pCol->CheckPoint(NewPos.x, Pos.y))
				{
					Vel.x = 0;
					NewPos.x = Pos.x;
				}
				if(pCol->CheckPoint(Pos.x, NewPos.y))
				{
					Vel.y = 0;
					NewPos.y = Pos.y;
				}
				if(pCol->CheckPoint(NewPos.x, NewPos.y))
					NewPos = Pos;
			}
		}

		Pos = NewPos;
		Vel.x *= AirFriction;
	}

	return Pos;
}

vec2 CAimbot::PredictProjectileAim(int Dummy, int TargetId, int Weapon)
{
	if(!m_pClient || TargetId < 0)
		return vec2(0, 0);

	int LocalId = m_pClient->m_aLocalIds[Dummy];
	if(LocalId < 0)
		return vec2(0, 0);

	vec2 MyPos = m_pClient->m_aClients[LocalId].m_Predicted.m_Pos;
	vec2 TargetPos = m_pClient->m_aClients[TargetId].m_Predicted.m_Pos;

	const CTuningParams *pTuning = GetCurrentTuning();
	if(!pTuning)
		return TargetPos;

	float Curvature = 0;
	float Speed = 0;

	switch(Weapon)
	{
	case WEAPON_GUN:
		Curvature = pTuning->m_GunCurvature;
		Speed = pTuning->m_GunSpeed;
		break;
	case WEAPON_GRENADE:
		Curvature = pTuning->m_GrenadeCurvature;
		Speed = pTuning->m_GrenadeSpeed;
		break;
	default:
		return TargetPos;
	}

	if(Speed < 1.0f)
		return TargetPos;

	vec2 TargetFuturePos = TargetPos;

	for(int OuterIter = 0; OuterIter < 3; OuterIter++)
	{
		float Dist = distance(MyPos, TargetFuturePos);
		if(Dist < 1.0f)
			break;

		float FlightTime = Dist / Speed;
		int FlightTicks = std::max(1, (int)(FlightTime * (float)SERVER_TICK_SPEED));

		TargetFuturePos = PredictTargetPos(TargetId, FlightTicks);
	}

	vec2 AimPos = TargetFuturePos;

	for(int Iter = 0; Iter < 5; Iter++)
	{
		vec2 Dir = AimPos - MyPos;
		float DirLen = length(Dir);
		if(DirLen < 1.0f)
			break;
		Dir = Dir / DirLen;

		vec2 ShootPos = MyPos + Dir * CCharacterCore::PhysicalSize() * 0.75f;

		float TimeSec = distance(ShootPos, TargetFuturePos) / Speed;

		vec2 ProjLand = CalcPos(ShootPos, Dir, Curvature, Speed, TimeSec);

		vec2 Error = TargetFuturePos - ProjLand;
		AimPos = AimPos + Error;

		if(length(Error) < 0.5f)
			break;
	}

	return AimPos;
}

vec2 CAimbot::PredictHookAim(int Dummy, int TargetId)
{
	if(!m_pClient || TargetId < 0)
		return vec2(0, 0);

	int LocalId = m_pClient->m_aLocalIds[Dummy];
	if(LocalId < 0)
		return vec2(0, 0);

	vec2 MyPos = m_pClient->m_aClients[LocalId].m_Predicted.m_Pos;
	vec2 TargetPos = m_pClient->m_aClients[TargetId].m_Predicted.m_Pos;

	const CTuningParams *pTuning = GetCurrentTuning();
	if(!pTuning)
		return TargetPos;

	float HookFireSpeed = pTuning->m_HookFireSpeed;
	if(HookFireSpeed < 1.0f)
		return TargetPos;

	vec2 PredictedPos = TargetPos;

	for(int Iter = 0; Iter < 3; Iter++)
	{
		float Dist = distance(MyPos, PredictedPos);
		if(Dist < 1.0f)
			break;

		int FlightTicks = std::max(1, (int)(Dist / HookFireSpeed));
		PredictedPos = PredictTargetPos(TargetId, FlightTicks);
	}

	return PredictedPos;
}