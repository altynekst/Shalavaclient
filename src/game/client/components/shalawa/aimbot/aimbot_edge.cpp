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

vec2 CAimbot::FindEdgePoint(int Dummy, int TargetId)
{
	if(!m_pClient || TargetId < 0)
		return vec2(0, 0);

	int LocalId = m_pClient->m_aLocalIds[Dummy];
	if(LocalId < 0)
		return vec2(0, 0);

	vec2 MyPos = m_pClient->m_aClients[LocalId].m_Predicted.m_Pos;
	vec2 TargetPos = m_pClient->m_aClients[TargetId].m_Predicted.m_Pos;

	if(HasLineOfSight(MyPos, TargetPos))
		return vec2(0, 0);

	CCollision *pCol = m_pClient->Collision();
	if(!pCol)
		return vec2(0, 0);

	const CTuningParams *pTuning = GetCurrentTuning();
	if(!pTuning)
		return vec2(0, 0);

	float HookLength = pTuning->m_HookLength;
	float HitboxRadius = CCharacterCore::PhysicalSize() / 2.0f + 2.0f;

	vec2 DirectDir = TargetPos - MyPos;
	float DirectDist = length(DirectDir);
	if(DirectDist < 1.0f || DirectDist > HookLength)
		return vec2(0, 0);
	DirectDir = DirectDir / DirectDist;

	float BaseAngle = atan2f(DirectDir.y, DirectDir.x);
	float ScanRange = 35.0f * (pi / 180.0f);
	int ScanSteps = 48;

	vec2 BestDir = vec2(0, 0);
	float BestScore = 1e9f;

	for(int i = -ScanSteps; i <= ScanSteps; i++)
	{
		float Angle = BaseAngle + ScanRange * (float)i / (float)ScanSteps;
		vec2 Dir = vec2(cosf(Angle), sinf(Angle));
		vec2 End = MyPos + Dir * HookLength;

		vec2 CollisionPoint;
		int Hit = pCol->IntersectLineTeleHook(MyPos, End, &CollisionPoint, nullptr, nullptr);

		float CollisionDist = Hit ? distance(MyPos, CollisionPoint) : HookLength;

		vec2 AP = TargetPos - MyPos;
		float t = dot(AP, Dir);
		t = std::clamp(t, 0.0f, std::min(CollisionDist, HookLength));
		vec2 ClosestPoint = MyPos + Dir * t;

		float DistToTarget = distance(TargetPos, ClosestPoint);

		if(DistToTarget <= HitboxRadius)
		{
			float TargetAlongLine = dot(AP, Dir);
			if(!Hit || CollisionDist > TargetAlongLine - HitboxRadius)
			{
				float Score = DistToTarget + fabsf((float)i) * 0.01f;
				if(Score < BestScore)
				{
					BestScore = Score;
					BestDir = Dir * DirectDist;
				}
			}
		}
	}

	return BestDir;
}