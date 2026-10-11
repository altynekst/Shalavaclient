#include "aimbot.h"

#include <base/math.h>
#include <base/system.h>
#include <base/vmath.h>
#include <algorithm>
#include <cmath>
#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <game/client/gameclient.h>
#include <game/client/components/controls.h>
#include <game/client/components/camera.h>
#include <game/collision.h>
#include <game/gamecore.h>

CAimbot::CAimbot()
{
	m_pControls = nullptr;
	m_pClient = nullptr;
	m_CurrentTargetId = -1;
	m_CurrentAimPoint = vec2(0, 0);
	m_Active = false;
}

void CAimbot::Init(CControls *pControls, CGameClient *pClient)
{
	m_pControls = pControls;
	m_pClient = pClient;
}

void CAimbot::Reset()
{
	m_CurrentTargetId = -1;
	m_CurrentAimPoint = vec2(0, 0);
	m_Active = false;
}

const CTuningParams *CAimbot::GetCurrentTuning()
{
	if(!m_pClient)
		return nullptr;
	return &m_pClient->m_aTuning[g_Config.m_ClDummy];
}

bool CAimbot::HasLineOfSight(vec2 From, vec2 To)
{
	if(!m_pClient)
		return false;
	CCollision *pCol = m_pClient->Collision();
	if(!pCol)
		return false;
	vec2 CollisionPoint;
	int Hit = pCol->IntersectLine(From, To, &CollisionPoint, nullptr);
	return Hit == 0;
}

void CAimbot::Tick(int Dummy)
{
	m_Active = false;
	m_CurrentTargetId = -1;

	if(!g_Config.m_shalawaAimbot)
		return;
	if(!m_pControls || !m_pClient)
		return;
	if(!m_pClient->m_Snap.m_pLocalCharacter)
		return;

	int ClientId = m_pClient->m_aLocalIds[Dummy];
	if(ClientId < 0 || !m_pClient->m_aClients[ClientId].m_Active)
		return;

	vec2 MyPos = m_pClient->m_aClients[ClientId].m_Predicted.m_Pos;
	int CurrentWeapon = m_pClient->m_Snap.m_pLocalCharacter->m_Weapon;

	bool HookHeld = (m_pControls->m_aInputData[Dummy].m_Hook != 0);
	bool FireHeld = (m_pControls->m_aInputData[Dummy].m_Fire & 1) != 0;
	bool HookMode = g_Config.m_shalawaAimbotHook && HookHeld;

	if(g_Config.m_shalawaAimbotOnFire && !HookMode && !FireHeld)
		return;

	int TargetId = FindTarget(Dummy, HookMode);
	if(TargetId < 0)
		return;

	m_CurrentTargetId = TargetId;

	vec2 TargetPos = m_pClient->m_aClients[TargetId].m_Predicted.m_Pos;
	vec2 AimPoint;
	bool IsEdgeShot = false;

	if(HookMode)
	{
		if(g_Config.m_shalawaAimbotEdge)
		{
			vec2 EdgePt = FindEdgePoint(Dummy, TargetId);
			if(length(EdgePt) > 0.1f)
			{
				AimPoint = MyPos + EdgePt;
				IsEdgeShot = true;
			}
			else
			{
				AimPoint = PredictHookAim(Dummy, TargetId);
			}
		}
		else
		{
			AimPoint = PredictHookAim(Dummy, TargetId);
		}
	}
	else
	{
		switch(CurrentWeapon)
		{
		case WEAPON_HAMMER:
		{
			float HammerRange = CCharacterCore::PhysicalSize() * 1.25f;
			if(distance(MyPos, TargetPos) > HammerRange + 10.0f)
				return;
			AimPoint = TargetPos;
			break;
		}
		case WEAPON_GUN:
		case WEAPON_GRENADE:
			AimPoint = PredictProjectileAim(Dummy, TargetId, CurrentWeapon);
			break;
		case WEAPON_SHOTGUN:
		case WEAPON_LASER:
			AimPoint = TargetPos;
			break;
		default:
			AimPoint = TargetPos;
			break;
		}
	}

	m_CurrentAimPoint = AimPoint;

	vec2 AimRelative;

	if(IsEdgeShot)
	{
		vec2 AimDir = AimPoint - MyPos;
		float AimLen = length(AimDir);
		if(AimLen < 0.001f)
			return;

		float CurrentDist = length(m_pControls->m_aMousePos[Dummy]);
		float MaxDist = m_pControls->GetMaxMouseDistance();
		float MinDist = m_pControls->GetMinMouseDistance();
		if(MaxDist < 1.0f)
			MaxDist = 1.0f;

		float DistRatio = CurrentDist / MaxDist;
		float FinalDist = std::clamp(DistRatio * MaxDist, MinDist, MaxDist);

		AimRelative = (AimDir / AimLen) * FinalDist;
	}
	else
	{
		AimRelative = ApplySmoothAim(Dummy, AimPoint, MyPos);
	}

	if(g_Config.m_shalawaAimbotSilent)
	{
		m_pControls->m_aInputData[Dummy].m_TargetX = (int)AimRelative.x;
		m_pControls->m_aInputData[Dummy].m_TargetY = (int)AimRelative.y;
	}
	else
	{
		m_pControls->m_aMousePos[Dummy] = AimRelative;
		m_pControls->m_aInputData[Dummy].m_TargetX = (int)AimRelative.x;
		m_pControls->m_aInputData[Dummy].m_TargetY = (int)AimRelative.y;
	}

	m_Active = true;
}

int CAimbot::FindTarget(int Dummy, bool HookMode)
{
	int LocalId = m_pClient->m_aLocalIds[Dummy];
	int OtherId = m_pClient->m_aLocalIds[!Dummy];
	vec2 MyPos = m_pClient->m_aClients[LocalId].m_Predicted.m_Pos;

	vec2 MouseDir = m_pControls->m_aMousePos[Dummy];
	float MouseLen = length(MouseDir);
	if(MouseLen < 0.001f)
		MouseDir = vec2(1, 0);
	else
		MouseDir = MouseDir / MouseLen;

	float MaxRange = (float)g_Config.m_shalawaAimbotRange;

	if(HookMode)
	{
		const CTuningParams *pTuning = GetCurrentTuning();
		if(pTuning)
			MaxRange = std::min(MaxRange, (float)pTuning->m_HookLength);
	}

	float HalfFovRad = g_Config.m_shalawaAimbotFov * (pi / 180.0f) / 2.0f;

	int BestTarget = -1;
	float BestScore = 1e9f;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == LocalId || i == OtherId)
			continue;
		if(!m_pClient->m_Snap.m_aCharacters[i].m_Active)
			continue;
		if(!m_pClient->m_aClients[i].m_Active)
			continue;

		vec2 Pos = m_pClient->m_aClients[i].m_Predicted.m_Pos;
		float Dist = distance(MyPos, Pos);

		if(Dist > MaxRange || Dist < 0.1f)
			continue;

		vec2 ToTarget = (Pos - MyPos) / Dist;
		float DotProduct = std::clamp(dot(MouseDir, ToTarget), -1.0f, 1.0f);
		float Angle = acosf(DotProduct);

		if(Angle > HalfFovRad)
			continue;

		float Score;
		if(g_Config.m_shalawaAimbotMode == 0)
			Score = Angle;
		else
			Score = Dist;

		if(Score < BestScore)
		{
			BestScore = Score;
			BestTarget = i;
		}
	}

	return BestTarget;
}

vec2 CAimbot::ApplySmoothAim(int Dummy, vec2 AimPoint, vec2 PlayerPos)
{
	vec2 CurrentMouse = m_pControls->m_aMousePos[Dummy];
	float CurrentDist = length(CurrentMouse);
	float MaxDist = m_pControls->GetMaxMouseDistance();
	float MinDist = m_pControls->GetMinMouseDistance();

	if(MaxDist < 1.0f)
		MaxDist = 1.0f;
	float DistRatio = std::clamp(CurrentDist / MaxDist, 0.0f, 1.0f);

	vec2 CurrentDir = (CurrentDist > 0.001f) ? (CurrentMouse / CurrentDist) : vec2(1, 0);

	vec2 HitboxPoint = GetHitboxEdgePoint(PlayerPos, AimPoint, CurrentDir);

	vec2 AimDir = HitboxPoint - PlayerPos;
	float AimLen = length(AimDir);
	if(AimLen < 0.001f)
		return CurrentMouse;

	vec2 AimNormalized = AimDir / AimLen;

	float FinalDist = std::clamp(DistRatio * MaxDist, MinDist, MaxDist);

	return AimNormalized * FinalDist;
}

vec2 CAimbot::GetHitboxEdgePoint(vec2 PlayerPos, vec2 TargetPos, vec2 CurrentAimDir)
{
	float HitboxRadius = CCharacterCore::PhysicalSize() / 2.0f;
	float EdgeFactor = 0.75f;

	vec2 ToTarget = TargetPos - PlayerPos;
	float ToTargetLen = length(ToTarget);
	if(ToTargetLen < 0.001f)
		return TargetPos;

	vec2 ToTargetDir = ToTarget / ToTargetLen;
	vec2 Perp = vec2(-ToTargetDir.y, ToTargetDir.x);

	float PerpComponent = dot(CurrentAimDir, Perp);

	vec2 Offset = Perp * PerpComponent * HitboxRadius * EdgeFactor;

	float OffsetLen = length(Offset);
	if(OffsetLen > HitboxRadius * EdgeFactor)
		Offset = (Offset / OffsetLen) * HitboxRadius * EdgeFactor;

	return TargetPos + Offset;
}

void CAimbot::OnRender()
{
	if(!g_Config.m_shalawaAimbotVisuals)
		return;
	if(!m_Active || m_CurrentTargetId < 0)
		return;
	if(!m_pClient || !m_pControls)
		return;

	int Dummy = g_Config.m_ClDummy;
	int LocalId = m_pClient->m_aLocalIds[Dummy];
	if(LocalId < 0)
		return;

	vec2 MyPos = m_pClient->m_aClients[LocalId].m_Predicted.m_Pos;
	vec2 TargetPos = m_pClient->m_aClients[m_CurrentTargetId].m_Predicted.m_Pos;

	IGraphics *pGraphics = m_pClient->Graphics();
	if(!pGraphics)
		return;

	float aOldPoints[4];
	pGraphics->GetScreen(&aOldPoints[0], &aOldPoints[1], &aOldPoints[2], &aOldPoints[3]);

	float aScreenPoints[4];
	pGraphics->MapScreenToWorld(
		m_pClient->m_Camera.m_Center.x, m_pClient->m_Camera.m_Center.y,
		100.0f, 100.0f, 100.0f, 0.0f, 0.0f,
		pGraphics->ScreenAspect(), m_pClient->m_Camera.m_Zoom,
		aScreenPoints);
	pGraphics->MapScreen(aScreenPoints[0], aScreenPoints[1], aScreenPoints[2], aScreenPoints[3]);

	pGraphics->TextureClear();
	pGraphics->LinesBegin();
	pGraphics->SetColor(1.0f, 0.2f, 0.2f, 0.7f);

	IGraphics::CLineItem Line(MyPos.x, MyPos.y, TargetPos.x, TargetPos.y);
	pGraphics->LinesDraw(&Line, 1);

	float Radius = CCharacterCore::PhysicalSize() / 2.0f;
	int Segments = 16;
	for(int i = 0; i < Segments; i++)
	{
		float Angle1 = 2.0f * pi * i / Segments;
		float Angle2 = 2.0f * pi * (i + 1) / Segments;
		IGraphics::CLineItem Seg(
			TargetPos.x + cosf(Angle1) * Radius, TargetPos.y + sinf(Angle1) * Radius,
			TargetPos.x + cosf(Angle2) * Radius, TargetPos.y + sinf(Angle2) * Radius);
		pGraphics->LinesDraw(&Seg, 1);
	}

	if(m_CurrentAimPoint != TargetPos)
	{
		pGraphics->SetColor(0.2f, 1.0f, 0.2f, 0.5f);
		IGraphics::CLineItem AimLine(MyPos.x, MyPos.y, m_CurrentAimPoint.x, m_CurrentAimPoint.y);
		pGraphics->LinesDraw(&AimLine, 1);
	}

	pGraphics->LinesEnd();
	pGraphics->MapScreen(aOldPoints[0], aOldPoints[1], aOldPoints[2], aOldPoints[3]);
}