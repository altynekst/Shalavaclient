#include "fake_aim.h"

#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/client/components/controls.h>
#include <base/math.h>

#include <cmath>
#include <cstdlib>

bool CFakeAim::IsActive() const
{
    return g_Config.m_shalawaFakeAim != 0
        && GameClient()->m_Snap.m_pLocalCharacter != nullptr
        && Client()->State() == IClient::STATE_ONLINE;
}

bool CFakeAim::IsBlocked(int Dummy) const
{
    return time_get() < m_aBlockUntil[Dummy];
}

void CFakeAim::OnAction(int Dummy)
{
    m_aBlockUntil[Dummy] = time_get() + time_freq() * 150 / 1000;
}

vec2 CFakeAim::FindNearestPlayer(int Dummy)
{
    const int LocalId = GameClient()->m_aLocalIds[Dummy];
    if(LocalId < 0 || !GameClient()->m_aClients[LocalId].m_Active)
        return vec2(100.0f, 0.0f);

    const vec2 LocalPos = GameClient()->m_aClients[LocalId].m_Predicted.m_Pos;
    vec2 BestDir = vec2(100.0f, 0.0f);
    float BestDist = 1e30f;

    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(i == LocalId || !GameClient()->m_aClients[i].m_Active)
            continue;
        const vec2 Dir = GameClient()->m_aClients[i].m_Predicted.m_Pos - LocalPos;
        const float Dist = length(Dir);
        if(Dist < BestDist)
        {
            BestDist = Dist;
            BestDir = Dir;
        }
    }
    return BestDir;
}

void CFakeAim::UpdateFakeTarget(int Dummy)
{
    const vec2 RealTarget = GameClient()->m_Controls.m_aMousePos[Dummy];

    if(!m_aInitialized[Dummy])
    {
        m_aRobotLastTarget[Dummy] = RealTarget;
        m_aFakeTarget[Dummy] = RealTarget;
        m_aInitialized[Dummy] = true;
    }

    if(IsBlocked(Dummy))
    {
        m_aFakeTarget[Dummy] = RealTarget;
        return;
    }

    const int Mode = g_Config.m_shalawaFakeAimMode;
    const float RealLen = length(RealTarget);
    const float R = maximum(RealLen, 50.0f);

    switch(Mode)
    {
    case 0:
        m_aFakeTarget[Dummy] = RealTarget;
        break;

    case 1:
    {
        const int CurFire = GameClient()->m_Controls.m_aLastData[Dummy].m_Fire;
        const int CurHook = GameClient()->m_Controls.m_aLastData[Dummy].m_Hook;
        const bool Fired = (CurFire != m_aPrevFire[Dummy]);
        const bool Hooked = (CurHook != 0 && m_aPrevHook[Dummy] == 0);
        m_aPrevFire[Dummy] = CurFire;
        m_aPrevHook[Dummy] = CurHook;
        if(Fired || Hooked)
            m_aRobotLastTarget[Dummy] = RealTarget;
        m_aFakeTarget[Dummy] = m_aRobotLastTarget[Dummy];
        break;
    }

    case 2:
    {
        const float Speed = g_Config.m_shalawaFakeAimSpeed / 100.0f;
        m_SpinAngle += Speed;
        if(m_SpinAngle > 2.0f * pi)
            m_SpinAngle -= 2.0f * pi;

        if(g_Config.m_shalawaFakeAimSpeed > 200)
        {
            const float Wave = std::sin(m_SpinAngle * 3.0f) * 0.3f + 1.0f;
            const float DynR = R * Wave;
            m_aFakeTarget[Dummy] = vec2(std::cos(m_SpinAngle) * DynR, std::sin(m_SpinAngle) * DynR);
        }
        else
        {
            m_aFakeTarget[Dummy] = vec2(std::cos(m_SpinAngle) * R, std::sin(m_SpinAngle) * R);
        }
        break;
    }

    case 3:
    {
        const float Angle = (rand() % 36000) / 36000.0f * 2.0f * pi;
        const float RandR = R * (0.5f + (rand() % 100) / 200.0f);
        m_aFakeTarget[Dummy] = vec2(std::cos(Angle) * RandR, std::sin(Angle) * RandR);
        break;
    }

    case 4:
    {
        if(RealLen < 1.0f)
            m_aFakeTarget[Dummy] = vec2(-100.0f, 0.0f);
        else
            m_aFakeTarget[Dummy] = -RealTarget;
        break;
    }

    case 5:
        m_aFakeTarget[Dummy] = FindNearestPlayer(Dummy);
        break;

    default:
        m_aFakeTarget[Dummy] = RealTarget;
        break;
    }

    if(m_aFakeTarget[Dummy].x == 0.0f && m_aFakeTarget[Dummy].y == 0.0f)
        m_aFakeTarget[Dummy].x = 1.0f;
}

void CFakeAim::RenderFakeAimVisual()
{
    if(!g_Config.m_shalawaFakeAimVisible)
        return;

    const int Dummy = g_Config.m_ClDummy;
    const int ClientId = GameClient()->m_aLocalIds[Dummy];
    if(ClientId < 0 || !GameClient()->m_aClients[ClientId].m_Active)
        return;

    const vec2 PlayerPos = GameClient()->m_aClients[ClientId].m_Predicted.m_Pos;
    const vec2 FakeDir = m_aFakeTarget[Dummy];
    const float FakeLen = length(FakeDir);
    if(FakeLen < 1.0f)
        return;

    const vec2 Dir = FakeDir / FakeLen;
    const vec2 Perp = vec2(-Dir.y, Dir.x);
    const float DrawLen = minimum(FakeLen, 200.0f);
    const vec2 End = PlayerPos + Dir * DrawLen;

    Graphics()->TextureClear();
    Graphics()->QuadsBegin();

    const bool Blocked = IsBlocked(Dummy);
    const float Alpha = 0.5f;

    if(Blocked)
        Graphics()->SetColor(0.2f, 1.0f, 0.2f, Alpha);
    else
        Graphics()->SetColor(1.0f, 0.3f, 0.3f, Alpha);

    const float LineWidth = 1.5f;
    IGraphics::CFreeformItem Line(
        PlayerPos.x + Perp.x * LineWidth, PlayerPos.y + Perp.y * LineWidth,
        PlayerPos.x - Perp.x * LineWidth, PlayerPos.y - Perp.y * LineWidth,
        End.x + Perp.x * LineWidth * 0.3f, End.y + Perp.y * LineWidth * 0.3f,
        End.x - Perp.x * LineWidth * 0.3f, End.y - Perp.y * LineWidth * 0.3f
    );
    Graphics()->QuadsDrawFreeform(&Line, 1);

    const float DotSize = 4.0f;
    IGraphics::CFreeformItem Dot(
        End.x - DotSize, End.y - DotSize,
        End.x + DotSize, End.y - DotSize,
        End.x - DotSize, End.y + DotSize,
        End.x + DotSize, End.y + DotSize
    );
    Graphics()->QuadsDrawFreeform(&Dot, 1);

    if(!Blocked && g_Config.m_shalawaFakeAimMode == 2 && g_Config.m_shalawaFakeAimSpeed > 100)
    {
        Graphics()->SetColor(1.0f, 0.3f, 0.3f, Alpha * 0.3f);
        const int Segments = 32;
        const float CircleR = minimum(DrawLen, 120.0f);
        for(int i = 0; i < Segments; i++)
        {
            const float A1 = 2.0f * pi * i / Segments;
            const float A2 = 2.0f * pi * (i + 1) / Segments;
            const float RW = 1.0f;
            IGraphics::CFreeformItem Seg(
                PlayerPos.x + std::cos(A1) * (CircleR - RW), PlayerPos.y + std::sin(A1) * (CircleR - RW),
                PlayerPos.x + std::cos(A1) * (CircleR + RW), PlayerPos.y + std::sin(A1) * (CircleR + RW),
                PlayerPos.x + std::cos(A2) * (CircleR - RW), PlayerPos.y + std::sin(A2) * (CircleR - RW),
                PlayerPos.x + std::cos(A2) * (CircleR + RW), PlayerPos.y + std::sin(A2) * (CircleR + RW)
            );
            Graphics()->QuadsDrawFreeform(&Seg, 1);
        }
    }

    Graphics()->QuadsEnd();
}

void CFakeAim::OnRender()
{
    if(!IsActive())
        return;

    for(int i = 0; i < 2; i++)
        UpdateFakeTarget(i);

    RenderFakeAimVisual();
}