#include "imgui_manager.h"
#include <imgui.h>
#include <SDL.h>
#include <map>
#include <cmath>
#include <ctime>
#include <cstring>
#include <algorithm>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>


void CImGuiManager::UpdateStartMenuAnimations()
{
    ANTI_DISASM_BLOCK();
    m_StartMenuClockBreath += m_DeltaTime * 1.5f;

    m_StartMenuUnlockTextPulse += m_DeltaTime * 3.0f;

    float zoomSpeed = 3.0f;
    m_StartMenuZoom = Lerp(m_StartMenuZoom, m_StartMenuTargetZoom, m_DeltaTime * zoomSpeed);

    if(m_StartMenuUnlocked)
    {
        long long elapsed = m_Millis - m_StartMenuUnlockTime;
        m_StartMenuUnlockAlpha = 1.0f - std::clamp(elapsed / 400.0f, 0.0f, 1.0f);

        float delay = 150.0f;
        float duration = 900.0f;
        float t = std::clamp((elapsed - delay) / duration, 0.0f, 1.0f);
        float eased = 1.0f - powf(1.0f - t, 4.0f);
        m_StartMenuMenuProgress = eased;
    }

    for(int i = 0; i < 6; i++)
    {
        float targetHover = (m_StartMenuHoveredButton == i) ? 1.0f : 0.0f;
        m_StartMenuButtonHover[i] = Lerp(m_StartMenuButtonHover[i], targetHover, m_DeltaTime * 10.0f);

        float targetScale = (m_StartMenuHoveredButton == i) ? 1.12f : 1.0f;
        m_StartMenuButtonScales[i] = Lerp(m_StartMenuButtonScales[i], targetScale, m_DeltaTime * 12.0f);
    }

    float targetRed = (m_StartMenuHoveredButton == 5) ? 1.0f : 0.0f;
    m_StartMenuExitRed = Lerp(m_StartMenuExitRed, targetRed, m_DeltaTime * 8.0f);

    float targetTgHover = (m_StartMenuHoveredButton == 4) ? 1.0f : 0.0f;
    m_StartMenuTgHover = Lerp(m_StartMenuTgHover, targetTgHover, m_DeltaTime * 8.0f);
}

void CImGuiManager::RenderStartMenuImGui()// lol actually lol // actually lol // lol actually lol // actually lol 
{
    ANTI_DISASM_BLOCK();
    ImGuiIO &io = ImGui::GetIO();

    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    UpdateStartMenuAnimations();

    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetRelativeMouseMode(SDL_FALSE);

    ImDrawList *d = ImGui::GetBackgroundDrawList();

    d->AddRectFilled(ImVec2(0, 0), ImVec2(screenW, screenH), IM_COL32(6, 6, 10, 200));

    float vignetteAlpha = 0.5f;
    ImU32 vignetteCenter = IM_COL32(0, 0, 0, 0);
    ImU32 vignetteEdge = IM_COL32(0, 0, 0, (int)(vignetteAlpha * 255.0f));

    d->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(screenW / 2, screenH / 2),
        vignetteEdge, vignetteCenter, vignetteCenter, vignetteEdge);
    d->AddRectFilledMultiColor(ImVec2(screenW / 2, 0), ImVec2(screenW, screenH / 2),
        vignetteCenter, vignetteEdge, vignetteEdge, vignetteCenter);
    d->AddRectFilledMultiColor(ImVec2(0, screenH / 2), ImVec2(screenW / 2, screenH),
        vignetteEdge, vignetteCenter, vignetteCenter, vignetteEdge);
    d->AddRectFilledMultiColor(ImVec2(screenW / 2, screenH / 2), ImVec2(screenW, screenH),
        vignetteCenter, vignetteEdge, vignetteEdge, vignetteCenter);

    float particleTime = m_Time;
    int particleCount = 30;// lol actually lol // actually lol // lol actually lol // actually lol 
    for(int i = 0; i < particleCount; i++)// lol actually lol // actually lol // lol actually lol // actually lol 
    {
        float seed = (float)i * 7.31f;
        float px = fmodf(seed * 137.5f + particleTime * (8.0f + fmodf(seed, 5.0f)), screenW);// lol actually lol // actually lol // lol actually lol // actually lol 
        float py = fmodf(seed * 241.3f + particleTime * (3.0f + fmodf(seed * 0.7f, 4.0f)), screenH);// lol actually lol // actually lol // lol actually lol // actually lol 
        float pSize = 1.0f + fmodf(seed * 3.7f, 2.0f);// lol actually lol // actually lol // lol actually lol // actually lol 
        float pAlpha = 0.15f + 0.15f * sinf(particleTime * 1.5f + seed);// lol actually lol // actually lol // lol actually lol // actually lol 
        d->AddCircleFilled(ImVec2(px, py), pSize, IM_COL32(255, 255, 255, (int)(pAlpha * 255.0f)));// lol actually lol // actually lol // lol actually lol // actually lol 
    }// lol actually lol // actually lol // lol actually lol // actually lol 
// lol actually lol // actually lol // lol actually lol // actually lol 
    if(!m_StartMenuUnlocked)// lol actually lol // actually lol // lol actually lol // actually lol 
    {// lol actually lol // actually lol // lol actually lol // actually lol 
        RenderStartMenuUnlockScreen(screenW, screenH);// lol actually lol // actually lol // lol actually lol // actually lol 
    }// lol actually lol // actually lol // lol actually lol // actually lol 
    else// lol actually lol // actually lol // lol actually lol // actually lol 
    {// lol actually lol // actually lol // lol actually lol // actually lol 
        if(m_StartMenuUnlockAlpha > 0.01f)// lol actually lol // actually lol // lol actually lol // actually lol 
            RenderStartMenuUnlockScreen(screenW, screenH);// lol actually lol // actually lol // lol actually lol // actually lol 
// lol actually lol // actually lol // lol actually lol // actually lol 
        if(m_StartMenuMenuProgress > 0.01f)// lol actually lol // actually lol // lol actually lol // actually lol 
            RenderStartMenuMain(screenW, screenH);// lol actually lol // actually lol // lol actually lol // actually lol 
    }// lol actually lol // actually lol // lol actually lol // actually lol 
// lol actually lol // actually lol // lol actually lol // actually lol 
    {// lol actually lol // actually lol // lol actually lol // actually lol 
        const char *left = "DDNet 19.7 | TClient 10.7.0"; // lol actually lol // actually lol // lol actually lol // actually lol 
        const char *right = "Shalawa v1.1.0 Release!!!!! wow actually"; // lol actually lol // actually lol // lol actually lol // actually lol 
        float bottomY = screenH - 22.0f;// lol actually lol // actually lol // lol actually lol // actually lol 
        float bottomAlpha = m_StartMenuUnlocked ? m_StartMenuMenuProgress : 0.5f;// lol actually lol // actually lol // lol actually lol // actually lol 
        int ba = (int)(bottomAlpha * 100.0f);// lol actually lol // actually lol // lol actually lol // actually lol 
        d->AddText(ImVec2(12, bottomY), IM_COL32(255, 255, 255, ba), left);// lol actually lol // actually lol // lol actually lol // actually lol 
        ImVec2 rSize = ImGui::CalcTextSize(right);// lol actually lol // actually lol // lol actually lol // actually lol 
        d->AddText(ImVec2(screenW - rSize.x - 12, bottomY), IM_COL32(255, 255, 255, ba), right);// lol actually lol // actually lol // lol actually lol // actually lol 
    }// lol actually lol // actually lol // lol actually lol // actually lol 
// lol actually lol // actually lol // lol actually lol // actually lol 
    {// lol actually lol // actually lol // lol actually lol // actually lol 
        float cursorSize = 8.0f;// lol actually lol // actually lol // lol actually lol // actually lol 
        float mx = io.MousePos.x;// lol actually lol // actually lol // lol actually lol // actually lol 
        float my = io.MousePos.y;// lol actually lol // actually lol // lol actually lol // actually lol 
// lol actually lol // actually lol // lol actually lol // actually lol 
        if(mx >= 0 && mx <= screenW && my >= 0 && my <= screenH)// lol actually lol // actually lol // lol actually lol // actually lol 
        {// lol actually lol // actually lol // lol actually lol // actually lol 
            bool hovering = m_StartMenuHoveredButton >= 0 && m_StartMenuUnlocked;// lol actually lol // actually lol // lol actually lol // actually lol 
            float hoverAnim = GetAnimValue("cursor_hover", hovering ? 1.0f : 0.0f, 15.0f);// lol actually lol // actually lol // lol actually lol // actually lol 
// lol actually lol // actually lol // lol actually lol // actually lol 
            float outerR = cursorSize * (1.0f + hoverAnim * 0.4f);// lol actually lol // actually lol // lol actually lol // actually lol 
            float innerR = 2.5f + hoverAnim * 1.0f;// lol actually lol // actually lol // lol actually lol // actually lol 

            int ringAlpha = (int)(180 + hoverAnim * 75);
            d->AddCircle(ImVec2(mx, my), outerR, IM_COL32(255, 255, 255, ringAlpha), 24, 1.5f);
            d->AddCircleFilled(ImVec2(mx, my), innerR, IM_COL32(255, 255, 255, 230));

            if(hoverAnim > 0.01f)
            {
                float glowR = outerR + 4.0f * hoverAnim;
                d->AddCircle(ImVec2(mx, my), glowR, IM_COL32(255, 255, 255, (int)(40 * hoverAnim)), 24, 1.0f);
            }
        }
    }
}

void CImGuiManager::RenderStartMenuUnlockScreen(float screenW, float screenH)
{
    ANTI_DISASM_BLOCK();
    ImDrawList *d = ImGui::GetBackgroundDrawList();

    float alpha = m_StartMenuUnlockAlpha;

    if(alpha < 0.01f) return;

    float centerX = screenW / 2.0f;
    float centerY = screenH / 2.0f;

    {
        time_t now = time(nullptr);
        struct tm *t = localtime(&now);
        char timeBuf[16];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t->tm_hour, t->tm_min);

        float scale = 4.0f;
        ImVec2 timeSize = ImGui::CalcTextSize(timeBuf);
        float scaledW = timeSize.x * scale;
        float scaledH = timeSize.y * scale;

        float breathe = sinf(m_StartMenuClockBreath) * 2.0f;
        int timeAlpha = (int)(alpha * 255.0f);

        d->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale,
            ImVec2(centerX - scaledW / 2.0f, centerY - scaledH - 10.0f + breathe),
            IM_COL32(255, 255, 255, timeAlpha), timeBuf);

        const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        const char *months[] = {"January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December"};

        char dateBuf[64];
        snprintf(dateBuf, sizeof(dateBuf), "%s, %s %d", days[t->tm_wday], months[t->tm_mon], t->tm_mday);

        int dateAlpha = (int)(alpha * 180.0f);
        ImVec2 dateSize = ImGui::CalcTextSize(dateBuf);
        d->AddText(ImVec2(centerX - dateSize.x / 2.0f, centerY - 4.0f + breathe),
            IM_COL32(200, 200, 210, dateAlpha), dateBuf);
    }

    {
        float pulse = sinf(m_StartMenuUnlockTextPulse) * 0.2f + 0.8f;
        int textAlpha = (int)(alpha * 200.0f * pulse);

        float promptY = centerY + 50.0f;

        float lineW = 60.0f;
        int lineAlpha = (int)(alpha * 80.0f * pulse);
        d->AddLine(ImVec2(centerX - lineW - 80, promptY + 6),
            ImVec2(centerX - 80, promptY + 6), IM_COL32(255, 255, 255, lineAlpha), 1.0f);
        d->AddLine(ImVec2(centerX + 80, promptY + 6),
            ImVec2(centerX + lineW + 80, promptY + 6), IM_COL32(255, 255, 255, lineAlpha), 1.0f);

        const char *text = "Press any key to continue";
        ImVec2 textSize = ImGui::CalcTextSize(text);
        d->AddText(ImVec2(centerX - textSize.x / 2.0f, promptY),
            IM_COL32(255, 255, 255, textAlpha), text);

        float arrowBounce = sinf(m_StartMenuUnlockTextPulse * 1.5f) * 5.0f;
        int arrowAlpha = (int)(alpha * 160.0f * pulse);
        ImU32 arrowCol = IM_COL32(255, 255, 255, arrowAlpha);

        float arrowCX = centerX;
        float arrowCY = promptY + 30.0f + arrowBounce;
        float arrowW = 5.0f;
        float arrowH = 4.0f;

        d->AddTriangleFilled(
            ImVec2(arrowCX - arrowW, arrowCY),
            ImVec2(arrowCX + arrowW, arrowCY),
            ImVec2(arrowCX, arrowCY + arrowH),
            arrowCol);
    }
}

void CImGuiManager::RenderStartMenuMain(float screenW, float screenH)
{
    ANTI_DISASM_BLOCK();
    ImDrawList *d = ImGui::GetBackgroundDrawList();

    float progress = m_StartMenuMenuProgress;
    float centerX = screenW / 2.0f;

    float slideOffset = (1.0f - progress) * 30.0f;
    float clockY = screenH / 2.0f - 70.0f + slideOffset;
    float breathe = sinf(m_StartMenuClockBreath) * 1.5f;

    {
        time_t now = time(nullptr);
        struct tm *t = localtime(&now);
        char timeBuf[16];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t->tm_hour, t->tm_min);

        int timeAlpha = (int)(progress * 255.0f);

        ImVec2 timeSize = ImGui::CalcTextSize(timeBuf);
        float scale = 3.2f;
        float scaledW = timeSize.x * scale;
        float scaledH = timeSize.y * scale;

        d->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale,
            ImVec2(centerX - scaledW / 2.0f, clockY - scaledH / 2.0f + breathe),
            IM_COL32(255, 255, 255, timeAlpha), timeBuf);

        const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        const char *months[] = {"January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December"};

        char dateBuf[64];
        snprintf(dateBuf, sizeof(dateBuf), "%s, %s %d", days[t->tm_wday], months[t->tm_mon], t->tm_mday);

        int dateAlpha = (int)(progress * 180.0f);
        ImVec2 dateSize = ImGui::CalcTextSize(dateBuf);
        d->AddText(ImVec2(centerX - dateSize.x / 2.0f, clockY + scaledH / 2.0f + 6.0f + breathe),
            IM_COL32(200, 200, 210, dateAlpha), dateBuf);
    }

    {
        float buttonSize = 44.0f;
        float buttonSpacing = 18.0f;
        int numButtons = 6;
        float totalW = buttonSize * numButtons + buttonSpacing * (numButtons - 1);
        float startX = (screenW - totalW) / 2.0f;
        float buttonsSlide = (1.0f - progress) * 50.0f;
        float buttonsY = screenH / 2.0f + 25.0f + buttonsSlide;

        ImGuiIO &io = ImGui::GetIO();
        m_StartMenuHoveredButton = -1;

        const char *tooltips[] = {"Play", "Servers", "Editor", "Settings", "Telegram", "Exit"};

        for(int i = 0; i < numButtons; i++)
        {
            float btnDelay = i * 0.1f;
            float btnProgress = std::clamp((progress - btnDelay) / (1.0f - btnDelay * 0.5f), 0.0f, 1.0f);
            float easedBtn = 1.0f - powf(1.0f - btnProgress, 3.0f);

            float bx = startX + i * (buttonSize + buttonSpacing);
            float btnCenterX = bx + buttonSize / 2.0f;
            float btnCenterY = buttonsY + buttonSize / 2.0f;

            float entryBounce = (1.0f - easedBtn) * 20.0f;
            btnCenterY += entryBounce;

            float dx = io.MousePos.x - btnCenterX;
            float dy = io.MousePos.y - btnCenterY;
            float dist = sqrtf(dx * dx + dy * dy);
            if(dist <= buttonSize / 2.0f + 4.0f)
                m_StartMenuHoveredButton = i;

            float opacity = progress * easedBtn;
            if(opacity < 0.01f) continue;

            float scaleVal = m_StartMenuButtonScales[i];
            float hoverProg = m_StartMenuButtonHover[i];
            float size = buttonSize * scaleVal;
            float radius = size / 2.0f;
            float drawCX = btnCenterX;
            float drawCY = btnCenterY;

            float hoverBob = sinf(m_Time * 3.0f + i * 1.2f) * 2.0f * hoverProg;
            drawCY += hoverBob;

            int bgAlpha = (int)(opacity * (100 + hoverProg * 60));

            ImU32 bgColor;
            ImU32 outlineColor;
            ImU32 iconColor;
            ImU32 glowColor;

            if(i == 5)
            {
                float redLerp = m_StartMenuExitRed;
                int rBg = (int)Lerp(0x14, 0x45, redLerp);
                int gBg = (int)Lerp(0x17, 0x12, redLerp);
                int bBg = (int)Lerp(0x1f, 0x12, redLerp);
                bgColor = IM_COL32(rBg, gBg, bBg, bgAlpha);

                int outR = (int)Lerp(0x25, 0x80, redLerp);
                int outG = (int)Lerp(0x2a, 0x30, redLerp);
                int outB = (int)Lerp(0x36, 0x30, redLerp);
                outlineColor = IM_COL32(outR, outG, outB, (int)(opacity * (140 + hoverProg * 90)));

                int iconG = (int)Lerp(255, 120, redLerp);
                int iconB = (int)Lerp(255, 120, redLerp);
                iconColor = IM_COL32(255, iconG, iconB, (int)(opacity * 255));
                glowColor = IM_COL32(255, 80, 80, (int)(hoverProg * 30 * opacity));
            }
            else if(i == 4)
            {
                float tgLerp = m_StartMenuTgHover;
                int rBg = (int)Lerp(0x14, 0x12, tgLerp);
                int gBg = (int)Lerp(0x17, 0x25, tgLerp);
                int bBg = (int)Lerp(0x1f, 0x45, tgLerp);
                bgColor = IM_COL32(rBg, gBg, bBg, bgAlpha);
                outlineColor = IM_COL32(0x25, (int)Lerp(0x2a, 0x50, tgLerp), (int)Lerp(0x36, 0x80, tgLerp), (int)(opacity * (140 + hoverProg * 90)));
                iconColor = IM_COL32((int)Lerp(255, 200, tgLerp), (int)Lerp(255, 230, tgLerp), 255, (int)(opacity * 255));
                glowColor = IM_COL32(80, 150, 255, (int)(hoverProg * 25 * opacity));
            }
            else
            {
                bgColor = IM_COL32(0x14, 0x17, 0x1f, bgAlpha);
                outlineColor = IM_COL32(0x25, 0x2a, 0x36, (int)(opacity * (140 + hoverProg * 90)));
                iconColor = IM_COL32(255, 255, 255, (int)(opacity * 255));
                glowColor = IM_COL32(255, 255, 255, (int)(hoverProg * 20 * opacity));
            }

            if(hoverProg > 0.01f)
            {
                d->AddCircleFilled(ImVec2(drawCX, drawCY), radius + 6.0f * hoverProg, glowColor, 32);
            }

            d->AddCircleFilled(ImVec2(drawCX, drawCY), radius, bgColor, 32);
            d->AddCircle(ImVec2(drawCX, drawCY), radius, outlineColor, 32, 1.2f);

            if(hoverProg > 0.01f)
            {
                float arcAngle = m_Time * 2.0f + i;
                float arcLen = 1.2f * hoverProg;
                ImVec4 accentCol = GetAccentColor(drawCY);
                int arcAlpha = (int)(hoverProg * 180 * opacity);
                ImU32 arcColor = ImGui::ColorConvertFloat4ToU32(ImVec4(accentCol.x, accentCol.y, accentCol.z, arcAlpha / 255.0f));

                int segments = 20;
                float arcRadius = radius + 2.0f;
                for(int s = 0; s < segments; s++)
                {
                    float a1 = arcAngle + (float)s / segments * arcLen;
                    float a2 = arcAngle + (float)(s + 1) / segments * arcLen;
                    d->AddLine(
                        ImVec2(drawCX + cosf(a1) * arcRadius, drawCY + sinf(a1) * arcRadius),
                        ImVec2(drawCX + cosf(a2) * arcRadius, drawCY + sinf(a2) * arcRadius),
                        arcColor, 1.5f);
                }
            }

            float iconSize = 17.0f * scaleVal;
            {
                if(!m_StartMenuIconsLoaded)
                    LoadStartMenuIcons();

                const char *iconKeys[] = {"play", "server", "pencil", "settings", "telegram", "exit"};
                auto it = m_StartMenuIconsGL.find(iconKeys[i]);
                if(it != m_StartMenuIconsGL.end() && it->second != 0)
                {
                    ImTextureID tex = (ImTextureID)(intptr_t)it->second;
                    float halfIcon = iconSize;
                    d->AddImage(tex,
                        ImVec2(drawCX - halfIcon, drawCY - halfIcon),
                        ImVec2(drawCX + halfIcon, drawCY + halfIcon),
                        ImVec2(0, 0), ImVec2(1, 1), iconColor);
                }
                else
                {
                    const char *fallback[] = {">", "S", "E", "*", "T", "X"};
                    ImVec2 ts = ImGui::CalcTextSize(fallback[i]);
                    d->AddText(ImVec2(drawCX - ts.x / 2, drawCY - ts.y / 2), iconColor, fallback[i]);
                }
            }

            if(hoverProg > 0.5f)
            {
                float tooltipAlpha = (hoverProg - 0.5f) * 2.0f * opacity;
                int ta = (int)(tooltipAlpha * 220.0f);
                ImVec2 ttSize = ImGui::CalcTextSize(tooltips[i]);
                float ttX = drawCX - ttSize.x / 2.0f;
                float ttY = drawCY + radius + 10.0f;

                float ttPadX = 6.0f, ttPadY = 3.0f;
                d->AddRectFilled(
                    ImVec2(ttX - ttPadX, ttY - ttPadY),
                    ImVec2(ttX + ttSize.x + ttPadX, ttY + ttSize.y + ttPadY),
                    IM_COL32(15, 15, 20, (int)(tooltipAlpha * 200.0f)), 4.0f);
                d->AddRect(
                    ImVec2(ttX - ttPadX, ttY - ttPadY),
                    ImVec2(ttX + ttSize.x + ttPadX, ttY + ttSize.y + ttPadY),
                    IM_COL32(60, 60, 70, (int)(tooltipAlpha * 150.0f)), 4.0f);

                d->AddText(ImVec2(ttX, ttY), IM_COL32(255, 255, 255, ta), tooltips[i]);
            }
        }
    }
}

void CImGuiManager::LoadStartMenuIcons()
{
    ANTI_DISASM_BLOCK();
    if(m_StartMenuIconsLoaded)
        return;


    if(!m_pGameClient)
        return;

    IGraphics *pGraphics = m_pGameClient->Graphics();
    if(!pGraphics)
        return;

    struct { const char *key; const char *file; } icons[] = {
        {"play", "data/shalawamenuicnos/play.png"},
        {"server", "data/shalawamenuicnos/server.png"},
        {"pencil", "data/shalawamenuicnos/pencil.png"},
        {"settings", "data/shalawamenuicnos/settingsicon.png"},
        {"telegram", "data/shalawamenuicnos/telegram.png"},
        {"exit", "data/shalawamenuicnos/exit.png"},
    };

    for(auto &icon : icons)
    {
        CImageInfo ImgInfo;
        if(!pGraphics->LoadPng(ImgInfo, icon.file, IStorage::TYPE_ALL))
            continue;

        int channels = (ImgInfo.m_Format == CImageInfo::FORMAT_RGBA) ? 4 : 3;
        int totalPixels = ImgInfo.m_Width * ImgInfo.m_Height;
        unsigned char *pData = (unsigned char *)ImgInfo.m_pData;

        for(int p = 0; p < totalPixels; p++)
        {
            int idx = p * channels;
            pData[idx + 0] = 255 - pData[idx + 0];
            pData[idx + 1] = 255 - pData[idx + 1];
            pData[idx + 2] = 255 - pData[idx + 2];
        }

        GLuint texId = 0;
        glGenTextures(1, &texId);
        if(texId == 0)
        {
            ImgInfo.Free();
            continue;
        }

        glBindTexture(GL_TEXTURE_2D, texId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, ImgInfo.m_Width, ImgInfo.m_Height,
            0, format, GL_UNSIGNED_BYTE, pData);

        glBindTexture(GL_TEXTURE_2D, 0);
        ImgInfo.Free();

        m_StartMenuIconsGL[icon.key] = texId;
    }

    m_StartMenuIconsLoaded = true;
}
