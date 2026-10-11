#include "hookride.h"

#include <game/client/gameclient.h>
#include <game/client/components/controls.h>
#include <game/collision.h>
#include <engine/shared/config.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>
#include <cmath>

static float s_TargetX[2] = {OBF_F(0), OBF_F(0)};
static float s_TargetY[2] = {OBF_F(0), OBF_F(0)};
static bool s_HasHooked[2] = {false, false};

CHookRide::CHookRide()
{
    ANTI_DISASM_BLOCK();
    m_pControls = nullptr;
    m_pGameClient = nullptr;
}

void CHookRide::Init(CControls *pControls, CGameClient *pGameClient)
{
    ANTI_DISASM_BLOCK();
    m_pControls = pControls;
    m_pGameClient = pGameClient;
}

CCollision *CHookRide::Collision()
{
    ANTI_DISASM_BLOCK();
    return m_pGameClient->Collision();
}

vec2 CHookRide::FindHookPoint(vec2 Pos, float Range)
{
    ANTI_DISASM_BLOCK();
    vec2 Best = vec2(OI(0), OI(0));
    float BestDist = Range;

    for(int Deg = -OI(135); Deg <= -OI(45); Deg += OI(5))
    {
        float Angle = Deg * pi / OBF_F(180);
        vec2 Dir = vec2(std::cos(Angle), std::sin(Angle));
        vec2 End = Pos + Dir * Range;

        vec2 Hit, Before;
        if(Collision()->IntersectLine(Pos, End, &Hit, &Before))
        {
            float Dist = distance(Pos, Before);
            if(Dist < OBF_F(50))
                continue;
            if(Dist < BestDist)
            {
                BestDist = Dist;
                Best = Before;
            }
        }
    }

    return Best;
}

void CHookRide::Tick(int LocalId)
{
    ANTI_DISASM_BLOCK();
    if(!m_pGameClient || !m_pControls)
        return;
    if(LocalId < 0 || LocalId > 1)
        return;
    int ClientId = m_pGameClient->m_aLocalIds[LocalId];
    if(ClientId < OI(0))
        return;

    vec2 Pos = m_pGameClient->m_aClients[ClientId].m_Predicted.m_Pos;
    vec2 Vel = m_pGameClient->m_aClients[ClientId].m_Predicted.m_Vel;
    CNetObj_PlayerInput *pInput = &m_pControls->m_aInputData[LocalId];

    if(!g_Config.m_shalawaHookRide)
    {
        s_TargetX[LocalId] = Pos.x;
        s_TargetY[LocalId] = Pos.y;
        if(s_HasHooked[LocalId])
        {
            pInput->m_Hook = OI(0);
            s_HasHooked[LocalId] = false;
        }
        return;
    }

    s_HasHooked[LocalId] = true;

    float Speed = (float)g_Config.m_shalawaHookRideSpeed;
    if(m_pControls->m_aInputDirectionRight[LocalId])
        s_TargetX[LocalId] += Speed;
    if(m_pControls->m_aInputDirectionLeft[LocalId])
        s_TargetX[LocalId] -= Speed;
    if(pInput->m_Jump)
        s_TargetY[LocalId] -= Speed;
    if(m_pGameClient->Input()->KeyIsPressed(KEY_S))
        s_TargetY[LocalId] += Speed;

    m_pControls->m_aInputDirectionLeft[LocalId] = OI(0);
    m_pControls->m_aInputDirectionRight[LocalId] = OI(0);
    pInput->m_Jump = OI(0);

    vec2 HookPos = FindHookPoint(Pos, OBF_F(380));
    if(HookPos == vec2(OI(0), OI(0)))
    {
        pInput->m_Hook = OI(0);
        return;
    }

    if(Pos.x + Vel.x - s_TargetX[LocalId] > OBF_F(1))
        pInput->m_Direction = -OI(1);
    else if(s_TargetX[LocalId] - Pos.x + Vel.x > OBF_F(1))
        pInput->m_Direction = OI(1);
    else
        pInput->m_Direction = OI(0);

    float Priority = std::abs(Pos.y - s_TargetY[LocalId]) / OFR(6685,1000);
    if(Priority < OBF_F(1))
        Priority = OBF_F(1);

    float VelMod = OBF_F(0);
    if(s_TargetY[LocalId] < Pos.y)
        VelMod = -OFR(55,100) * Priority;
    else if(s_TargetY[LocalId] > Pos.y)
        VelMod = OFR(55,100) * Priority;

    if(Vel.y > VelMod)
    {
        pInput->m_TargetX = (int)(HookPos.x - Pos.x);
        pInput->m_TargetY = (int)(HookPos.y - Pos.y);
        pInput->m_Hook = OI(1);
    }
    else
    {
        pInput->m_Hook = OI(0);
    }
}