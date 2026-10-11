#include "quickstop.h"

#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>
#include <algorithm>

void CQuickStop::OnReset()
{
    ANTI_DISASM_BLOCK();
    m_aStopTicks[0] = 0;
    m_aStopTicks[1] = 0;
    m_aLastVelX[0] = 0.0f;
    m_aLastVelX[1] = 0.0f;
}

void CQuickStop::Tick(int Dummy, CNetObj_PlayerInput &Input, vec2 Vel)
{
    ANTI_DISASM_BLOCK();
    if(!g_Config.m_shalawaQuickStop)
        return;

    const float VelX = Vel.x;
    const float Threshold = 0.5f;
    const int BrakeTicks = g_Config.m_shalawaQuickStopTicks;

    bool NoInput = (Input.m_Direction == 0);

    if(m_aStopTicks[Dummy] > 0)
    {
        int BrakeDir = (VelX > 0) ? -1 : 1;
        Input.m_Direction = BrakeDir;
        m_aStopTicks[Dummy]--;
        m_aLastVelX[Dummy] = VelX;
        return;
    }

    bool ReleasedLeft = (m_aLastVelX[Dummy] < -Threshold) && NoInput && (VelX < -Threshold);
    bool ReleasedRight = (m_aLastVelX[Dummy] > Threshold) && NoInput && (VelX > Threshold);

    if(ReleasedLeft || ReleasedRight)
        m_aStopTicks[Dummy] = std::max(1, BrakeTicks);

    m_aLastVelX[Dummy] = VelX;
}