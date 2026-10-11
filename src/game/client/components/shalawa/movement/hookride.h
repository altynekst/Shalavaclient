#ifndef GAME_CLIENT_COMPONENTS_HOOKRIDE_H
#define GAME_CLIENT_COMPONENTS_HOOKRIDE_H

#include <base/vmath.h>

class CControls;
class CGameClient;
class CCollision;

class CHookRide
{
    CControls *m_pControls;
    CGameClient *m_pGameClient;

    CCollision *Collision();
    vec2 FindHookPoint(vec2 Pos, float Range);

public:
    CHookRide();
    void Init(CControls *pControls, CGameClient *pGameClient);
    void Tick(int LocalId);
};

#endif