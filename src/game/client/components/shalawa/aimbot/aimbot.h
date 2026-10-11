#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_AIMBOT_AIMBOT_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_AIMBOT_AIMBOT_H

#include <base/vmath.h>

class CControls;
class CGameClient;
class CCollision;
class CTuningParams;

class CAimbot
{
public:
	CAimbot();
	void Init(CControls *pControls, CGameClient *pClient);
	void Reset();
	void Tick(int Dummy);
	void OnRender();

	int GetCurrentTargetId() const { return m_CurrentTargetId; }
	vec2 GetCurrentAimPoint() const { return m_CurrentAimPoint; }
	bool IsActive() const { return m_Active; }

private:
	CControls *m_pControls;
	CGameClient *m_pClient;

	int m_CurrentTargetId;
	vec2 m_CurrentAimPoint;
	bool m_Active;

	int FindTarget(int Dummy, bool HookMode);

	vec2 PredictTargetPos(int TargetId, int Ticks);
	vec2 PredictProjectileAim(int Dummy, int TargetId, int Weapon);
	vec2 PredictHookAim(int Dummy, int TargetId);

	vec2 FindEdgePoint(int Dummy, int TargetId);

	vec2 ApplySmoothAim(int Dummy, vec2 AimPoint, vec2 PlayerPos);
	vec2 GetHitboxEdgePoint(vec2 PlayerPos, vec2 TargetPos, vec2 CurrentAimDir);

	bool HasLineOfSight(vec2 From, vec2 To);
	const CTuningParams *GetCurrentTuning();
};

#endif