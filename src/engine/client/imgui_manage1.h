#pragma once

#include <imgui.h>
#include <SDL.h>
#include <engine/graphics.h>
#include <string>
#include <unordered_map>
#include <map>
#include <vector>
#include <GL/glew.h>
#include <functional>
#include <game/client/components/shalawa/misc/watermark.h>

class CAutoAnswer;
class CChatSpam;
class CGameClient;

struct SFeatureCard
{
    std::string m_Id;
    std::string m_Name;
    std::string m_Description;
    int m_WeaponIcon = -1;
    std::string m_TextIcon;
    bool m_Expanded = false;
    std::function<void()> m_RenderContent;
    std::function<bool()> m_IsEnabled;
};

struct SColorSettingsModern
{
    int m_Mode = 0;
    float m_SolidColor[4] = {0.345f, 0.396f, 0.949f, 1.0f};
    float m_GradientColors[4][4] = {
        {0.345f, 0.396f, 0.949f, 1.0f},
        {0.922f, 0.271f, 0.620f, 1.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f, 1.0f}
    };
    int m_GradientColorCount = 2;
    float m_AnimationSpeed = 3.0f;
};

struct SMenuConfigModern
{
    int m_Theme = 0;
    SColorSettingsModern m_ColorSettings;
    bool m_CollapseSidebar = true;
    bool m_CompactMode = false;
    bool m_ShowDescriptions = true;
    float m_MenuOpacity = 1.0f;
    bool m_Animations = true;
};

class CImGuiManagerModern
{
public:
    static CImGuiManagerModern &Instance();
    

    void Init(SDL_Window *pWindow, SDL_GLContext Context);
    void Shutdown();
    bool ProcessEvent(SDL_Event *event);
    void Render();
    void RenderContent_Internal();
    CWatermark m_Watermark;
    bool IsMenuOpen() const { return m_MenuOpen; }
    void SetChatSpam(CChatSpam *pChatSpam) { m_pChatSpam = pChatSpam; }
    void SetGameClient(CGameClient *pGameClient);
    void InitFromParent(SDL_Window *pWindow);
    CGameClient *GetGameClient() { return m_pGameClient; }
    void SetLogoTexture(IGraphics::CTextureHandle Texture) { m_LogoTexture = Texture; }
    
    bool m_MenuOpen = false;
	bool m_TextInputActive = false;

private:
    CImGuiManagerModern();
    
    CAutoAnswer *m_pAutoAnswer = nullptr;
    CChatSpam *m_pChatSpam = nullptr;

    void BeginFrame();
    void EndFrame();
    void SetupStyle();
    void UpdateAnimations();
    void InitFeatureCards();
    bool m_IconsLoaded = false;
    void RenderContent(ImVec2 pos, ImVec2 size);
    bool m_IconsError = false;
    std::vector<std::string> m_MissingIcons;

    std::unordered_map<std::string, IGraphics::CTextureHandle> m_Icons;
    void LoadIcons(); // i dont know need is or no
    std::map<std::string, GLuint> m_TabIconsGL;
    bool m_TabIconsLoaded = false;
    void LoadTabIcons();
    GLuint LoadTextureFromPng(const char *path);
    ImTextureID GetIconTexture(const std::string &name);
    void RenderIcon(ImDrawList *d, ImVec2 pos, ImVec2 size, const std::string &name, float alpha = 1.0f);
    ImVec4 GetAccentColor(float alpha = 1.0f);
    ImVec4 GetGradientColor(float t, float alpha = 1.0f);
    ImU32 GetAccentU32(float alpha = 1.0f);
    ImVec4 LerpColor(ImVec4 a, ImVec4 b, float t);
    float GetAnimValue(const std::string &key, float target, float speed = 12.0f);
    
    ImTextureID GetWeaponTexture(int weapon);
    void RenderWeaponIcon(ImDrawList *d, ImVec2 pos, ImVec2 size, int weapon, float alpha = 1.0f);
    
    void RenderBackground(ImVec2 pos, ImVec2 size);
    void RenderSidebar(ImVec2 pos, ImVec2 size);
    void RenderHeader(ImVec2 pos, ImVec2 size);
    void RenderErrorWindow();
    void RenderSearchBar(ImVec2 pos, float width);
    void RenderSearchResults();
    
    void RenderFeatureCard(SFeatureCard &card, float width);
    bool RenderToggle(const char *id, bool *v);
    bool RenderToggle(const char *id, int *v);
    bool RenderSlider(const char *label, int *v, int min, int max);
    bool RenderSliderFloat(const char *label, float *v, float min, float max);
    bool RenderCombo(const char *label, int *current, const char *const items[], int count);
    bool RenderButton(const char *label, bool primary = true);
    bool RenderColorPicker(const char *label, float col[4]);
    void RenderSectionHeader(const char *label);
    void RenderSubText(const char *text);
    bool RenderInputText(const char *label, char *buf, size_t bufSize, const char *hint = nullptr);
    void RefreshTasReplayList();
    
    SDL_Window *m_pWindow = nullptr;
    IGraphics::CTextureHandle m_LogoTexture;
    bool m_Initialized = false;
    CGameClient *m_pGameClient = nullptr;
    
    SMenuConfigModern m_Config;
    
    int m_CurrentTab = 0;
    int m_PreviousTab = -1;
    float m_TabTransition = 1.0f;
    
    float m_MenuAlpha = 0.0f;
    float m_MenuScale = 0.9f;
    float m_GradientTime = 0.0f;
    float m_GlowPulse = 0.0f;
    double m_LastTime = 0.0;
    float m_DeltaTime = 0.016f;
    
    char m_SearchQuery[128] = "";
    bool m_SearchFocused = false;
    int m_SearchSelectedIndex = -1;
    
    std::unordered_map<std::string, float> m_AnimState;
    std::vector<SFeatureCard> m_FeatureCards[8];
    
    char m_aAutoAnswerTrigger[128] = "";
    char m_aAutoAnswerResponse[256] = "";
    bool m_AutoAnswerCaseSensitive = false;
    bool m_AutoAnswerExactMatch = false;
    char m_aChatSpamMessage[256] = "";
    char m_ConfigName[64] = "";
    char m_aTasRecordName[64] = "";
    std::vector<std::string> m_vTasReplays;
    int m_TasReplayIndex = -1;
    bool m_TasReplayListDirty = true;
    
    struct SSearchResult
    {
        std::string m_Name;
        std::string m_Category;
        std::string m_TextIcon;
        int m_WeaponIcon;
        int m_TabIndex;
        std::string m_CardId;
    };
    std::vector<SSearchResult> m_SearchResults;
    
    struct SNavItem
    {
        std::string m_Icon;
        std::string m_Label;
        int m_Tab;
    };

    std::vector<SNavItem> m_FeaturesNav;
    std::vector<SNavItem> m_SystemNav;

    struct
    {
        ImVec4 m_BgPrimary;
        ImVec4 m_BgSecondary;
        ImVec4 m_BgTertiary;
        ImVec4 m_BgHover;
        ImVec4 m_TextPrimary;
        ImVec4 m_TextSecondary;
        ImVec4 m_TextTertiary;
        ImVec4 m_Border;
        ImVec4 m_BorderLight;
    } m_Theme;
    
    enum EWeaponIcon
    {
        WEAPON_HAMMER = 0,
        WEAPON_GUN,
        WEAPON_SHOTGUN,
        WEAPON_GRENADE,
        WEAPON_LASER,
        WEAPON_NINJA,
        WEAPON_HOOK,
        NUM_WEAPONS
    };
};