#pragma once

#include <base/vmath.h>
#include <base/system.h>
#include <game/client/component.h>

class CFakeAim : public CComponent
{
public:
    int Sizeof() const override { return sizeof(*this); }
    void OnRender() override;

    vec2 GetFakeTarget(int Dummy) const { return m_aFakeTarget[Dummy]; }
    bool IsActive() const;
    bool IsBlocked(int Dummy) const;
    void OnAction(int Dummy);

    CFakeAim()
    {
        for(int i = 0; i < 2; i++)
        {
            m_aFakeTarget[i] = vec2(100.0f, 0.0f);
            m_aRobotLastTarget[i] = vec2(100.0f, 0.0f);
            m_aPrevFire[i] = 0;
            m_aPrevHook[i] = 0;
            m_aInitialized[i] = false;
            m_aBlockUntil[i] = 0;
        }
        m_SpinAngle = 0.0f;
    }

private:
    vec2 m_aFakeTarget[2];
    vec2 m_aRobotLastTarget[2];
    int m_aPrevFire[2];
    int m_aPrevHook[2];
    float m_SpinAngle;
    bool m_aInitialized[2];
    int64_t m_aBlockUntil[2];

    void UpdateFakeTarget(int Dummy);
    vec2 FindNearestPlayer(int Dummy);
    void RenderFakeAimVisual();
};