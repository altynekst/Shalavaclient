#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_VISUALS_HUD_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_VISUALS_HUD_H

#include <functional>
#include <string>
#include <vector>
#include <imgui.h>
#include <unordered_map>

class CGameClient;
class CWatermark;

enum EAnchor
{
    ANCHOR_TOP_LEFT = 0,
    ANCHOR_TOP_RIGHT,
    ANCHOR_BOTTOM_LEFT,
    ANCHOR_BOTTOM_RIGHT,
    ANCHOR_TOP_CENTER,
    ANCHOR_BOTTOM_CENTER,
    ANCHOR_LEFT_CENTER,
    ANCHOR_RIGHT_CENTER,
    ANCHOR_CENTER,
    ANCHOR_COUNT
};

enum EHudElement
{
    HUD_ARRAYLIST = 0,
    HUD_KEYBINDS,
    HUD_SPEEDOMETER,
    HUD_TARGET,
    HUD_NOTIFICATIONS,
    HUD_IOF,
    HUD_COUNT
};

struct SSnapLine
{
    ImVec2 m_Start;
    ImVec2 m_End;
    bool m_Horizontal;
    float m_Alpha;
};

struct SHudElementData
{
    EHudElement m_Type;
    std::string m_Name;
    int *m_pX;
    int *m_pY;
    int *m_pAnchor;
    int *m_pScale;
    float m_BaseWidth;
    float m_BaseHeight;
    float m_DragOffsetX;
    float m_DragOffsetY;
    bool m_Dragging;
    bool m_Resizing;
    int m_ResizeCorner;
    float m_HoverAnim;
    float m_SelectAnim;
    std::function<void(ImVec2, ImVec2, float)> m_RenderFunc;
};

class CHudEditor
{
public:
    CHudEditor();
    
    void Init(CGameClient *pGameClient);
    void Update();
    void Render();
    void RenderEditorOverlay();
    bool ProcessEvent(void *pEvent);
    
    void RegisterElement(EHudElement type, const std::string &name,
                        int *pX, int *pY, int *pAnchor, int *pScale,
                        float baseW, float baseH,
                        std::function<void(ImVec2, ImVec2, float)> renderFunc);
    
    ImVec2 GetElementPosition(EHudElement type);
    ImVec2 GetElementSize(EHudElement type);
    float GetElementScale(EHudElement type);
    
    bool IsActive() const { return m_Active; }
    void SetActive(bool active) { m_Active = active; }
    
private:
    float Lerp(float a, float b, float t);
    float EaseOutBack(float t);
    ImVec4 LerpColor(ImVec4 a, ImVec4 b, float t);
    ImVec4 HSVtoRGB(float h, float s, float v);
    ImVec2 AnchorToScreen(int anchor, float offsetX, float offsetY, float width, float height);
    void ScreenToAnchor(ImVec2 pos, float width, float height, int &outAnchor, float &outX, float &outY);
    void UpdateSnapLines(SHudElementData &elem, ImVec2 pos, ImVec2 size);
    ImVec2 ApplySnap(SHudElementData &elem, ImVec2 pos, ImVec2 size);
    void HandleMouseInput();
    void HandleKeyboardInput();
    void DrawBlurredShadow(ImDrawList *d, ImVec2 pos, ImVec2 size, int range, ImVec4 color);
    bool IsInsideElement(ImVec2 mousePos, ImVec2 elemPos, ImVec2 elemSize);
    int GetResizeCorner(ImVec2 mousePos, ImVec2 elemPos, ImVec2 elemSize);
    void RenderSnapLines();
    void RenderElementOutline(SHudElementData &elem, ImVec2 pos, ImVec2 size);
    void RenderResizeHandles(SHudElementData &elem, ImVec2 pos, ImVec2 size);
    void RenderAnchorPreview(SHudElementData &elem);
    void RenderEditorUI();
    void RenderElement(SHudElementData &elem);
    
private:
    CGameClient *m_pGameClient;
    bool m_Active;
    bool m_Initialized;
    float m_DeltaTime;
    float m_LastTime;
    float m_Time;
    
    ImVec2 m_ScreenSize;
    ImVec2 m_ScreenCenter;
    
    std::vector<SHudElementData> m_Elements;
    std::vector<SSnapLine> m_SnapLines;
    
    int m_SelectedElement;
    int m_HoveredElement;
};
class CShalawaHud
{
public:
    CShalawaHud();
    
    static CShalawaHud &Instance();
    
    void Init(CGameClient *pGameClient);
    void SetWatermark(class CWatermark *pWatermark) { m_pWatermark = pWatermark; }
    void Update();
    void Render();
    bool ProcessEvent(void *pEvent);
    ImVec2 GetElementPosition(EHudElement type) { return m_Editor.GetElementPosition(type); }
    ImVec2 GetElementSize(EHudElement type) { return m_Editor.GetElementSize(type); }
    float GetElementScale(EHudElement type) { return m_Editor.GetElementScale(type); }
    CHudEditor &GetEditor() { return m_Editor; }
    void AddNotification(const std::string &text, bool enabled);
    void AddNotification(const std::string &text, const std::string &status, bool isPositive);
    
private:
    void SetupElementRenderers();
    float Lerp(float a, float b, float t);
    ImVec4 HSVtoRGB(float h, float s, float v);
    ImU32 GetAccent(float alpha = 1.0f);
    void DrawBlurredShadow(ImDrawList *d, ImVec2 pos, ImVec2 size, int range, ImVec4 color);
    void UpdateTime();
    std::string GetTimeString();
    void RenderIOF(ImVec2 pos, ImVec2 size, float scale);
    int GetPing();
        struct SHudModule {
        std::string m_Name;
        std::string m_Suffix;
        bool m_Enabled;
        bool m_PrevEnabled;
        float m_Animation;
        float m_Alpha;
    };
    std::unordered_map<std::string, SHudModule> m_Modules;

    float m_SpeedHistory[128];
    int m_SpeedHistoryIdx;
    float m_SpeedHistoryTimer;
    float m_MaxSpeed;
    float m_SmoothedSpeed;

    struct SHudNotification {
        std::string m_Text;
        std::string m_CustomStatus;
        bool m_Enabled;
        float m_SpawnTime;
        float m_Progress;
        float m_SlideAnim;
    };
    std::vector<SHudNotification> m_HudNotifications;
    

    void UpdateModules();
    void UpdateSpeedHistory();
    float EaseOutBack(float t);
    float SmoothStep(float t);
    float GetSpeed();
    std::string GetServerName();
    void RenderWatermark(ImVec2 pos, ImVec2 size, float scale);
    void RenderArraylist(ImVec2 pos, ImVec2 size, float scale);
    ImVec4 GetArraylistColor(int index, int total, float alpha);
    void RenderArraylistSkeet(ImDrawList *d, ImVec2 pos, ImVec2 size, std::vector<SHudModule*> &sorted);
    void RenderArraylistNeverlose(ImDrawList *d, ImVec2 pos, ImVec2 size, std::vector<SHudModule*> &sorted);
    void RenderArraylistGamesense(ImDrawList *d, ImVec2 pos, ImVec2 size, std::vector<SHudModule*> &sorted);
    void RenderArraylistCustom(ImDrawList *d, ImVec2 pos, ImVec2 size, std::vector<SHudModule*> &sorted);
    void RenderArraylistChatGPT(ImDrawList *d, ImVec2 pos, ImVec2 size, std::vector<SHudModule*> &sorted);
    void RenderArraylistClaude(ImDrawList *d, ImVec2 pos, ImVec2 size, std::vector<SHudModule*> &sorted);
    void RenderArraylistDeepSeek(ImDrawList *d, ImVec2 pos, ImVec2 size, std::vector<SHudModule*> &sorted);
    void RenderArraylistCloud(ImDrawList *d, ImVec2 pos, ImVec2 size, std::vector<SHudModule*> &sorted);
    void RenderArraylistSingularity(ImDrawList *d, ImVec2 pos, ImVec2 size, std::vector<SHudModule*> &sorted);
    void RenderKeybinds(ImVec2 pos, ImVec2 size, float scale);
    void RenderSpeedometer(ImVec2 pos, ImVec2 size, float scale);
    void RenderNotifications(ImVec2 pos, ImVec2 size, float scale);
    void RenderTasHud(ImVec2 pos, ImVec2 size, float scale);
    
private:
    CGameClient *m_pGameClient;
    class CWatermark *m_pWatermark;
    bool m_Initialized;
    float m_DeltaTime;
    float m_LastTime;
    float m_Time;
    unsigned int m_Millis;
    int m_Fps;
    int m_FrameCount;
    float m_FpsTimer;
    float m_GradientHue;
    float m_KeyStates[6];
    CHudEditor m_Editor;
    
    // Welcome message (old, replaced by notification system)
    float m_WelcomeMessageAlpha;
    float m_WelcomeMessageTime;
    void RenderWelcomeMessage();
    
    // Notification timer
    float m_NotificationTime;
};

#endif