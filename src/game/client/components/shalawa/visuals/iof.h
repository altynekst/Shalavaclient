#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_VISUALS_IOF_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_VISUALS_IOF_H

#include <imgui.h>

class CAudioCapture;

class CIOF
{
public:
    CIOF();
    ~CIOF();
    static CIOF &Instance();

    void Init();
    void Shutdown();
    void Update(float deltaTime);
    void Render(ImVec2 pos, ImVec2 size, float scale, int style);

private:
    ImVec4 HSVtoRGB(float h, float s, float v);
    ImVec4 LerpColor(ImVec4 a, ImVec4 b, float t);
    void DrawBlurredShadow(ImDrawList *d, ImVec2 pos, ImVec2 size, int range, ImVec4 color);

    void RenderBars(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale);
    void RenderWave(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale);
    void RenderCircular(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale);
    void RenderDots(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale);
    void RenderPulse(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale);
    void RenderTrackInfo(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale);
    void RenderMiniEq(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale);
    void RenderVolumeBar(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale);

    CAudioCapture *m_pAudio;
    bool m_Initialized;
    float m_Time;
    float m_GradientHue;
    float m_BeatScale;
    float m_BeatGlow;
};

#endif