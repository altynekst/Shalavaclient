#include "iof.h"
#include "audio_capture.h"
#include <engine/shared/config.h>
#include <cmath>
#include <algorithm>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265f
#endif

CIOF::CIOF()
{
    m_pAudio = nullptr;
    m_Initialized = false;
    m_Time = 0; m_GradientHue = 0;
    m_BeatScale = 0; m_BeatGlow = 0;
}

CIOF::~CIOF() { Shutdown(); }

CIOF &CIOF::Instance() { static CIOF s; return s; }

void CIOF::Init()
{
    if(m_Initialized) return;
    m_pAudio = new CAudioCapture();
    m_pAudio->Init();
    m_Initialized = true;
}

void CIOF::Shutdown()
{
    if(m_pAudio) { m_pAudio->Shutdown(); delete m_pAudio; m_pAudio = nullptr; }
    m_Initialized = false;
}

void CIOF::Update(float dt)
{
    if(!m_Initialized || !m_pAudio) return;
    m_Time += dt;
    m_GradientHue += dt * 0.08f;
    if(m_GradientHue > 1.0f) m_GradientHue -= 1.0f;
    m_pAudio->Update(dt, (float)g_Config.m_shalawaIOFSensitivity, (float)g_Config.m_shalawaIOFSmoothing);
    if(m_pAudio->IsBeat()) { m_BeatScale = 1.0f; m_BeatGlow = 1.0f; }
    m_BeatScale = std::max(0.0f, m_BeatScale - dt * 10.0f);
    m_BeatGlow = std::max(0.0f, m_BeatGlow - dt * 6.0f);
}

ImVec4 CIOF::HSVtoRGB(float h, float s, float v)
{
    float c = v * s, x = c * (1 - fabsf(fmodf(h * 6, 2) - 1)), m = v - c;
    float r, g, b;
    if(h < 1.f/6) {r=c;g=x;b=0;} else if(h < 2.f/6) {r=x;g=c;b=0;}
    else if(h < 3.f/6) {r=0;g=c;b=x;} else if(h < 4.f/6) {r=0;g=x;b=c;}
    else if(h < 5.f/6) {r=x;g=0;b=c;} else {r=c;g=0;b=x;}
    return ImVec4(r+m,g+m,b+m,1);
}

ImVec4 CIOF::LerpColor(ImVec4 a, ImVec4 b, float t)
{
    t = std::clamp(t, 0.f, 1.f);
    return ImVec4(a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t, a.w+(b.w-a.w)*t);
}

void CIOF::DrawBlurredShadow(ImDrawList *d, ImVec2 p, ImVec2 s, int r, ImVec4 c)
{
    for(int i = r; i >= 0; i--)
    {
        float a = (c.w / (r + 1)) * (r - i + 1) * 0.5f;
        float e = (float)i * 1.5f;
        d->AddRectFilled(ImVec2(p.x-e,p.y-e), ImVec2(p.x+s.x+e,p.y+s.y+e),
            ImGui::ColorConvertFloat4ToU32(ImVec4(c.x,c.y,c.z,a)), 8);
    }
}

void CIOF::RenderMiniEq(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale)
{
    if(!m_pAudio || !m_pAudio->IsCapturing()) return;
    int bars = 4;
    float barW = 2.5f * scale;
    float gap = 1.5f * scale;
    float totalW = bars * barW + (bars - 1) * gap;
    float startX = pos.x + (size.x - totalW) * 0.5f;

    for(int i = 0; i < bars; i++)
    {
        float v = m_pAudio->GetBand(i * 6 + 1);
        v = std::clamp(v * 2.0f + 0.15f, 0.15f, 1.0f);
        float barH = v * size.y;
        float bx = startX + i * (barW + gap);
        float by = pos.y + size.y - barH;
        ImVec4 col = HSVtoRGB(fmodf(m_GradientHue + i * 0.08f, 1.f), 0.6f, 0.95f);
        col.w = 0.85f;
        d->AddRectFilled(ImVec2(bx, by), ImVec2(bx + barW, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(col), barW * 0.3f);
    }
}

void CIOF::RenderVolumeBar(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale)
{
    if(!m_pAudio) return;
    float vol = std::clamp(m_pAudio->GetVolume() * 3.0f, 0.f, 1.f);

    d->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(255, 255, 255, 8), size.y * 0.5f);

    float fillW = vol * size.x;
    if(fillW > 0)
    {
        ImVec4 colL = HSVtoRGB(m_GradientHue, 0.6f, 0.9f);
        ImVec4 colR = HSVtoRGB(fmodf(m_GradientHue + 0.2f, 1.f), 0.6f, 0.9f);
        colL.w = 0.5f + m_BeatGlow * 0.3f;
        colR.w = 0.5f + m_BeatGlow * 0.3f;
        d->AddRectFilledMultiColor(pos, ImVec2(pos.x + fillW, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(colL), ImGui::ColorConvertFloat4ToU32(colR),
            ImGui::ColorConvertFloat4ToU32(colR), ImGui::ColorConvertFloat4ToU32(colL));
    }
}

void CIOF::RenderTrackInfo(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale)
{
    if(!m_pAudio) return;
    float padX = 8 * scale, padY = 5 * scale;
    float fontH = ImGui::GetTextLineHeight();

    const char *title = m_pAudio->GetTrackTitle();
    const char *artist = m_pAudio->GetTrackArtist();

    float eqW = 20 * scale;
    float eqH = fontH * 1.5f;
    RenderMiniEq(d, ImVec2(pos.x + size.x - padX - eqW, pos.y + padY), ImVec2(eqW, eqH), scale);

    float maxTextW = size.x - padX * 2 - eqW - 8 * scale;
    d->PushClipRect(ImVec2(pos.x + padX, pos.y + padY),
        ImVec2(pos.x + padX + maxTextW, pos.y + size.y), true);

    ImVec4 titleCol = HSVtoRGB(m_GradientHue, 0.45f, 1.0f);
    titleCol.w = 0.95f;
    d->AddText(ImVec2(pos.x + padX, pos.y + padY),
        ImGui::ColorConvertFloat4ToU32(titleCol), title ? title : "...");

    if(artist && artist[0])
    {
        d->AddText(ImVec2(pos.x + padX, pos.y + padY + fontH + 1 * scale),
            IM_COL32(130, 130, 155, 190), artist);
    }

    d->PopClipRect();
}

void CIOF::RenderBars(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale)
{
    int n = 28;
    float gap = 1.5f * scale;
    float barW = (size.x - gap * (n - 1)) / n;
    float groundY = pos.y + size.y * 0.78f;

    for(int i = 0; i < n; i++)
    {
        int bi = i * CAudioCapture::SPECTRUM_BANDS / n;
        float v = std::clamp(m_pAudio->GetBand(bi) + m_BeatScale * 0.12f, 0.f, 1.f);
        float maxH = (groundY - pos.y) * 0.95f;
        float h = std::max(v * maxH, 1.5f * scale);

        float bx = pos.x + i * (barW + gap);
        float by = groundY - h;

        float hue = fmodf(m_GradientHue + (float)i / n * 0.35f, 1.f);
        ImVec4 col = HSVtoRGB(hue, 0.65f, 0.95f);

        ImVec4 top = col; top.w = 0.85f + m_BeatGlow * 0.15f;
        ImVec4 bot = col; bot.w = 0.3f;
        d->AddRectFilledMultiColor(ImVec2(bx, by), ImVec2(bx + barW, groundY),
            ImGui::ColorConvertFloat4ToU32(top), ImGui::ColorConvertFloat4ToU32(top),
            ImGui::ColorConvertFloat4ToU32(bot), ImGui::ColorConvertFloat4ToU32(bot));

        float capH = 2 * scale;
        ImVec4 capCol = col; capCol.w = 0.95f;
        d->AddRectFilled(ImVec2(bx, by), ImVec2(bx + barW, by + capH),
            ImGui::ColorConvertFloat4ToU32(capCol), 1);

        float refH = h * 0.25f;
        ImVec4 refTop = col; refTop.w = 0.12f;
        ImVec4 refBot = col; refBot.w = 0.0f;
        d->AddRectFilledMultiColor(ImVec2(bx, groundY + 1 * scale),
            ImVec2(bx + barW, groundY + 1 * scale + refH),
            ImGui::ColorConvertFloat4ToU32(refTop), ImGui::ColorConvertFloat4ToU32(refTop),
            ImGui::ColorConvertFloat4ToU32(refBot), ImGui::ColorConvertFloat4ToU32(refBot));
    }
}

void CIOF::RenderWave(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale)
{
    int n = 64;
    float cy = pos.y + size.y * 0.5f;
    std::vector<ImVec2> pts;
    for(int i = 0; i < n; i++)
    {
        int wi = i * CAudioCapture::WAVEFORM_SAMPLES / n;
        float v = m_pAudio->GetWaveform(wi);
        float x = pos.x + (float)i / (n - 1) * size.x;
        float y = cy + v * size.y * 0.4f * (1 + m_BeatScale * 0.3f);
        pts.push_back(ImVec2(x, y));
    }
    if(pts.size() > 1)
    {
        ImVec4 glowC = HSVtoRGB(m_GradientHue, 0.5f, 0.8f);
        glowC.w = 0.15f + m_BeatGlow * 0.1f;
        d->AddPolyline(pts.data(), (int)pts.size(),
            ImGui::ColorConvertFloat4ToU32(glowC), 0, 4 * scale);

        ImVec4 lc = HSVtoRGB(m_GradientHue, 0.65f, 1.0f);
        lc.w = 0.9f;
        d->AddPolyline(pts.data(), (int)pts.size(),
            ImGui::ColorConvertFloat4ToU32(lc), 0, 1.5f * scale);
    }
}

void CIOF::RenderCircular(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale)
{
    float cx = pos.x + size.x * 0.5f, cy = pos.y + size.y * 0.5f;
    float baseR = std::min(size.x, size.y) * 0.25f;
    float maxR = std::min(size.x, size.y) * 0.45f;
    int n = 48;
    float beatR = baseR * (1 + m_BeatScale * 0.1f);

    ImVec4 innerGlow = HSVtoRGB(m_GradientHue, 0.4f, 0.6f);
    innerGlow.w = 0.08f + m_BeatGlow * 0.1f;
    d->AddCircleFilled(ImVec2(cx, cy), beatR * 0.8f,
        ImGui::ColorConvertFloat4ToU32(innerGlow), 48);

    ImVec4 innerLine = HSVtoRGB(m_GradientHue, 0.5f, 0.7f);
    innerLine.w = 0.3f;
    d->AddCircle(ImVec2(cx, cy), beatR,
        ImGui::ColorConvertFloat4ToU32(innerLine), 48, 1 * scale);

    for(int i = 0; i < n; i++)
    {
        float angle = (float)i / n * 2 * M_PI - M_PI / 2;
        int bi = i * CAudioCapture::SPECTRUM_BANDS / n;
        float v = std::clamp(m_pAudio->GetBand(bi) + m_BeatScale * 0.08f, 0.f, 1.f);
        float r1 = beatR, r2 = beatR + v * (maxR - baseR);
        float hue = fmodf(m_GradientHue + (float)i / n * 0.4f, 1.f);
        ImVec4 col = HSVtoRGB(hue, 0.7f, 0.95f);
        col.w = 0.3f + v * 0.6f;

        float cosA = cosf(angle), sinA = sinf(angle);
        d->AddLine(ImVec2(cx + cosA * r1, cy + sinA * r1),
                   ImVec2(cx + cosA * r2, cy + sinA * r2),
                   ImGui::ColorConvertFloat4ToU32(col), (1.5f + v * 1.5f) * scale);
    }
}

void CIOF::RenderDots(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale)
{
    int n = 36;
    float cy = pos.y + size.y * 0.55f;
    for(int i = 0; i < n; i++)
    {
        int bi = i * CAudioCapture::SPECTRUM_BANDS / n;
        float v = std::clamp(m_pAudio->GetBand(bi) + m_BeatScale * 0.1f, 0.f, 1.f);
        float x = pos.x + (float)i / (n - 1) * size.x;
        float wave = sinf(m_Time * 1.5f + i * 0.25f) * size.y * 0.08f;
        float y = cy + wave - v * size.y * 0.35f;
        float r = (1.5f + v * 3.0f) * scale;
        float hue = fmodf(m_GradientHue + (float)i / n * 0.35f, 1.f);
        ImVec4 col = HSVtoRGB(hue, 0.65f, 0.95f);

        ImVec4 glow = col; glow.w = v * 0.2f;
        d->AddCircleFilled(ImVec2(x, y), r * 2.5f,
            ImGui::ColorConvertFloat4ToU32(glow));

        col.w = 0.5f + v * 0.5f;
        d->AddCircleFilled(ImVec2(x, y), r,
            ImGui::ColorConvertFloat4ToU32(col));

        ImVec4 lineCol = col; lineCol.w = v * 0.15f;
        d->AddLine(ImVec2(x, y + r), ImVec2(x, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(lineCol), 1 * scale);
    }
}

void CIOF::RenderPulse(ImDrawList *d, ImVec2 pos, ImVec2 size, float scale)
{
    float cx = pos.x + size.x * 0.5f, cy = pos.y + size.y * 0.5f;
    float maxR = std::min(size.x, size.y) * 0.45f;
    float bass = m_pAudio->GetBass(), mid = m_pAudio->GetMid(), treb = m_pAudio->GetTreble();

    struct Ring { float v; float hueOff; float baseR; };
    Ring rings[] = {
        {bass, 0.0f, maxR * 0.9f},
        {mid, 0.15f, maxR * 0.65f},
        {treb, 0.35f, maxR * 0.4f}
    };

    for(int i = 0; i < 3; i++)
    {
        float v = std::clamp(rings[i].v + m_BeatScale * 0.15f, 0.f, 1.f);
        float r = rings[i].baseR * (0.5f + v * 0.5f);
        float hue = fmodf(m_GradientHue + rings[i].hueOff, 1.f);
        ImVec4 col = HSVtoRGB(hue, 0.6f, 0.9f);

        ImVec4 glow = col; glow.w = v * 0.1f;
        d->AddCircle(ImVec2(cx, cy), r + 3 * scale,
            ImGui::ColorConvertFloat4ToU32(glow), 48, 4 * scale);

        col.w = 0.2f + v * 0.5f;
        d->AddCircle(ImVec2(cx, cy), r,
            ImGui::ColorConvertFloat4ToU32(col), 48, (1.5f + v * 2) * scale);
    }

    float centerV = (bass + mid + treb) / 3.0f;
    ImVec4 centerCol = HSVtoRGB(m_GradientHue, 0.5f, 1.0f);
    centerCol.w = 0.3f + centerV * 0.5f + m_BeatGlow * 0.2f;
    float centerR = (3 + centerV * 5 + m_BeatScale * 3) * scale;
    d->AddCircleFilled(ImVec2(cx, cy), centerR,
        ImGui::ColorConvertFloat4ToU32(centerCol));
}

void CIOF::Render(ImVec2 pos, ImVec2 size, float scale, int style)
{
    if(!m_Initialized || !m_pAudio) return;
    if(!ImGui::GetCurrentContext()) return;
    ImDrawList *d = ImGui::GetBackgroundDrawList();

    float infoH = 36 * scale;
    float volBarH = 3 * scale;
    float vizH = size.y - infoH - volBarH - 6 * scale;

    ImVec4 glowCol = HSVtoRGB(m_GradientHue, 0.5f, 0.7f);
    glowCol.w = 0.06f + m_BeatGlow * 0.1f;
    DrawBlurredShadow(d, pos, size, 6, glowCol);

    d->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(8, 8, 12, 240), 8 * scale);

    ImVec4 borderCol = HSVtoRGB(m_GradientHue, 0.35f, 0.45f);
    borderCol.w = 0.12f + m_BeatGlow * 0.15f;
    d->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
        ImGui::ColorConvertFloat4ToU32(borderCol), 8 * scale, 0, 1);

    float lineH = 2 * scale;
    ImVec4 lineL = HSVtoRGB(m_GradientHue, 0.7f, 0.9f);
    ImVec4 lineR = HSVtoRGB(fmodf(m_GradientHue + 0.3f, 1.f), 0.7f, 0.9f);
    lineL.w = 0.7f + m_BeatGlow * 0.3f;
    lineR.w = 0.7f + m_BeatGlow * 0.3f;
    d->AddRectFilledMultiColor(
        ImVec2(pos.x + 1, pos.y + 1),
        ImVec2(pos.x + size.x - 1, pos.y + 1 + lineH),
        ImGui::ColorConvertFloat4ToU32(lineL), ImGui::ColorConvertFloat4ToU32(lineR),
        ImGui::ColorConvertFloat4ToU32(lineR), ImGui::ColorConvertFloat4ToU32(lineL));

    RenderTrackInfo(d, ImVec2(pos.x, pos.y + lineH), ImVec2(size.x, infoH), scale);

    float sepY = pos.y + lineH + infoH;
    float sepMidX = pos.x + size.x * 0.5f;
    ImU32 sepTrans = IM_COL32(255, 255, 255, 0);
    ImVec4 sepC = HSVtoRGB(m_GradientHue, 0.3f, 0.6f);
    sepC.w = 0.2f;
    ImU32 sepVis = ImGui::ColorConvertFloat4ToU32(sepC);
    d->AddRectFilledMultiColor(
        ImVec2(pos.x + 8 * scale, sepY),
        ImVec2(sepMidX, sepY + 1),
        sepTrans, sepVis, sepVis, sepTrans);
    d->AddRectFilledMultiColor(
        ImVec2(sepMidX, sepY),
        ImVec2(pos.x + size.x - 8 * scale, sepY + 1),
        sepVis, sepTrans, sepTrans, sepVis);

    ImVec2 vizPos(pos.x + 4 * scale, sepY + 3 * scale);
    ImVec2 vizSize(size.x - 8 * scale, vizH);

    d->PushClipRect(vizPos, ImVec2(vizPos.x + vizSize.x, vizPos.y + vizSize.y), true);
    switch(style)
    {
        case 0: RenderBars(d, vizPos, vizSize, scale); break;
        case 1: RenderWave(d, vizPos, vizSize, scale); break;
        case 2: RenderCircular(d, vizPos, vizSize, scale); break;
        case 3: RenderDots(d, vizPos, vizSize, scale); break;
        case 4: RenderPulse(d, vizPos, vizSize, scale); break;
        default: RenderBars(d, vizPos, vizSize, scale); break;
    }
    d->PopClipRect();

    float volY = pos.y + size.y - volBarH - 2 * scale;
    RenderVolumeBar(d, ImVec2(pos.x + 6 * scale, volY),
        ImVec2(size.x - 12 * scale, volBarH), scale);
}