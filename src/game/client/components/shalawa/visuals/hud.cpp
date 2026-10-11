#include "hud.h"
#include <game/client/gameclient.h>
#include <game/client/components/shalawa/misc/watermark.h>
#include <game/client/components/shalawa/visuals/iof.h>
#include <engine/shared/config.h>
#include <SDL.h>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <base/vmath.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>

CHudEditor::CHudEditor()
{
    ANTI_DISASM_BLOCK();
    m_pGameClient = nullptr;
    m_Active = false;
    m_Initialized = false;
    m_DeltaTime = 0.0f;
    m_LastTime = 0.0f;
    m_Time = 0.0f;
    m_SelectedElement = -1;
    m_HoveredElement = -1;
}

void CHudEditor::Init(CGameClient *pGameClient)
{
    ANTI_DISASM_BLOCK();
    m_pGameClient = pGameClient;
    m_Elements.clear();
    m_Initialized = true;
    m_LastTime = (float)SDL_GetTicks() / 1000.0f;
}

float CHudEditor::Lerp(float a, float b, float t)
{
    return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
}

float CHudEditor::EaseOutBack(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    float c1 = 1.70158f;
    float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
}

ImVec4 CHudEditor::LerpColor(ImVec4 a, ImVec4 b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

ImVec4 CHudEditor::HSVtoRGB(float h, float s, float v)
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

void CHudEditor::RegisterElement(EHudElement type, const std::string &name,
                                 int *pX, int *pY, int *pAnchor, int *pScale,
                                 float baseW, float baseH,
                                 std::function<void(ImVec2, ImVec2, float)> renderFunc)
{
    ANTI_DISASM_BLOCK();
    if(!pX || !pY || !pAnchor || !pScale)
        return;
    
    SHudElementData elem;
    elem.m_Type = type;
    elem.m_Name = name;
    elem.m_pX = pX;
    elem.m_pY = pY;
    elem.m_pAnchor = pAnchor;
    elem.m_pScale = pScale;
    elem.m_BaseWidth = baseW;
    elem.m_BaseHeight = baseH;
    elem.m_DragOffsetX = 0;
    elem.m_DragOffsetY = 0;
    elem.m_Dragging = false;
    elem.m_Resizing = false;
    elem.m_ResizeCorner = -1;
    elem.m_HoverAnim = 0.0f;
    elem.m_SelectAnim = 0.0f;
    elem.m_RenderFunc = renderFunc;
    
    if((int)type < (int)m_Elements.size())
        m_Elements[type] = elem;
    else
        m_Elements.push_back(elem);
}

ImVec2 CHudEditor::AnchorToScreen(int anchor, float offsetX, float offsetY, float width, float height)
{
    float x = 0, y = 0;
    
    switch(anchor)
    {
        case ANCHOR_TOP_LEFT:
            x = offsetX;
            y = offsetY;
            break;
        case ANCHOR_TOP_RIGHT:
            x = m_ScreenSize.x - width + offsetX;
            y = offsetY;
            break;
        case ANCHOR_BOTTOM_LEFT:
            x = offsetX;
            y = m_ScreenSize.y - height + offsetY;
            break;
        case ANCHOR_BOTTOM_RIGHT:
            x = m_ScreenSize.x - width + offsetX;
            y = m_ScreenSize.y - height + offsetY;
            break;
        case ANCHOR_TOP_CENTER:
            x = m_ScreenCenter.x - width / 2 + offsetX;
            y = offsetY;
            break;
        case ANCHOR_BOTTOM_CENTER:
            x = m_ScreenCenter.x - width / 2 + offsetX;
            y = m_ScreenSize.y - height + offsetY;
            break;
        case ANCHOR_LEFT_CENTER:
            x = offsetX;
            y = m_ScreenCenter.y - height / 2 + offsetY;
            break;
        case ANCHOR_RIGHT_CENTER:
            x = m_ScreenSize.x - width + offsetX;
            y = m_ScreenCenter.y - height / 2 + offsetY;
            break;
        case ANCHOR_CENTER:
            x = m_ScreenCenter.x - width / 2 + offsetX;
            y = m_ScreenCenter.y - height / 2 + offsetY;
            break;
    }
    
    return ImVec2(x, y);
}

void CHudEditor::ScreenToAnchor(ImVec2 pos, float width, float height, int &outAnchor, float &outX, float &outY)
{
    float centerX = pos.x + width / 2;
    float centerY = pos.y + height / 2;
    
    bool left = centerX < m_ScreenSize.x / 3;
    bool right = centerX > m_ScreenSize.x * 2 / 3;
    bool top = centerY < m_ScreenSize.y / 3;
    bool bottom = centerY > m_ScreenSize.y * 2 / 3;
    
    if(left && top)
    {
        outAnchor = ANCHOR_TOP_LEFT;
        outX = pos.x;
        outY = pos.y;
    }
    else if(right && top)
    {
        outAnchor = ANCHOR_TOP_RIGHT;
        outX = pos.x - (m_ScreenSize.x - width);
        outY = pos.y;
    }
    else if(left && bottom)
    {
        outAnchor = ANCHOR_BOTTOM_LEFT;
        outX = pos.x;
        outY = pos.y - (m_ScreenSize.y - height);
    }
    else if(right && bottom)
    {
        outAnchor = ANCHOR_BOTTOM_RIGHT;
        outX = pos.x - (m_ScreenSize.x - width);
        outY = pos.y - (m_ScreenSize.y - height);
    }
    else if(!left && !right && top)
    {
        outAnchor = ANCHOR_TOP_CENTER;
        outX = pos.x - (m_ScreenCenter.x - width / 2);
        outY = pos.y;
    }
    else if(!left && !right && bottom)
    {
        outAnchor = ANCHOR_BOTTOM_CENTER;
        outX = pos.x - (m_ScreenCenter.x - width / 2);
        outY = pos.y - (m_ScreenSize.y - height);
    }
    else if(left && !top && !bottom)
    {
        outAnchor = ANCHOR_LEFT_CENTER;
        outX = pos.x;
        outY = pos.y - (m_ScreenCenter.y - height / 2);
    }
    else if(right && !top && !bottom)
    {
        outAnchor = ANCHOR_RIGHT_CENTER;
        outX = pos.x - (m_ScreenSize.x - width);
        outY = pos.y - (m_ScreenCenter.y - height / 2);
    }
    else
    {
        outAnchor = ANCHOR_CENTER;
        outX = pos.x - (m_ScreenCenter.x - width / 2);
        outY = pos.y - (m_ScreenCenter.y - height / 2);
    }
}

void CHudEditor::UpdateSnapLines(SHudElementData &elem, ImVec2 pos, ImVec2 size)
{
    ANTI_DISASM_BLOCK();
    m_SnapLines.clear();
    
    if(!g_Config.m_shalawaHudShowGuides)
        return;
    
    float threshold = (float)g_Config.m_shalawaHudSnapThreshold;
    float elemCenterX = pos.x + size.x / 2;
    float elemCenterY = pos.y + size.y / 2;
    
    if(fabsf(elemCenterX - m_ScreenCenter.x) < threshold)
    {
        SSnapLine line;
        line.m_Start = ImVec2(m_ScreenCenter.x, 0);
        line.m_End = ImVec2(m_ScreenCenter.x, m_ScreenSize.y);
        line.m_Horizontal = false;
        line.m_Alpha = 1.0f - fabsf(elemCenterX - m_ScreenCenter.x) / threshold;
        m_SnapLines.push_back(line);
    }
    
    if(fabsf(elemCenterY - m_ScreenCenter.y) < threshold)
    {
        SSnapLine line;
        line.m_Start = ImVec2(0, m_ScreenCenter.y);
        line.m_End = ImVec2(m_ScreenSize.x, m_ScreenCenter.y);
        line.m_Horizontal = true;
        line.m_Alpha = 1.0f - fabsf(elemCenterY - m_ScreenCenter.y) / threshold;
        m_SnapLines.push_back(line);
    }
    
    if(fabsf(pos.x) < threshold)
    {
        SSnapLine line;
        line.m_Start = ImVec2(0, 0);
        line.m_End = ImVec2(0, m_ScreenSize.y);
        line.m_Horizontal = false;
        line.m_Alpha = 1.0f - fabsf(pos.x) / threshold;
        m_SnapLines.push_back(line);
    }
    
    if(fabsf(pos.x + size.x - m_ScreenSize.x) < threshold)
    {
        SSnapLine line;
        line.m_Start = ImVec2(m_ScreenSize.x, 0);
        line.m_End = ImVec2(m_ScreenSize.x, m_ScreenSize.y);
        line.m_Horizontal = false;
        line.m_Alpha = 1.0f - fabsf(pos.x + size.x - m_ScreenSize.x) / threshold;
        m_SnapLines.push_back(line);
    }
    
    if(fabsf(pos.y) < threshold)
    {
        SSnapLine line;
        line.m_Start = ImVec2(0, 0);
        line.m_End = ImVec2(m_ScreenSize.x, 0);
        line.m_Horizontal = true;
        line.m_Alpha = 1.0f - fabsf(pos.y) / threshold;
        m_SnapLines.push_back(line);
    }
    
    if(fabsf(pos.y + size.y - m_ScreenSize.y) < threshold)
    {
        SSnapLine line;
        line.m_Start = ImVec2(0, m_ScreenSize.y);
        line.m_End = ImVec2(m_ScreenSize.x, m_ScreenSize.y);
        line.m_Horizontal = true;
        line.m_Alpha = 1.0f - fabsf(pos.y + size.y - m_ScreenSize.y) / threshold;
        m_SnapLines.push_back(line);
    }
    
    for(size_t i = 0; i < m_Elements.size(); i++)
    {
        if(&m_Elements[i] == &elem)
            continue;
        
        float scale = (float)*m_Elements[i].m_pScale / 100.0f;
        float otherW = m_Elements[i].m_BaseWidth * scale;
        float otherH = m_Elements[i].m_BaseHeight * scale;
        ImVec2 otherPos = AnchorToScreen(*m_Elements[i].m_pAnchor, (float)*m_Elements[i].m_pX, 
                                          (float)*m_Elements[i].m_pY, otherW, otherH);
        
        float otherCenterX = otherPos.x + otherW / 2;
        float otherCenterY = otherPos.y + otherH / 2;
        
        if(fabsf(elemCenterX - otherCenterX) < threshold)
        {
            SSnapLine line;
            line.m_Start = ImVec2(otherCenterX, std::min(pos.y, otherPos.y));
            line.m_End = ImVec2(otherCenterX, std::max(pos.y + size.y, otherPos.y + otherH));
            line.m_Horizontal = false;
            line.m_Alpha = 1.0f - fabsf(elemCenterX - otherCenterX) / threshold;
            m_SnapLines.push_back(line);
        }
        
        if(fabsf(elemCenterY - otherCenterY) < threshold)
        {
            SSnapLine line;
            line.m_Start = ImVec2(std::min(pos.x, otherPos.x), otherCenterY);
            line.m_End = ImVec2(std::max(pos.x + size.x, otherPos.x + otherW), otherCenterY);
            line.m_Horizontal = true;
            line.m_Alpha = 1.0f - fabsf(elemCenterY - otherCenterY) / threshold;
            m_SnapLines.push_back(line);
        }
        
        if(fabsf(pos.x - otherPos.x) < threshold)
        {
            SSnapLine line;
            line.m_Start = ImVec2(otherPos.x, std::min(pos.y, otherPos.y) - 5);
            line.m_End = ImVec2(otherPos.x, std::max(pos.y + size.y, otherPos.y + otherH) + 5);
            line.m_Horizontal = false;
            line.m_Alpha = 1.0f - fabsf(pos.x - otherPos.x) / threshold;
            m_SnapLines.push_back(line);
        }
        
        if(fabsf((pos.x + size.x) - (otherPos.x + otherW)) < threshold)
        {
            SSnapLine line;
            line.m_Start = ImVec2(otherPos.x + otherW, std::min(pos.y, otherPos.y) - 5);
            line.m_End = ImVec2(otherPos.x + otherW, std::max(pos.y + size.y, otherPos.y + otherH) + 5);
            line.m_Horizontal = false;
            line.m_Alpha = 1.0f - fabsf((pos.x + size.x) - (otherPos.x + otherW)) / threshold;
            m_SnapLines.push_back(line);
        }
    }
}

ImVec2 CHudEditor::ApplySnap(SHudElementData &elem, ImVec2 pos, ImVec2 size)
{
    float threshold = (float)g_Config.m_shalawaHudSnapThreshold;
    float elemCenterX = pos.x + size.x / 2;
    float elemCenterY = pos.y + size.y / 2;
    
    if(fabsf(elemCenterX - m_ScreenCenter.x) < threshold)
        pos.x = m_ScreenCenter.x - size.x / 2;
    
    if(fabsf(elemCenterY - m_ScreenCenter.y) < threshold)
        pos.y = m_ScreenCenter.y - size.y / 2;
    
    if(fabsf(pos.x) < threshold)
        pos.x = 0;
    
    if(fabsf(pos.x + size.x - m_ScreenSize.x) < threshold)
        pos.x = m_ScreenSize.x - size.x;
    
    if(fabsf(pos.y) < threshold)
        pos.y = 0;
    
    if(fabsf(pos.y + size.y - m_ScreenSize.y) < threshold)
        pos.y = m_ScreenSize.y - size.y;
    
    for(size_t i = 0; i < m_Elements.size(); i++)
    {
        if(&m_Elements[i] == &elem)
            continue;
        
        float scale = (float)*m_Elements[i].m_pScale / 100.0f;
        float otherW = m_Elements[i].m_BaseWidth * scale;
        float otherH = m_Elements[i].m_BaseHeight * scale;
        ImVec2 otherPos = AnchorToScreen(*m_Elements[i].m_pAnchor, (float)*m_Elements[i].m_pX,
                                          (float)*m_Elements[i].m_pY, otherW, otherH);
        
        float otherCenterX = otherPos.x + otherW / 2;
        float otherCenterY = otherPos.y + otherH / 2;
        elemCenterX = pos.x + size.x / 2;
        elemCenterY = pos.y + size.y / 2;
        
        if(fabsf(elemCenterX - otherCenterX) < threshold)
            pos.x = otherCenterX - size.x / 2;
        
        if(fabsf(elemCenterY - otherCenterY) < threshold)
            pos.y = otherCenterY - size.y / 2;
        
        if(fabsf(pos.x - otherPos.x) < threshold)
            pos.x = otherPos.x;
        
        if(fabsf((pos.x + size.x) - (otherPos.x + otherW)) < threshold)
            pos.x = otherPos.x + otherW - size.x;
    }
    
    return pos;
}

ImVec2 CHudEditor::GetElementPosition(EHudElement type)
{
    if((int)type >= (int)m_Elements.size())
        return ImVec2(0, 0);
    
    SHudElementData &elem = m_Elements[type];
    if(!elem.m_pScale || !elem.m_pX || !elem.m_pY || !elem.m_pAnchor)
        return ImVec2(0, 0);
    float scale = (float)*elem.m_pScale / 100.0f;
    float w = elem.m_BaseWidth * scale;
    float h = elem.m_BaseHeight * scale;
    
    return AnchorToScreen(*elem.m_pAnchor, (float)*elem.m_pX, (float)*elem.m_pY, w, h);
}

ImVec2 CHudEditor::GetElementSize(EHudElement type)
{
    if((int)type >= (int)m_Elements.size())
        return ImVec2(100, 30);
    
    SHudElementData &elem = m_Elements[type];
    if(!elem.m_pScale)
        return ImVec2(100, 30);
    float scale = (float)*elem.m_pScale / 100.0f;
    return ImVec2(elem.m_BaseWidth * scale, elem.m_BaseHeight * scale);
}

float CHudEditor::GetElementScale(EHudElement type)
{
    if((int)type >= (int)m_Elements.size())
        return 1.0f;
    
    if(!m_Elements[type].m_pScale)
        return 1.0f;
    return (float)*m_Elements[type].m_pScale / 100.0f;
}

void CHudEditor::DrawBlurredShadow(ImDrawList *d, ImVec2 pos, ImVec2 size, int range, ImVec4 color)
{
    int step = range > 6 ? 3 : (range > 3 ? 2 : 1);
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

bool CHudEditor::IsInsideElement(ImVec2 mousePos, ImVec2 elemPos, ImVec2 elemSize)
{
    return mousePos.x >= elemPos.x && mousePos.x <= elemPos.x + elemSize.x &&
           mousePos.y >= elemPos.y && mousePos.y <= elemPos.y + elemSize.y;
}

int CHudEditor::GetResizeCorner(ImVec2 mousePos, ImVec2 elemPos, ImVec2 elemSize)
{
    float handleSize = 10.0f;
    
    if(mousePos.x >= elemPos.x + elemSize.x - handleSize && 
       mousePos.y >= elemPos.y + elemSize.y - handleSize &&
       mousePos.x <= elemPos.x + elemSize.x + handleSize &&
       mousePos.y <= elemPos.y + elemSize.y + handleSize)
        return 3;
    
    if(mousePos.x >= elemPos.x - handleSize && 
       mousePos.y >= elemPos.y + elemSize.y - handleSize &&
       mousePos.x <= elemPos.x + handleSize &&
       mousePos.y <= elemPos.y + elemSize.y + handleSize)
        return 2;
    
    if(mousePos.x >= elemPos.x + elemSize.x - handleSize && 
       mousePos.y >= elemPos.y - handleSize &&
       mousePos.x <= elemPos.x + elemSize.x + handleSize &&
       mousePos.y <= elemPos.y + handleSize)
        return 1;
    
    if(mousePos.x >= elemPos.x - handleSize && 
       mousePos.y >= elemPos.y - handleSize &&
       mousePos.x <= elemPos.x + handleSize &&
       mousePos.y <= elemPos.y + handleSize)
        return 0;
    
    return -1;
}

void CHudEditor::HandleMouseInput()
{
    ANTI_DISASM_BLOCK();
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;
    
    
    bool mouseDown = io.MouseDown[0];
    bool mouseClicked = io.MouseClicked[0];
    
    if(!io.MouseDown[0])
    {
        for(auto &elem : m_Elements)
        {
            if(!elem.m_pScale || !elem.m_pX || !elem.m_pY || !elem.m_pAnchor)
            {
                elem.m_Dragging = false;
                elem.m_Resizing = false;
                continue;
            }
            if(elem.m_Dragging || elem.m_Resizing)
            {
                float scale = (float)*elem.m_pScale / 100.0f;
                float w = elem.m_BaseWidth * scale;
                float h = elem.m_BaseHeight * scale;
                ImVec2 pos = AnchorToScreen(*elem.m_pAnchor, (float)*elem.m_pX, (float)*elem.m_pY, w, h);
                
                int newAnchor;
                float newX, newY;
                ScreenToAnchor(pos, w, h, newAnchor, newX, newY);
                
                *elem.m_pAnchor = newAnchor;
                *elem.m_pX = (int)newX;
                *elem.m_pY = (int)newY;
            }
            elem.m_Dragging = false;
            elem.m_Resizing = false;
        }
        return;
    }
    
    for(auto &elem : m_Elements)
    {
        if(!elem.m_pScale || !elem.m_pX || !elem.m_pY || !elem.m_pAnchor)
            continue;
        if(elem.m_Resizing)
        {
            float scale = (float)*elem.m_pScale / 100.0f;
            float w = elem.m_BaseWidth * scale;
            float h = elem.m_BaseHeight * scale;
            ImVec2 pos = AnchorToScreen(*elem.m_pAnchor, (float)*elem.m_pX, (float)*elem.m_pY, w, h);
            
            float newScale = scale;
            
            if(elem.m_ResizeCorner == 3)
            {
                float deltaX = mousePos.x - (pos.x + w);
                float deltaY = mousePos.y - (pos.y + h);
                float delta = (deltaX + deltaY) / 2.0f;
                newScale = scale + delta / 100.0f;
            }
            
            newScale = std::clamp(newScale, 0.5f, 2.0f);
            *elem.m_pScale = (int)(newScale * 100.0f);
            
            return;
        }
        
        if(elem.m_Dragging)
        {
            float scale = (float)*elem.m_pScale / 100.0f;
            float w = elem.m_BaseWidth * scale;
            float h = elem.m_BaseHeight * scale;
            
            ImVec2 newPos(mousePos.x + elem.m_DragOffsetX, mousePos.y + elem.m_DragOffsetY);
            newPos = ApplySnap(elem, newPos, ImVec2(w, h));
            UpdateSnapLines(elem, newPos, ImVec2(w, h));
            
            int newAnchor;
            float newX, newY;
            ScreenToAnchor(newPos, w, h, newAnchor, newX, newY);
            
            *elem.m_pAnchor = newAnchor;
            *elem.m_pX = (int)newX;
            *elem.m_pY = (int)newY;
            
            return;
        }
    }
    
    if(io.MouseClicked[0])
    {
        m_SelectedElement = -1;
        
        for(int i = (int)m_Elements.size() - 1; i >= 0; i--)
        {
            SHudElementData &elem = m_Elements[i];
            float scale = (float)*elem.m_pScale / 100.0f;
            float w = elem.m_BaseWidth * scale;
            float h = elem.m_BaseHeight * scale;
            ImVec2 pos = AnchorToScreen(*elem.m_pAnchor, (float)*elem.m_pX, (float)*elem.m_pY, w, h);
            
            int corner = GetResizeCorner(mousePos, pos, ImVec2(w, h));
            if(corner >= 0)
            {
                elem.m_Resizing = true;
                elem.m_ResizeCorner = corner;
                m_SelectedElement = i;
                return;
            }
            
            if(IsInsideElement(mousePos, pos, ImVec2(w, h)))
            {
                elem.m_Dragging = true;
                elem.m_DragOffsetX = pos.x - mousePos.x;
                elem.m_DragOffsetY = pos.y - mousePos.y;
                m_SelectedElement = i;
                return;
            }
        }
    }
}

void CHudEditor::HandleKeyboardInput()
{
    ANTI_DISASM_BLOCK();
    if(m_SelectedElement < 0 || m_SelectedElement >= (int)m_Elements.size())
        return;
    
    SHudElementData &elem = m_Elements[m_SelectedElement];
    
    ImGuiIO &io = ImGui::GetIO();
    float step = io.KeyShift ? 10.0f : 1.0f;
    
    if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
        *elem.m_pX -= (int)step;
    if(ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        *elem.m_pX += (int)step;
    if(ImGui::IsKeyPressed(ImGuiKey_UpArrow))
        *elem.m_pY -= (int)step;
    if(ImGui::IsKeyPressed(ImGuiKey_DownArrow))
        *elem.m_pY += (int)step;
    
    if(ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))
    {
        *elem.m_pX = 0;
        *elem.m_pY = 0;
        *elem.m_pScale = 100;
    }
}

void CHudEditor::Update()
{
    ANTI_DISASM_BLOCK();
    if(!m_Initialized)
        return;
    
    m_Active = (g_Config.m_shalawaHudEditor != 0);

    for(auto &elem : m_Elements)
    {
        if(!elem.m_pScale || !elem.m_pX || !elem.m_pY || !elem.m_pAnchor)
            continue;
        if(*elem.m_pScale <= 0)
            *elem.m_pScale = 100;
    }
    
    float currentTime = (float)SDL_GetTicks() / OBF_F(1000);
    m_DeltaTime = std::clamp(currentTime - m_LastTime, 0.0001f, 0.1f);
    m_LastTime = currentTime;
    m_Time += m_DeltaTime;
    
    ImGuiIO &io = ImGui::GetIO();
    m_ScreenSize = io.DisplaySize;
    m_ScreenCenter = ImVec2(m_ScreenSize.x / 2, m_ScreenSize.y / 2);
    
    m_HoveredElement = -1;
    for(int i = (int)m_Elements.size() - 1; i >= 0; i--)
    {
        SHudElementData &elem = m_Elements[i];
        float scale = (float)*elem.m_pScale / 100.0f;
        float w = elem.m_BaseWidth * scale;
        float h = elem.m_BaseHeight * scale;
        ImVec2 pos = AnchorToScreen(*elem.m_pAnchor, (float)*elem.m_pX, (float)*elem.m_pY, w, h);
        
        if(IsInsideElement(io.MousePos, pos, ImVec2(w, h)))
        {
            m_HoveredElement = i;
            break;
        }
    }
    
    for(int i = 0; i < (int)m_Elements.size(); i++)
    {
        float hoverTarget = (m_HoveredElement == i) ? 1.0f : 0.0f;
        float selectTarget = (m_SelectedElement == i) ? 1.0f : 0.0f;
        m_Elements[i].m_HoverAnim = Lerp(m_Elements[i].m_HoverAnim, hoverTarget, m_DeltaTime * 10.0f);
        m_Elements[i].m_SelectAnim = Lerp(m_Elements[i].m_SelectAnim, selectTarget, m_DeltaTime * 10.0f);
    }
}

void CHudEditor::RenderSnapLines()
{
    ANTI_DISASM_BLOCK();
    ImDrawList *d = ImGui::GetBackgroundDrawList();
    
    for(const auto &line : m_SnapLines)
    {
        ImVec4 color = line.m_Horizontal ? 
            ImVec4(0.2f, 0.8f, 1.0f, line.m_Alpha * 0.8f) :
            ImVec4(1.0f, 0.4f, 0.6f, line.m_Alpha * 0.8f);
        
        ImU32 col = ImGui::ColorConvertFloat4ToU32(color);
        d->AddLine(line.m_Start, line.m_End, col, 1.0f);
        
        ImVec4 glowColor = color;
        glowColor.w *= 0.3f;
        d->AddLine(ImVec2(line.m_Start.x - 1, line.m_Start.y - 1),
                   ImVec2(line.m_End.x - 1, line.m_End.y - 1),
                   ImGui::ColorConvertFloat4ToU32(glowColor), 3.0f);
    }
}

void CHudEditor::RenderElementOutline(SHudElementData &elem, ImVec2 pos, ImVec2 size)
{
    ANTI_DISASM_BLOCK();
    ImDrawList *d = ImGui::GetBackgroundDrawList();
    
    float hoverAlpha = elem.m_HoverAnim * 0.3f + elem.m_SelectAnim * 0.5f;
    
    ImVec4 accentColor = HSVtoRGB(fmodf(m_Time * 0.1f, 1.0f), 0.7f, 0.9f);
    accentColor.w = 0.1f + hoverAlpha;
    
    d->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                     ImGui::ColorConvertFloat4ToU32(accentColor), 4.0f);
    
    ImVec4 borderColor = accentColor;
    borderColor.w = 0.5f + elem.m_SelectAnim * 0.5f;
    
    float borderWidth = 1.0f + elem.m_SelectAnim * 1.0f;
    d->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
               ImGui::ColorConvertFloat4ToU32(borderColor), 4.0f, 0, borderWidth);
    
    if(elem.m_SelectAnim > 0.1f)
    {
        float dashOffset = fmodf(m_Time * 50.0f, 20.0f);
        ImVec4 dashColor = borderColor;
        dashColor.w = elem.m_SelectAnim * 0.8f;
        
        for(float i = dashOffset; i < size.x; i += 20.0f)
        {
            float dashLen = std::min(10.0f, size.x - i);
            d->AddLine(ImVec2(pos.x + i, pos.y), ImVec2(pos.x + i + dashLen, pos.y),
                       ImGui::ColorConvertFloat4ToU32(dashColor), 2.0f);
            d->AddLine(ImVec2(pos.x + i, pos.y + size.y), ImVec2(pos.x + i + dashLen, pos.y + size.y),
                       ImGui::ColorConvertFloat4ToU32(dashColor), 2.0f);
        }
    }
}

void CHudEditor::RenderResizeHandles(SHudElementData &elem, ImVec2 pos, ImVec2 size)
{
    ANTI_DISASM_BLOCK();
    if(elem.m_SelectAnim < 0.1f)
        return;
    
    ImDrawList *d = ImGui::GetBackgroundDrawList();
    
    float handleSize = 8.0f * elem.m_SelectAnim;
    ImVec4 handleColor = HSVtoRGB(fmodf(m_Time * 0.1f, 1.0f), 0.7f, 1.0f);
    handleColor.w = elem.m_SelectAnim;
    
    ImVec2 corners[4] = {
        pos,
        ImVec2(pos.x + size.x, pos.y),
        ImVec2(pos.x, pos.y + size.y),
        ImVec2(pos.x + size.x, pos.y + size.y)
    };
    
    for(int i = 0; i < 4; i++)
    {
        d->AddRectFilled(ImVec2(corners[i].x - handleSize / 2, corners[i].y - handleSize / 2),
                         ImVec2(corners[i].x + handleSize / 2, corners[i].y + handleSize / 2),
                         ImGui::ColorConvertFloat4ToU32(handleColor), 2.0f);
        
        ImVec4 borderCol = handleColor;
        borderCol.x = borderCol.y = borderCol.z = 1.0f;
        d->AddRect(ImVec2(corners[i].x - handleSize / 2, corners[i].y - handleSize / 2),
                   ImVec2(corners[i].x + handleSize / 2, corners[i].y + handleSize / 2),
                   ImGui::ColorConvertFloat4ToU32(borderCol), 2.0f);
    }
}

void CHudEditor::RenderAnchorPreview(SHudElementData &elem)
{
    ANTI_DISASM_BLOCK();
    if(elem.m_SelectAnim < 0.1f)
        return;
    
    ImDrawList *d = ImGui::GetBackgroundDrawList();
    
    ImVec2 anchorPositions[9] = {
        ImVec2(30, 30),
        ImVec2(m_ScreenSize.x - 80, 30),
        ImVec2(30, m_ScreenSize.y - 30),
        ImVec2(m_ScreenSize.x - 80, m_ScreenSize.y - 30),
        ImVec2(m_ScreenCenter.x - 40, 30),
        ImVec2(m_ScreenCenter.x - 40, m_ScreenSize.y - 30),
        ImVec2(30, m_ScreenCenter.y),
        ImVec2(m_ScreenSize.x - 80, m_ScreenCenter.y),
        ImVec2(m_ScreenCenter.x - 40, m_ScreenCenter.y)
    };
    
    int currentAnchor = *elem.m_pAnchor;
    
    for(int i = 0; i < 9; i++)
    {
        float alpha = (i == currentAnchor) ? elem.m_SelectAnim : elem.m_SelectAnim * 0.3f;
        ImVec4 color = (i == currentAnchor) ? 
            HSVtoRGB(fmodf(m_Time * 0.1f, 1.0f), 0.7f, 0.9f) :
            ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        color.w = alpha;
        
        float size = (i == currentAnchor) ? 8.0f : 5.0f;
        d->AddCircleFilled(anchorPositions[i], size, ImGui::ColorConvertFloat4ToU32(color));
    }
}

void CHudEditor::RenderEditorUI()
{
    ANTI_DISASM_BLOCK();
    ImDrawList *d = ImGui::GetBackgroundDrawList();
    ImGuiIO &io = ImGui::GetIO();

    float panelW = 230.0f;
    float headerH = 42.0f;
    float itemH = 32.0f;
    float footerH = 28.0f;
    float panelH = headerH + m_Elements.size() * itemH + footerH + 16.0f;

    ImVec2 panelPos(m_ScreenSize.x - panelW - 20, 40);
    ImVec4 accentColor = HSVtoRGB(fmodf(m_Time * 0.1f, 1.0f), 0.7f, 0.9f);

    DrawBlurredShadow(d, panelPos, ImVec2(panelW, panelH), 10, ImVec4(0, 0, 0, 0.5f));
    d->AddRectFilled(panelPos, ImVec2(panelPos.x + panelW, panelPos.y + panelH),
                     IM_COL32(18, 18, 22, 245), 8.0f);
    d->AddRectFilled(panelPos, ImVec2(panelPos.x + panelW, panelPos.y + 2),
                     ImGui::ColorConvertFloat4ToU32(accentColor),
                     8.0f, ImDrawFlags_RoundCornersTop);

    ImVec2 titleSize = ImGui::CalcTextSize("HUD Editor");
    d->AddText(ImVec2(panelPos.x + (panelW - titleSize.x) / 2, panelPos.y + 14),
               IM_COL32(255, 255, 255, 255), "HUD Editor");

    d->AddLine(ImVec2(panelPos.x + 12, panelPos.y + headerH - 2),
               ImVec2(panelPos.x + panelW - 12, panelPos.y + headerH - 2),
               IM_COL32(40, 40, 50, 200));

    float y = panelPos.y + headerH;

    for(int i = 0; i < (int)m_Elements.size(); i++)
    {
        SHudElementData &elem = m_Elements[i];
        bool isSelected = (m_SelectedElement == i);

        ImVec2 itemMin(panelPos.x + 8, y + 2);
        ImVec2 itemMax(panelPos.x + panelW - 8, y + itemH - 2);

        bool itemHovered = (io.MousePos.x >= itemMin.x && io.MousePos.x <= itemMax.x &&
                            io.MousePos.y >= itemMin.y && io.MousePos.y <= itemMax.y);

        if(itemHovered && io.MouseClicked[0])
            m_SelectedElement = i;

        ImVec4 bgColor;
        if(isSelected)
            bgColor = ImVec4(accentColor.x * 0.2f, accentColor.y * 0.2f,
                             accentColor.z * 0.2f, 0.6f);
        else if(itemHovered)
            bgColor = ImVec4(0.15f, 0.15f, 0.18f, 0.5f);
        else
            bgColor = ImVec4(0.08f, 0.08f, 0.10f, 0.3f);

        d->AddRectFilled(itemMin, itemMax,
                         ImGui::ColorConvertFloat4ToU32(bgColor), 4.0f);

        if(isSelected)
        {
            ImVec4 borderCol = accentColor;
            borderCol.w = 0.4f;
            d->AddRect(itemMin, itemMax,
                       ImGui::ColorConvertFloat4ToU32(borderCol), 4.0f);
        }

        float dotRadius = 3.0f;
        ImVec4 dotCol = HSVtoRGB(fmodf(m_Time * 0.1f + i * 0.15f, 1.0f), 0.7f, 0.9f);
        d->AddCircleFilled(ImVec2(itemMin.x + 12, itemMin.y + (itemH - 4) / 2),
                           dotRadius, ImGui::ColorConvertFloat4ToU32(dotCol));

        d->AddText(ImVec2(itemMin.x + 22,
                          itemMin.y + (itemH - 4 - ImGui::CalcTextSize("A").y) / 2),
                   IM_COL32(255, 255, 255, isSelected ? 255 : 190),
                   elem.m_Name.c_str());

        char scaleText[16];
        snprintf(scaleText, sizeof(scaleText), "%d%%", *elem.m_pScale);
        ImVec2 scaleSize = ImGui::CalcTextSize(scaleText);
        d->AddText(ImVec2(itemMax.x - scaleSize.x - 8,
                          itemMin.y + (itemH - 4 - scaleSize.y) / 2),
                   IM_COL32(100, 100, 120, 180), scaleText);

        y += itemH;
    }

    float footerY = y + 6;
    d->AddLine(ImVec2(panelPos.x + 12, footerY),
               ImVec2(panelPos.x + panelW - 12, footerY),
               IM_COL32(40, 40, 50, 200));

    d->AddText(ImVec2(panelPos.x + 14, footerY + 8),
               IM_COL32(80, 80, 100, 160), "ESC close | Arrows nudge | DEL reset");
}

void CHudEditor::RenderElement(SHudElementData &elem)
{
    ANTI_DISASM_BLOCK();
    if(!elem.m_pScale || !elem.m_pX || !elem.m_pY || !elem.m_pAnchor)
        return;
    float scale = (float)*elem.m_pScale / 100.0f;
    float w = elem.m_BaseWidth * scale;
    float h = elem.m_BaseHeight * scale;
    ImVec2 pos = AnchorToScreen(*elem.m_pAnchor, (float)*elem.m_pX, (float)*elem.m_pY, w, h);
    
    if(elem.m_RenderFunc)
        elem.m_RenderFunc(pos, ImVec2(w, h), scale);
}

void CHudEditor::Render()
{
    ANTI_DISASM_BLOCK();
    if(!m_Initialized || !ImGui::GetCurrentContext())
        return;
    
    for(auto &elem : m_Elements)
    {
        RenderElement(elem);
    }
}

void CHudEditor::RenderEditorOverlay()
{
    ANTI_DISASM_BLOCK();
    if(!m_Active || !m_Initialized || !ImGui::GetCurrentContext())
        return;
    
    ImGuiIO &io = ImGui::GetIO();
    
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | 
                              ImGuiWindowFlags_NoResize | 
                              ImGuiWindowFlags_NoMove | 
                              ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoSavedSettings |
                              ImGuiWindowFlags_NoNav;
    
    ImGui::Begin("##HudEditorOverlay", nullptr, flags);
    ImGui::InvisibleButton("##HudEditorCapture", io.DisplaySize);
    ImGui::End();
    
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    
    ImDrawList *d = ImGui::GetBackgroundDrawList();
    d->AddRectFilled(ImVec2(0, 0), m_ScreenSize, IM_COL32(0, 0, 0, 100));
    
    float gridSize = 100.0f;
    for(float gx = gridSize; gx < m_ScreenSize.x; gx += gridSize)
        d->AddLine(ImVec2(gx, 0), ImVec2(gx, m_ScreenSize.y), IM_COL32(255, 255, 255, 8), 1.0f);
    for(float gy = gridSize; gy < m_ScreenSize.y; gy += gridSize)
        d->AddLine(ImVec2(0, gy), ImVec2(m_ScreenSize.x, gy), IM_COL32(255, 255, 255, 8), 1.0f);

    d->AddLine(ImVec2(m_ScreenSize.x / 3, 0), ImVec2(m_ScreenSize.x / 3, m_ScreenSize.y),
               IM_COL32(255, 255, 255, 15), 1.0f);
    d->AddLine(ImVec2(m_ScreenSize.x * 2 / 3, 0), ImVec2(m_ScreenSize.x * 2 / 3, m_ScreenSize.y),
               IM_COL32(255, 255, 255, 15), 1.0f);
    d->AddLine(ImVec2(0, m_ScreenSize.y / 3), ImVec2(m_ScreenSize.x, m_ScreenSize.y / 3),
               IM_COL32(255, 255, 255, 15), 1.0f);
    d->AddLine(ImVec2(0, m_ScreenSize.y * 2 / 3), ImVec2(m_ScreenSize.x, m_ScreenSize.y * 2 / 3),
               IM_COL32(255, 255, 255, 15), 1.0f);

    d->AddLine(ImVec2(m_ScreenCenter.x, 0), ImVec2(m_ScreenCenter.x, m_ScreenSize.y),
               IM_COL32(255, 255, 255, 40), 1.0f);
    d->AddLine(ImVec2(0, m_ScreenCenter.y), ImVec2(m_ScreenSize.x, m_ScreenCenter.y),
               IM_COL32(255, 255, 255, 40), 1.0f);
    
    for(auto &elem : m_Elements)
    {
        float scale = (float)*elem.m_pScale / 100.0f;
        float w = elem.m_BaseWidth * scale;
        float h = elem.m_BaseHeight * scale;
        ImVec2 pos = AnchorToScreen(*elem.m_pAnchor, (float)*elem.m_pX, (float)*elem.m_pY, w, h);
        
        RenderElementOutline(elem, pos, ImVec2(w, h));
        RenderResizeHandles(elem, pos, ImVec2(w, h));
    }
    
    RenderSnapLines();
    
    if(m_SelectedElement >= 0 && m_SelectedElement < (int)m_Elements.size())
        RenderAnchorPreview(m_Elements[m_SelectedElement]);
    
    RenderEditorUI();
    
    HandleMouseInput();
    HandleKeyboardInput();
}

bool CHudEditor::ProcessEvent(void *pEvent)
{
    ANTI_DISASM_BLOCK();
    if(!m_Active || !pEvent)
        return false;
    
    SDL_Event *event = (SDL_Event *)pEvent;
    
    if(event->type == SDL_KEYDOWN)
    {
        if(event->key.keysym.scancode == SDL_SCANCODE_ESCAPE)
        {
            m_Active = false;
            g_Config.m_shalawaHudEditor = 0;
            SDL_SetRelativeMouseMode(SDL_TRUE);
            SDL_ShowCursor(SDL_DISABLE);
            return true;
        }
        
        if(event->key.keysym.scancode == SDL_SCANCODE_INSERT)
        {
            m_Active = false;
            g_Config.m_shalawaHudEditor = 0;
            return false;
        }
    }
    
    return true;
}

CShalawaHud::CShalawaHud()
{
    ANTI_DISASM_BLOCK();
    m_pGameClient = nullptr;
    m_pWatermark = nullptr;
    m_Initialized = false;
    m_DeltaTime = 0.0f;
    m_LastTime = 0.0f;
    m_Time = 0.0f;
    m_GradientHue = 0.0f;
    m_Fps = 0;
    m_FrameCount = 0;
    m_FpsTimer = 0.0f;
    m_Millis = 0;
    m_WelcomeMessageAlpha = 1.0f;
    m_WelcomeMessageTime = 0.0f;
    m_NotificationTime = 0.0f;
    m_SpeedHistoryIdx = 0;
    m_SpeedHistoryTimer = 0.0f;
    m_MaxSpeed = 0.0f;
    m_SmoothedSpeed = 0.0f;
    for(int i = 0; i < 128; i++)
        m_SpeedHistory[i] = 0.0f;
    for(int i = 0; i < 6; i++)
        m_KeyStates[i] = 0.0f;
}

CShalawaHud &CShalawaHud::Instance()
{
    static CShalawaHud s_Instance;
    return s_Instance;
}

void CShalawaHud::Init(CGameClient *pGameClient)
{
    ANTI_DISASM_BLOCK();
    m_pGameClient = pGameClient;
    m_Initialized = true;
    m_LastTime = (float)SDL_GetTicks() / 1000.0f;
    
    m_Editor.Init(pGameClient);
    SetupElementRenderers();
    AddNotification("Shalawa loaded", true);
}

void CShalawaHud::SetupElementRenderers()
{
    ANTI_DISASM_BLOCK();
    m_Editor.RegisterElement(HUD_ARRAYLIST, (const char*)AY_OBFUSCATE("Arraylist"),
        &g_Config.m_shalawaHudArraylistX, &g_Config.m_shalawaHudArraylistY,
        &g_Config.m_shalawaHudArraylistAnchor, &g_Config.m_shalawaHudArraylistScale,
        140.0f, 200.0f,
        [this](ImVec2 pos, ImVec2 size, float scale) { RenderArraylist(pos, size, scale); });

    m_Editor.RegisterElement(HUD_KEYBINDS, (const char*)AY_OBFUSCATE("Keybinds"),
        &g_Config.m_shalawaHudKeybindsX, &g_Config.m_shalawaHudKeybindsY,
        &g_Config.m_shalawaHudKeybindsAnchor, &g_Config.m_shalawaHudKeybindsScale,
        105.0f, 105.0f,
        [this](ImVec2 pos, ImVec2 size, float scale) { RenderKeybinds(pos, size, scale); });

    m_Editor.RegisterElement(HUD_SPEEDOMETER, (const char*)AY_OBFUSCATE("Speedometer"),
        &g_Config.m_shalawaHudSpeedX, &g_Config.m_shalawaHudSpeedY,
        &g_Config.m_shalawaHudSpeedAnchor, &g_Config.m_shalawaHudSpeedScale,
        110.0f, 85.0f,
        [this](ImVec2 pos, ImVec2 size, float scale) { RenderSpeedometer(pos, size, scale); });

    m_Editor.RegisterElement(HUD_NOTIFICATIONS, (const char*)AY_OBFUSCATE("Notifications"),
        &g_Config.m_shalawaHudNotifyX, &g_Config.m_shalawaHudNotifyY,
        &g_Config.m_shalawaHudNotifyAnchor, &g_Config.m_shalawaHudNotifyScale,
        180.0f, 210.0f,
        [this](ImVec2 pos, ImVec2 size, float scale) { RenderNotifications(pos, size, scale); });

    m_Editor.RegisterElement(HUD_IOF, (const char*)AY_OBFUSCATE("IOF Player"),
        &g_Config.m_shalawaHudIofX, &g_Config.m_shalawaHudIofY,
        &g_Config.m_shalawaHudIofAnchor, &g_Config.m_shalawaHudIofScale,
        200.0f, 70.0f,
        [this](ImVec2 pos, ImVec2 size, float scale) { RenderIOF(pos, size, scale); });
}

void CShalawaHud::RenderIOF(ImVec2 pos, ImVec2 size, float scale)
{
    if(!g_Config.m_shalawaIOF)
        return;

    CIOF &iof = CIOF::Instance();

    static bool s_IOFInitialized = false;
    if(!s_IOFInitialized)
    {
        iof.Init();
        s_IOFInitialized = true;
    }

    iof.Update(m_DeltaTime);
    iof.Render(pos, size, scale, g_Config.m_shalawaIOFStyle);
}

float CShalawaHud::Lerp(float a, float b, float t)
{
    return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
}

ImVec4 CShalawaHud::HSVtoRGB(float h, float s, float v)
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

ImU32 CShalawaHud::GetAccent(float alpha)
{
    ImVec4 col = HSVtoRGB(m_GradientHue, 0.65f, 0.95f);
    col.w = alpha;
    return ImGui::ColorConvertFloat4ToU32(col);
}

void CShalawaHud::DrawBlurredShadow(ImDrawList *d, ImVec2 pos, ImVec2 size, int range, ImVec4 color)
{
    int step = range > 6 ? 3 : (range > 3 ? 2 : 1);
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

void CShalawaHud::UpdateTime()
{
    float currentTime = (float)SDL_GetTicks() / OBF_F(1000);
    m_DeltaTime = std::clamp(currentTime - m_LastTime, 0.0001f, 0.1f);
    m_LastTime = currentTime;
    m_Millis = SDL_GetTicks();
    
    m_FrameCount++;
    m_FpsTimer += m_DeltaTime;
    if(m_FpsTimer >= 0.5f)
    {
        m_Fps = (m_FpsTimer > 0.0f) ? (int)(m_FrameCount / m_FpsTimer) : 0;
        m_FrameCount = 0;
        m_FpsTimer = 0.0f;
    }
    
    m_Time += m_DeltaTime;
    m_GradientHue += m_DeltaTime * 0.1f;
    if(m_GradientHue > 1.0f)
        m_GradientHue -= 1.0f;
}

float CShalawaHud::EaseOutBack(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    float c1 = 1.70158f;
    float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
}

float CShalawaHud::SmoothStep(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void CShalawaHud::AddNotification(const std::string &text, bool enabled)
{
    if(!m_Initialized)
        return;
    SHudNotification notif;
    notif.m_Text = text;
    notif.m_Enabled = enabled;
    notif.m_SpawnTime = (float)SDL_GetTicks() / 1000.0f;
    notif.m_Progress = 0.0f;
    notif.m_SlideAnim = 0.0f;
    m_HudNotifications.push_back(notif);
    while(m_HudNotifications.size() > 5)
        m_HudNotifications.erase(m_HudNotifications.begin());
}

void CShalawaHud::UpdateModules()
{
    ANTI_DISASM_BLOCK();
    auto setMod = [this](const char *name, bool enabled, const char *suffix = "") {
        auto it = m_Modules.find(name);
        if(it == m_Modules.end())
        {
            SHudModule mod;
            mod.m_Name = name;
            mod.m_Suffix = suffix;
            mod.m_Enabled = enabled;
            mod.m_PrevEnabled = enabled;
            mod.m_Animation = enabled ? 0.0f : 1.0f;
            mod.m_Alpha = 0.0f;
            m_Modules[name] = mod;
        }
        else
        {
            if(it->second.m_PrevEnabled != enabled)
            {
                AddNotification(name, enabled);
                it->second.m_PrevEnabled = enabled;
            }
            it->second.m_Enabled = enabled;
            it->second.m_Suffix = suffix;
        }
    };
    {
        const char *avoidSuffix = "";
        switch(g_Config.m_shalawaAvoidMode)
        {
            case 1: avoidSuffix = (const char*)AY_OBFUSCATE("Legit"); break;
            case 2: avoidSuffix = (const char*)AY_OBFUSCATE("Blatant"); break;
            case 3: avoidSuffix = (const char*)AY_OBFUSCATE("AI Bot"); break;
        }
        setMod((const char*)AY_OBFUSCATE("Avoid"), g_Config.m_shalawaAvoidMode > 0, avoidSuffix);
    }
    setMod((const char*)AY_OBFUSCATE("Auto Answer"), g_Config.m_shalawaAutoAnswer != 0);
    setMod((const char*)AY_OBFUSCATE("Chat Spam"), g_Config.m_shalawaChatSpam != 0);
    setMod((const char*)AY_OBFUSCATE("TAS"), g_Config.m_shalawaPlaying != 0);
    setMod((const char*)AY_OBFUSCATE("Avoid Visuals"), g_Config.m_shalawaAvoidVisuals != 0);
    setMod((const char*)AY_OBFUSCATE("Auto Fire"), g_Config.m_shalawaAutoFire != 0);
    {
        const char *balSuffix = "";
        switch(g_Config.m_shalawaBalanceMode)
        {
            case 0: balSuffix = (const char*)AY_OBFUSCATE("Passive"); break;
            case 1: balSuffix = (const char*)AY_OBFUSCATE("Aggressive"); break;
            case 2: balSuffix = (const char*)AY_OBFUSCATE("Adaptive"); break;
            case 3: balSuffix = (const char*)AY_OBFUSCATE("Predictive"); break;
        }
        setMod((const char*)AY_OBFUSCATE("Balance Bot"), g_Config.m_shalawaBalanceBot != 0, balSuffix);
    }
    setMod((const char*)AY_OBFUSCATE("Hook Ride"), g_Config.m_shalawaHookRide != 0);
    setMod((const char*)AY_OBFUSCATE("Unfreeze Bot"), g_Config.m_shalawaUnfreezeBot != 0);
    setMod((const char*)AY_OBFUSCATE("Hide Cheat"), g_Config.m_shalawaHideCheat != 0);
    setMod((const char*)AY_OBFUSCATE("Hit Particles"), g_Config.m_shalawaHitParticles != 0);
    setMod((const char*)AY_OBFUSCATE("China Hat"), g_Config.m_shalawaChinaHat != 0);
    setMod((const char*)AY_OBFUSCATE("Watermark"), g_Config.m_shalawaWatermark != 0);
    setMod((const char*)AY_OBFUSCATE("Arraylist"), g_Config.m_shalawaArraylist != 0);
    setMod((const char*)AY_OBFUSCATE("Fast Input"), g_Config.m_ClFastInput != 0);
    setMod((const char*)AY_OBFUSCATE("Hide Chat Bubble"), g_Config.m_ClHideChatBubble != 0);
    setMod((const char*)AY_OBFUSCATE("HUD Editor"), g_Config.m_shalawaHudEditor != 0);
    setMod((const char*)AY_OBFUSCATE("Legit Avoid"), g_Config.m_shalawaLegitAvoidFreeze != 0);
    setMod((const char*)AY_OBFUSCATE("Blatant Avoid"), g_Config.m_shalawaAvoidFreeze != 0);
    setMod((const char*)AY_OBFUSCATE("Tail"), g_Config.m_shalawaTail != 0);
    setMod((const char*)AY_OBFUSCATE("Quick Stop"), g_Config.m_shalawaQuickStop != 0);
    setMod((const char*)AY_OBFUSCATE("Spoof"), g_Config.m_shalawaSpoofVersion != 0);
    setMod((const char*)AY_OBFUSCATE("SOCKS5"), g_Config.m_shalawaSocks5Enabled != 0);
    for(auto &pair : m_Modules)
    {
        float target = pair.second.m_Enabled ? 0.0f : 1.0f;
        pair.second.m_Animation = Lerp(pair.second.m_Animation, target, m_DeltaTime * 10.0f);
        float alphaTarget = pair.second.m_Enabled ? 1.0f : 0.0f;
        pair.second.m_Alpha = Lerp(pair.second.m_Alpha, alphaTarget, m_DeltaTime * 12.0f);
    }
}

void CShalawaHud::UpdateSpeedHistory()
{
    ANTI_DISASM_BLOCK();
    float speed = GetSpeed();
    m_SmoothedSpeed = Lerp(m_SmoothedSpeed, speed, m_DeltaTime * 8.0f);

    m_SpeedHistoryTimer += m_DeltaTime;
    if(m_SpeedHistoryTimer >= 0.05f)
    {
        m_SpeedHistory[m_SpeedHistoryIdx] = m_SmoothedSpeed;
        m_SpeedHistoryIdx = (m_SpeedHistoryIdx + 1) % 128;
        m_SpeedHistoryTimer = 0.0f;
    }

    if(speed > m_MaxSpeed)
        m_MaxSpeed = speed;
    else
        m_MaxSpeed = Lerp(m_MaxSpeed, speed, m_DeltaTime * 0.3f);
    if(m_MaxSpeed < 1.0f)
        m_MaxSpeed = 1.0f;
}

std::string CShalawaHud::GetTimeString()
{
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", t);
    return std::string(buf);
}

int CShalawaHud::GetPing()
{
    if(!m_Initialized || !m_pGameClient)
        return 0;
    if(!m_pGameClient->Client())
        return 0;
    if(m_pGameClient->Client()->State() != IClient::STATE_ONLINE)
        return 0;

    const CNetObj_PlayerInfo *pInfo = m_pGameClient->m_Snap.m_pLocalInfo;
    if(!pInfo)
        return 0;

    return pInfo->m_Latency;
}

float CShalawaHud::GetSpeed()
{
    if(!m_Initialized || !m_pGameClient)
        return 0.0f;
    if(!m_pGameClient->Client())
        return 0.0f;
    if(m_pGameClient->Client()->State() != IClient::STATE_ONLINE)
        return 0.0f;

    const CNetObj_Character *pChar = m_pGameClient->m_Snap.m_pLocalCharacter;
    if(!pChar)
        return 0.0f;

    float vx = pChar->m_VelX / 256.0f;
    float vy = pChar->m_VelY / 256.0f;
    return sqrtf(vx * vx + vy * vy);
}

std::string CShalawaHud::GetServerName()
{
    if(!m_pGameClient || !m_pGameClient->Client())
        return "Offline";
    
    if(m_pGameClient->Client()->State() != IClient::STATE_ONLINE)
        return "Offline";
    
    return "Server";
}

void CShalawaHud::RenderWatermark(ImVec2 pos, ImVec2 size, float scale)
{
    ANTI_DISASM_BLOCK();
    if(!g_Config.m_shalawaWatermark || !m_pWatermark)
        return;
    
    // Use CWatermark to render with the selected style
    m_pWatermark->RenderWatermarkAtPosition(pos, g_Config.m_shalawaWatermarkStyle);
}

void CShalawaHud::RenderArraylist(ImVec2 pos, ImVec2 size, float scale)
{
    ANTI_DISASM_BLOCK();
    if(!g_Config.m_shalawaArraylist || !ImGui::GetCurrentContext())
        return;

    std::vector<SHudModule*> sorted;
    for(auto &pair : m_Modules)
    {
        if(pair.second.m_Alpha > 0.01f)
            sorted.push_back(&pair.second);
    }

    if(sorted.empty())
        return;

    std::sort(sorted.begin(), sorted.end(), [](SHudModule *a, SHudModule *b) {
        float wA = ImGui::CalcTextSize(a->m_Name.c_str()).x;
        if(!a->m_Suffix.empty()) wA += 10.0f + ImGui::CalcTextSize(a->m_Suffix.c_str()).x;
        float wB = ImGui::CalcTextSize(b->m_Name.c_str()).x;
        if(!b->m_Suffix.empty()) wB += 10.0f + ImGui::CalcTextSize(b->m_Suffix.c_str()).x;
        return wA > wB;
    });

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(size.x, size.y), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus;

    if(ImGui::Begin("##shalawa_arraylist", nullptr, flags))
    {
        ImGui::SetWindowFontScale(scale);
        ImDrawList *d = ImGui::GetWindowDrawList();
        int style = g_Config.m_shalawaArraylistStyle;
        switch(style)
        {
            case 0: RenderArraylistSkeet(d, pos, size, sorted); break;
            case 1: RenderArraylistNeverlose(d, pos, size, sorted); break;
            case 2: RenderArraylistGamesense(d, pos, size, sorted); break;
            case 3: RenderArraylistCustom(d, pos, size, sorted); break;
            case 4: RenderArraylistChatGPT(d, pos, size, sorted); break;
            case 5: RenderArraylistClaude(d, pos, size, sorted); break;
            case 6: RenderArraylistDeepSeek(d, pos, size, sorted); break;
            case 7: RenderArraylistCloud(d, pos, size, sorted); break;
            case 8: RenderArraylistSingularity(d, pos, size, sorted); break;
            default: RenderArraylistCustom(d, pos, size, sorted); break;
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void CShalawaHud::RenderArraylistCloud(ImDrawList *d, ImVec2 pos, ImVec2 size,
                                        std::vector<SHudModule*> &sorted)
{
    const int total = (int)sorted.size();
    if(total <= 0)
        return;

    d->PushClipRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), true);

    // ════════════════════════════════════════════
    //              VISUAL CONSTANTS
    // ════════════════════════════════════════════
    
    const float fontH      = ImGui::GetTextLineHeight();
    const float itemH      = fontH + 12.0f;
    const float gapY       = 3.0f;
    const float padX       = 14.0f;
    const float headerH    = 36.0f;
    const float footerH    = 24.0f;
    const float rounding   = 8.0f;
    const float energyBarW = 3.5f;
    const float hexRadius  = 5.0f;

    // ════════════════════════════════════════════
    //              MEASURE CONTENT
    // ════════════════════════════════════════════
    
    float maxLabelW = 0.0f;
    int activeCount = 0;
    
    for(auto *mod : sorted)
    {
        if(mod->m_Enabled) activeCount++;
        
        float w = ImGui::CalcTextSize(mod->m_Name.c_str()).x;
        if(!mod->m_Suffix.empty())
            w += 10.0f + ImGui::CalcTextSize(mod->m_Suffix.c_str()).x + 16.0f;
        if(w > maxLabelW) maxLabelW = w;
    }

    float panelW = padX * 2.0f + energyBarW + 12.0f + maxLabelW + 10.0f;
    panelW = std::min(panelW, size.x);
    
    float listH = total * (itemH + gapY) - gapY;
    float panelH = headerH + listH + footerH + 8.0f;
    panelH = std::min(panelH, size.y);
    
    float panelX = pos.x + size.x - panelW;
    float panelY = pos.y;

    // ════════════════════════════════════════════
    //              BACKGROUND
    // ════════════════════════════════════════════

    // Glassmorphism background
    DrawBlurredShadow(d, ImVec2(panelX, panelY), ImVec2(panelW, panelH), 
        12, ImVec4(0.05f, 0.1f, 0.2f, 0.5f));
    
    // Main glass panel
    d->AddRectFilled(ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(8, 12, 20, 220), rounding);
    
    // Inner glow gradient (top to bottom)
    d->AddRectFilledMultiColor(
        ImVec2(panelX + 1, panelY + 1),
        ImVec2(panelX + panelW - 1, panelY + panelH * 0.4f),
        IM_COL32(40, 80, 120, 25),
        IM_COL32(40, 80, 120, 25),
        IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, 0));

    // Animated border gradient
    const int borderSegs = 16;
    for(int s = 0; s < borderSegs; s++)
    {
        float t0 = (float)s / borderSegs;
        float t1 = (float)(s + 1) / borderSegs;
        
        // Animated hue shift along border
        float hue0 = fmodf(m_GradientHue + t0 * 0.3f + sinf(m_Time + t0 * 6.28f) * 0.05f, 1.0f);
        float hue1 = fmodf(m_GradientHue + t1 * 0.3f + sinf(m_Time + t1 * 6.28f) * 0.05f, 1.0f);
        
        ImVec4 c0 = HSVtoRGB(hue0, 0.6f, 0.9f);
        ImVec4 c1 = HSVtoRGB(hue1, 0.6f, 0.9f);
        c0.w = 0.4f; c1.w = 0.4f;
        
        // Top border segment
        float x0 = panelX + t0 * panelW;
        float x1 = panelX + t1 * panelW;
        d->AddRectFilled(ImVec2(x0, panelY), ImVec2(x1, panelY + 1.5f),
            ImGui::ColorConvertFloat4ToU32(c0), 0, ImDrawFlags_RoundCornersTop);
    }

    // Corner accents (cyber corners)
    float cornerLen = 12.0f;
    ImU32 cornerCol = ImGui::ColorConvertFloat4ToU32(
        HSVtoRGB(m_GradientHue, 0.7f, 1.0f));
    
    // Top-left corner
    d->AddLine(ImVec2(panelX + 1, panelY + cornerLen),
        ImVec2(panelX + 1, panelY + 1), cornerCol, 1.5f);
    d->AddLine(ImVec2(panelX + 1, panelY + 1),
        ImVec2(panelX + cornerLen, panelY + 1), cornerCol, 1.5f);
    
    // Top-right corner
    d->AddLine(ImVec2(panelX + panelW - 1, panelY + cornerLen),
        ImVec2(panelX + panelW - 1, panelY + 1), cornerCol, 1.5f);
    d->AddLine(ImVec2(panelX + panelW - 1, panelY + 1),
        ImVec2(panelX + panelW - cornerLen, panelY + 1), cornerCol, 1.5f);
    
    // Bottom-left corner
    d->AddLine(ImVec2(panelX + 1, panelY + panelH - cornerLen),
        ImVec2(panelX + 1, panelY + panelH - 1), cornerCol, 1.5f);
    d->AddLine(ImVec2(panelX + 1, panelY + panelH - 1),
        ImVec2(panelX + cornerLen, panelY + panelH - 1), cornerCol, 1.5f);
    
    // Bottom-right corner
    d->AddLine(ImVec2(panelX + panelW - 1, panelY + panelH - cornerLen),
        ImVec2(panelX + panelW - 1, panelY + panelH - 1), cornerCol, 1.5f);
    d->AddLine(ImVec2(panelX + panelW - 1, panelY + panelH - 1),
        ImVec2(panelX + panelW - cornerLen, panelY + panelH - 1), cornerCol, 1.5f);

    // ════════════════════════════════════════════
    //                 HEADER
    // ════════════════════════════════════════════
    
    // Animated wave line under header
    float waveY = panelY + headerH - 0.5f;
    int waveStep = 12;
    for(int wx = 0; wx < (int)panelW; wx += waveStep)
    {
        float waveOffset0 = sinf(m_Time * 3.0f + wx * 0.08f) * 2.0f;
        float waveOffset1 = sinf(m_Time * 3.0f + (wx + waveStep) * 0.08f) * 2.0f;
        float waveAlpha = 0.3f + 0.2f * sinf(m_Time * 2.0f + wx * 0.05f);
        ImVec4 waveCol = HSVtoRGB(fmodf(m_GradientHue + wx / panelW * 0.2f, 1.0f), 0.6f, 1.0f);
        waveCol.w = waveAlpha;
        
        d->AddLine(
            ImVec2(panelX + wx, waveY + waveOffset0),
            ImVec2(panelX + wx + waveStep, waveY + waveOffset1),
            ImGui::ColorConvertFloat4ToU32(waveCol), 1.0f);
    }

    // Title with glow
    const char* title = "cloud";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    float titleX = panelX + padX;
    float titleY = panelY + (headerH - fontH) / 2;
    
    // Title glow
    ImVec4 titleGlow = HSVtoRGB(m_GradientHue, 0.7f, 1.0f);
    for(int g = 4; g >= 0; g--)
    {
        titleGlow.w = 0.08f * (1.0f - (float)g / 4.0f);
        d->AddText(ImVec2(titleX - g, titleY), 
            ImGui::ColorConvertFloat4ToU32(titleGlow), title);
        d->AddText(ImVec2(titleX + g, titleY), 
            ImGui::ColorConvertFloat4ToU32(titleGlow), title);
        d->AddText(ImVec2(titleX, titleY - g), 
            ImGui::ColorConvertFloat4ToU32(titleGlow), title);
        d->AddText(ImVec2(titleX, titleY + g), 
            ImGui::ColorConvertFloat4ToU32(titleGlow), title);
    }
    
    d->AddText(ImVec2(titleX, titleY), IM_COL32(255, 255, 255, 230), title);

    // Active count badge
    char countBuf[16];
    snprintf(countBuf, sizeof(countBuf), "%d/%d", activeCount, total);
    ImVec2 countSize = ImGui::CalcTextSize(countBuf);
    float badgeW = countSize.x + 16.0f;
    float badgeH = countSize.y + 6.0f;
    float badgeX = panelX + panelW - padX - badgeW;
    float badgeY = panelY + (headerH - badgeH) / 2;
    
    // Badge background with gradient
    ImVec4 badgeBg = HSVtoRGB(m_GradientHue, 0.5f, 0.3f);
    badgeBg.w = 0.5f;
    d->AddRectFilled(ImVec2(badgeX, badgeY), 
        ImVec2(badgeX + badgeW, badgeY + badgeH),
        ImGui::ColorConvertFloat4ToU32(badgeBg), badgeH / 2);
    
    // Badge border
    ImVec4 badgeBrd = HSVtoRGB(m_GradientHue, 0.6f, 0.8f);
    badgeBrd.w = 0.6f;
    d->AddRect(ImVec2(badgeX, badgeY), 
        ImVec2(badgeX + badgeW, badgeY + badgeH),
        ImGui::ColorConvertFloat4ToU32(badgeBrd), badgeH / 2, 0, 1.0f);
    
    // Badge text
    ImVec4 badgeTxt = HSVtoRGB(m_GradientHue, 0.7f, 1.0f);
    badgeTxt.w = 0.9f;
    d->AddText(ImVec2(badgeX + (badgeW - countSize.x) / 2, badgeY + 3),
        ImGui::ColorConvertFloat4ToU32(badgeTxt), countBuf);

    // ════════════════════════════════════════════
    //              ENERGY BAR (Left)
    // ════════════════════════════════════════════
    
    float energyX = panelX + padX;
    float energyStartY = panelY + headerH + 4.0f;
    float energyEndY = panelY + panelH - footerH - 4.0f;
    float energyH = energyEndY - energyStartY;
    
    // Energy bar background
    d->AddRectFilled(ImVec2(energyX, energyStartY),
        ImVec2(energyX + energyBarW, energyEndY),
        IM_COL32(20, 25, 35, 200), energyBarW / 2);
    
    // Simple animated energy bar — два градиентных прямоугольника вместо N циклов
    float flowPhase = fmodf(m_Time * 0.4f, 1.0f);
    ImVec4 fc0 = HSVtoRGB(fmodf(m_GradientHue, 1.0f), 0.7f, 1.0f);
    ImVec4 fc1 = HSVtoRGB(fmodf(m_GradientHue + 0.15f, 1.0f), 0.7f, 1.0f);
    fc0.w = 0.7f; fc1.w = 0.3f;
    d->AddRectFilledMultiColor(
        ImVec2(energyX + 0.5f, energyStartY),
        ImVec2(energyX + energyBarW - 0.5f, energyEndY),
        ImGui::ColorConvertFloat4ToU32(fc0),
        ImGui::ColorConvertFloat4ToU32(fc0),
        ImGui::ColorConvertFloat4ToU32(fc1),
        ImGui::ColorConvertFloat4ToU32(fc1));
    // бегущий блик
    float blipY = energyStartY + fmodf(flowPhase * energyH * 2.0f, energyH);
    d->AddRectFilled(ImVec2(energyX, blipY), ImVec2(energyX + energyBarW, blipY + 8.0f),
        IM_COL32(255, 255, 255, 80), energyBarW / 2);
    
    // Connection dots on energy bar (for visible modules)
    float dotY = energyStartY;
    for(int i = 0; i < total; i++)
    {
        float alpha = sorted[i]->m_Alpha;
        if(alpha < 0.1f) 
        {
            dotY += itemH + gapY;
            continue;
        }
        
        float dotCenterY = dotY + itemH / 2;
        
        // Pulse effect for enabled modules
        if(sorted[i]->m_Enabled)
        {
            float pulse = 0.5f + 0.5f * sinf(m_Time * 5.0f + i * 0.8f);
            ImVec4 pulseCol = GetArraylistColor(i, total, 0.3f * pulse * alpha);
            d->AddCircleFilled(ImVec2(energyX + energyBarW / 2, dotCenterY), 
                6.0f + pulse * 2.0f, ImGui::ColorConvertFloat4ToU32(pulseCol));
        }
        
        // Connection dot
        ImVec4 dotCol = GetArraylistColor(i, total, alpha);
        d->AddCircleFilled(ImVec2(energyX + energyBarW / 2, dotCenterY), 
            3.0f, ImGui::ColorConvertFloat4ToU32(dotCol));
        d->AddCircle(ImVec2(energyX + energyBarW / 2, dotCenterY), 
            3.0f, IM_COL32(255, 255, 255, (int)(80 * alpha)), 1.0f);
        
        dotY += itemH + gapY;
    }

    // ════════════════════════════════════════════
    //                 MODULES
    // ════════════════════════════════════════════
    
    float itemStartX = energyX + energyBarW + 10.0f;
    float y = panelY + headerH + 2.0f;

    for(int i = 0; i < total; i++)
    {
        SHudModule *mod = sorted[i];
        const float alpha = std::clamp(mod->m_Alpha, 0.0f, 1.0f);
        
        if(alpha <= 0.01f)
        {
            y += itemH + gapY;
            continue;
        }

        // Staggered slide animation
        float rawT = std::clamp(1.0f - mod->m_Animation, 0.0f, 1.0f);
        float stT = std::clamp(rawT - i * 0.018f, 0.0f, 1.0f);
        float ease = EaseOutBack(stT);
        float slide = (1.0f - ease) * (panelW + 40.0f);

        ImVec4 accent = GetArraylistColor(i, total, alpha);
        ImU32 accentU = ImGui::ColorConvertFloat4ToU32(accent);
        
        float rowX = itemStartX + slide;
        float rowY = y;
        float rowW = panelW - padX - energyBarW - 14.0f;

        // Row background with holographic shimmer
        float shimmer = sinf(m_Time * 2.0f + i * 0.5f + rowX * 0.01f) * 0.5f + 0.5f;
        ImVec4 rowBg = ImVec4(accent.x * 0.08f, accent.y * 0.08f, accent.z * 0.08f, 0.7f * alpha);
        
        if(i % 2 == 0)
            d->AddRectFilled(ImVec2(rowX - 2, rowY), 
                ImVec2(rowX + rowW + 2, rowY + itemH),
                ImGui::ColorConvertFloat4ToU32(rowBg), 4.0f);
        
        // Shimmer line across row
        if(shimmer > 0.7f && mod->m_Enabled)
        {
            float shimmerX = rowX + (shimmer - 0.7f) / 0.3f * rowW;
            ImVec4 shimmerCol = accent;
            shimmerCol.w = (shimmer - 0.7f) / 0.3f * 0.15f * alpha;
            d->AddLine(ImVec2(shimmerX - 20, rowY + 2),
                ImVec2(shimmerX + 20, rowY + itemH - 2),
                ImGui::ColorConvertFloat4ToU32(shimmerCol), 2.0f);
        }

        // Connection line from energy bar to hexagon
        float hexX = rowX + 8.0f;
        float hexY = rowY + itemH / 2;
        float energyDotY = panelY + headerH + 2.0f + i * (itemH + gapY) + itemH / 2;
        
        d->AddLine(ImVec2(energyX + energyBarW + 2, energyDotY),
            ImVec2(hexX - hexRadius - 2, hexY),
            IM_COL32(60, 70, 90, (int)(100 * alpha)), 1.0f);

        // Hexagonal bullet point with glow
        float breathScale = 1.0f + 0.15f * sinf(m_Time * 3.0f + i * 0.6f);
        
        // Hex glow
        for(int g = 3; g >= 0; g--)
        {
            float gr = hexRadius * breathScale + g * 3.0f;
            ImVec4 glowC = accent;
            glowC.w = 0.12f * (1.0f - (float)g / 3.0f) * alpha;
            
            // Draw hexagon glow (6 vertices)
            ImVec2 hexPts[6];
            for(int v = 0; v < 6; v++)
            {
                float angle = v * 3.14159f / 3.0f - 3.14159f / 6.0f;
                hexPts[v] = ImVec2(hexX + cosf(angle) * gr, hexY + sinf(angle) * gr);
            }
            d->AddConvexPolyFilled(hexPts, 6, ImGui::ColorConvertFloat4ToU32(glowC));
        }
        
        // Hex fill
        ImVec2 hexPts[6];
        for(int v = 0; v < 6; v++)
        {
            float angle = v * 3.14159f / 3.0f - 3.14159f / 6.0f;
            hexPts[v] = ImVec2(hexX + cosf(angle) * hexRadius * breathScale, 
                              hexY + sinf(angle) * hexRadius * breathScale);
        }
        d->AddConvexPolyFilled(hexPts, 6, accentU);
        
        // Hex border
        ImVec4 hexBrd = ImVec4(1.0f, 1.0f, 1.0f, 0.3f * alpha);
        d->AddPolyline(hexPts, 6, ImGui::ColorConvertFloat4ToU32(hexBrd), true, 1.0f);
        
        // Inner hex detail
        for(int v = 0; v < 6; v++)
        {
            float angle = v * 3.14159f / 3.0f - 3.14159f / 6.0f;
            hexPts[v] = ImVec2(hexX + cosf(angle) * hexRadius * 0.5f * breathScale, 
                              hexY + sinf(angle) * hexRadius * 0.5f * breathScale);
        }
        d->AddPolyline(hexPts, 6, IM_COL32(255, 255, 255, (int)(60 * alpha)), true, 1.0f);

        // Module name with shadow
        float textX = hexX + hexRadius + 10.0f;
        float textY = rowY + (itemH - fontH) / 2;
        
        // Text shadow
        d->AddText(ImVec2(textX + 1, textY + 1), 
            IM_COL32(0, 0, 0, (int)(120 * alpha)), mod->m_Name.c_str());
        
        // Main text with color pulse for enabled
        ImVec4 textCol = ImVec4(0.92f, 0.94f, 0.98f, alpha);
        if(mod->m_Enabled)
        {
            float textPulse = 0.92f + 0.08f * sinf(m_Time * 2.5f + i * 0.4f);
            textCol.x = textPulse;
            textCol.y = textPulse;
            textCol.z = textPulse + 0.02f;
        }
        d->AddText(ImVec2(textX, textY), 
            ImGui::ColorConvertFloat4ToU32(textCol), mod->m_Name.c_str());

        // Suffix badge
        if(!mod->m_Suffix.empty())
        {
            float nameW = ImGui::CalcTextSize(mod->m_Name.c_str()).x;
            ImVec2 suffSize = ImGui::CalcTextSize(mod->m_Suffix.c_str());
            
            float suffX = textX + nameW + 8.0f;
            float suffY = textY - 1.0f;
            float suffW = suffSize.x + 12.0f;
            float suffH = suffSize.y + 4.0f;
            float suffR = suffH / 2.0f;
            
            // Badge glow for enabled
            if(mod->m_Enabled)
            {
                ImVec4 suffGlow = accent;
                suffGlow.w = 0.2f * alpha;
                d->AddRectFilled(ImVec2(suffX - 2, suffY - 2),
                    ImVec2(suffX + suffW + 2, suffY + suffH + 2),
                    ImGui::ColorConvertFloat4ToU32(suffGlow), suffR + 2);
            }
            
            // Badge background
            ImVec4 suffBg = accent;
            suffBg.w = 0.18f * alpha;
            d->AddRectFilled(ImVec2(suffX, suffY),
                ImVec2(suffX + suffW, suffY + suffH),
                ImGui::ColorConvertFloat4ToU32(suffBg), suffR);
            
            // Badge border
            ImVec4 suffBrd = accent;
            suffBrd.w = 0.4f * alpha;
            d->AddRect(ImVec2(suffX, suffY),
                ImVec2(suffX + suffW, suffY + suffH),
                ImGui::ColorConvertFloat4ToU32(suffBrd), suffR, 0, 1.0f);
            
            // Badge text
            ImVec4 suffTxt = accent;
            suffTxt.w = 0.85f * alpha;
            d->AddText(ImVec2(suffX + 6, suffY + 2),
                ImGui::ColorConvertFloat4ToU32(suffTxt), mod->m_Suffix.c_str());
        }

        // Status indicator (right side)
        float statusX = rowX + rowW - 8.0f;
        float statusY = rowY + itemH / 2;
        
        if(mod->m_Enabled)
        {
            // Breathing glow for enabled
            float statusPulse = 0.5f + 0.5f * sinf(m_Time * 4.0f + i * 0.7f);
            ImVec4 statusGlow = ImVec4(0.2f, 0.9f, 0.4f, 0.25f * statusPulse * alpha);
            d->AddCircleFilled(ImVec2(statusX, statusY), 8.0f,
                ImGui::ColorConvertFloat4ToU32(statusGlow));
            
            // Green dot
            d->AddCircleFilled(ImVec2(statusX, statusY), 3.5f,
                IM_COL32(50, 230, 100, (int)(255 * alpha)));
            d->AddCircle(ImVec2(statusX, statusY), 3.5f,
                IM_COL32(255, 255, 255, (int)(80 * alpha)));
        }
        else
        {
            // Dim dot for disabled
            d->AddCircleFilled(ImVec2(statusX, statusY), 2.5f,
                IM_COL32(80, 85, 95, (int)(200 * alpha)));
        }

        y += itemH + gapY;
        if(y > pos.y + size.y - footerH - 10.0f)
            break;
    }

    // ════════════════════════════════════════════
    //                 FOOTER
    // ════════════════════════════════════════════
    
    float footerY = panelY + panelH - footerH;
    
    // Footer separator line with gradient
    {
        ImVec4 sepCol = HSVtoRGB(m_GradientHue, 0.5f, 0.8f);
        d->AddRectFilledMultiColor(
            ImVec2(panelX, footerY), ImVec2(panelX + panelW, footerY + 1.0f),
            IM_COL32(0,0,0,0),
            IM_COL32(0,0,0,0),
            ImGui::ColorConvertFloat4ToU32(ImVec4(sepCol.x, sepCol.y, sepCol.z, 0.15f)),
            IM_COL32(0,0,0,0));
    }
    
    // Time display
    std::string timeStr = GetTimeString();
    ImVec2 timeSize = ImGui::CalcTextSize(timeStr.c_str());
    d->AddText(ImVec2(panelX + panelW - padX - timeSize.x, footerY + 6),
        IM_COL32(80, 100, 130, 180), timeStr.c_str());
    
    // FPS display
    char fpsBuf[16];
    snprintf(fpsBuf, sizeof(fpsBuf), "%d fps", m_Fps);
    d->AddText(ImVec2(panelX + padX, footerY + 6),
        IM_COL32(80, 100, 130, 180), fpsBuf);
    
    // Small decorative line in footer center
    float centerLineW = 30.0f;
    float centerLineX = panelX + (panelW - centerLineW) / 2;
    ImVec4 lineCol = HSVtoRGB(m_GradientHue, 0.6f, 0.9f);
    lineCol.w = 0.4f;
    d->AddLine(ImVec2(centerLineX, footerY + 11),
        ImVec2(centerLineX + centerLineW, footerY + 11),
        ImGui::ColorConvertFloat4ToU32(lineCol), 1.5f);

    d->PopClipRect();
}

void CShalawaHud::RenderArraylistSingularity(ImDrawList *d, ImVec2 pos, ImVec2 size, std::vector<SHudModule*> &sorted)
{
    const int total = (int)sorted.size();
    if (total <= 0)
        return;

    d->PushClipRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), true);

    // ════════════════════════════════════════════
    //              КОНСТАНТЫ И РАЗМЕРЫ
    // ════════════════════════════════════════════
    const float fontH = ImGui::GetTextLineHeight();
    const float itemH = fontH + 16.0f; // Делаем модули высокими и массивными
    const float gapY = 4.0f;
    const float padX = 18.0f;
    const float headerH = 40.0f;
    const float footerH = 20.0f;
    
    // Вычисляем ширину панели с учетом "Glitch" эффектов и суффиксов
    float maxLabelW = 0.0f;
    for (auto *mod : sorted)
    {
        float w = ImGui::CalcTextSize(mod->m_Name.c_str()).x;
        if (!mod->m_Suffix.empty())
            w += 15.0f + ImGui::CalcTextSize(mod->m_Suffix.c_str()).x;
        if (w > maxLabelW) maxLabelW = w;
    }

    float panelW = maxLabelW + padX * 2.0f + 50.0f; // +50 для коннекторов и иконок
    panelW = std::min(panelW, size.x);
    float listH = total * (itemH + gapY);
    float panelH = headerH + listH + footerH;
    panelH = std::min(panelH, size.y);
    float panelX = pos.x + size.x - panelW;
    float panelY = pos.y;

    // ════════════════════════════════════════════
    //        ГОЛОГРАФИЧЕСКИЙ ФОН И МАТРИЦА
    // ════════════════════════════════════════════
    
    // Размытая тень от всей панели
    DrawBlurredShadow(d, ImVec2(panelX, panelY), ImVec2(panelW, panelH), 15, ImVec4(0.0f, 0.0f, 0.0f, 0.6f));

    // Основное "стекло" панели (очень темное, киберпанковое)
    d->AddRectFilled(ImVec2(panelX, panelY), ImVec2(panelX + panelW, panelY + panelH), IM_COL32(5, 7, 12, 230), 4.0f);

    // Анимированная сетка на фоне (Grid)
    float gridOffset = fmodf(m_Time * 15.0f, 20.0f);
    for (float gy = panelY + gridOffset; gy < panelY + panelH; gy += 40.0f) {
        float alphaFade = sinf((gy - panelY) / panelH * 3.1415f); // Плавное затухание к краям
        d->AddLine(ImVec2(panelX, gy), ImVec2(panelX + panelW, gy), IM_COL32(255, 255, 255, (int)(10 * alphaFade)));
    }
    for (float gx = panelX + gridOffset; gx < panelX + panelW; gx += 40.0f) {
        float alphaFade = sinf((gx - panelX) / panelW * 3.1415f);
        d->AddLine(ImVec2(gx, panelY), ImVec2(gx, panelY + panelH), IM_COL32(255, 255, 255, (int)(10 * alphaFade)));
    }

    // Сканирующий лазер (сверху вниз)
    float scanLineY = panelY + fmodf(m_Time * 100.0f, panelH);
    ImVec4 scanColor = HSVtoRGB(m_GradientHue, 0.8f, 1.0f);
    d->AddRectFilledMultiColor(
        ImVec2(panelX, scanLineY - 15.0f), ImVec2(panelX + panelW, scanLineY),
        IM_COL32(0,0,0,0), IM_COL32(0,0,0,0), 
        ImGui::ColorConvertFloat4ToU32(ImVec4(scanColor.x, scanColor.y, scanColor.z, 0.3f)), 
        ImGui::ColorConvertFloat4ToU32(ImVec4(scanColor.x, scanColor.y, scanColor.z, 0.3f))
    );
    d->AddLine(ImVec2(panelX, scanLineY), ImVec2(panelX + panelW, scanLineY), ImGui::ColorConvertFloat4ToU32(scanColor), 2.0f);

    // ════════════════════════════════════════════
    //                    HEADER
    // ════════════════════════════════════════════
    ImVec2 titleSize = ImGui::CalcTextSize("S I N G U L A R I T Y");
    float titleX = panelX + (panelW - titleSize.x) / 2.0f;
    float titleY = panelY + 10.0f;
    
    // Глитч-текст для заголовка
    d->AddText(ImVec2(titleX - 2.0f, titleY), IM_COL32(255, 0, 50, 150), "S I N G U L A R I T Y");
    d->AddText(ImVec2(titleX + 2.0f, titleY), IM_COL32(0, 200, 255, 150), "S I N G U L A R I T Y");
    d->AddText(ImVec2(titleX, titleY), IM_COL32(255, 255, 255, 255), "S I N G U L A R I T Y");

    // Подзаголовок: System Active | Версия
    d->AddText(ImVec2(panelX + 10.0f, panelY + headerH - 12.0f), IM_COL32(100, 150, 255, 150), "SYS.CORE // ACTIVE");

    // ════════════════════════════════════════════
    //            НЕЙРОННЫЙ ХРЕБЕТ (СЛЕВА)
    // ════════════════════════════════════════════
    float spineX = panelX + 15.0f;
    float spineStartY = panelY + headerH;
    float spineEndY = panelY + panelH - footerH;
    
    d->AddLine(ImVec2(spineX, spineStartY), ImVec2(spineX, spineEndY), IM_COL32(50, 60, 80, 150), 2.0f);
    
    // Бегущие импульсы по хребту
    float pulseY = spineStartY + fmodf(m_Time * 150.0f, spineEndY - spineStartY);
    d->AddRectFilled(ImVec2(spineX - 1.5f, pulseY - 10.0f), ImVec2(spineX + 1.5f, pulseY + 10.0f), IM_COL32(255, 255, 255, 255));
    d->AddCircleFilled(ImVec2(spineX, pulseY), 4.0f, ImGui::ColorConvertFloat4ToU32(scanColor));

    // ════════════════════════════════════════════
    //                  МОДУЛИ
    // ════════════════════════════════════════════
    float y = panelY + headerH;

    for (int i = 0; i < total; i++)
    {
        SHudModule *mod = sorted[i];
        const float alpha = std::clamp(mod->m_Alpha, 0.0f, 1.0f);
        
        if (alpha <= 0.01f) {
            y += itemH + gapY;
            continue;
        }

        ImVec4 accent = GetArraylistColor(i, total, alpha);
        ImU32 accentU = ImGui::ColorConvertFloat4ToU32(accent);

        // Хитрая кривая анимации
        float rawAnim = std::clamp(1.0f - mod->m_Animation, 0.0f, 1.0f);
        float ease = EaseOutBack(rawAnim);
        float slide = (1.0f - ease) * (panelW + 100.0f);

        float rowX = panelX + 30.0f + slide; // Отступ от хребта
        float rowY = y;
        float rowW = panelW - 40.0f;
        
        // ── Нейронная связь (ломаная линия от хребта к модулю) ──
        float nodeY = rowY + itemH / 2.0f;
        if (mod->m_Enabled) {
            d->AddLine(ImVec2(spineX, nodeY), ImVec2(spineX + 10.0f, nodeY), accentU, 1.5f);
            d->AddLine(ImVec2(spineX + 10.0f, nodeY), ImVec2(rowX - 5.0f, nodeY), accentU, 1.5f);
        } else {
            d->AddLine(ImVec2(spineX, nodeY), ImVec2(rowX - 5.0f, nodeY), IM_COL32(50, 60, 80, (int)(150 * alpha)), 1.0f);
        }

        // ── Фон модуля (Кибер-скос) ──
        ImVec2 bgPts[4] = {
            ImVec2(rowX, rowY),
            ImVec2(rowX + rowW, rowY),
            ImVec2(rowX + rowW - 8.0f, rowY + itemH),
            ImVec2(rowX - 8.0f, rowY + itemH)
        };
        ImVec4 rowBgCol = accent;
        rowBgCol.w = 0.05f * alpha + (mod->m_Enabled ? 0.05f : 0.0f); // Чуть ярче если включен
        d->AddConvexPolyFilled(bgPts, 4, ImGui::ColorConvertFloat4ToU32(rowBgCol));
        
        // Левая граница модуля (Акцент)
        d->AddLine(bgPts[0], bgPts[3], accentU, 2.0f);

        // ── Вращающийся реактор (Иконка слева от текста) ──
        float reactorX = rowX + 10.0f;
        float reactorR = 6.0f;
        float rotation = m_Time * (mod->m_Enabled ? 4.0f : 1.0f) + i; // Крутится быстрее если включен
        
        ImVec2 poly[3];
        for (int v = 0; v < 3; v++) {
            float angle = rotation + v * (2.0f * 3.14159f / 3.0f);
            poly[v] = ImVec2(reactorX + cosf(angle) * reactorR, nodeY + sinf(angle) * reactorR);
        }
        
        if (mod->m_Enabled) {
            ImVec4 reactorGlow = accent;
            reactorGlow.w = 0.3f * alpha;
            d->AddCircleFilled(ImVec2(reactorX, nodeY), reactorR * 2.5f,
                ImGui::ColorConvertFloat4ToU32(reactorGlow));
            d->AddConvexPolyFilled(poly, 3, IM_COL32(255, 255, 255, (int)(255 * alpha)));
        }
        d->AddPolyline(poly, 3, accentU, true, 1.5f);

        // ── Текст с Хроматической Аберрацией при анимации ──
        float textX = reactorX + 15.0f;
        float textY = rowY + (itemH - fontH) / 2.0f;
        
        float glitchIntensity = fabsf(1.0f - ease) * 10.0f; // Глитчит только когда выезжает/заезжает
        
        if (glitchIntensity > 0.1f) {
            // Красный и синий каналы разъезжаются
            d->AddText(ImVec2(textX - glitchIntensity, textY), IM_COL32(255, 0, 50, (int)(150 * alpha)), mod->m_Name.c_str());
            d->AddText(ImVec2(textX + glitchIntensity, textY), IM_COL32(0, 200, 255, (int)(150 * alpha)), mod->m_Name.c_str());
        }
        
        d->AddText(ImVec2(textX, textY), IM_COL32(240, 245, 255, (int)(255 * alpha)), mod->m_Name.c_str());

        // ── Кибер-суффикс ──
        if (!mod->m_Suffix.empty())
        {
            float nameW = ImGui::CalcTextSize(mod->m_Name.c_str()).x;
            ImVec2 sufSize = ImGui::CalcTextSize(mod->m_Suffix.c_str());
            float sufX = textX + nameW + 10.0f;
            float sufY = textY;
            
            // Рамка вокруг суффикса вида [ SUFFIX ]
            d->AddText(ImVec2(sufX, sufY), IM_COL32(100, 100, 120, (int)(200 * alpha)), "[");
            d->AddText(ImVec2(sufX + 8.0f, sufY), ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, alpha * 0.8f)), mod->m_Suffix.c_str());
            d->AddText(ImVec2(sufX + 8.0f + sufSize.x + 2.0f, sufY), IM_COL32(100, 100, 120, (int)(200 * alpha)), "]");
        }

        // ── Particals (Искры вокруг включенного модуля) ──
        if (mod->m_Enabled) {
            for (int p = 0; p < 3; p++) {
                float seed = (float)(i * 1337 + p * 7919);
                float px = rowX + rowW - 20.0f + sinf(seed) * 20.0f;
                float py = rowY + fmodf(seed * 0.618f, 1.0f) * itemH;
                py -= fmodf(m_Time * (10.0f + fmodf(seed * 0.37f, 1.0f) * 20.0f), itemH);
                if (py < rowY) py += itemH;
                float pSize = 1.0f + fmodf(seed * 0.123f, 1.0f) * 2.0f;
                d->AddRectFilled(ImVec2(px, py), ImVec2(px + pSize, py + pSize), accentU);
            }
        }

        y += itemH + gapY;
        if (y > pos.y + size.y - footerH) break; // Защита от переполнения
    }

    // ════════════════════════════════════════════
    //                  FOOTER
    // ════════════════════════════════════════════
    float footerY = panelY + panelH - footerH;
    
    // Линия отсечения с градиентом
    d->AddRectFilledMultiColor(
        ImVec2(panelX, footerY), ImVec2(panelX + panelW, footerY + 1.0f),
        IM_COL32(0,0,0,0), ImGui::ColorConvertFloat4ToU32(scanColor), 
        ImGui::ColorConvertFloat4ToU32(scanColor), IM_COL32(0,0,0,0)
    );

    // Технический текст внизу
    int activeCount = 0;
    for (auto* m : sorted) if (m->m_Enabled) activeCount++;
    
    char statBuf[64];
    snprintf(statBuf, sizeof(statBuf), "MEMORY: %d%% | MODULES: %d/%d", (int)(fmodf(m_Time * 5.0f, 10.0f) + 40.0f), activeCount, total);
    
    d->AddText(ImGui::GetFont(), fontH * 0.8f, ImVec2(panelX + 10.0f, footerY + 4.0f), 
               IM_COL32(100, 120, 150, 180), statBuf, NULL, panelW);

    std::string timeStr = GetTimeString();
    d->AddText(ImGui::GetFont(), fontH * 0.8f, ImVec2(panelX + panelW - ImGui::CalcTextSize(timeStr.c_str()).x - 10.0f, footerY + 4.0f), 
               IM_COL32(200, 220, 255, 180), timeStr.c_str(), NULL, panelW);

    d->PopClipRect();
}

ImVec4 CShalawaHud::GetArraylistColor(int index, int total, float alpha)
{
    float offset = total > 1 ? (float)index / (float)(total - 1) : 0.0f;
    ImVec4 col;
    int mode = g_Config.m_shalawaArraylistMode;
    switch(mode)
    {
        case 0:
            col = HSVtoRGB(fmodf(m_GradientHue + index * 0.05f, 1.0f), 0.65f, 0.95f);
            break;
        case 1:
        {
            float t = std::abs(std::fmod(m_Millis / 1000.0f * 3.0f + offset * 0.5f, 2.0f));
            if(t > 1.0f) t = 2.0f - t;
            ImVec4 c1(0.56f, 0.56f, 0.94f, 1.0f);
            ImVec4 c2(0.94f, 0.56f, 0.78f, 1.0f);
            col = ImVec4(c1.x + (c2.x - c1.x) * t, c1.y + (c2.y - c1.y) * t,
                         c1.z + (c2.z - c1.z) * t, 1.0f);
            break;
        }
        case 2:
            col = HSVtoRGB(fmodf(m_Millis / 2000.0f * 2.0f + offset, 1.0f), 0.7f, 1.0f);
            break;
        case 3:
        {
            float h = (float)((sin(m_Millis / 1000.0f * 2.0f + offset * 3.14159f) + 1.0) / 2.0);
            col = HSVtoRGB(h, 0.7f, 1.0f);
            break;
        }
        case 4:
        {
            float brightness = 1.0f - offset * 0.45f;
            float saturation = 0.65f - offset * 0.25f;
            col = HSVtoRGB(fmodf(m_GradientHue, 1.0f), saturation, brightness);
            break;
        }
        default:
            col = HSVtoRGB(fmodf(m_GradientHue, 1.0f), 0.65f, 0.95f);
            break;
    }
    col.w = alpha;
    return col;
}



void CShalawaHud::RenderArraylistClaude(ImDrawList *d, ImVec2 pos, ImVec2 size,
                                         std::vector<SHudModule*> &sorted)
{
    const int total = (int)sorted.size();
    if(total <= 0)
        return;

    d->PushClipRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), true);

    // ──────────── visual constants ────────────
    const float fontH    = ImGui::GetTextLineHeight();
    const float itemH    = fontH + 10.0f;
    const float gapY     = 2.0f;
    const float padX     = 12.0f;
    const float headerH  = 28.0f;
    const float footerH  = 22.0f;
    const float rounding = 5.0f;
    const float spineOff = 16.0f;              // spine X relative to panel left
    const float textOff  = spineOff + 20.0f;   // text  X relative to panel left
    const float diamondR = 3.5f;

    // ──────────── measure content ────────────
    float maxLabelW = 0.0f;
    for(auto *mod : sorted)
    {
        float w = ImGui::CalcTextSize(mod->m_Name.c_str()).x;
        if(!mod->m_Suffix.empty())
            w += 8.0f + ImGui::CalcTextSize(mod->m_Suffix.c_str()).x + 14.0f;
        if(w > maxLabelW) maxLabelW = w;
    }

    float panelW = textOff + maxLabelW + padX + 4.0f;
    panelW = std::min(panelW, size.x);
    float listH  = total * (itemH + gapY);
    float panelH = headerH + listH + footerH;
    panelH = std::min(panelH, size.y);
    float panelX = pos.x + size.x - panelW;
    float panelY = pos.y;

    // ════════════════════════════════════════════
    //                BACKGROUND
    // ════════════════════════════════════════════

    if(g_Config.m_shalawaArraylistShadow)
        DrawBlurredShadow(d, ImVec2(panelX, panelY),
                          ImVec2(panelW, panelH), 8, ImVec4(0, 0, 0, 0.4f));

    // main fill
    d->AddRectFilled(ImVec2(panelX, panelY),
                     ImVec2(panelX + panelW, panelY + panelH),
                     IM_COL32(6, 6, 10, 230), rounding);

    // outer border
    d->AddRect(ImVec2(panelX, panelY),
               ImVec2(panelX + panelW, panelY + panelH),
               IM_COL32(255, 255, 255, 14), rounding, 0, 1.0f);

    // scanline texture (very subtle horizontal lines)
    for(float sy = panelY + 3.0f; sy < panelY + panelH - 3.0f; sy += 4.0f)
        d->AddLine(ImVec2(panelX + 2, sy),
                   ImVec2(panelX + panelW - 2, sy), IM_COL32(255, 255, 255, 2));

    // bottom glass reflection
    float refH = std::min(panelH * 0.15f, 40.0f);
    d->AddRectFilledMultiColor(
        ImVec2(panelX + 1, panelY + panelH - refH),
        ImVec2(panelX + panelW - 1, panelY + panelH - 1),
        IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0),
        IM_COL32(255, 255, 255, 6), IM_COL32(255, 255, 255, 6));

        // top gradient accent bar
    const int gradSegs = 12;
    for(int s = 0; s < gradSegs; s++)
    {
        float t0 = (float)s / gradSegs;
        float t1 = (float)(s + 1) / gradSegs;
        ImVec4 c0 = GetArraylistColor((int)(t0 * std::max(1, total - 1)), total, 0.85f);
        ImVec4 c1 = GetArraylistColor((int)(t1 * std::max(1, total - 1)), total, 0.85f);
        d->AddRectFilledMultiColor(
            ImVec2(panelX + t0 * panelW, panelY),
            ImVec2(panelX + t1 * panelW, panelY + 2.0f),
            ImGui::ColorConvertFloat4ToU32(c0), ImGui::ColorConvertFloat4ToU32(c1),
            ImGui::ColorConvertFloat4ToU32(c1), ImGui::ColorConvertFloat4ToU32(c0));
    }

    // glow under the top bar
    for(int g = 4; g >= 0; g--)
    {
        float h = (float)(g + 1) * 3.0f;
        ImVec4 gc = GetArraylistColor(0, total, 0.85f);
        gc.w = 0.03f * (1.0f - (float)g / 4.0f);
        d->AddRectFilled(ImVec2(panelX, panelY + 2),
                         ImVec2(panelX + panelW, panelY + 2 + h),
                         ImGui::ColorConvertFloat4ToU32(gc));
    }

    // ════════════════════════════════════════════
    //                  HEADER
    // ════════════════════════════════════════════

    ImVec4 hdrAccent = GetArraylistColor(0, total, 0.85f);

    d->AddText(ImVec2(panelX + padX, panelY + (headerH - fontH) / 2),
               IM_COL32(255, 255, 255, 185), "shalawa");

    // active count badge
    int activeCnt = 0;
    for(auto *m : sorted)
        if(m->m_Enabled) activeCnt++;
    char cBuf[16];
    snprintf(cBuf, sizeof(cBuf), "%d", activeCnt);
    ImVec2 cSz = ImGui::CalcTextSize(cBuf);
    float bw = cSz.x + 12.0f, bh = cSz.y + 4.0f;
    float bx = panelX + panelW - padX - bw;
    float by = panelY + (headerH - bh) / 2.0f;

    d->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh),
        ImGui::ColorConvertFloat4ToU32(ImVec4(hdrAccent.x, hdrAccent.y, hdrAccent.z, 0.15f)),
        bh / 2);
    d->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh),
        ImGui::ColorConvertFloat4ToU32(ImVec4(hdrAccent.x, hdrAccent.y, hdrAccent.z, 0.35f)),
        bh / 2, 0, 1.0f);
    d->AddText(ImVec2(bx + (bw - cSz.x) / 2, by + 2),
        ImGui::ColorConvertFloat4ToU32(ImVec4(hdrAccent.x, hdrAccent.y, hdrAccent.z, 0.85f)),
        cBuf);

    // separator
    d->AddLine(ImVec2(panelX + padX, panelY + headerH - 1),
               ImVec2(panelX + panelW - padX, panelY + headerH - 1),
               IM_COL32(255, 255, 255, 16));

    // ════════════════════════════════════════════
    //             SPINE (vertical line)
    // ════════════════════════════════════════════

    float spAbsX = panelX + spineOff;

    // gather visible node Y positions
    struct SpineNode { float y; float alpha; int idx; };
    std::vector<SpineNode> nodes;
    {
        float yy = panelY + headerH;
        for(int i = 0; i < total; i++)
        {
            float a = std::clamp(sorted[i]->m_Alpha, 0.0f, 1.0f);
            if(a > 0.1f)
                nodes.push_back({yy + itemH / 2, a, i});
            yy += itemH + gapY;
        }
    }

    // draw spine segments between adjacent visible nodes
    for(size_t n = 1; n < nodes.size(); n++)
    {
        float minA = std::min(nodes[n - 1].alpha, nodes[n].alpha);
        d->AddLine(ImVec2(spAbsX, nodes[n - 1].y),
                   ImVec2(spAbsX, nodes[n].y),
                   IM_COL32(50, 50, 70, (int)(55 * minA)), 1.0f);
    }

    // traveling light along the spine
    if(nodes.size() >= 2)
    {
        float pp = fmodf(m_Time * 0.7f, 2.0f);
        if(pp > 1.0f) pp = 2.0f - pp;  // ping-pong

        float lightY = nodes.front().y +
                       (nodes.back().y - nodes.front().y) * pp;

        int lIdx = std::clamp((int)(pp * ((int)nodes.size() - 1)),
                              0, (int)nodes.size() - 1);
        ImVec4 lc = GetArraylistColor(nodes[lIdx].idx, total, 0.65f);

        for(int g = 5; g >= 0; g--)
        {
            float gr = (float)g * 4.5f;
            ImVec4 gc = lc;
            gc.w = 0.04f * (1.0f - (float)g / 5.0f);
            d->AddCircleFilled(ImVec2(spAbsX, lightY), gr,
                               ImGui::ColorConvertFloat4ToU32(gc));
        }
        d->AddCircleFilled(ImVec2(spAbsX, lightY), 2.0f,
            ImGui::ColorConvertFloat4ToU32(ImVec4(lc.x, lc.y, lc.z, 0.85f)));
    }

    // ════════════════════════════════════════════
    //                  ITEMS
    // ════════════════════════════════════════════

    float y = panelY + headerH;

    for(int i = 0; i < total; i++)
    {
        SHudModule *mod = sorted[i];
        const float alpha = std::clamp(mod->m_Alpha, 0.0f, 1.0f);
        if(alpha <= 0.01f)
        {
            y += itemH + gapY;
            continue;
        }

        // staggered slide-in
        float rawT  = std::clamp(1.0f - mod->m_Animation, 0.0f, 1.0f);
        float stT   = std::clamp(rawT - i * 0.015f, 0.0f, 1.0f);
        float ease   = EaseOutBack(stT);
        float slide  = (1.0f - ease) * (panelW + 30.0f);

        ImVec4 accent = GetArraylistColor(i, total, alpha);
        ImU32  acU    = ImGui::ColorConvertFloat4ToU32(accent);
        float  nodeY  = y + itemH / 2;

        // ── row background (alternating) ──
        int bgA = (i % 2 == 0) ? (int)(22 * alpha) : (int)(14 * alpha);
        d->AddRectFilled(
            ImVec2(panelX + 3 + slide, y + 1),
            ImVec2(panelX + panelW - 3 + slide, y + itemH - 1),
            IM_COL32(18, 18, 28, bgA), 3.0f);

        // ── diamond node ──
        float dX = panelX + spineOff + slide;
        float dS = diamondR * (0.65f + 0.35f * alpha);

        // diamond glow
        ImVec4 glowC = accent;
        glowC.w = 0.15f * alpha;
        d->AddCircleFilled(ImVec2(dX, nodeY), dS * 2.8f,
                           ImGui::ColorConvertFloat4ToU32(glowC));

        // breathing pulse
        float pulse = 0.5f + 0.5f * sinf(m_Time * 2.5f + i * 0.7f);
        ImVec4 pulseC = accent;
        pulseC.w = 0.06f * pulse * alpha;
        d->AddCircleFilled(ImVec2(dX, nodeY), dS * (1.6f + pulse),
                           ImGui::ColorConvertFloat4ToU32(pulseC));

        // diamond fill
        ImVec2 dp[4] = {
            ImVec2(dX,      nodeY - dS),
            ImVec2(dX + dS, nodeY     ),
            ImVec2(dX,      nodeY + dS),
            ImVec2(dX - dS, nodeY     )
        };
        d->AddConvexPolyFilled(dp, 4, acU);

        // diamond border (subtle)
        ImVec4 dbCol = accent;
        dbCol.w = alpha * 0.4f;
        ImU32 dbU = ImGui::ColorConvertFloat4ToU32(dbCol);
        for(int j = 0; j < 4; j++)
            d->AddLine(dp[j], dp[(j + 1) % 4], dbU, 1.0f);

        // ── connector line ──
        float connEnd = panelX + textOff - 3.0f + slide;
        d->AddLine(ImVec2(dX + dS + 1, nodeY),
                   ImVec2(connEnd, nodeY),
                   IM_COL32(50, 50, 70, (int)(45 * alpha)), 1.0f);

        // small dot at connector end
        d->AddCircleFilled(ImVec2(connEnd, nodeY), 1.5f,
                           IM_COL32(60, 60, 80, (int)(60 * alpha)));

        // ── module name ──
        float tx = panelX + textOff + slide;
        float ty = y + (itemH - fontH) / 2.0f;

        d->AddText(ImVec2(tx + 1, ty + 1),
                   IM_COL32(0, 0, 0, (int)(90 * alpha)),
                   mod->m_Name.c_str());
        d->AddText(ImVec2(tx, ty),
                   IM_COL32(228, 228, 238, (int)(255 * alpha)),
                   mod->m_Name.c_str());

        // ── suffix tag (pill badge) ──
        if(!mod->m_Suffix.empty())
        {
            float nw  = ImGui::CalcTextSize(mod->m_Name.c_str()).x;
            ImVec2 ss = ImGui::CalcTextSize(mod->m_Suffix.c_str());

            float tgX = tx + nw + 8.0f;
            float tgY = y + (itemH - ss.y - 4.0f) / 2.0f;
            float tgW = ss.x + 10.0f;
            float tgH = ss.y + 4.0f;
            float tgR = tgH / 2.0f;

            ImVec4 tBg  = accent; tBg.w  = 0.12f * alpha;
            ImVec4 tBrd = accent; tBrd.w = 0.28f * alpha;
            ImVec4 tTxt = accent; tTxt.w = 0.72f * alpha;

            d->AddRectFilled(ImVec2(tgX, tgY), ImVec2(tgX + tgW, tgY + tgH),
                             ImGui::ColorConvertFloat4ToU32(tBg), tgR);
            d->AddRect(ImVec2(tgX, tgY), ImVec2(tgX + tgW, tgY + tgH),
                       ImGui::ColorConvertFloat4ToU32(tBrd), tgR, 0, 1.0f);
            d->AddText(ImVec2(tgX + 5, tgY + 2),
                       ImGui::ColorConvertFloat4ToU32(tTxt),
                       mod->m_Suffix.c_str());
        }

        y += itemH + gapY;
        if(y > pos.y + size.y - footerH)
            break;
    }

    // ════════════════════════════════════════════
    //                  FOOTER
    // ════════════════════════════════════════════

    float fY = panelY + panelH - footerH;

    d->AddLine(ImVec2(panelX + padX, fY + 1),
               ImVec2(panelX + panelW - padX, fY + 1),
               IM_COL32(255, 255, 255, 16));

    // timestamp right
    std::string ts  = GetTimeString();
    ImVec2      tsz = ImGui::CalcTextSize(ts.c_str());
    d->AddText(ImVec2(panelX + panelW - padX - tsz.x,
                      fY + (footerH - tsz.y) / 2),
               IM_COL32(75, 75, 95, 140), ts.c_str());

    // branding left
    d->AddText(ImVec2(panelX + padX, fY + (footerH - fontH) / 2),
               IM_COL32(50, 50, 70, 90), "claude");

    d->PopClipRect();
}

void CShalawaHud::RenderArraylistDeepSeek(ImDrawList *d, ImVec2 pos, ImVec2 size,
                                          std::vector<SHudModule*> &sorted)
{
    const int total = (int)sorted.size();
    if(total <= 0)
        return;

    d->PushClipRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), true);

    // ──────────── константы ────────────
    const float fontH    = ImGui::GetTextLineHeight();
    const float itemH    = fontH + 10.0f;
    const float gapY     = 2.5f;
    const float padX     = 14.0f;
    const float headerH  = 32.0f;
    const float rounding = 6.0f;
    const float iconR    = 4.0f;
    const float waveW    = 2.0f;    // ширина волновой линии
    const float waveSpd  = 120.0f;  // скорость анимации волны

    // ──────────── измеряем ширину ────────────
    float maxLabelW = 0.0f;
    for(auto *mod : sorted)
    {
        float w = ImGui::CalcTextSize(mod->m_Name.c_str()).x;
        if(!mod->m_Suffix.empty())
            w += 8.0f + ImGui::CalcTextSize(mod->m_Suffix.c_str()).x + 12.0f;
        if(w > maxLabelW) maxLabelW = w;
    }

    // ──────────── размеры панели ────────────
    float panelW = padX * 2.0f + maxLabelW + 35.0f;
    panelW = std::min(panelW, size.x);
    float listH  = total * (itemH + gapY);
    float panelH = headerH + listH + 4.0f;
    panelH = std::min(panelH, size.y);
    float panelX = pos.x + size.x - panelW;
    float panelY = pos.y;

    // ════════════════════════════════════════════
    //                ФОН
    // ════════════════════════════════════════════

    // основной фон с градиентом
    ImVec4 bgTop    = ImVec4(0.06f, 0.08f, 0.12f, 0.92f);
    ImVec4 bgBottom = ImVec4(0.04f, 0.05f, 0.08f, 0.92f);
    d->AddRectFilledMultiColor(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        ImGui::ColorConvertFloat4ToU32(bgTop),
        ImGui::ColorConvertFloat4ToU32(bgTop),
        ImGui::ColorConvertFloat4ToU32(bgBottom),
        ImGui::ColorConvertFloat4ToU32(bgBottom));

    // стеклянный эффект
    d->AddRectFilledMultiColor(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + headerH * 0.7f),
        IM_COL32(255, 255, 255, 6),
        IM_COL32(255, 255, 255, 6),
        IM_COL32(255, 255, 255, 0),
        IM_COL32(255, 255, 255, 0));

    // тонкая рамка
    d->AddRect(ImVec2(panelX, panelY),
               ImVec2(panelX + panelW, panelY + panelH),
               IM_COL32(40, 50, 70, 40), rounding, 0, 1.0f);

    // внутренняя тень
    for(int i = 0; i < 3; i++)
    {
        float inset = (float)i * 0.5f;
        d->AddRect(ImVec2(panelX + inset, panelY + inset),
                   ImVec2(panelX + panelW - inset, panelY + panelH - inset),
                   IM_COL32(0, 0, 0, 10), rounding - inset * 0.5f, 0, 1.0f);
    }

    // ════════════════════════════════════════════
    //                ХЕДЕР
    // ════════════════════════════════════════════

    // градиентная полоса сверху (DeepSeek blue)
    for(int i = 0; i < 12; i++)
    {
        float t0 = (float)i / 12.0f;
        float t1 = (float)(i + 1) / 12.0f;
        
        // сине-бирюзовый градиент
        ImVec4 c0(0.1f, 0.5f, 0.8f, 0.8f);
        ImVec4 c1(0.0f, 0.3f, 0.6f, 0.8f);
        if(t0 > 0.5f)
        {
            c0 = ImVec4(0.0f, 0.3f, 0.6f, 0.8f);
            c1 = ImVec4(0.1f, 0.2f, 0.4f, 0.8f);
        }
        
        d->AddRectFilledMultiColor(
            ImVec2(panelX + t0 * panelW, panelY),
            ImVec2(panelX + t1 * panelW, panelY + 3.0f),
            ImGui::ColorConvertFloat4ToU32(c0),
            ImGui::ColorConvertFloat4ToU32(c1),
            ImGui::ColorConvertFloat4ToU32(c1),
            ImGui::ColorConvertFloat4ToU32(c0));
    }

    // текст заголовка
    d->AddText(ImVec2(panelX + padX, panelY + (headerH - fontH) / 2),
               IM_COL32(200, 220, 255, 200), "DeepSeek");

    // счётчик модулей
    int activeCnt = 0;
    for(auto *m : sorted)
        if(m->m_Enabled) activeCnt++;
    char countBuf[16];
    snprintf(countBuf, sizeof(countBuf), "%d/%d", activeCnt, total);
    ImVec2 cntSz = ImGui::CalcTextSize(countBuf);
    d->AddText(ImVec2(panelX + panelW - padX - cntSz.x,
                      panelY + (headerH - fontH) / 2),
               IM_COL32(100, 150, 200, 180), countBuf);

    // разделитель
    d->AddLine(ImVec2(panelX + padX, panelY + headerH - 1),
               ImVec2(panelX + panelW - padX, panelY + headerH - 1),
               IM_COL32(40, 60, 90, 80));

    // ════════════════════════════════════════════
    //               ВОЛНОВАЯ ЛИНИЯ
    // ════════════════════════════════════════════

    float waveX = panelX + 22.0f;
    float waveStartY = panelY + headerH + 4.0f;
    float waveEndY = waveStartY + listH - 8.0f;

    // основная линия
    d->AddLine(ImVec2(waveX, waveStartY),
               ImVec2(waveX, waveEndY),
               IM_COL32(30, 60, 90, 60), waveW);

    // анимированная волна
    float wavePhase = fmodf(m_Time * 2.0f, 2.0f * 3.14159f);
    int waveSegments = 32;
    for(int s = 0; s < waveSegments; s++)
    {
        float y0 = waveStartY + (float)s / waveSegments * (waveEndY - waveStartY);
        float y1 = waveStartY + (float)(s + 1) / waveSegments * (waveEndY - waveStartY);
        
        float amp = sinf(y0 * 0.02f + wavePhase) * 3.0f;
        float amp2 = sinf(y1 * 0.02f + wavePhase) * 3.0f;
        
        ImVec4 waveColor(0.3f, 0.6f, 1.0f, 0.6f);
        waveColor.w *= 0.3f + 0.7f * (0.5f + 0.5f * sinf(m_Time * 1.5f + s * 0.2f));
        
        d->AddLine(ImVec2(waveX + amp, y0),
                   ImVec2(waveX + amp2, y1),
                   ImGui::ColorConvertFloat4ToU32(waveColor), 1.5f);
    }

    // ════════════════════════════════════════════
    //                ЭЛЕМЕНТЫ
    // ════════════════════════════════════════════

    float y = panelY + headerH;

    for(int i = 0; i < total; i++)
    {
        SHudModule *mod = sorted[i];
        const float alpha = std::clamp(mod->m_Alpha, 0.0f, 1.0f);
        if(alpha <= 0.01f)
        {
            y += itemH + gapY;
            continue;
        }

        // анимация появления
        float rawT  = std::clamp(1.0f - mod->m_Animation, 0.0f, 1.0f);
        float stT   = std::clamp(rawT - i * 0.02f, 0.0f, 1.0f);
        float ease   = EaseOutBack(stT);
        float slide  = (1.0f - ease) * (panelW + 20.0f);

        // цвет для DeepSeek (сине-голубая гамма)
        ImVec4 accent;
        if(g_Config.m_shalawaArraylistMode == 0) // градиентный режим
        {
            float hue = fmodf(0.55f + i * 0.03f, 1.0f); // синие оттенки
            accent = HSVtoRGB(hue, 0.7f, 0.9f);
        }
        else
        {
            // стандартный DeepSeek blue
            accent = ImVec4(0.1f, 0.5f, 0.8f, 1.0f);
        }
        accent.w = alpha;

        ImU32  acU    = ImGui::ColorConvertFloat4ToU32(accent);
        float  rowX   = panelX + slide;
        float  rowY   = y;

        // ── фон строки ──
        ImU32 rowBg;
        if(i % 2 == 0)
            rowBg = IM_COL32(12, 16, 22, (int)(200 * alpha));
        else
            rowBg = IM_COL32(10, 14, 20, (int)(200 * alpha));
        
        d->AddRectFilled(
            ImVec2(rowX + 4, rowY + 1),
            ImVec2(rowX + panelW - 4, rowY + itemH - 1),
            rowBg, rounding - 2.0f);

        // ── точка на волне ──
        float dotY = rowY + itemH / 2.0f;
        float dotX = waveX;
        
        // пульсация точки
        float pulse = 0.7f + 0.3f * sinf(m_Time * 3.0f + i * 0.5f);
        float dotR = iconR * (0.8f + 0.2f * pulse);
        
        // свечение
        for(int g = 3; g >= 0; g--)
        {
            float gr = dotR + (float)g * 3.0f;
            ImVec4 glow = accent;
            glow.w = 0.08f * (1.0f - (float)g / 3.0f) * alpha;
            d->AddCircleFilled(ImVec2(dotX, dotY), gr,
                               ImGui::ColorConvertFloat4ToU32(glow));
        }
        
        // сама точка
        d->AddCircleFilled(ImVec2(dotX, dotY), dotR, acU);
        
        // обводка точки
        d->AddCircle(ImVec2(dotX, dotY), dotR + 0.5f,
                     IM_COL32(255, 255, 255, (int)(60 * alpha)), 1.0f);

        // ── текст модуля ──
        float textX = dotX + 18.0f;
        float textY = rowY + (itemH - fontH) / 2.0f;
        
        // тень текста
        d->AddText(ImVec2(textX + 1, textY + 1),
                   IM_COL32(0, 0, 0, (int)(80 * alpha)),
                   mod->m_Name.c_str());
        
        // основной текст
        d->AddText(ImVec2(textX, textY),
                   IM_COL32(220, 230, 255, (int)(255 * alpha)),
                   mod->m_Name.c_str());

        // ── суффикс (если есть) ──
        if(!mod->m_Suffix.empty())
        {
            float nameW = ImGui::CalcTextSize(mod->m_Name.c_str()).x;
            ImVec2 suffSz = ImGui::CalcTextSize(mod->m_Suffix.c_str());
            
            float suffX = textX + nameW + 8.0f;
            float suffY = textY;
            
            // фон суффикса
            float suffW = suffSz.x + 10.0f;
            float suffH = suffSz.y + 4.0f;
            float suffR = suffH / 2.0f;
            
            ImVec4 suffBg = accent;
            suffBg.w = 0.15f * alpha;
            d->AddRectFilled(ImVec2(suffX, suffY - 2),
                             ImVec2(suffX + suffW, suffY + suffH),
                             ImGui::ColorConvertFloat4ToU32(suffBg), suffR);
            
            // текст суффикса
            ImVec4 suffText = accent;
            suffText.w = 0.8f * alpha;
            d->AddText(ImVec2(suffX + 5, suffY),
                       ImGui::ColorConvertFloat4ToU32(suffText),
                       mod->m_Suffix.c_str());
        }

        // ── индикатор состояния ──
        float stateX = rowX + panelW - 24.0f;
        float stateY = rowY + itemH / 2.0f;
        float stateR = 3.0f;
        
        ImU32 stateColor;
        if(mod->m_Enabled)
            stateColor = IM_COL32(100, 200, 100, (int)(255 * alpha)); // зелёный
        else
            stateColor = IM_COL32(200, 100, 100, (int)(255 * alpha)); // красный
        
        // пульсация для включенных
        if(mod->m_Enabled)
        {
            float statePulse = 0.5f + 0.5f * sinf(m_Time * 2.0f + i * 0.3f);
            d->AddCircleFilled(ImVec2(stateX, stateY), stateR * (1.0f + statePulse * 0.3f),
                               IM_COL32(100, 200, 100, (int)(80 * alpha * statePulse)));
        }
        
        d->AddCircleFilled(ImVec2(stateX, stateY), stateR, stateColor);
        d->AddCircle(ImVec2(stateX, stateY), stateR + 1.0f,
                     IM_COL32(255, 255, 255, (int)(40 * alpha)), 1.0f);

        y += itemH + gapY;
        if(y > pos.y + size.y - 10.0f)
            break;
    }

    // ════════════════════════════════════════════
    //                ФУТЕР
    // ════════════════════════════════════════════

    float footerY = panelY + panelH - 18.0f;
    
    // разделитель
    d->AddLine(ImVec2(panelX + padX, footerY),
               ImVec2(panelX + panelW - padX, footerY),
               IM_COL32(40, 60, 90, 60));
    
    // время справа
    std::string timeStr = GetTimeString();
    ImVec2 timeSz = ImGui::CalcTextSize(timeStr.c_str());
    d->AddText(ImVec2(panelX + panelW - padX - timeSz.x, footerY + 4),
               IM_COL32(80, 120, 180, 140), timeStr.c_str());
    
    // иконка слева (символ AI)
    d->AddText(ImVec2(panelX + padX, footerY + 4),
               IM_COL32(100, 180, 255, 180), "α");

    d->PopClipRect();
}


void CShalawaHud::RenderArraylistSkeet(ImDrawList *d, ImVec2 pos, ImVec2 size, std::vector<SHudModule*> &sorted)
{
    int total = (int)sorted.size();
    float fontH = ImGui::GetTextLineHeight();
    float itemH = fontH + 2.0f;
    float padX = 6.0f;

    float maxW = 0;
    for(auto *mod : sorted)
    {
        std::string n = mod->m_Name + (mod->m_Suffix.empty() ? "" : " " + mod->m_Suffix);
        float w = ImGui::CalcTextSize(n.c_str()).x;
        if(w > maxW) maxW = w;
    }
    float panelW = maxW + padX * 2 + 4.0f;
    float panelX = pos.x + size.x - panelW;
    float panelY = pos.y;
    float panelH = total * itemH;

    d->AddRectFilled(ImVec2(panelX - 1, panelY - 1),
                     ImVec2(panelX + panelW + 1, panelY + panelH + 1),
                     IM_COL32(50, 50, 55, 255));
    d->AddRectFilled(ImVec2(panelX, panelY),
                     ImVec2(panelX + panelW, panelY + panelH),
                     IM_COL32(8, 8, 10, 252));
    d->AddRect(ImVec2(panelX + 1, panelY + 1),
               ImVec2(panelX + panelW - 1, panelY + panelH - 1),
               IM_COL32(40, 40, 45, 180));

    const int skeetSegs = 12;
    for(int s = 0; s < skeetSegs; s++)
    {
        float t0 = (float)s / skeetSegs;
        float t1 = (float)(s + 1) / skeetSegs;
        ImVec4 c0 = GetArraylistColor((int)(t0 * std::max(1, total - 1)), total, 1.0f);
        ImVec4 c1 = GetArraylistColor((int)(t1 * std::max(1, total - 1)), total, 1.0f);
        d->AddRectFilledMultiColor(ImVec2(panelX + t0 * panelW, panelY - 2), ImVec2(panelX + t1 * panelW, panelY),
            ImGui::ColorConvertFloat4ToU32(c0), ImGui::ColorConvertFloat4ToU32(c1),
            ImGui::ColorConvertFloat4ToU32(c1), ImGui::ColorConvertFloat4ToU32(c0));
    }

    for(int i = 0; i < total; i++)
    {
        SHudModule *mod = sorted[i];
        std::string fullName = mod->m_Name;
        if(!mod->m_Suffix.empty())
            fullName += " " + mod->m_Suffix;

        float textW = ImGui::CalcTextSize(fullName.c_str()).x;
        float iy = panelY + i * itemH;
        float alpha = mod->m_Alpha;
        float slideOffset = (1.0f - EaseOutBack(1.0f - mod->m_Animation)) * panelW;

        ImVec4 col = GetArraylistColor(i, total, alpha);
        ImU32 color = ImGui::ColorConvertFloat4ToU32(col);
        float rowX = panelX + slideOffset;

        if(g_Config.m_shalawaArraylistBackground)
        {
            d->AddRectFilled(ImVec2(rowX + 2, iy),
                             ImVec2(panelX + panelW - 2 + slideOffset, iy + itemH),
                             IM_COL32(0, 0, 0, (int)(40 * alpha)));
        }

        float textX = panelX + panelW - padX - textW + slideOffset;
        float textY = iy + (itemH - fontH) / 2.0f;

        d->AddText(ImVec2(textX + 1, textY + 1), IM_COL32(0, 0, 0, (int)(80 * alpha)), fullName.c_str());
        d->AddText(ImVec2(textX, textY), color, fullName.c_str());

        d->AddRectFilled(ImVec2(panelX + panelW - 2 + slideOffset, iy),
                         ImVec2(panelX + panelW + slideOffset, iy + itemH), color);
    }
}

void CShalawaHud::RenderArraylistNeverlose(ImDrawList *d, ImVec2 pos, ImVec2 size, std::vector<SHudModule*> &sorted)
{
    int total = (int)sorted.size();
    float fontH = ImGui::GetTextLineHeight();
    float itemH = fontH + 6.0f;
    float padX = 10.0f;
    float gap = 1.0f;

    float maxW = 0;
    for(auto *mod : sorted)
    {
        std::string n = mod->m_Name;
        float w = ImGui::CalcTextSize(n.c_str()).x;
        if(!mod->m_Suffix.empty())
            w += ImGui::CalcTextSize((" " + mod->m_Suffix).c_str()).x;
        if(w > maxW) maxW = w;
    }
    float panelW = maxW + padX * 2 + 6.0f;
    float panelX = pos.x + size.x - panelW;
    float panelY = pos.y;

    float headerH = 22.0f;
    ImVec4 accent = GetArraylistColor(0, total, 1.0f);
    ImU32 accentU32 = ImGui::ColorConvertFloat4ToU32(accent);

    DrawBlurredShadow(d, ImVec2(panelX, panelY), ImVec2(panelW, headerH), 6, ImVec4(0, 0, 0, 0.4f));
    d->AddRectFilled(ImVec2(panelX, panelY), ImVec2(panelX + panelW, panelY + headerH),
                     IM_COL32(22, 22, 28, 250), 6.0f, ImDrawFlags_RoundCornersTop);
    d->AddRectFilled(ImVec2(panelX, panelY), ImVec2(panelX + panelW, panelY + 2.0f),
                     accentU32, 6.0f, ImDrawFlags_RoundCornersTop);

    ImVec2 titleSize = ImGui::CalcTextSize("modules");
    d->AddText(ImVec2(panelX + (panelW - titleSize.x) / 2, panelY + (headerH - titleSize.y) / 2 + 1),
               IM_COL32(255, 255, 255, 160), "modules");

    float listY = panelY + headerH;

    for(int i = 0; i < total; i++)
    {
        SHudModule *mod = sorted[i];
        std::string fullName = mod->m_Name;
        float iy = listY + i * (itemH + gap);
        float alpha = mod->m_Alpha;
        float slideOffset = (1.0f - EaseOutBack(1.0f - mod->m_Animation)) * panelW;

        ImVec4 col = GetArraylistColor(i, total, alpha);
        ImU32 color = ImGui::ColorConvertFloat4ToU32(col);
        float rowX = panelX + slideOffset;

        int bgAlpha = (int)(200 * alpha);
        ImU32 bgCol = (i % 2 == 0) ? IM_COL32(18, 18, 22, bgAlpha) : IM_COL32(22, 22, 28, bgAlpha);
        d->AddRectFilled(ImVec2(rowX, iy), ImVec2(panelX + panelW + slideOffset, iy + itemH), bgCol);

        float textX = rowX + padX;
        float textY = iy + (itemH - fontH) / 2.0f;

        d->AddText(ImVec2(textX, textY), color, fullName.c_str());

        if(!mod->m_Suffix.empty())
        {
            float nameW = ImGui::CalcTextSize(fullName.c_str()).x;
            ImVec4 suffixCol = col;
            suffixCol.w = alpha * 0.5f;
            d->AddText(ImVec2(textX + nameW, textY),
                       ImGui::ColorConvertFloat4ToU32(suffixCol),
                       (" " + mod->m_Suffix).c_str());
        }

        d->AddRectFilled(ImVec2(rowX, iy), ImVec2(rowX + 2.5f, iy + itemH), color);
    }

    float bottomY = listY + total * (itemH + gap);
    d->AddRectFilled(ImVec2(panelX, bottomY - 1), ImVec2(panelX + panelW, bottomY + 3),
                     IM_COL32(15, 15, 18, 250), 6.0f, ImDrawFlags_RoundCornersBottom);
}

void CShalawaHud::RenderArraylistGamesense(ImDrawList *d, ImVec2 pos, ImVec2 size, std::vector<SHudModule*> &sorted)
{
    int total = (int)sorted.size();
    float fontH = ImGui::GetTextLineHeight();
    float itemH = fontH + 3.0f;
    float padX = 6.0f;

    float maxW = 0;
    for(auto *mod : sorted)
    {
        std::string n = mod->m_Name + (mod->m_Suffix.empty() ? "" : " " + mod->m_Suffix);
        float w = ImGui::CalcTextSize(n.c_str()).x;
        if(w > maxW) maxW = w;
    }
    float panelW = maxW + padX * 2 + 4.0f;
    float panelX = pos.x + size.x - panelW;
    float panelY = pos.y;

    for(int i = 0; i < total; i++)
    {
        SHudModule *mod = sorted[i];
        std::string fullName = mod->m_Name;
        if(!mod->m_Suffix.empty())
            fullName += " " + mod->m_Suffix;

        float textW = ImGui::CalcTextSize(fullName.c_str()).x;
        float iy = panelY + i * itemH;
        float alpha = mod->m_Alpha;
        float slideOffset = (1.0f - EaseOutBack(1.0f - mod->m_Animation)) * panelW;

        ImVec4 col = GetArraylistColor(i, total, alpha);
        ImU32 color = ImGui::ColorConvertFloat4ToU32(col);
        float rowRight = panelX + panelW + slideOffset;

        if(g_Config.m_shalawaArraylistBackground)
        {
            d->AddRectFilled(ImVec2(panelX + slideOffset, iy),
                             ImVec2(rowRight, iy + itemH),
                             IM_COL32(12, 12, 14, (int)(220 * alpha)));
        }

        d->AddRectFilled(ImVec2(rowRight - 2.0f, iy),
                         ImVec2(rowRight, iy + itemH), color);

        float textX = rowRight - 2.0f - padX - textW;
        float textY = iy + (itemH - fontH) / 2.0f;

        if(g_Config.m_shalawaArraylistShadow)
        {
            d->AddText(ImVec2(textX + 1, textY + 1),
                       IM_COL32(0, 0, 0, (int)(90 * alpha)), fullName.c_str());
        }

        d->AddText(ImVec2(textX, textY), color, fullName.c_str());
    }
}

void CShalawaHud::RenderArraylistCustom(ImDrawList *d, ImVec2 pos, ImVec2 size, std::vector<SHudModule*> &sorted)
{
    int total = (int)sorted.size();
    float fontH = ImGui::GetTextLineHeight();
    float itemH = fontH + 5.0f;
    float padX = 8.0f;
    float barW = 2.5f;
    float gap = 2.0f;

    float maxW = 0;
    for(auto *mod : sorted)
    {
        std::string n = mod->m_Name + (mod->m_Suffix.empty() ? "" : " " + mod->m_Suffix);
        float w = ImGui::CalcTextSize(n.c_str()).x;
        if(w > maxW) maxW = w;
    }
    float panelW = maxW + padX * 2 + barW + 4.0f;
    float panelX = pos.x + size.x - panelW;
    float panelY = pos.y;

    for(int i = 0; i < total; i++)
    {
        SHudModule *mod = sorted[i];
        std::string fullName = mod->m_Name;
        if(!mod->m_Suffix.empty())
            fullName += " " + mod->m_Suffix;

        float textW = ImGui::CalcTextSize(fullName.c_str()).x;
        float iy = panelY + i * (itemH + gap);
        float alpha = mod->m_Alpha;
        float slideOffset = (1.0f - EaseOutBack(1.0f - mod->m_Animation)) * (panelW + 20.0f);

        ImVec4 col = GetArraylistColor(i, total, alpha);
        ImU32 color = ImGui::ColorConvertFloat4ToU32(col);
        float rowX = panelX + slideOffset;
        float rowRight = panelX + panelW + slideOffset;

        if(g_Config.m_shalawaArraylistBackground)
        {
            d->AddRectFilled(ImVec2(rowX, iy), ImVec2(rowRight, iy + itemH),
                             IM_COL32(4, 4, 6, (int)(200 * alpha)), 2.0f);
        }

        if(g_Config.m_shalawaArraylistShadow)
        {
            ImVec4 glowCol = col;
            glowCol.w = 0.1f * alpha;
            for(int g = 4; g >= 1; g--)
            {
                float expand = (float)g * 2.0f;
                ImVec4 gc = glowCol;
                gc.w = glowCol.w * ((float)(4 - g + 1) / 4.0f);
                d->AddRectFilled(ImVec2(rowX - expand, iy - expand * 0.5f),
                                 ImVec2(rowRight + expand, iy + itemH + expand * 0.5f),
                                 ImGui::ColorConvertFloat4ToU32(gc), 3.0f);
            }
        }

        d->AddRectFilled(ImVec2(rowRight - barW, iy), ImVec2(rowRight, iy + itemH), color, 1.0f);

        ImVec4 barGlow = col;
        barGlow.w = 0.2f * alpha;
        for(int g = 3; g >= 1; g--)
        {
            float expand = (float)g * 1.5f;
            ImVec4 gc = barGlow;
            gc.w = barGlow.w * ((float)(3 - g + 1) / 3.0f);
            d->AddRectFilled(ImVec2(rowRight - barW - expand, iy),
                             ImVec2(rowRight + expand, iy + itemH),
                             ImGui::ColorConvertFloat4ToU32(gc), 1.0f);
        }

        float textX = rowRight - barW - padX - textW;
        float textY = iy + (itemH - fontH) / 2.0f;

        d->AddText(ImVec2(textX + 1, textY + 1), IM_COL32(0, 0, 0, (int)(60 * alpha)), fullName.c_str());
        d->AddText(ImVec2(textX, textY), color, fullName.c_str());
    }
}

void CShalawaHud::RenderArraylistChatGPT(ImDrawList *d, ImVec2 pos, ImVec2 size, std::vector<SHudModule*> &sorted)
{
    const int total = (int)sorted.size();
    if(total <= 0)
        return;

    // Клиппинг: не рисуем за пределами HUD-элемента (важно, если модулей много)
    const ImVec2 clipMin = pos;
    const ImVec2 clipMax = ImVec2(pos.x + size.x, pos.y + size.y);
    d->PushClipRect(clipMin, clipMax, true);

    // Визуальные константы (они автоматом масштабируются из-за SetWindowFontScale(scale) выше)
    const float fontH    = ImGui::GetTextLineHeight();
    const float itemH    = fontH + 8.0f;
    const float gapY     = 3.0f;
    const float padX     = 10.0f;
    const float barW     = 2.5f;
    const float iconR    = 3.0f;
    const float rounding = 7.0f;

    auto BuildLabel = [](const SHudModule *m) -> std::string
    {
        if(m->m_Suffix.empty())
            return m->m_Name;
        // аккуратнее и короче, чем "(suffix)"
        return m->m_Name + " • " + m->m_Suffix;
    };

    // Меряем максимальную ширину строки (для фоновой подложки)
    float maxRowW = 0.0f;
    for(auto *mod : sorted)
    {
        std::string label = BuildLabel(mod);
        const float textW = ImGui::CalcTextSize(label.c_str()).x;
        const float rowW  = textW + padX * 2.0f + iconR * 2.0f + 6.0f + barW;
        maxRowW = std::max(maxRowW, rowW);
    }

    // Правая граница — как в классическом arraylist (прижат вправо)
    const float xRight = pos.x + size.x;

    // Фоновая панель (опционально)
    const float panelPad = 4.0f;
    const float panelW = std::min(maxRowW + panelPad * 2.0f, size.x);
    const float panelX = xRight - panelW;
    const float panelY = pos.y;

    float listH = total * itemH + (total - 1) * gapY;
    listH = std::min(listH + panelPad * 2.0f, size.y);

    if(g_Config.m_shalawaArraylistBackground)
    {
        if(g_Config.m_shalawaArraylistShadow)
            DrawBlurredShadow(d, ImVec2(panelX, panelY), ImVec2(panelW, listH), 8, ImVec4(0, 0, 0, 0.35f));

        // "glass" фон
        d->AddRectFilled(ImVec2(panelX, panelY),
                         ImVec2(panelX + panelW, panelY + listH),
                         IM_COL32(10, 10, 14, 170), rounding + 2.0f);

        // тонкая рамка
        d->AddRect(ImVec2(panelX, panelY),
                   ImVec2(panelX + panelW, panelY + listH),
                   IM_COL32(255, 255, 255, 18), rounding + 2.0f, 0, 1.0f);

        // верхняя акцентная линия градиентом
        for(int i = 0; i < 64; i++)
        {
            float t0 = (float)i / 64.0f;
            float t1 = (float)(i + 1) / 64.0f;
            ImVec4 c0 = GetArraylistColor((int)(t0 * (total - 1)), total, 0.9f);
            ImVec4 c1 = GetArraylistColor((int)(t1 * (total - 1)), total, 0.9f);

            float x0 = panelX + t0 * panelW;
            float x1 = panelX + t1 * panelW;
            d->AddRectFilledMultiColor(ImVec2(x0, panelY),
                                       ImVec2(x1, panelY + 2.0f),
                                       ImGui::ColorConvertFloat4ToU32(c0),
                                       ImGui::ColorConvertFloat4ToU32(c1),
                                       ImGui::ColorConvertFloat4ToU32(c1),
                                       ImGui::ColorConvertFloat4ToU32(c0));
        }
    }

    // Рисуем строки
    float y = panelY + panelPad;

    for(int i = 0; i < total; i++)
    {
        SHudModule *mod = sorted[i];
        const float alpha = std::clamp(mod->m_Alpha, 0.0f, 1.0f);
        if(alpha <= 0.01f)
            continue;

        std::string label = BuildLabel(mod);
        const float textW = ImGui::CalcTextSize(label.c_str()).x;

        // ширина строки "пилюли" — по контенту (красиво и компактно)
        float rowW = textW + padX * 2.0f + iconR * 2.0f + 6.0f + barW;
        rowW = std::min(rowW, size.x); // safety

        // slide: enabled -> 0, disabled -> уезжает вправо
        const float t = std::clamp(1.0f - mod->m_Animation, 0.0f, 1.0f);
        const float ease = EaseOutBack(t);
        const float slide = (1.0f - ease) * (panelW + 24.0f);

        const float rowX = xRight - rowW + slide;
        const ImVec2 rMin(rowX, y);
        const ImVec2 rMax(rowX + rowW, y + itemH);

        ImVec4 accent = GetArraylistColor(i, total, alpha);
        ImU32 accentU32 = ImGui::ColorConvertFloat4ToU32(accent);

        // строка-фон
        ImU32 bg = IM_COL32(12, 12, 16, (int)(195 * alpha));
        d->AddRectFilled(rMin, rMax, bg, rounding);

        // лёгкий "shine" сверху
        d->AddRectFilledMultiColor(rMin,
                                   ImVec2(rMax.x, rMin.y + itemH * 0.55f),
                                   IM_COL32(255, 255, 255, (int)(18 * alpha)),
                                   IM_COL32(255, 255, 255, (int)(18 * alpha)),
                                   IM_COL32(255, 255, 255, 0),
                                   IM_COL32(255, 255, 255, 0));

        // тонкая обводка
        d->AddRect(rMin, rMax, IM_COL32(255, 255, 255, (int)(28 * alpha)), rounding, 0, 1.0f);

        // правая цветная полоска
        d->AddRectFilled(ImVec2(rMax.x - barW, rMin.y), rMax, accentU32, rounding, ImDrawFlags_RoundCornersRight);

        // иконка-точка слева
        ImVec2 ic(rMin.x + padX + iconR, (rMin.y + rMax.y) * 0.5f);
        d->AddCircleFilled(ic, iconR, accentU32);
        d->AddCircle(ic, iconR + 1.0f, IM_COL32(0, 0, 0, (int)(120 * alpha)));

        // текст
        ImVec2 textPos(rMin.x + padX + iconR * 2.0f + 6.0f, rMin.y + (itemH - fontH) * 0.5f);
        d->AddText(ImVec2(textPos.x + 1, textPos.y + 1), IM_COL32(0, 0, 0, (int)(120 * alpha)), label.c_str());
        d->AddText(textPos, IM_COL32(235, 235, 240, (int)(255 * alpha)), label.c_str());

        y += itemH + gapY;
        if(y > clipMax.y) // дальше всё равно не видно
            break;
    }

    d->PopClipRect();
}

void CShalawaHud::RenderKeybinds(ImVec2 pos, ImVec2 size, float scale)
{
    ANTI_DISASM_BLOCK();
    if(!g_Config.m_shalawaWatermarkKeybinds)
        return;

    ImDrawList *d = ImGui::GetBackgroundDrawList();

    const Uint8 *keys = SDL_GetKeyboardState(nullptr);
    if(!keys)
        return;
    Uint32 mouse = SDL_GetMouseState(nullptr, nullptr);

    bool pressed[6] = {
        keys[SDL_SCANCODE_W] != 0,
        keys[SDL_SCANCODE_A] != 0,
        keys[SDL_SCANCODE_S] != 0,
        keys[SDL_SCANCODE_D] != 0,
        (mouse & SDL_BUTTON(1)) != 0,
        (mouse & SDL_BUTTON(3)) != 0
    };

    for(int i = 0; i < 6; i++)
    {
        float target = pressed[i] ? 1.0f : 0.0f;
        m_KeyStates[i] = Lerp(m_KeyStates[i], target, m_DeltaTime * 15.0f);
    }

    ImVec4 accentCol = HSVtoRGB(m_GradientHue, 0.7f, 0.9f);

    float panelW = size.x;
    float panelH = size.y;

    DrawBlurredShadow(d, pos, ImVec2(panelW, panelH), 5, ImVec4(0, 0, 0, 0.3f));
    d->AddRectFilled(pos, ImVec2(pos.x + panelW, pos.y + panelH),
                     IM_COL32(15, 15, 18, 230), 6.0f * scale);

    float headerH = 20.0f * scale;
    d->AddRectFilled(pos, ImVec2(pos.x + panelW, pos.y + headerH),
                     IM_COL32(20, 20, 25, 255), 6.0f * scale, ImDrawFlags_RoundCornersTop);
    d->AddRectFilled(pos, ImVec2(pos.x + panelW, pos.y + 2),
                     ImGui::ColorConvertFloat4ToU32(accentCol), 6.0f * scale, ImDrawFlags_RoundCornersTop);

    ImVec2 titleSize = ImGui::CalcTextSize("Keybinds");
    d->AddText(ImVec2(pos.x + (panelW - titleSize.x) / 2, pos.y + (headerH - titleSize.y) / 2 + 1),
               IM_COL32(255, 255, 255, 200), "Keybinds");

    float contentY = pos.y + headerH + 6 * scale;
    float keySize = 24.0f * scale;
    float gap = 3.0f * scale;
    float contentX = pos.x + (panelW - (keySize * 3 + gap * 2)) / 2;

    auto drawKey = [&](float kx, float ky, float w, float h, const char *label, float state) {
        ImVec2 kmin(kx, ky);
        ImVec2 kmax(kx + w, ky + h);

        ImVec4 bgCol(0.08f + state * accentCol.x * 0.3f,
                     0.08f + state * accentCol.y * 0.3f,
                     0.10f + state * accentCol.z * 0.3f, 0.92f);

        if(state > 0.05f)
        {
            ImVec4 glowCol = accentCol;
            glowCol.w = state * 0.12f;
            for(int g = 3; g >= 0; g--)
            {
                float expand = (float)g * 1.5f;
                ImVec4 gc = glowCol;
                gc.w = glowCol.w * (1.0f - (float)g / 3.0f);
                d->AddRectFilled(ImVec2(kmin.x - expand, kmin.y - expand),
                                 ImVec2(kmax.x + expand, kmax.y + expand),
                                 ImGui::ColorConvertFloat4ToU32(gc), 4.0f * scale);
            }
        }

        d->AddRectFilled(kmin, kmax, ImGui::ColorConvertFloat4ToU32(bgCol), 4.0f * scale);

        float borderAlpha = 0.12f + state * 0.4f;
        ImVec4 borderCol = accentCol;
        borderCol.w = borderAlpha;
        d->AddRect(kmin, kmax, ImGui::ColorConvertFloat4ToU32(borderCol), 4.0f * scale);

        ImVec2 ts = ImGui::CalcTextSize(label);
        float alpha = 0.5f + state * 0.5f;
        d->AddText(ImVec2(kx + (w - ts.x) / 2, ky + (h - ts.y) / 2),
                   IM_COL32(255, 255, 255, (int)(alpha * 255)), label);
    };

    drawKey(contentX + keySize + gap, contentY, keySize, keySize, "W", m_KeyStates[0]);
    drawKey(contentX, contentY + keySize + gap, keySize, keySize, "A", m_KeyStates[1]);
    drawKey(contentX + keySize + gap, contentY + keySize + gap, keySize, keySize, "S", m_KeyStates[2]);
    drawKey(contentX + (keySize + gap) * 2, contentY + keySize + gap, keySize, keySize, "D", m_KeyStates[3]);

    float mouseY = contentY + keySize * 2 + gap * 3;
    float mouseW = (keySize * 3 + gap * 2 - gap) / 2;
    drawKey(contentX, mouseY, mouseW, 16.0f * scale, "M1", m_KeyStates[4]);
    drawKey(contentX + mouseW + gap, mouseY, mouseW, 16.0f * scale, "M2", m_KeyStates[5]);
}

void CShalawaHud::RenderSpeedometer(ImVec2 pos, ImVec2 size, float scale)
{
    ANTI_DISASM_BLOCK();
    if(!g_Config.m_shalawaWatermarkSpeed)
        return;

    ImDrawList *d = ImGui::GetBackgroundDrawList();
    ImVec4 accentCol = HSVtoRGB(m_GradientHue, 0.7f, 0.9f);

    float panelW = size.x;
    float panelH = size.y;

    DrawBlurredShadow(d, pos, ImVec2(panelW, panelH), 5, ImVec4(0, 0, 0, 0.3f));
    d->AddRectFilled(pos, ImVec2(pos.x + panelW, pos.y + panelH),
                     IM_COL32(15, 15, 18, 230), 6.0f * scale);
    d->AddRectFilled(pos, ImVec2(pos.x + panelW, pos.y + 2),
                     ImGui::ColorConvertFloat4ToU32(accentCol),
                     6.0f * scale, ImDrawFlags_RoundCornersTop);

    char speedText[32];
    snprintf(speedText, sizeof(speedText), "%.1f", m_SmoothedSpeed);
    ImVec2 speedSize = ImGui::CalcTextSize(speedText);

    float centerX = pos.x + panelW / 2;
    float textY = pos.y + 8 * scale;

    d->AddText(ImVec2(centerX - speedSize.x / 2 + 1, textY + 1),
               IM_COL32(0, 0, 0, 80), speedText);
    d->AddText(ImVec2(centerX - speedSize.x / 2, textY),
               ImGui::ColorConvertFloat4ToU32(accentCol), speedText);

    ImVec2 bpsSize = ImGui::CalcTextSize("bps");
    d->AddText(ImVec2(centerX - bpsSize.x / 2, textY + speedSize.y + 1),
               IM_COL32(100, 100, 110, 200), "bps");

    float graphY = textY + speedSize.y + bpsSize.y + 6 * scale;
    float graphH = pos.y + panelH - graphY - 6 * scale;
    float graphW = panelW - 12 * scale;
    float graphX = pos.x + 6 * scale;

    if(graphH > 8.0f)
    {
        d->AddRectFilled(ImVec2(graphX, graphY),
                         ImVec2(graphX + graphW, graphY + graphH),
                         IM_COL32(0, 0, 0, 60), 3.0f * scale);

        int numPoints = std::min(128, (int)(graphW / 1.5f));
        if(numPoints > 2)
        {
            std::vector<ImVec2> points;
            for(int i = 0; i < numPoints; i++)
            {
                int idx = (m_SpeedHistoryIdx - numPoints + i + 128) % 128;
                float val = (m_MaxSpeed > 0.001f) ? m_SpeedHistory[idx] / m_MaxSpeed : 0.0f;
                val = std::clamp(val, 0.0f, 1.0f);
                float px = graphX + (float)i / (float)(numPoints - 1) * graphW;
                float py = graphY + graphH - val * (graphH - 2);
                points.push_back(ImVec2(px, py));
            }

            for(int i = 0; i < (int)points.size() - 1; i++)
            {
                ImVec4 fillCol = accentCol;
                fillCol.w = 0.12f;
                ImU32 fillTop = ImGui::ColorConvertFloat4ToU32(fillCol);
                fillCol.w = 0.01f;
                ImU32 fillBot = ImGui::ColorConvertFloat4ToU32(fillCol);
                d->AddRectFilledMultiColor(
                    ImVec2(points[i].x, points[i].y),
                    ImVec2(points[i + 1].x, graphY + graphH),
                    fillTop, fillTop, fillBot, fillBot);
            }

            ImVec4 lineCol = accentCol;
            lineCol.w = 0.8f;
            d->AddPolyline(points.data(), (int)points.size(),
                           ImGui::ColorConvertFloat4ToU32(lineCol), 0, 1.5f * scale);
        }
    }
}


void CShalawaHud::RenderNotifications(ImVec2 pos, ImVec2 size, float scale)
{
    ANTI_DISASM_BLOCK();
    if(!g_Config.m_shalawaWatermarkNotifications)
        return;

    float currentTime = (float)SDL_GetTicks() / OBF_F(1000);
    for(auto it = m_HudNotifications.begin(); it != m_HudNotifications.end();)
    {
        float age = currentTime - it->m_SpawnTime;
        if(age < 0.4f)
        {
            it->m_Progress = EaseOutBack(age / 0.4f);
            it->m_SlideAnim = it->m_Progress;
        }
        else if(age < 2.6f)
        {
            it->m_Progress = 1.0f;
            it->m_SlideAnim = 1.0f;
        }
        else if(age < 3.0f)
        {
            float fadeProgress = (age - 2.6f) / 0.4f;
            it->m_Progress = 1.0f - SmoothStep(fadeProgress);
            it->m_SlideAnim = 1.0f - fadeProgress;
        }
        else
        {
            it = m_HudNotifications.erase(it);
            continue;
        }
        ++it;
    }

    if(m_HudNotifications.empty())
        return;

    ImDrawList *d = ImGui::GetBackgroundDrawList();
    float notifH = 42.0f * scale;

    for(size_t i = 0; i < m_HudNotifications.size(); i++)
    {
        SHudNotification &n = m_HudNotifications[i];
        float slideOffset = (1.0f - n.m_SlideAnim) * 50.0f;
        float alpha = std::clamp(n.m_Progress, 0.0f, 1.0f);

        ImVec2 nPos(pos.x + slideOffset, pos.y + i * (notifH + 6.0f * scale));

        DrawBlurredShadow(d, nPos, ImVec2(size.x, notifH), 5,
                          ImVec4(0, 0, 0, 0.3f * alpha));
        d->AddRectFilled(nPos, ImVec2(nPos.x + size.x, nPos.y + notifH),
                         IM_COL32(15, 15, 18, (int)(240 * alpha)), 8.0f * scale);

        ImVec4 lineCol = n.m_Enabled ?
            ImVec4(0.3f, 0.8f, 0.45f, alpha) :
            ImVec4(0.8f, 0.3f, 0.3f, alpha);
        d->AddRectFilled(nPos, ImVec2(nPos.x + 3 * scale, nPos.y + notifH),
                         ImGui::ColorConvertFloat4ToU32(lineCol),
                         8.0f * scale, ImDrawFlags_RoundCornersLeft);

        d->AddText(ImVec2(nPos.x + 12 * scale, nPos.y + 8 * scale),
                   IM_COL32(255, 255, 255, (int)(255 * alpha)), n.m_Text.c_str());

        const char *status;
        if(!n.m_CustomStatus.empty())
            status = n.m_CustomStatus.c_str();
        else
            status = n.m_Enabled ? "Enabled" : "Disabled";
        ImU32 statusCol = n.m_Enabled ?
            IM_COL32(80, 200, 120, (int)(255 * alpha)) :
            IM_COL32(200, 80, 80, (int)(255 * alpha));
        d->AddText(ImVec2(nPos.x + 12 * scale, nPos.y + 24 * scale), statusCol, status);
    }
}

void CShalawaHud::AddNotification(const std::string &text, const std::string &status, bool isPositive)
{
    if(!m_Initialized)
        return;

    SHudNotification notif;
    notif.m_Text = text;
    notif.m_CustomStatus = status;
    notif.m_Enabled = isPositive;
    notif.m_SpawnTime = (float)SDL_GetTicks() / 1000.0f;
    notif.m_Progress = 0.0f;
    notif.m_SlideAnim = 0.0f;
    m_HudNotifications.push_back(notif);
    while(m_HudNotifications.size() > 5)
        m_HudNotifications.erase(m_HudNotifications.begin());
}

void CShalawaHud::RenderTasHud(ImVec2 pos, ImVec2 size, float scale)
{
    if(!m_pGameClient)
        return;

    ImDrawList *d = ImGui::GetBackgroundDrawList();

    bool recording = g_Config.m_shalawaRecording != 0;
    bool playing = g_Config.m_shalawaPlaying != 0;

    std::string status = recording ? "REC" : (playing ? "PLAY" : "IDLE");
    std::string tickStr;

    if(g_Config.m_shalawaTasWorld && g_Config.m_shalawaTasInit && m_pGameClient)
    {
        int tick = m_pGameClient->m_Controls.GetTasTick();
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", tick);
        tickStr = buf;
    }

    ImVec4 accentCol = HSVtoRGB(m_GradientHue, 0.65f, 0.95f);

    float padX = 10.0f * scale;
    float padY = 6.0f * scale;
    float labelW = ImGui::CalcTextSize("TAS").x;
    float statusW = ImGui::CalcTextSize(status.c_str()).x;
    float tickW = tickStr.empty() ? 0.0f : ImGui::CalcTextSize(tickStr.c_str()).x;
    float dotSpace = recording ? 14.0f * scale : 0.0f;
    float sepW = 10.0f;
    float totalW = labelW + sepW + dotSpace + statusW +
                   (tickW > 0 ? sepW + tickW : 0) + padX * 2;
    float totalH = ImGui::CalcTextSize("T").y + padY * 2;

    DrawBlurredShadow(d, pos, ImVec2(totalW, totalH), 4, ImVec4(0, 0, 0, 0.3f));
    d->AddRectFilled(pos, ImVec2(pos.x + totalW, pos.y + totalH),
                     IM_COL32(15, 15, 18, 230), 6.0f * scale);
    d->AddRectFilled(pos, ImVec2(pos.x + totalW, pos.y + 2),
                     ImGui::ColorConvertFloat4ToU32(accentCol),
                     6.0f * scale, ImDrawFlags_RoundCornersTop);

    float tx = pos.x + padX;
    float ty = pos.y + padY;

    d->AddText(ImVec2(tx, ty), ImGui::ColorConvertFloat4ToU32(accentCol), "TAS");
    tx += labelW + 4;
    d->AddText(ImVec2(tx, ty), IM_COL32(60, 60, 70, 200), "|");
    tx += sepW;

    if(recording)
    {
        float dotR = 3.5f * scale;
        float pulse = 0.5f + 0.5f * sinf(m_Time * 4.0f);
        d->AddCircleFilled(ImVec2(tx + dotR, ty + ImGui::CalcTextSize("R").y / 2),
                           dotR, IM_COL32(255, 50, 50, (int)(255 * pulse)));
        tx += dotR * 2 + 4;
    }

    ImU32 statusCol = recording ? IM_COL32(255, 80, 80, 255) :
                      (playing ? IM_COL32(80, 200, 120, 255) :
                                 IM_COL32(150, 150, 160, 200));
    d->AddText(ImVec2(tx, ty), statusCol, status.c_str());
    tx += statusW + 4;

    if(!tickStr.empty())
    {
        d->AddText(ImVec2(tx, ty), IM_COL32(60, 60, 70, 200), "|");
        tx += sepW;
        d->AddText(ImVec2(tx, ty), IM_COL32(200, 200, 210, 230), tickStr.c_str());
    }
}

void CShalawaHud::Update()
{
    ANTI_DISASM_BLOCK();
    if(!m_Initialized)
        return;

    if(!m_pGameClient)
        return;

    if(!m_pGameClient->Client())
        return;

    if(m_pGameClient->Client()->State() != IClient::STATE_ONLINE &&
       m_pGameClient->Client()->State() != IClient::STATE_DEMOPLAYBACK)
        return;

    UpdateTime();
    UpdateModules();
    UpdateSpeedHistory();
    m_Editor.Update();

    if(g_Config.m_shalawaHudEditor && !m_Editor.IsActive())
        m_Editor.SetActive(true);
    else if(!g_Config.m_shalawaHudEditor && m_Editor.IsActive())
        m_Editor.SetActive(false);
}

void CShalawaHud::Render()
{
    ANTI_DISASM_BLOCK();
    if(!m_Initialized || !m_pGameClient || !ImGui::GetCurrentContext())
        return;
    
    // Render watermark in fixed position (top-left by default)
    if(g_Config.m_shalawaWatermark && m_pWatermark)
    {
        ImGuiIO &io = ImGui::GetIO();
        ImVec2 pos(10.0f, 10.0f); // Fixed position top-left
        m_pWatermark->RenderWatermarkAtPosition(pos, g_Config.m_shalawaWatermarkStyle);
    }
    
    // Render TAS HUD in fixed position (top-right by default)
    if(g_Config.m_shalawaTasHud && (g_Config.m_shalawaRecording || g_Config.m_shalawaPlaying) && m_pGameClient)
    {
        ImGuiIO &io = ImGui::GetIO();
        ImVec2 pos(io.DisplaySize.x - 150.0f, 10.0f); // Fixed position top-right
        RenderTasHud(pos, ImVec2(140.0f, 30.0f), 1.0f);
    }
    
    m_Editor.Render();
    m_Editor.RenderEditorOverlay();
    
    // Welcome message is now shown in Notifications element
}

void CShalawaHud::RenderWelcomeMessage()
{
    ANTI_DISASM_BLOCK();
    if(!ImGui::GetCurrentContext())
        return;
    
    m_WelcomeMessageTime += m_DeltaTime;
    
    // Fade out after 4.5 seconds (0.5 seconds for fade animation)
    const float DISPLAY_TIME = 4.5f;
    const float FADE_TIME = 0.5f;
    const float TOTAL_TIME = DISPLAY_TIME + FADE_TIME;
    
    if(m_WelcomeMessageTime >= TOTAL_TIME)
    {
        m_WelcomeMessageAlpha = 0.0f;
        return;
    }
    
    // Calculate alpha
    if(m_WelcomeMessageTime >= DISPLAY_TIME)
    {
        // Fade out phase
        float fadeProgress = (m_WelcomeMessageTime - DISPLAY_TIME) / FADE_TIME;
        m_WelcomeMessageAlpha = 1.0f - fadeProgress;
    }
    else
    {
        m_WelcomeMessageAlpha = 1.0f;
    }
    
    if(m_WelcomeMessageAlpha <= 0.01f)
        return;
    
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *d = ImGui::GetBackgroundDrawList();
    
    const char *text = "Shalawa Loaded";
    ImVec2 textSize = ImGui::CalcTextSize(text);
    
    // Center position
    float x = (io.DisplaySize.x - textSize.x) / 2.0f;
    float y = io.DisplaySize.y * 0.85f; // Near bottom
    
    // Rainbow gradient effect
    ImVec4 color = HSVtoRGB(fmodf(m_GradientHue, 1.0f), 0.7f, 0.95f);
    color.w = m_WelcomeMessageAlpha;
    ImU32 col = ImGui::ColorConvertFloat4ToU32(color);
    
    // Shadow
    d->AddText(ImVec2(x + 2, y + 2), IM_COL32(0, 0, 0, (int)(100 * m_WelcomeMessageAlpha)), text);
    
    // Main text
    d->AddText(ImVec2(x, y), col, text);
}

bool CShalawaHud::ProcessEvent(void *pEvent)
{
    ANTI_DISASM_BLOCK();
    if(!pEvent)
        return false;
    return m_Editor.ProcessEvent(pEvent);
}
