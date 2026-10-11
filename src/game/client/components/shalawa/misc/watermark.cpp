#include "watermark.h"

#include <game/client/gameclient.h>
#include <engine/shared/config.h>
#include <game/client/components/shalawa/visuals/hud.h>
#include <engine/client.h>
#include <SDL.h>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>

CWatermark::CWatermark()
{
    ANTI_DISASM_BLOCK();
    m_pGameClient = nullptr;
    m_Initialized = false;
    m_Fps = 0;
    m_FrameCount = 0;
    m_FpsTimer = 0.0f;
    m_LastTime = 0.0f;
    m_DeltaTime = 0.0f;
    m_AnimTime = 0.0f;
    m_GradientHue = 0.0f;
    m_Millis = 0;
    m_WatermarkAlpha = 0.0f;
    m_ArraylistAlpha = 0.0f;
    
    for(int i = 0; i < 6; i++)
        m_KeyStates[i] = 0.0f;
}

void CWatermark::Init(CGameClient *pGameClient)
{
    ANTI_DISASM_BLOCK();
    m_pGameClient = pGameClient;
    m_Initialized = true;
    m_LastTime = (float)SDL_GetTicks() / 1000.0f;
}

float CWatermark::Lerp(float a, float b, float t)
{
    return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
}

float CWatermark::SmoothStep(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float CWatermark::EaseOutBack(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    float c1 = 1.70158f;
    float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
}

float CWatermark::EaseOutElastic(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    if(t == 0.0f || t == 1.0f) return t;
    float p = 0.3f;
    return powf(2.0f, -10.0f * t) * sinf((t - p / 4.0f) * (2.0f * 3.14159f) / p) + 1.0f;
}

ImVec4 CWatermark::LerpColor(ImVec4 a, ImVec4 b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

ImVec4 CWatermark::HSVtoRGB(float h, float s, float v)
{
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h * 6.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r, g, b;
    
    if(h < 1.0f/6.0f) { r = c; g = x; b = 0; }
    else if(h < 2.0f/6.0f) { r = x; g = c; b = 0; }
    else if(h < 3.0f/6.0f) { r = 0; g = c; b = x; }
    else if(h < 4.0f/6.0f) { r = 0; g = x; b = c; }
    else if(h < 5.0f/6.0f) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    
    return ImVec4(r + m, g + m, b + m, 1.0f);
}

ImVec4 CWatermark::GetAstolfoColor(float offset)
{
    float speed = 3.0f;
    float t = std::abs(std::fmod(m_Millis / 1000.0f * speed + offset, 2.0f));
    if(t > 1.0f) t = 2.0f - t;
    
    ImVec4 c1(0.56f, 0.56f, 0.94f, 1.0f);
    ImVec4 c2(0.94f, 0.56f, 0.78f, 1.0f);
    return LerpColor(c1, c2, t);
}

ImVec4 CWatermark::GetRainbowColor(float offset, int mode)
{
    float h;
    float speed = 2.0f;
    
    switch(mode)
    {
        case 0:
            h = std::fmod(m_Millis / 2000.0f * speed + offset, 1.0f);
            break;
        case 1:
            h = (float)((std::sin(m_Millis / 1000.0f * speed + offset * 3.14159f) + 1.0) / 2.0);
            break;
        case 2:
            h = std::fmod(offset - m_Millis / 3000.0f * speed, 1.0f);
            if(h < 0) h += 1.0f;
            break;
        default:
            h = 0.0f;
    }
    return HSVtoRGB(h, 0.7f, 1.0f);
}

ImU32 CWatermark::GetAccent(float alpha)
{
    ImVec4 col = HSVtoRGB(m_GradientHue, 0.65f, 0.95f);
    col.w = alpha;
    return ImGui::ColorConvertFloat4ToU32(col);
}

ImU32 CWatermark::GetArraylistColor(int index, int total, float alpha)
{
    float offset = total > 0 ? (float)index / (float)total : 0.0f;
    ImVec4 col;
    
    int mode = g_Config.m_shalawaArraylistMode;
    switch(mode)
    {
        case 0:
            col = HSVtoRGB(m_GradientHue, 0.65f, 0.95f);
            break;
        case 1:
            col = GetAstolfoColor(offset * 0.5f);
            break;
        case 2:
            col = GetRainbowColor(offset, 0);
            break;
        case 3:
            col = GetRainbowColor(offset, 1);
            break;
        case 4:
            col = GetRainbowColor(offset, 2);
            break;
        default:
            col = ImVec4(1, 1, 1, 1);
    }
    
    col.w = alpha;
    return ImGui::ColorConvertFloat4ToU32(col);
}

void CWatermark::DrawBlurredShadow(ImDrawList *d, ImVec2 pos, ImVec2 size, int range, ImVec4 color)
{
    int step = range > 8 ? 4 : (range > 4 ? 3 : 2);
    for(int i = range; i >= 0; i -= step)
    {
        float t = (float)i / (float)range;
        float alpha = (1.0f - t) * (1.0f - t) * color.w * 0.4f;
        if(alpha < 0.005f) continue;
        float expand = (float)i * 2.0f;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, alpha));
        d->AddRectFilled(ImVec2(pos.x - expand, pos.y - expand),
                         ImVec2(pos.x + size.x + expand, pos.y + size.y + expand), col, 4.0f);
    }
}

void CWatermark::DrawGradientRectH(ImDrawList *d, ImVec2 min, ImVec2 max, ImU32 left, ImU32 right)
{
    d->AddRectFilledMultiColor(min, max, left, right, right, left);
}

void CWatermark::DrawGradientRectV(ImDrawList *d, ImVec2 min, ImVec2 max, ImU32 top, ImU32 bottom)
{
    d->AddRectFilledMultiColor(min, max, top, top, bottom, bottom);
}

void CWatermark::RenderWatermarkClaude()
{
    ANTI_DISASM_BLOCK();
    if(!ImGui::GetCurrentContext())
        return;
    ImDrawList *d = ImGui::GetBackgroundDrawList();

    std::string info = BuildInfoString(" \xC2\xB7 ");
    std::string title = "shalawa";

    float titleW = ImGui::CalcTextSize(title.c_str()).x;
    float infoW = info.empty() ? 0.0f : ImGui::CalcTextSize(info.c_str()).x;
    float lineH = ImGui::CalcTextSize("A").y;
    float padX = 14.0f, padY = 8.0f;
    float innerW = std::max(titleW + 70.0f, infoW + 20.0f);
    float width = innerW + padX * 2;
    float height = (info.empty() ? lineH + padY * 2 : lineH * 2 + padY * 2 + 4.0f);
    ImVec2 pos = GetWatermarkPosition(width, height);

    float breathe = 0.03f * sinf(m_AnimTime * 1.8f);
    float bgAlpha = 0.92f + breathe;

    DrawBlurredShadow(d, pos, ImVec2(width, height), 10,
        ImVec4(0.85f, 0.45f, 0.2f, 0.12f + breathe));

    ImU32 bgCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.08f, 0.06f, 0.05f, bgAlpha));
    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), bgCol, 10.0f);

    ImVec4 warmBorder(0.85f, 0.55f, 0.3f, 0.15f + breathe * 2.0f);
    d->AddRect(pos, ImVec2(pos.x + width, pos.y + height),
        ImGui::ColorConvertFloat4ToU32(warmBorder), 10.0f, 0, 0.8f);

    ImVec4 claudeOrange(0.9f, 0.55f, 0.25f, 1.0f);
    ImVec4 claudeWarm(0.95f, 0.7f, 0.45f, 1.0f);

    float gradProgress = std::fmod(m_AnimTime * 0.3f, 1.0f);
    float gradT = 0.5f + 0.5f * sinf(gradProgress * 6.2832f);
    ImVec4 accentAnim = LerpColor(claudeOrange, claudeWarm, gradT);
    ImU32 accentU32 = ImGui::ColorConvertFloat4ToU32(accentAnim);

    float barY = pos.y + height - 2.5f;
    float barProgress = std::fmod(m_AnimTime * 0.4f, 1.0f);
    float barStart = barProgress * width * 0.6f;
    float barLen = width * 0.4f;

    ImVec4 barCol1 = accentAnim; barCol1.w = 0.0f;
    ImVec4 barCol2 = accentAnim; barCol2.w = 0.7f;
    ImVec4 barCol3 = accentAnim; barCol3.w = 0.0f;

    d->AddRectFilledMultiColor(
        ImVec2(pos.x + barStart, barY),
        ImVec2(pos.x + barStart + barLen * 0.5f, barY + 2.5f),
        ImGui::ColorConvertFloat4ToU32(barCol1),
        ImGui::ColorConvertFloat4ToU32(barCol2),
        ImGui::ColorConvertFloat4ToU32(barCol2),
        ImGui::ColorConvertFloat4ToU32(barCol1));
    d->AddRectFilledMultiColor(
        ImVec2(pos.x + barStart + barLen * 0.5f, barY),
        ImVec2(pos.x + std::min(barStart + barLen, width), barY + 2.5f),
        ImGui::ColorConvertFloat4ToU32(barCol2),
        ImGui::ColorConvertFloat4ToU32(barCol3),
        ImGui::ColorConvertFloat4ToU32(barCol3),
        ImGui::ColorConvertFloat4ToU32(barCol2));

    float tx = pos.x + padX;
    float ty = pos.y + padY;

    d->AddText(ImVec2(tx + 1, ty + 1), IM_COL32(0, 0, 0, 100), title.c_str());
    d->AddText(ImVec2(tx, ty), IM_COL32(240, 235, 230, 255), title.c_str());

    float byX = tx + titleW + 8;
    std::string byText = "by Claude";
    float byPulse = 0.7f + 0.15f * sinf(m_AnimTime * 2.0f);
    ImU32 byCol = ImGui::ColorConvertFloat4ToU32(ImVec4(accentAnim.x, accentAnim.y, accentAnim.z, byPulse));
    d->AddText(ImVec2(byX, ty), byCol, byText.c_str());

    if(!info.empty())
    {
        float iy = ty + lineH + 4.0f;
        d->AddText(ImVec2(tx, iy), IM_COL32(160, 150, 140, 200), info.c_str());
    }

    float dotX = pos.x + width - padX - 4;
    float dotY = pos.y + padY + 3;
    float dotPulse = 0.5f + 0.5f * sinf(m_AnimTime * 3.0f);
    ImVec4 dotCol = accentAnim;
    dotCol.w = 0.4f + dotPulse * 0.6f;
    d->AddCircleFilled(ImVec2(dotX, dotY), 3.0f, ImGui::ColorConvertFloat4ToU32(dotCol));
    dotCol.w = 0.1f + dotPulse * 0.15f;
    d->AddCircleFilled(ImVec2(dotX, dotY), 6.0f, ImGui::ColorConvertFloat4ToU32(dotCol));
}

void CWatermark::UpdateTime()
{
    ANTI_DISASM_BLOCK();
    float currentTime = (float)SDL_GetTicks() / OBF_F(1000);
    m_DeltaTime = std::clamp(currentTime - m_LastTime, 0.0001f, 0.1f);
    m_LastTime = currentTime;
    m_Millis = SDL_GetTicks();
    
    m_FrameCount++;
    m_FpsTimer += m_DeltaTime;
    if(m_FpsTimer >= 0.5f)
    {
        m_Fps = (int)(m_FrameCount / m_FpsTimer);
        m_FrameCount = 0;
        m_FpsTimer = 0.0f;
    }
    
    m_AnimTime += m_DeltaTime;
    m_GradientHue += m_DeltaTime * 0.1f;
    if(m_GradientHue > 1.0f)
        m_GradientHue -= 1.0f;
    
    m_WatermarkAlpha = Lerp(m_WatermarkAlpha, 1.0f, m_DeltaTime * 8.0f);
}

std::string CWatermark::GetTimeString()
{
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", t);
    return std::string(buf);
}

std::string CWatermark::GetDateString()
{
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    char buf[16];
    strftime(buf, sizeof(buf), "%d.%m.%Y", t);
    return std::string(buf);
}

int CWatermark::GetPing()
{
    if(!m_pGameClient || !m_pGameClient->m_Snap.m_pLocalInfo)
        return 0;
    return m_pGameClient->m_Snap.m_pLocalInfo->m_Latency;
}

int CWatermark::GetPlayerCount()
{
    if(!m_pGameClient)
        return 0;
    
    int count = 0;
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(m_pGameClient->m_Snap.m_apPlayerInfos[i])
            count++;
    }
    return count;
}

std::string CWatermark::GetServerName()
{
    if(!m_pGameClient || !m_pGameClient->Client())
        return std::string((char*)AY_OBFUSCATE("Offline"));
    
    if(m_pGameClient->Client()->State() != IClient::STATE_ONLINE)
        return std::string((char*)AY_OBFUSCATE("Offline"));
        
    static std::string cachedName = (char*)AY_OBFUSCATE("Server");
    static float lastUpdate = -1.0f;
    
    if(m_AnimTime - lastUpdate < 1.0f && lastUpdate > 0.0f)
        return cachedName;
        
    lastUpdate = m_AnimTime;
    
    NETADDR Addr = m_pGameClient->Client()->ServerAddress();
    IServerBrowser *pBrowser = m_pGameClient->ServerBrowser();
    if(pBrowser)
    {
        CServerBrowser::CServerEntry *pEntry = ((CServerBrowser*)pBrowser)->Find(Addr);
        if(pEntry && pEntry->m_Info.m_aName[0])
        {
            std::string name(pEntry->m_Info.m_aName);
            if(name.length() > 18)
                cachedName = name.substr(0, 15) + std::string((char*)AY_OBFUSCATE("..."));
            else
                cachedName = name;
            return cachedName;
        }
    }
    
    return std::string((char*)AY_OBFUSCATE("Server"));
}

float CWatermark::GetSpeed()
{
    if(!m_pGameClient)
        return 0.0f;
    
    vec2 vel = m_pGameClient->m_Snap.m_pLocalCharacter ? 
               vec2(m_pGameClient->m_Snap.m_pLocalCharacter->m_VelX / 256.0f,
                    m_pGameClient->m_Snap.m_pLocalCharacter->m_VelY / 256.0f) : vec2(0, 0);
    return length(vel);
}

std::string CWatermark::BuildInfoString(const char *separator)
{
    static std::string cachedInfo;
    static float lastUpdate = -1.0f;
    static std::string lastSep;
    
    if(m_AnimTime - lastUpdate < 0.5f && lastUpdate > 0.0f && lastSep == separator)
        return cachedInfo;
        
    lastUpdate = m_AnimTime;
    lastSep = separator;
    
    std::string info;
    auto addSep = [&]() { if(!info.empty()) info += separator; };

    if(g_Config.m_shalawaWatermarkFps)
    {
        addSep();
        char b[16]; snprintf(b, 16, "%dfps", m_Fps);
        info += b;
    }
    if(g_Config.m_shalawaWatermarkServer &&
       m_pGameClient && m_pGameClient->Client() &&
       m_pGameClient->Client()->State() == IClient::STATE_ONLINE)
    {
        addSep();
        info += GetServerName();
    }
    if(g_Config.m_shalawaWatermarkPlayers && m_pGameClient)
    {
        addSep();
        char b[16]; snprintf(b, 16, "%d players", GetPlayerCount());
        info += b;
    }
    if(g_Config.m_shalawaWatermarkPing && m_pGameClient)
    {
        addSep();
        char b[16]; snprintf(b, 16, "%dms", GetPing());
        info += b;
    }
    if(g_Config.m_shalawaWatermarkTime)
    {
        addSep();
        info += GetTimeString();
    }
    
    cachedInfo = info;
    return cachedInfo;
}

std::vector<std::string> CWatermark::BuildInfoSegments()
{
    static std::vector<std::string> cachedSegs;
    static float lastUpdate = -1.0f;
    
    if(m_AnimTime - lastUpdate < 0.5f && lastUpdate > 0.0f)
        return cachedSegs;
        
    lastUpdate = m_AnimTime;
    std::vector<std::string> segs;
    
    if(g_Config.m_shalawaWatermarkFps)
    {
        char b[16]; snprintf(b, 16, "%d", m_Fps);
        segs.push_back(b);
    }
    if(g_Config.m_shalawaWatermarkServer &&
       m_pGameClient && m_pGameClient->Client() &&
       m_pGameClient->Client()->State() == IClient::STATE_ONLINE)
    {
        segs.push_back(GetServerName());
    }
    if(g_Config.m_shalawaWatermarkPlayers && m_pGameClient)
    {
        char b[16]; snprintf(b, 16, "%d", GetPlayerCount());
        segs.push_back(b);
    }
    if(g_Config.m_shalawaWatermarkPing && m_pGameClient)
    {
        char b[16]; snprintf(b, 16, "%d", GetPing());
        segs.push_back(b);
    }
    if(g_Config.m_shalawaWatermarkTime)
    {
        segs.push_back(GetTimeString().substr(0, 5));
    }
    
    cachedSegs = segs;
    return cachedSegs;
}

void CWatermark::SetModuleState(const std::string &name, bool enabled, const std::string &suffix)
{
    ANTI_DISASM_BLOCK();
    auto it = m_ArraylistEntries.find(name);
    if(it == m_ArraylistEntries.end())
    {
        SArraylistEntry entry;
        entry.m_Name = name;
        entry.m_Suffix = suffix;
        entry.m_Enabled = enabled;
        entry.m_Animation = enabled ? 0.0f : 1.0f;
        entry.m_Alpha = 0.0f;
        m_ArraylistEntries[name] = entry;
    }
    else
    {
        it->second.m_Enabled = enabled;
        it->second.m_Suffix = suffix;
    }
}

void CWatermark::UpdateArraylist()
{
    ANTI_DISASM_BLOCK();
    for(auto &pair : m_ArraylistEntries)
    {
        float target = pair.second.m_Enabled ? 0.0f : 1.0f;
        pair.second.m_Animation = Lerp(pair.second.m_Animation, target, m_DeltaTime * 10.0f);
        
        float alphaTarget = pair.second.m_Enabled ? 1.0f : 0.0f;
        pair.second.m_Alpha = Lerp(pair.second.m_Alpha, alphaTarget, m_DeltaTime * 12.0f);
    }
}

void CWatermark::RenderWatermarkModern()
{
    ANTI_DISASM_BLOCK();
    if(!ImGui::GetCurrentContext())
        return;
    ImDrawList *d = ImGui::GetBackgroundDrawList();

    std::string info = BuildInfoString(" | ");
    std::string full = "shalawa";
    if(!info.empty())
        full += " | " + info;

    float textW = ImGui::CalcTextSize(full.c_str()).x;
    float padX = 12.0f, padY = 6.0f;
    float width = textW + padX * 2.0f;
    float height = ImGui::CalcTextSize("A").y + padY * 2.0f;
    ImVec2 pos = GetWatermarkPosition(width, height);

    DrawBlurredShadow(d, pos, ImVec2(width, height), 6, ImVec4(0.0f, 0.0f, 0.0f, 0.4f));
    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
                     IM_COL32(16, 16, 20, 240), 6.0f);

    ImVec4 accent = HSVtoRGB(std::fmod(m_AnimTime * 0.15f, 1.0f), 0.75f, 1.0f);
    ImU32 accentCol = ImGui::ColorConvertFloat4ToU32(accent);

    float tx = pos.x + padX;
    float ty = pos.y + padY;

    d->AddText(ImVec2(tx + 1.0f, ty + 1.0f), IM_COL32(0, 0, 0, 120), "s");
    d->AddText(ImVec2(tx, ty), accentCol, "s");
    float sW = ImGui::CalcTextSize("s").x;

    d->AddText(ImVec2(tx + sW + 1.0f, ty + 1.0f), IM_COL32(0, 0, 0, 80), "halawa");
    d->AddText(ImVec2(tx + sW, ty), IM_COL32(255, 255, 255, 255), "halawa");

    if(!info.empty())
    {
        float nameW = ImGui::CalcTextSize("shalawa").x;
        std::string rest = " | " + info;
        d->AddText(ImVec2(tx + nameW, ty), IM_COL32(120, 120, 135, 200), rest.c_str());
    }

    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + 2.0f),
                     accentCol, 6.0f, ImDrawFlags_RoundCornersTop);
}

void CWatermark::RenderWatermarkRise()
{
    ANTI_DISASM_BLOCK();
    if(!ImGui::GetCurrentContext())
        return;
    ImDrawList *d = ImGui::GetBackgroundDrawList();

    std::string info = BuildInfoString(" \xE2\x80\xA2 ");
    float nameW = ImGui::CalcTextSize("Shalawa").x;
    float infoW = info.empty() ? 0.0f : ImGui::CalcTextSize(info.c_str()).x;
    float lineH = ImGui::CalcTextSize("A").y;

    float innerW = std::max(nameW, infoW) + 24.0f;
    float width = innerW + 4.0f;
    float height = (info.empty() ? lineH + 12.0f : lineH * 2.0f + 18.0f);
    ImVec2 pos = GetWatermarkPosition(width, height);

    DrawBlurredShadow(d, pos, ImVec2(width, height), 8, ImVec4(0.0f, 0.0f, 0.0f, 0.5f));
    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
                     IM_COL32(18, 18, 22, 240), 5.0f);

    ImVec4 accent = HSVtoRGB(m_GradientHue, 0.7f, 0.9f);
    ImU32 accentU32 = ImGui::ColorConvertFloat4ToU32(accent);

    d->AddRectFilled(pos, ImVec2(pos.x + 3.0f, pos.y + height),
                     accentU32, 5.0f, ImDrawFlags_RoundCornersLeft);

    ImVec4 glowCol = accent;
    glowCol.w = 0.08f;
    for(int i = 3; i >= 0; i--)
    {
        float expand = (float)i * 2.0f;
        d->AddRectFilled(ImVec2(pos.x - expand, pos.y - expand),
                         ImVec2(pos.x + 4, pos.y + height + expand),
                         ImGui::ColorConvertFloat4ToU32(glowCol), 4.0f);
    }

    float tx = pos.x + 12.0f;
    d->AddText(ImVec2(tx + 1.0f, pos.y + 6.0f + 1.0f), IM_COL32(0, 0, 0, 80), "Shalawa");
    d->AddText(ImVec2(tx, pos.y + 6.0f), IM_COL32(255, 255, 255, 255), "Shalawa");

    if(!info.empty())
    {
        d->AddText(ImVec2(tx, pos.y + 6.0f + lineH + 4.0f),
                   IM_COL32(130, 130, 145, 220), info.c_str());
    }
}

void CWatermark::RenderWatermarkGamesense()
{
    ANTI_DISASM_BLOCK();
    if(!ImGui::GetCurrentContext())
        return;
    ImDrawList *d = ImGui::GetBackgroundDrawList();

    std::string info = BuildInfoString(" | ");
    std::string full = "shalawa";
    if(!info.empty())
        full += " | " + info;

    float textW = ImGui::CalcTextSize(full.c_str()).x;
    float padX = 8.0f;
    float width = textW + padX * 2.0f;
    float height = 20.0f;
    ImVec2 pos = GetWatermarkPosition(width, height);

    ImVec4 c1 = HSVtoRGB(std::fmod(m_AnimTime * 0.25f, 1.0f), 0.8f, 1.0f);
    ImVec4 c2 = HSVtoRGB(std::fmod(m_AnimTime * 0.25f + 0.3f, 1.0f), 0.8f, 1.0f);
    ImU32 left = ImGui::ColorConvertFloat4ToU32(c1);
    ImU32 right = ImGui::ColorConvertFloat4ToU32(c2);
    d->AddRectFilledMultiColor(pos, ImVec2(pos.x + width, pos.y + 2.0f),
                               left, right, right, left);

    d->AddRectFilled(ImVec2(pos.x, pos.y + 2.0f), ImVec2(pos.x + width, pos.y + height),
                     IM_COL32(24, 24, 28, 250));

    d->AddLine(ImVec2(pos.x, pos.y + height), ImVec2(pos.x + width, pos.y + height),
               IM_COL32(40, 40, 48, 255));

    float tx = pos.x + padX;
    float ty = pos.y + 5.0f;

    const char *ptr = full.c_str();
    float cx = tx;
    std::string token;
    for(int i = 0; ptr[i]; i++)
    {
        if(ptr[i] == '|')
        {
            if(!token.empty())
            {
                while(!token.empty() && token.back() == ' ') token.pop_back();
                d->AddText(ImVec2(cx, ty), IM_COL32(255, 255, 255, 240), token.c_str());
                cx += ImGui::CalcTextSize(token.c_str()).x;
                token.clear();
            }
            d->AddText(ImVec2(cx, ty), IM_COL32(60, 60, 70, 200), " | ");
            cx += ImGui::CalcTextSize(" | ").x;
            if(ptr[i + 1] == ' ') i++;
        }
        else
        {
            token += ptr[i];
        }
    }
    if(!token.empty())
    {
        while(!token.empty() && token.back() == ' ') token.pop_back();
        d->AddText(ImVec2(cx, ty), IM_COL32(255, 255, 255, 240), token.c_str());
    }
}

void CWatermark::RenderWatermarkNL()
{
    ANTI_DISASM_BLOCK();
    if(!ImGui::GetCurrentContext())
        return;
    ImDrawList *d = ImGui::GetBackgroundDrawList();

    std::string info = BuildInfoString(" | ");
    std::string full = "SHALAWA | v1.1.0";
    if(!info.empty())
        full += " | " + info;

    float textW = ImGui::CalcTextSize(full.c_str()).x;
    float padX = 10.0f, padY = 5.0f;
    float width = textW + padX * 2.0f;
    float height = ImGui::CalcTextSize("A").y + padY * 2.0f;
    ImVec2 pos = GetWatermarkPosition(width, height);

    DrawBlurredShadow(d, pos, ImVec2(width, height), 6, ImVec4(0.0f, 0.0f, 0.0f, 0.35f));
    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
                     IM_COL32(20, 20, 24, 245), 8.0f);

    ImVec4 borderCol(0.01f, 0.01f, 0.01f, 0.06f);
    d->AddRect(pos, ImVec2(pos.x + width, pos.y + height),
               ImGui::ColorConvertFloat4ToU32(borderCol), 8.0f);

    ImVec4 accent = HSVtoRGB(m_GradientHue, 0.65f, 0.95f);

    float tx = pos.x + padX;
    float ty = pos.y + padY;

    float nameW = ImGui::CalcTextSize("SHALAWA").x;
    d->AddText(ImVec2(tx + 1.0f, ty + 1.0f), IM_COL32(0, 0, 0, 100), "SHALAWA");
    d->AddText(ImVec2(tx, ty), ImGui::ColorConvertFloat4ToU32(accent), "SHALAWA");

    std::string rest = " | v1.1.0";
    if(!info.empty())
        rest += " | " + info;
    d->AddText(ImVec2(tx + nameW, ty), IM_COL32(160, 160, 170, 220), rest.c_str());
}

void CWatermark::RenderWatermarkSkeet()
{
    ANTI_DISASM_BLOCK();
    if(!ImGui::GetCurrentContext())
        return;
    ImDrawList *d = ImGui::GetBackgroundDrawList();

    std::string info = BuildInfoString(" | ");
    std::string full = "shalawa";
    if(!info.empty())
        full += " | " + info;

    float textW = ImGui::CalcTextSize(full.c_str()).x;
    float padX = 8.0f;
    float width = textW + padX * 2.0f;
    float height = 19.0f;
    ImVec2 pos = GetWatermarkPosition(width, height);

    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
                     IM_COL32(50, 50, 55, 255));

    d->AddRectFilled(ImVec2(pos.x + 1.0f, pos.y + 1.0f),
                     ImVec2(pos.x + width - 1.0f, pos.y + height - 1.0f),
                     IM_COL32(8, 8, 10, 255));

    d->AddRect(ImVec2(pos.x + 2.0f, pos.y + 2.0f),
               ImVec2(pos.x + width - 2.0f, pos.y + height - 2.0f),
               IM_COL32(50, 50, 55, 180));

    ImVec4 accent = HSVtoRGB(m_GradientHue, 0.7f, 0.9f);

    float tx = pos.x + padX;
    float ty = pos.y + 4.0f;

    float nameW = ImGui::CalcTextSize("shalawa").x;
    d->AddText(ImVec2(tx, ty), ImGui::ColorConvertFloat4ToU32(accent), "shalawa");

    if(!info.empty())
    {
        std::string rest = " | " + info;
        d->AddText(ImVec2(tx + nameW, ty), IM_COL32(200, 200, 210, 240), rest.c_str());
    }

    ImVec4 topLine = accent;
    topLine.w = 0.8f;
    d->AddLine(ImVec2(pos.x + 3.0f, pos.y + 2.0f),
               ImVec2(pos.x + width - 3.0f, pos.y + 2.0f),
               ImGui::ColorConvertFloat4ToU32(topLine), 1.0f);
}

void CWatermark::RenderWatermarkStylo()
{
    if(!ImGui::GetCurrentContext())
        return;
    ImDrawList *d = ImGui::GetBackgroundDrawList();
    ImVec2 pos = GetWatermarkPosition(110, 45);

    ImVec4 accent = HSVtoRGB(m_GradientHue, 0.7f, 0.95f);

    d->AddText(ImVec2(pos.x + 2, pos.y + 2), IM_COL32(0, 0, 0, 150), "SHALAWA");
    d->AddText(ImVec2(pos.x, pos.y), IM_COL32(255, 255, 255, 255), "SHALAWA");

    float nameW = ImGui::CalcTextSize("SHALAWA").x;
    d->AddText(ImVec2(pos.x + nameW + 5, pos.y + 2),
               IM_COL32(255, 255, 255, 100), "v1.1.0");

    std::string timeStr = GetTimeString().substr(0, 5);
    std::string info;
    if(g_Config.m_shalawaWatermarkFps)
    {
        char b[16]; snprintf(b, 16, "%dfps", m_Fps);
        info = b;
        if(g_Config.m_shalawaWatermarkTime)
            info += " | " + timeStr;
    }
    else if(g_Config.m_shalawaWatermarkTime)
    {
        info = timeStr;
    }

    if(!info.empty())
    {
        float infoW = ImGui::CalcTextSize(info.c_str()).x + 14;
        float lineH = ImGui::CalcTextSize("A").y;
        float iy = pos.y + lineH + 6;

        d->AddRectFilledMultiColor(ImVec2(pos.x, iy),
                                   ImVec2(pos.x + infoW, iy + lineH + 4),
                                   IM_COL32(0, 0, 0, 160), IM_COL32(0, 0, 0, 0),
                                   IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 160));

        d->AddText(ImVec2(pos.x + 6, iy + 2), IM_COL32(255, 255, 255, 220), info.c_str());
    }

    d->AddRectFilled(ImVec2(pos.x, pos.y + ImGui::CalcTextSize("A").y + 2),
                     ImVec2(pos.x + nameW, pos.y + ImGui::CalcTextSize("A").y + 3),
                     ImGui::ColorConvertFloat4ToU32(accent));
}

void CWatermark::RenderWatermarkMinimal()
{
    if(!ImGui::GetCurrentContext())
        return;
    ImDrawList *d = ImGui::GetBackgroundDrawList();

    std::string info = "SHALAWA";
    std::string extra = BuildInfoString(" ");
    if(!extra.empty())
        info += " " + extra;

    ImVec2 textSize = ImGui::CalcTextSize(info.c_str());
    float padX = 12.0f, padY = 6.0f;
    float width = textSize.x + padX * 2;
    float height = textSize.y + padY * 2;
    ImVec2 pos = GetWatermarkPosition(width, height);

    ImVec4 accent = HSVtoRGB(m_GradientHue, 0.65f, 0.95f);

    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
                     IM_COL32(0, 0, 0, 180), height / 2);

    ImVec4 borderCol = accent;
    borderCol.w = 0.15f;
    d->AddRect(pos, ImVec2(pos.x + width, pos.y + height),
               ImGui::ColorConvertFloat4ToU32(borderCol), height / 2, 0, 1.0f);

    float tx = pos.x + padX;
    float ty = pos.y + padY;

    float nameW = ImGui::CalcTextSize("SHALAWA").x;
    d->AddText(ImVec2(tx, ty), ImGui::ColorConvertFloat4ToU32(accent), "SHALAWA");

    if(!extra.empty())
    {
        d->AddText(ImVec2(tx + nameW, ty),
                   IM_COL32(180, 180, 190, 200), (" " + extra).c_str());
    }
}

void CWatermark::RenderWatermarkPills()
{
    if(!ImGui::GetCurrentContext())
        return;
    ImDrawList *d = ImGui::GetBackgroundDrawList();

    std::vector<std::string> items;
    items.push_back("SHALAWA");

    auto segs = BuildInfoSegments();
    for(auto &s : segs)
        items.push_back(s);

    float itemPad = 10.0f;
    float itemH = 24.0f;
    float gap = 5.0f;

    float totalWidth = 0;
    for(size_t i = 0; i < items.size(); i++)
    {
        totalWidth += ImGui::CalcTextSize(items[i].c_str()).x + itemPad * 2;
        if(i < items.size() - 1)
            totalWidth += gap;
    }

    ImVec2 basePos = GetWatermarkPosition(totalWidth, itemH);
    float x = basePos.x;
    float y = basePos.y;
    float rounding = itemH / 2;

    for(size_t i = 0; i < items.size(); i++)
    {
        float itemW = ImGui::CalcTextSize(items[i].c_str()).x + itemPad * 2;
        ImVec4 accentCol = HSVtoRGB(std::fmod(m_GradientHue + i * 0.08f, 1.0f), 0.7f, 0.9f);

        if(i == 0)
        {
            DrawBlurredShadow(d, ImVec2(x, y), ImVec2(itemW, itemH), 4, accentCol);
            d->AddRectFilled(ImVec2(x, y), ImVec2(x + itemW, y + itemH),
                             ImGui::ColorConvertFloat4ToU32(accentCol), rounding);
        }
        else
        {
            d->AddRectFilled(ImVec2(x, y), ImVec2(x + itemW, y + itemH),
                             IM_COL32(18, 18, 22, 240), rounding);

            ImVec4 borderCol = accentCol;
            float pulse = 0.25f + 0.15f * sinf(m_AnimTime * 2.5f + i * 0.7f);
            borderCol.w = pulse;
            d->AddRect(ImVec2(x, y), ImVec2(x + itemW, y + itemH),
                       ImGui::ColorConvertFloat4ToU32(borderCol), rounding, 0, 1.2f);
        }

        ImU32 textCol = (i == 0) ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 210, 240);
        ImVec2 textSize = ImGui::CalcTextSize(items[i].c_str());
        d->AddText(ImVec2(x + (itemW - textSize.x) / 2, y + (itemH - textSize.y) / 2),
                   textCol, items[i].c_str());

        x += itemW + gap;
    }
}

void CWatermark::RenderWatermarkGemini()
{
    ANTI_DISASM_BLOCK();
    if(!ImGui::GetCurrentContext())
        return;
        
    ImDrawList *d = ImGui::GetBackgroundDrawList();

    // Собираем информацию (пробелы для отступа, сепаратор нарисуем сами)
    std::string info = BuildInfoString("   ");
    std::string title = "SHALAWA";

    // Считаем ширину кастомного текста (посимвольно)
    float titleW = 0.0f;
    for (char c : title) {
        char cStr[2] = { c, '\0' };
        titleW += ImGui::CalcTextSize(cStr).x + 1.5f;
    }

    float infoW = info.empty() ? 0.0f : ImGui::CalcTextSize(info.c_str()).x;
    float height = 26.0f;
    float padX = 16.0f;
    
    // Ширина = отступ + текст + разделитель + инфо + место для крутящегося треугольника + отступ
    float width = padX + titleW + (info.empty() ? 0.0f : 15.0f + infoW) + 20.0f + padX;
    ImVec2 pos = GetWatermarkPosition(width, height);

    // --- ЦВЕТА (Neon Cyberpunk) ---
    float t = m_AnimTime * 0.8f;
    ImVec4 colCyan = ImVec4(0.0f, 0.85f, 1.0f, 1.0f);
    ImVec4 colMagenta = ImVec4(0.9f, 0.15f, 0.6f, 1.0f);
    ImVec4 colDark = ImVec4(0.05f, 0.05f, 0.08f, 0.90f); // Глубокий темный фон

    // --- ТЕНЬ И ФОН ---
    float pulse = 0.5f + 0.5f * sinf(m_AnimTime * 2.5f);
    ImVec4 shadowCol = LerpColor(colCyan, colMagenta, 0.5f + 0.5f * sinf(m_AnimTime));
    shadowCol.w = 0.2f + 0.15f * pulse; // Пульсирующая голографическая тень
    
    DrawBlurredShadow(d, pos, ImVec2(width, height), 10, shadowCol);
    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), ImGui::ColorConvertFloat4ToU32(colDark), height / 2.0f);

    // --- ОБВОДКА ---
    ImU32 borderCol = ImGui::ColorConvertFloat4ToU32(LerpColor(colCyan, colMagenta, std::abs(sinf(t))));
    d->AddRect(pos, ImVec2(pos.x + width, pos.y + height), borderCol, height / 2.0f, 0, 1.5f);

    // --- АНИМАЦИЯ ТЕКСТА (Волна + Радуга) ---
    float tx = pos.x + padX;
    float ty = pos.y + (height - ImGui::CalcTextSize("A").y) / 2.0f;

    for (int i = 0; i < title.length(); i++) 
    {
        char cStr[2] = { title[i], '\0' };
        float charOffset = i * 0.2f;
        
        // Буквы немного "прыгают"
        float waveY = ty + sinf(m_AnimTime * 5.0f + charOffset) * 1.5f; 
        
        // Буквы переливаются
        ImVec4 letterCol = LerpColor(colCyan, colMagenta, 0.5f + 0.5f * sinf(m_AnimTime * 2.0f + charOffset));
        
        // Рисуем маленькую тень от буквы, потом саму букву
        d->AddText(ImVec2(tx + 1, waveY + 1), IM_COL32(0, 0, 0, 150), cStr);
        d->AddText(ImVec2(tx, waveY), ImGui::ColorConvertFloat4ToU32(letterCol), cStr);

        tx += ImGui::CalcTextSize(cStr).x + 1.5f;
    }

    // --- ИНФОРМАЦИЯ И СЕПАРАТОР ---
    if (!info.empty()) 
    {
        tx += 6.0f;
        // Красивый физический разделитель (градиентная линия)
        ImVec4 sepCol = colMagenta;
        sepCol.w = 0.6f;
        d->AddLine(ImVec2(tx, pos.y + 6.0f), ImVec2(tx, pos.y + height - 6.0f), ImGui::ColorConvertFloat4ToU32(sepCol), 2.0f);
        tx += 10.0f;

        d->AddText(ImVec2(tx, ty), IM_COL32(230, 230, 240, 255), info.c_str());
    }

    // --- КРУТЯЩИЙСЯ ТРЕУГОЛЬНИК (TECH CORE) ---
    float centerX = pos.x + width - padX - 6.0f;
    float centerY = pos.y + height / 2.0f;
    float angle = m_AnimTime * 4.0f; // Скорость вращения
    float radius = 3.5f; // Размер 

    ImVec2 p1 = ImVec2(centerX + cosf(angle) * radius, centerY + sinf(angle) * radius);
    ImVec2 p2 = ImVec2(centerX + cosf(angle + 2.094f) * radius, centerY + sinf(angle + 2.094f) * radius); // 120 градусов
    ImVec2 p3 = ImVec2(centerX + cosf(angle + 4.188f) * radius, centerY + sinf(angle + 4.188f) * radius); // 240 градусов

    ImU32 triCol = ImGui::ColorConvertFloat4ToU32(LerpColor(colMagenta, colCyan, std::abs(cosf(t))));
    d->AddTriangleFilled(p1, p2, p3, triCol);
}

void CWatermark::RenderWatermarkGPT()
{
    ANTI_DISASM_BLOCK();
    if(!ImGui::GetCurrentContext())
        return;

    ImDrawList *d = ImGui::GetBackgroundDrawList();

    const float alpha = m_WatermarkAlpha;

    const std::string title = "shalawa";
    std::vector<std::string> segs = BuildInfoSegments();

    // --- layout ---
    const float h = 26.0f;
    const float rounding = h * 0.5f;

    const float leftBladeW = 30.0f;   // косой блок слева (иконка + срез)
    const float padRight = 10.0f;

    const float gap = 6.0f;
    const float chipPad = 8.0f;
    const float chipH = 18.0f;
    const float chipR = chipH * 0.5f;
    const float chipY = (h - chipH) * 0.5f;

    float titleW = ImGui::CalcTextSize(title.c_str()).x;

    float width = leftBladeW + titleW + padRight;
    for(const auto &s : segs)
        width += gap + (ImGui::CalcTextSize(s.c_str()).x + chipPad * 2.0f);

    ImVec2 pos = GetWatermarkPosition(width, h);

    // --- colors ---
    ImVec4 bgBase(0.04f, 0.045f, 0.07f, 0.92f * alpha);
    ImVec4 bgTop (0.07f, 0.08f, 0.12f, 0.55f * alpha);

    ImVec4 teal(0.20f, 0.95f, 0.85f, 1.0f);
    ImVec4 vio (0.62f, 0.35f, 0.98f, 1.0f);

    float glowT = 0.5f + 0.5f * sinf(m_AnimTime * 1.3f);
    ImVec4 accent = LerpColor(teal, vio, glowT);
    accent.w = 1.0f;

    // --- shadow ---
    ImVec4 sh = accent;
    sh.w = 0.18f * alpha;
    DrawBlurredShadow(d, pos, ImVec2(width, h), 10, sh);

    // --- base bg (rounded) ---
    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + h),
        ImGui::ColorConvertFloat4ToU32(bgBase), rounding);

    // top tint (gives "glass" depth)
    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + h * 0.55f),
        ImGui::ColorConvertFloat4ToU32(bgTop), rounding, ImDrawFlags_RoundCornersTop);

    // --- left slanted blade ---
    // quad: [pos] -> slight slant on right
    ImVec2 p0(pos.x,       pos.y);
    ImVec2 p1(pos.x + 18,  pos.y);
    ImVec2 p2(pos.x + 28,  pos.y + h);
    ImVec2 p3(pos.x,       pos.y + h);

    ImVec4 bladeCol = accent;
    bladeCol.w = 0.75f * alpha;
    d->AddQuadFilled(p0, p1, p2, p3, ImGui::ColorConvertFloat4ToU32(bladeCol));

    // inner cut line
    d->AddLine(ImVec2(pos.x + 18, pos.y), ImVec2(pos.x + 28, pos.y + h),
        IM_COL32(0, 0, 0, (int)(90 * alpha)), 1.0f);

    // --- "eye" icon ---
    ImVec2 c(pos.x + 14.0f, pos.y + h * 0.5f);
    float pulse = 0.5f + 0.5f * sinf(m_AnimTime * 3.0f);

    d->AddCircleFilled(c, 3.2f, IM_COL32(10, 10, 12, (int)(170 * alpha)));

    ImVec4 ring = accent;
    ring.w = (0.30f + 0.30f * pulse) * alpha;
    d->AddCircle(c, 6.5f, ImGui::ColorConvertFloat4ToU32(ring), 24, 1.2f);

    d->AddCircleFilled(c, 1.6f, IM_COL32(255, 255, 255, (int)(220 * alpha)));

    // --- title (per-letter shimmer) ---
    float tx = pos.x + leftBladeW;
    float ty = pos.y + (h - ImGui::CalcTextSize("A").y) * 0.5f;

    for(int i = 0; i < (int)title.size(); i++)
    {
        char cs[2] = { title[i], 0 };
        float off = i * 0.12f;

        ImVec4 lc = LerpColor(teal, vio, 0.5f + 0.5f * sinf(m_AnimTime * 2.2f + off));
        lc.w = alpha;

        d->AddText(ImVec2(tx + 1, ty + 1), IM_COL32(0, 0, 0, (int)(90 * alpha)), cs);
        d->AddText(ImVec2(tx, ty), ImGui::ColorConvertFloat4ToU32(lc), cs);

        tx += ImGui::CalcTextSize(cs).x;
    }

    // --- chips ---
    tx += 10.0f;
    for(size_t i = 0; i < segs.size(); i++)
    {
        const std::string &s = segs[i];
        float w = ImGui::CalcTextSize(s.c_str()).x + chipPad * 2.0f;

        ImVec2 cmin(tx, pos.y + chipY);
        ImVec2 cmax(tx + w, cmin.y + chipH);

        ImVec4 chipBg(0.10f, 0.11f, 0.16f, 0.85f * alpha);
        d->AddRectFilled(cmin, cmax, ImGui::ColorConvertFloat4ToU32(chipBg), chipR);

        float bt = (float)i * 0.7f + m_AnimTime * 1.6f;
        ImVec4 bcol = accent;
        bcol.w = (0.10f + 0.10f * sinf(bt)) * alpha;
        d->AddRect(cmin, cmax, ImGui::ColorConvertFloat4ToU32(bcol), chipR, 0, 1.0f);

        d->AddText(ImVec2(cmin.x + chipPad, cmin.y + (chipH - ImGui::CalcTextSize("A").y) * 0.5f),
            IM_COL32(230, 230, 240, (int)(235 * alpha)), s.c_str());

        tx += w + gap;
    }

    // --- moving sheen (slanted highlight) ---
    float sheenX = pos.x + fmodf(m_AnimTime * 90.0f, width + 80.0f) - 80.0f;
    ImVec4 sheenCol(1.0f, 1.0f, 1.0f, 0.06f * alpha);
    d->AddQuadFilled(
        ImVec2(sheenX,       pos.y),
        ImVec2(sheenX + 70,  pos.y),
        ImVec2(sheenX + 50,  pos.y + h),
        ImVec2(sheenX - 20,  pos.y + h),
        ImGui::ColorConvertFloat4ToU32(sheenCol));

    // --- scanlines (subtle) ---
    ImU32 sl = IM_COL32(0, 0, 0, (int)(18 * alpha));
    for(int yy = 2; yy < (int)h; yy += 6)
        d->AddLine(ImVec2(pos.x + rounding, pos.y + (float)yy),
                   ImVec2(pos.x + width - rounding, pos.y + (float)yy),
                   sl, 1.0f);

    // --- outline ---
    ImVec4 out = accent;
    out.w = 0.22f * alpha;
    d->AddRect(pos, ImVec2(pos.x + width, pos.y + h),
        ImGui::ColorConvertFloat4ToU32(out), rounding, 0, 1.3f);

    // tiny tag (signature)
    const char *tag = "mkIII";
    ImVec2 tagSz = ImGui::CalcTextSize(tag);
    d->AddText(ImVec2(pos.x + width - padRight - tagSz.x, pos.y + h - tagSz.y - 2.0f),
        IM_COL32(170, 170, 190, (int)(105 * alpha)), tag);
}

void CWatermark::RenderWatermarkGrok()
{
    ANTI_DISASM_BLOCK();
    if(!ImGui::GetCurrentContext())
        return;

    ImDrawList *d = ImGui::GetBackgroundDrawList();

    std::string title = "SHALAWA";
    std::string info = BuildInfoString("  •  ");

    float titleW = ImGui::CalcTextSize(title.c_str()).x;
    float infoW = info.empty() ? 0.0f : ImGui::CalcTextSize(info.c_str()).x;
    float padX = 18.0f, padY = 9.0f;
    float height = 32.0f;
    float width = padX * 2 + titleW + (info.empty() ? 0 : 28 + infoW) + 38; // место под иконку

    ImVec2 pos = GetWatermarkPosition(width, height);

    float t = m_AnimTime * 1.1f;

    // === Фон ===
    ImVec4 bg = ImVec4(0.035f, 0.03f, 0.055f, 0.94f);
    DrawBlurredShadow(d, pos, ImVec2(width, height), 12, ImVec4(0.4f, 0.15f, 0.85f, 0.18f));
    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), 
                     ImGui::ColorConvertFloat4ToU32(bg), 11.0f);

    // === Глубокая обводка с пульсацией ===
    float borderPulse = 0.7f + 0.3f * sinf(t * 3.5f);
    ImVec4 border = ImVec4(0.45f, 0.15f, 1.0f, borderPulse * 0.65f);
    d->AddRect(pos, ImVec2(pos.x + width, pos.y + height),
               ImGui::ColorConvertFloat4ToU32(border), 11.0f, 0, 2.2f);

    // === Левый акцентный блик ===
    d->AddRectFilled(ImVec2(pos.x, pos.y), 
                     ImVec2(pos.x + 4, pos.y + height),
                     IM_COL32(120, 80, 255, 180), 11.0f, ImDrawFlags_RoundCornersLeft);

    float tx = pos.x + padX;
    float ty = pos.y + (height - ImGui::CalcTextSize("A").y) / 2.0f + 1;

    // === SHALAWA с нейронным глитчем ===
    for(size_t i = 0; i < title.length(); i++)
    {
        char c[2] = {title[i], '\0'};
        float offset = sinf(t * 4.0f + i * 1.4f) * 0.8f;
        float glitch = (sinf(t * 47.0f + i * 13.0f) > 0.96f) ? sinf(t * 91.0f + i * 7.0f) * 0.7f : 0.0f; // редкий глитч

        ImVec4 col = HSVtoRGB(std::fmod(m_GradientHue + i * 0.12f, 1.0f), 0.85f, 0.98f);
        
        // тень
        d->AddText(ImVec2(tx + 1.5f, ty + offset + 1.5f), IM_COL32(0, 0, 0, 90), c);
        // основная буква
        d->AddText(ImVec2(tx + glitch, ty + offset), ImGui::ColorConvertFloat4ToU32(col), c);

        tx += ImGui::CalcTextSize(c).x + 1.0f;
    }

    // === Инфо ===
    if(!info.empty())
    {
        tx += 22.0f;
        d->AddText(ImVec2(tx, ty + 1), IM_COL32(255, 255, 255, 65), "•");
        d->AddText(ImVec2(tx + 9, ty), IM_COL32(180, 170, 220, 210), info.c_str());
    }

    // === Иконка GROK (справа) ===
    float iconX = pos.x + width - 26;
    float iconY = pos.y + height * 0.5f;

    float rot = t * 1.6f;
    ImU32 iconCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 0.8f, 1.0f, 0.9f + 0.1f * sinf(t*6)));

    // "x" с лёгким вращением
    d->AddText(ImVec2(iconX - 1, iconY - 7), IM_COL32(0, 0, 0, 80), "x");
    d->AddText(ImVec2(iconX, iconY - 8), iconCol, "x");

    // маленький круг вокруг
    d->AddCircle(ImVec2(iconX + 3.5f, iconY - 3.5f), 9.0f, iconCol, 32, 1.4f);
}

void CWatermark::RenderWatermarkSeed()
{
    ANTI_DISASM_BLOCK();
    if(!ImGui::GetCurrentContext())
        return;

    ImDrawList *d = ImGui::GetBackgroundDrawList();

    std::string info = BuildInfoString("  •  ");
    const char* title = "shalawa";
    const char* authorTag = "by Seed";

    float titleW = ImGui::CalcTextSize(title).x;
    float tagW = ImGui::CalcTextSize(authorTag).x;
    float infoW = info.empty() ? 0.0f : ImGui::CalcTextSize(info.c_str()).x;

    const float height = 34.0f;
    const float padX = 36.0f; // место под иконку ростка слева
    const float width = padX + titleW + 8.0f + tagW + (info.empty() ? 0.0f : 18.0f + infoW) + 16.0f;
    const float rounding = height / 2.0f;

    ImVec2 pos = GetWatermarkPosition(width, height);

    // 🎨 Уникальная палитра - не похожа ни на одну другую ватермарку
    ImVec4 colSproutGreen = ImVec4(0.32f, 0.85f, 0.56f, 1.0f);
    ImVec4 colSproutPeach = ImVec4(0.98f, 0.65f, 0.42f, 1.0f);
    ImVec4 bgColor = ImVec4(0.04f, 0.06f, 0.05f, 0.93f); // теплый темный, не холодный угольный

    float glowT = 0.5f + 0.5f * sinf(m_AnimTime * 0.9f);
    ImVec4 accentColor = LerpColor(colSproutGreen, colSproutPeach, glowT);

    // 💡 Пульсирующая мягкая тень
    ImVec4 shadowCol = accentColor;
    shadowCol.w = 0.17f + 0.08f * sinf(m_AnimTime * 2.2f);
    DrawBlurredShadow(d, pos, ImVec2(width, height), 11, shadowCol);

    // Основной фон
    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), ImGui::ColorConvertFloat4ToU32(bgColor), rounding);

    // Тонкая градиентная обводка
    ImVec4 borderCol = accentColor;
    borderCol.w = 0.22f;
    d->AddRect(pos, ImVec2(pos.x + width, pos.y + height), ImGui::ColorConvertFloat4ToU32(borderCol), rounding, 0, 1.4f);

    // 🪴 УНИКАЛЬНАЯ ИКОНКА: Прорастающее семечко / росток, которого нет у других ватермарок
    const float sproutX = pos.x + 17.0f;
    const float sproutY = pos.y + height / 2.0f + 2.0f;

    // Пульсирующее свечение вокруг ростка
    for(int i = 3; i > 0; i--)
    {
        ImVec4 glow = colSproutGreen;
        glow.w = 0.07f * i * (0.7f + 0.3f * sinf(m_AnimTime * 1.8f));
        d->AddCircleFilled(ImVec2(sproutX, sproutY - 4.0f), 4.0f + i * 2.5f, ImGui::ColorConvertFloat4ToU32(glow));
    }

    // Анимация покачивания листьев как от ветра (не синхронная)
    float leafSwingLeft = sinf(m_AnimTime * 2.4f) * 0.35f;
    float leafSwingRight = sinf(m_AnimTime * 2.4f + 3.14159f) * 0.35f;

    // Семечко-основание
    d->AddEllipseFilled(ImVec2(sproutX, sproutY), ImVec2(4.5f, 3.2f), IM_COL32(90, 65, 40, 255));
    // Стебель
    d->AddLine(ImVec2(sproutX, sproutY - 2.0f), ImVec2(sproutX, sproutY - 9.0f), ImGui::ColorConvertFloat4ToU32(colSproutGreen), 1.8f);
    // Листики покачивающиеся
    d->AddTriangleFilled(
        ImVec2(sproutX, sproutY - 7.0f),
        ImVec2(sproutX - 4.5f + leafSwingLeft * 3.0f, sproutY - 11.5f),
        ImVec2(sproutX - 1.5f + leafSwingLeft * 3.0f, sproutY - 13.0f),
        ImGui::ColorConvertFloat4ToU32(colSproutGreen)
    );
    d->AddTriangleFilled(
        ImVec2(sproutX, sproutY - 7.0f),
        ImVec2(sproutX + 4.5f + leafSwingRight * 3.0f, sproutY - 11.5f),
        ImVec2(sproutX + 1.5f + leafSwingRight * 3.0f, sproutY - 13.0f),
        ImGui::ColorConvertFloat4ToU32(colSproutGreen)
    );

    // 📝 Текст
    float tx = pos.x + padX;
    float ty = pos.y + (height - ImGui::CalcTextSize("A").y) / 2.0f;

    // Заголовок shalawa с очень плавной волной перелива цвета
    for(int i = 0; i < (int)strlen(title); i++)
    {
        char c[2] = { title[i], 0 };
        float offset = i * 0.14f;
        ImVec4 letterCol = LerpColor(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), colSproutGreen, 0.25f + 0.2f * sinf(m_AnimTime * 1.6f + offset));

        d->AddText(ImVec2(tx + 1, ty + 1), IM_COL32(0,0,0, 95), c);
        d->AddText(ImVec2(tx, ty), ImGui::ColorConvertFloat4ToU32(letterCol), c);
        tx += ImGui::CalcTextSize(c).x;
    }

    // Подпись автора - градиентная
    tx += 8.0f;
    ImU32 tagColor = ImGui::ColorConvertFloat4ToU32(accentColor);
    d->AddText(ImVec2(tx, ty), tagColor, authorTag);
    tx += tagW;

    // Информационная строка (фпс, пинг, время, сервер и тд)
    if(!info.empty())
    {
        tx += 12.0f;
        d->AddText(ImVec2(tx, ty + 1), IM_COL32(255, 255, 255, 70), "•");
        tx += 10.0f;
        d->AddText(ImVec2(tx, ty), IM_COL32(185, 195, 185, 220), info.c_str());
    }

    // ✨ Уникальная анимация: бегущие точки-семечки по нижней грани, вместо скучной полосы
    for(int i = 0; i < 3; i++)
    {
        float seedPos = std::fmod(m_AnimTime * 35.0f + i * (width / 3.0f), width + 40.0f) - 20.0f;
        ImVec4 seedDot = accentColor;
        seedDot.w = 0.45f - fabs(seedPos / width - 0.5f) * 0.4f;
        d->AddCircleFilled(ImVec2(pos.x + seedPos, pos.y + height - 3.5f), 1.2f, ImGui::ColorConvertFloat4ToU32(seedDot));
    }
}

void CWatermark::RenderWatermarkDeepSeek()
{
    ANTI_DISASM_BLOCK();
    if(!ImGui::GetCurrentContext())
        return;

    ImDrawList *d = ImGui::GetBackgroundDrawList();

    std::string info = BuildInfoString("  |  ");
    std::string title = "SHALAWA";
    std::string tagline = "by DeepSeek";

    // Рассчитываем размеры
    float titleW = ImGui::CalcTextSize(title.c_str()).x;
    float tagW = ImGui::CalcTextSize(tagline.c_str()).x;
    float infoW = info.empty() ? 0.0f : ImGui::CalcTextSize(info.c_str()).x;
    
    const float height = 36.0f;
    const float iconSize = 26.0f; // Иконка слева
    const float padding = 16.0f;
    
    // Общая ширина: иконка + отступ + заголовок + отступ + тэг + (инфо если есть)
    float width = iconSize + padding + titleW + padding + tagW + 20.0f;
    if(!info.empty()) {
        width += 20.0f + infoW;
    }
    
    ImVec2 pos = GetWatermarkPosition(width, height);
    float t = m_AnimTime * 1.5f;

    // Уникальная цветовая схема DeepSeek - глубокие синие тона с акцентами
    ImVec4 colDeepBlue = ImVec4(0.08f, 0.12f, 0.25f, 0.95f); // Темно-синий фон
    ImVec4 colAccentBlue = ImVec4(0.15f, 0.45f, 0.85f, 1.0f); // Акцентный синий
    ImVec4 colLightBlue = ImVec4(0.25f, 0.65f, 0.95f, 1.0f); // Светло-синий
    ImVec4 colCyan = ImVec4(0.4f, 0.85f, 1.0f, 1.0f); // Циановый для свечения

    // Плавные переходы цветов
    float pulse = 0.5f + 0.5f * sinf(t * 2.0f);
    ImVec4 accent = LerpColor(colAccentBlue, colLightBlue, pulse);
    ImVec4 glowColor = colCyan;
    glowColor.w = 0.15f + 0.1f * sinf(t * 3.0f);

    // --- Фон с градиентом ---
    DrawBlurredShadow(d, pos, ImVec2(width, height), 12, ImVec4(0.1f, 0.3f, 0.6f, 0.15f));
    
    // Основной фон
    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), 
                     ImGui::ColorConvertFloat4ToU32(colDeepBlue), 8.0f);
    
    // Верхний градиент
    ImVec4 topGrad = colDeepBlue;
    topGrad.x += 0.05f; topGrad.y += 0.05f; topGrad.z += 0.1f;
    d->AddRectFilledMultiColor(
        pos, ImVec2(pos.x + width, pos.y + height * 0.5f),
        ImGui::ColorConvertFloat4ToU32(topGrad),
        ImGui::ColorConvertFloat4ToU32(topGrad),
        ImGui::ColorConvertFloat4ToU32(colDeepBlue),
        ImGui::ColorConvertFloat4ToU32(colDeepBlue)
    );

    // --- Иконка DeepSeek (абстрактная волна/мозг) ---
    float iconX = pos.x + 8.0f;
    float iconY = pos.y + height * 0.5f;
    
    // Анимированные концентрические круги
    for(int i = 3; i > 0; i--) {
        float radius = 4.0f + i * 3.0f;
        float alpha = 0.08f - i * 0.02f;
        float anim = sinf(t * 1.8f + i * 0.8f) * 0.5f + 0.5f;
        
        ImVec4 waveCol = accent;
        waveCol.w = alpha * anim;
        d->AddCircle(ImVec2(iconX, iconY), radius, 
                     ImGui::ColorConvertFloat4ToU32(waveCol), 32, 1.5f);
    }
    
    // Центральный элемент - стилизованная "волна"
    float waveOffset = sinf(t * 2.5f) * 2.0f;
    d->AddBezierCubic(
        ImVec2(iconX - 6, iconY + waveOffset),
        ImVec2(iconX - 3, iconY - 4 - waveOffset),
        ImVec2(iconX + 3, iconY + 4 + waveOffset),
        ImVec2(iconX + 6, iconY - waveOffset),
        ImGui::ColorConvertFloat4ToU32(accent), 2.0f, 20
    );
    
    // Точки на волне
    for(int i = -2; i <= 2; i++) {
        float x = iconX + i * 3.0f;
        float y = iconY + sinf(t * 2.0f + i * 1.0f) * 2.0f;
        float pointAlpha = 0.7f + 0.3f * sinf(t * 3.0f + i * 0.5f);
        ImVec4 pointCol = accent;
        pointCol.w = pointAlpha;
        d->AddCircleFilled(ImVec2(x, y), 1.2f, ImGui::ColorConvertFloat4ToU32(pointCol));
    }

    // --- Текст ---
    float textX = pos.x + iconSize + 10.0f;
    float textY = pos.y + (height - ImGui::CalcTextSize("A").y) * 0.5f;

    // Заголовок SHALAWA с волнообразной анимацией букв
    for(size_t i = 0; i < title.length(); i++) {
        char c[2] = {title[i], '\0'};
        float charOffset = sinf(t * 2.0f + i * 0.4f) * 1.5f;
        float charAlpha = 0.8f + 0.2f * sinf(t * 1.8f + i * 0.3f);
        
        ImVec4 charColor = accent;
        charColor.w = charAlpha;
        
        // Тень
        d->AddText(ImVec2(textX + 1, textY + charOffset + 1), 
                   IM_COL32(0, 0, 0, 100), c);
        // Основной текст
        d->AddText(ImVec2(textX, textY + charOffset), 
                   ImGui::ColorConvertFloat4ToU32(charColor), c);
        
        textX += ImGui::CalcTextSize(c).x + (i == 0 ? 2.0f : 0.5f);
    }

    // Тэг "by DeepSeek"
    textX += 8.0f;
    ImVec4 tagColor = ImVec4(0.7f, 0.8f, 0.95f, 0.9f);
    d->AddText(ImVec2(textX, textY), ImGui::ColorConvertFloat4ToU32(tagColor), tagline.c_str());
    textX += tagW;

    // Информация (FPS, пинг и т.д.)
    if(!info.empty()) {
        textX += 12.0f;
        
        // Разделитель с анимацией
        float sepPulse = 0.5f + 0.5f * sinf(t * 4.0f);
        ImVec4 sepColor = accent;
        sepColor.w = 0.4f + 0.3f * sepPulse;
        d->AddLine(ImVec2(textX - 6, textY + 2), 
                   ImVec2(textX - 6, textY + ImGui::CalcTextSize("A").y - 2),
                   ImGui::ColorConvertFloat4ToU32(sepColor), 1.5f);
        
        textX += 4.0f;
        d->AddText(ImVec2(textX, textY), IM_COL32(180, 200, 230, 220), info.c_str());
    }

    // --- Нижняя акцентная полоса с анимацией "потока данных" ---
    float barY = pos.y + height - 3.0f;
    float barWidth = width * 0.7f;
    float barX = pos.x + (width - barWidth) * 0.5f;
    
    // Фон полосы
    d->AddRectFilled(ImVec2(barX, barY), 
                     ImVec2(barX + barWidth, barY + 2.0f),
                     IM_COL32(30, 50, 80, 100), 1.0f);
    
    // Анимированные точки потока данных
    for(int i = 0; i < 5; i++) {
        float posX = fmodf(t * 40.0f + i * 15.0f, barWidth);
        float size = 1.5f + sinf(t * 2.0f + i) * 0.5f;
        float alpha = 0.3f + 0.7f * sinf(t * 1.5f + i * 0.8f);
        
        ImVec4 dotColor = colCyan;
        dotColor.w = alpha;
        d->AddCircleFilled(ImVec2(barX + posX, barY + 1.0f), 
                          size, ImGui::ColorConvertFloat4ToU32(dotColor));
    }

    // --- Свечение по краям ---
    ImVec4 edgeGlow = glowColor;
    edgeGlow.w = 0.08f;
    d->AddRect(pos, ImVec2(pos.x + width, pos.y + height),
               ImGui::ColorConvertFloat4ToU32(edgeGlow), 8.0f, 0, 1.5f);
    
    // Угловые акценты
    float cornerSize = 6.0f;
    d->AddCircleFilled(ImVec2(pos.x + cornerSize, pos.y + cornerSize), 
                      1.5f, ImGui::ColorConvertFloat4ToU32(accent));
    d->AddCircleFilled(ImVec2(pos.x + width - cornerSize, pos.y + cornerSize), 
                      1.5f, ImGui::ColorConvertFloat4ToU32(accent));
    d->AddCircleFilled(ImVec2(pos.x + cornerSize, pos.y + height - cornerSize), 
                      1.5f, ImGui::ColorConvertFloat4ToU32(accent));
    d->AddCircleFilled(ImVec2(pos.x + width - cornerSize, pos.y + height - cornerSize), 
                      1.5f, ImGui::ColorConvertFloat4ToU32(accent));
}

void CWatermark::OnModuleToggle(const char *name, bool enabled)
{
    if(!g_Config.m_shalawaWatermarkNotifications)
        return;

    SNotification notif;
    notif.m_Text = name;
    notif.m_Enabled = enabled;
    notif.m_SpawnTime = (float)SDL_GetTicks() / 1000.0f;
    notif.m_Progress = 0.0f;
    notif.m_SlideAnim = 0.0f;

    m_Notifications.push_back(notif);

    while(m_Notifications.size() > 5)
        m_Notifications.erase(m_Notifications.begin());
}

void CWatermark::RenderNotifications()
{
    if(!ImGui::GetCurrentContext())
        return;
    if(!g_Config.m_shalawaWatermarkNotifications || m_Notifications.empty())
        return;
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *d = ImGui::GetBackgroundDrawList();
    float currentTime = (float)SDL_GetTicks() / OBF_F(1000);
    for(int i = (int)m_Notifications.size() - 1; i >= 0; i--)
    {
        SNotification &n = m_Notifications[i];
        float age = currentTime - n.m_SpawnTime;

        n.m_SlideAnim = Lerp(n.m_SlideAnim, 1.0f, m_DeltaTime * 8.0f);

        if(age < 3.5f)
            n.m_Progress = Lerp(n.m_Progress, 1.0f, m_DeltaTime * 8.0f);
        else
            n.m_Progress = Lerp(n.m_Progress, 0.0f, m_DeltaTime * 5.0f);

        if(age > 5.0f && n.m_Progress < 0.01f)
            m_Notifications.erase(m_Notifications.begin() + i);
    }

    if(m_Notifications.empty())
        return;
    float width = 160.0f;
    float height = 42.0f;
    float baseX = io.DisplaySize.x - width - 15.0f;
    float baseY = 55.0f;

    for(size_t i = 0; i < m_Notifications.size(); i++)
    {
        SNotification &n = m_Notifications[i];

        float slideOffset = (1.0f - n.m_SlideAnim) * 50.0f;
        float alpha = n.m_Progress;

        ImVec2 min(baseX + slideOffset, baseY + i * (height + 6.0f));
        ImVec2 max(min.x + width, min.y + height);

        DrawBlurredShadow(d, min, ImVec2(width, height), 5, ImVec4(0, 0, 0, 0.3f * alpha));
        d->AddRectFilled(min, max, IM_COL32(15, 15, 18, (int)(240 * alpha)), 8.0f);

        ImVec4 lineCol = n.m_Enabled ? ImVec4(0.3f, 0.8f, 0.45f, alpha) : ImVec4(0.8f, 0.3f, 0.3f, alpha);
        d->AddRectFilled(min, ImVec2(min.x + 3, max.y),
                         ImGui::ColorConvertFloat4ToU32(lineCol), 8.0f, ImDrawFlags_RoundCornersLeft);

        d->AddText(ImVec2(min.x + 12, min.y + 8), IM_COL32(255, 255, 255, (int)(255 * alpha)), n.m_Text.c_str());

        const char *status = n.m_Enabled ? "Enabled" : "Disabled";
        ImU32 statusCol = n.m_Enabled ? IM_COL32(80, 200, 120, (int)(255 * alpha)) : IM_COL32(200, 80, 80, (int)(255 * alpha));
        d->AddText(ImVec2(min.x + 12, min.y + 24), statusCol, status);
    }
}

void CWatermark::AddNotification(const char *text, bool isPositive)
{
    SNotification notif;
    notif.m_Text = text;
    notif.m_Enabled = isPositive;
    notif.m_SpawnTime = (float)SDL_GetTicks() / 1000.0f;
    notif.m_Progress = 0.0f;
    notif.m_SlideAnim = 0.0f;

    m_Notifications.push_back(notif);

    while(m_Notifications.size() > 5)
        m_Notifications.erase(m_Notifications.begin());
}

void CWatermark::RenderWatermarkAtPosition(ImVec2 pos, int style)
{
    ANTI_DISASM_BLOCK();
    if(!m_Initialized || !ImGui::GetCurrentContext())
        return;

    UpdateTime();

    switch(style)
    {
        case 0: RenderWatermarkModern(); break;
        case 1: RenderWatermarkRise(); break;
        case 2: RenderWatermarkGamesense(); break;
        case 3: RenderWatermarkNL(); break;
        case 4: RenderWatermarkSkeet(); break;
        case 5: RenderWatermarkStylo(); break;
        case 6: RenderWatermarkMinimal(); break;
        case 7: RenderWatermarkPills(); break;
        case 8: RenderWatermarkClaude(); break;
        case 9: RenderWatermarkGemini(); break;
        case 10: RenderWatermarkGPT(); break;
        case 11: RenderWatermarkGrok(); break;
        case 12: RenderWatermarkSeed(); break;
        case 13: RenderWatermarkDeepSeek(); break;
        default: RenderWatermarkModern(); break;
    }
}

ImVec2 CWatermark::GetWatermarkPosition(float width, float height)
{
    return ImVec2(10.0f, 10.0f);
}

void CWatermark::Render()
{
    ANTI_DISASM_BLOCK();
    if(!m_Initialized || !m_pGameClient || !g_Config.m_shalawaWatermark)
        return;

    if(!ImGui::GetCurrentContext())
        return;

    UpdateTime();

    SetModuleState("Avoid", g_Config.m_shalawaAvoidMode > 0);
    SetModuleState("Legit Avoid", g_Config.m_shalawaLegitAvoidFreeze != 0);
    SetModuleState("Blatant Avoid", g_Config.m_shalawaAvoidFreeze != 0);
    SetModuleState("Auto Answer", g_Config.m_shalawaAutoAnswer != 0);
    SetModuleState("Chat Spam", g_Config.m_shalawaChatSpam != 0);
    SetModuleState("TAS", g_Config.m_shalawaPlaying != 0);
    SetModuleState("Watermark", g_Config.m_shalawaWatermark != 0);
    SetModuleState("Avoid Visuals", g_Config.m_shalawaAvoidVisuals != 0);
    SetModuleState("Auto Fire", g_Config.m_shalawaAutoFire != 0);
    SetModuleState("Balance Bot", g_Config.m_shalawaBalanceBot != 0);
    SetModuleState("Hook Ride", g_Config.m_shalawaHookRide != 0);
    SetModuleState("Unfreeze Bot", g_Config.m_shalawaUnfreezeBot != 0);
    SetModuleState("Hide Cheat", g_Config.m_shalawaHideCheat != 0);
    SetModuleState("Hit Particles", g_Config.m_shalawaHitParticles != 0);
    SetModuleState("China Hat", g_Config.m_shalawaChinaHat != 0);
    SetModuleState("Arraylist", g_Config.m_shalawaArraylist != 0);
    SetModuleState("Fast Input", g_Config.m_ClFastInput != 0);
    SetModuleState("Hide Chat Bubble", g_Config.m_ClHideChatBubble != 0);
    SetModuleState("HUD Editor", g_Config.m_shalawaHudEditor != 0);
    SetModuleState("Tail", g_Config.m_shalawaTail != 0);
    SetModuleState("Quick Stop", g_Config.m_shalawaQuickStop != 0);
    SetModuleState("Spoof", g_Config.m_shalawaSpoofVersion != 0);
    SetModuleState("SOCKS5", g_Config.m_shalawaSocks5Enabled != 0);

    switch(g_Config.m_shalawaWatermarkStyle)
    {
        case 0: RenderWatermarkModern(); break;
        case 1: RenderWatermarkRise(); break;
        case 2: RenderWatermarkGamesense(); break;
        case 3: RenderWatermarkNL(); break;
        case 4: RenderWatermarkSkeet(); break;
        case 5: RenderWatermarkStylo(); break;
        case 6: RenderWatermarkMinimal(); break;
        case 7: RenderWatermarkPills(); break;
        case 8: RenderWatermarkClaude(); break;
        case 9: RenderWatermarkGemini(); break;
        case 10: RenderWatermarkGPT(); break;
        case 11: RenderWatermarkGrok(); break;
        case 12: RenderWatermarkSeed(); break;
        case 13: RenderWatermarkDeepSeek(); break;
        default: RenderWatermarkModern(); break;
    }

    RenderNotifications();
}
