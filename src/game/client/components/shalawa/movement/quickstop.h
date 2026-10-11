#pragma once

#include <game/client/component.h>
#include <base/vmath.h>
#include <generated/protocol.h>

class CQuickStop : public CComponent
{
public:
    int Sizeof() const override { return sizeof(*this); }
    void OnRender() override {}
    void OnReset() override;

    void Tick(int Dummy, CNetObj_PlayerInput &Input, vec2 Vel);

private:
    int m_aStopTicks[2];
    float m_aLastVelX[2];
};