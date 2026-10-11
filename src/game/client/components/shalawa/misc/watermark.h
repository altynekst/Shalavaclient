#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_MISC_WATERMARK_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_MISC_WATERMARK_H

#include <imgui.h>
#include <string>
#include <vector>
#include <unordered_map>

class CGameClient;

class CWatermark
{
public:
    CWatermark();
    
    void Init(CGameClient *pGameClient);
    void RenderWatermarkAtPosition(ImVec2 pos, int style);
    void Render();
    void OnModuleToggle(const char *name, bool enabled);
    void AddNotification(const char *text, bool isPositive);
    void SetModuleState(const std::string &name, bool enabled, const std::string &suffix = "");
    
private:
    CGameClient *m_pGameClient;
    bool m_Initialized;
    
    int m_Fps;
    int m_FrameCount;
    float m_FpsTimer;
    float m_LastTime;
    float m_DeltaTime;
    float m_AnimTime;
    float m_GradientHue;
    long long m_Millis;
    
    struct SNotification
    {
        std::string m_Text;
        bool m_Enabled;
        float m_SpawnTime;
        float m_Progress;
        float m_SlideAnim;
    };
    
    struct SArraylistEntry
    {
        std::string m_Name;
        std::string m_Suffix;
        bool m_Enabled;
        float m_Animation;
        float m_Alpha;
    };
    
    std::vector<SNotification> m_Notifications;
    std::unordered_map<std::string, SArraylistEntry> m_ArraylistEntries;
    
    float m_KeyStates[6];
    float m_WatermarkAlpha;
    float m_ArraylistAlpha;
    
    void UpdateTime();
    void UpdateNotifications();
    void UpdateArraylist();
    std::string BuildInfoString(const char *separator);
    std::vector<std::string> BuildInfoSegments();

    void RenderWatermarkModern();
    void RenderWatermarkRise();
    void RenderWatermarkGamesense();
    void RenderWatermarkNL();
    void RenderWatermarkSkeet();
    void RenderWatermarkStylo();
    void RenderWatermarkMinimal();
    void RenderWatermarkPills();
    void RenderWatermarkClaude();
    void RenderWatermarkGemini();
    void RenderWatermarkGPT();
    void RenderWatermarkGrok();
    void RenderWatermarkSeed();
    void RenderWatermarkDeepSeek();
    
    void RenderArraylist();
    void RenderNotifications(); 
    std::string GetTimeString();
    std::string GetDateString();
    int GetPing();
    int GetPlayerCount();
    std::string GetServerName();
    float GetSpeed();
    
    ImU32 GetAccent(float alpha = 1.0f);
    ImU32 GetArraylistColor(int index, int total, float alpha = 1.0f);
    ImVec4 GetAstolfoColor(float offset);
    ImVec4 GetRainbowColor(float offset, int mode);
    ImVec4 HSVtoRGB(float h, float s, float v);
    ImVec4 LerpColor(ImVec4 a, ImVec4 b, float t);
    float Lerp(float a, float b, float t);
    float SmoothStep(float t);
    ImVec2 GetWatermarkPosition(float width, float height);
    float EaseOutBack(float t);
    float EaseOutElastic(float t);
    
    void DrawBlurredShadow(ImDrawList *d, ImVec2 pos, ImVec2 size, int range, ImVec4 color);
    void DrawGradientRectH(ImDrawList *d, ImVec2 min, ImVec2 max, ImU32 left, ImU32 right);
    void DrawGradientRectV(ImDrawList *d, ImVec2 min, ImVec2 max, ImU32 top, ImU32 bottom);
};

#endif