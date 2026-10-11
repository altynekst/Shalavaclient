#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_MOVEMENT_UNFREEZE_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_MOVEMENT_UNFREEZE_H

#include <base/vmath.h>

class CCollision;
class CGameClient;
class CControls;

class CUnfreezeBot
{
public:
	void Init(CControls *pControls, CGameClient *pGameClient);
	void Tick(int LocalId);

private:
	CControls *m_pControls;
	CGameClient *m_pGameClient;

	CCollision *Collision() const;
	bool HasLaser(int LocalId) const;
	bool IsLaserEquipped(int LocalId) const;
	bool CanShoot(int LocalId) const;

	int PredictMovement(int LocalId, vec2 *pPositions, int MaxTicks, int *pEnterFreeze, int *pExitFreeze) const;
	bool SimulateLaserHit(vec2 From, vec2 Dir, vec2 TargetPos, float Radius, int *pTravelTicks) const;
	bool FindBounceShot(vec2 MyPos, vec2 *pPositions, int NumPositions, int EnterFreeze, int ExitFreeze, vec2 *pDir, int *pBestFireTick) const;
};

#endif