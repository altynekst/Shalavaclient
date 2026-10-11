#pragma once

#include <imgui.h>
#include <SDL.h>
#include <engine/graphics.h>
#include <string>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <vector>
#include <GL/glew.h>
#include <functional>
#include <cmath>

#include "menu_shared.h"
#include <game/client/components/shalawa/misc/watermark.h>
#include <game/client/components/shalawa/visuals/hitparticles.h>
#include "glass_renderer.h"
#include <game/client/components/shalawa/visuals/chinahat.h>
#include <game/client/components/shalawa/visuals/tail.h>

class CAutoAnswer;
class CChatSpam;
class CGameClient;

struct SSnowflake
{
    float x, y, size, speed, alpha, wobble, wobbleSpeed;
};

struct SColorSettings
{
    int m_Mode = 0;
    float m_SolidColor[4] = {0.4f, 0.6f, 1.0f, 1.0f};
    float m_EasingColor[4] = {1.0f, 0.4f, 0.7f, 1.0f};
    bool m_Easing = true;
    float m_AnimSpeed = 3.0f;
};

struct SMenuConfig
{
    SColorSettings m_Color;
    bool m_Snow = true;
    bool m_Blur = false;
    bool m_Glow = true;
    float m_GlowStrength = 5.0f;
    bool m_Icons = true;
    int m_MenuStyle = 0;
    bool m_Descriptions = true;
    bool m_ShowTyan = true;
    float m_BackgroundAlpha = 0.4f;
};

struct SCheckboxAnim
{
    float m_CheckAnim = 0.0f;
    float m_HoverAnim = 0.0f;
    float m_StretchAnim = 0.0f;
    float m_Velocity = 0.0f;
};

struct SCardAnim
{
	float m_HoverAnim = 0.0f;
	float m_ExpandAnim = 0.0f;
	float m_ToggleAnim = 0.0f;
	float m_PlusRotation = 0.0f;
	float m_AnimHeight = 65.0f;
	float m_ContentAlpha = 0.0f;
	float m_LastSettingsH = 0.0f;
	bool m_Expanded = false;
};

struct SSliderAnim
{
    float m_AnimatedPercentage = 0.0f;
    float m_KnobAnim = 0.0f;
    float m_HoverAnim = 0.0f;
    float m_InputAnim = 0.0f;
    float m_UnitsAlpha = 1.0f;
    float m_ValueOffsetX = 0.0f;
    float m_BackgroundAlpha = 0.0f;
    bool m_Dragging = false;
    bool m_InputMode = false;
    std::string m_InputText;
    int m_CursorPos = 0;
};

struct SButtonAnim
{
    float m_PressAnim = 0.0f;
    float m_HoverAnim = 0.0f;
    float m_ScaleAnim = 1.0f;
    float m_RippleAnim = 0.0f;
    float m_RippleX = 0.0f;
    float m_RippleY = 0.0f;
    bool m_WasPressed = false;
    bool m_RippleActive = false;
};

struct STextInputAnim
{
    float m_FocusAnim = 0.0f;
    float m_HoverAnim = 0.0f;
    float m_ScrollOffset = 0.0f;
    float m_TargetScroll = 0.0f;
    float m_CursorBlink = 0.0f;
    float m_SelectionAnim = 0.0f;
    bool m_Focused = false;
    int m_CursorPos = 0;
    int m_SelectionStart = -1;
    int m_SelectionEnd = -1;
    std::string m_Text;
};

struct SColorPickerAnim
{
    bool m_Expanded = false;
    float m_ExpandAnim = 0.0f;
    float m_HoverAnim = 0.0f;
    float m_PreviewHoverAnim = 0.0f;
    float m_ContentAlpha = 0.0f;
    float m_PaletteHandleAnim = 0.0f;
    float m_HueHandleAnim = 0.0f;
    float m_AlphaHandleAnim = 0.0f;
    bool m_DraggingPalette = false;
    bool m_DraggingHue = false;
    bool m_DraggingAlpha = false;
    float m_DisplayHue = 0.0f;
    float m_DisplaySat = 1.0f;
    float m_DisplayBri = 1.0f;
    float m_DisplayAlpha = 1.0f;
    bool m_HexActive = false;
    std::string m_HexText;
    int m_HexCursor = 0;
    int m_HexSelStart = -1;
    int m_HexSelEnd = -1;
    float m_HexInputAnim = 0.0f;
    float m_HexCursorBlink = 0.0f;
    float m_HexSelectionAnim = 0.0f;
};

struct SComboAnim
{
    float m_HoverAnim = 0.0f;
    bool m_Expanded = false;
    float m_ExpandAnim = 0.0f;
    std::unordered_map<std::string, float> m_OptionHovers;
};

struct SMultiSelectAnim
{
    float m_HoverAnim = 0.0f;
    float m_ExpandAnim = 0.0f;
    float m_ArrowRotation = 0.0f;
    float m_NoneAlpha = 0.0f;
    float m_ScrollOffset = 0.0f;
    float m_ScrollAnimated = 0.0f;
    bool m_Expanded = false;
    bool m_ScrollingRight = true;
    long long m_ScrollPauseTime = 0;
    std::unordered_map<std::string, float> m_OptionHovers;
    std::unordered_map<std::string, float> m_CheckAnims;
    std::unordered_map<std::string, float> m_ItemAlphas;
    std::unordered_map<std::string, float> m_ItemPositions;
    std::unordered_map<std::string, float> m_ItemTargets;
};

struct SModuleAnim
{
    float m_HoverAnim = 0.0f;
    float m_EnableAnim = 0.0f;
    float m_StarAnim = 0.0f;
};

class CImGuiManager
{
public:
    static CImGuiManager &Instance();

    void Init(SDL_Window *pWindow, SDL_GLContext Context);
    void Shutdown();
    bool ProcessEvent(SDL_Event *event);
    void Render();

    void SetGameClient(CGameClient *pGameClient);
    CGameClient *GetGameClient() { return m_pGameClient; }
    void SetAutoAnswer(CAutoAnswer *p) { m_pAutoAnswer = p; }
    void SetChatSpam(CChatSpam *p) { m_pChatSpam = p; }

    bool IsMenuOpen() const { return m_MenuOpen; }
    bool m_MenuOpen = false;

    SMenuConfig m_Config;
    CWatermark m_Watermark;
    int m_MenuStyle = 0;
    bool m_ShowStartMenu = false;
    bool m_StartMenuUnlocked = false;
    int m_StartMenuClickedButton = -1;

    char m_aAutoAnswerTrigger[128] = {0};
    char m_aAutoAnswerResponse[256] = {0};
    char m_aChatSpamMessage[256] = {0};

private:
    CImGuiManager();

    void BeginFrame();
    void EndFrame();
    void SetupStyle();
    void UpdateAnimations();
    void RegisterModules();

    void InitSnowflakes();
    std::unordered_map<std::string, SCardAnim> m_CardAnims;
    float m_TabContentAnim = 1.0f;
    float m_TabIndicatorX = 0.0f;
    float m_TabIndicatorW = 100.0f;
    float m_TabHoverAnims[MCAT_COUNT] = {};
    float m_CardScrollTarget = 0.0f;
    float m_CardScrollCurrent = 0.0f;
    std::mt19937 m_Rng;
    bool m_RngInit = false;
    void UpdateSnowflakes();
    void LoadTyanTexture();

    void RenderMenu();
    void RenderBackground();
    void RenderTyan();
    void RenderSnowflakes();

    void RenderMenuWindow();
    void RenderTabBar(float x, float y, float w, float alpha);
    void RenderCardGrid(float x, float y, float w, float h, float alpha);
    float RenderCard(SModuleEntry *pMod, float x, float y, float w, float alpha);
    void RenderPillToggle(ImDrawList *d, float x, float y, float tw, float th, bool isOn, SCardAnim &anim, float alpha);

    float RenderCheckbox(SSettingBase &setting, float x, float y, float w, float alpha = 1.0f);
    float RenderSlider(SSettingBase &setting, float x, float y, float w, float alpha = 1.0f);
    float RenderCombo(SSettingBase &setting, float x, float y, float w, float alpha = 1.0f);
    float RenderTextInput(SSettingBase &setting, float x, float y, float w, float alpha = 1.0f);
    float RenderButton(SSettingBase &setting, float x, float y, float w, float alpha = 1.0f);
    float RenderColorPicker(SSettingBase &setting, float x, float y, float w, float alpha = 1.0f);
    float RenderMultiSelect(SSettingBase &setting, float x, float y, float w, float alpha = 1.0f);
    float RenderTriggerList(SSettingBase &setting, float x, float y, float w, float alpha = 1.0f);

    void RenderSatBriPalette(float x, float y, float w, float h, SColorPickerAnim &anim, SSettingBase &setting);
    void RenderHueSlider(float x, float y, float w, float h, SColorPickerAnim &anim, SSettingBase &setting);
    void RenderAlphaSlider(float x, float y, float w, float h, SColorPickerAnim &anim, SSettingBase &setting);
    void RenderHexInput(float x, float y, float w, float h, SColorPickerAnim &anim, SSettingBase &setting);

    float Lerp(float a, float b, float t);
    float LerpHue(float current, float target, float speed);
    float SmoothStep(float t);
    float EaseOutBack(float t);
    float EaseOutBounce(float t);
    ImVec4 LerpColor(ImVec4 a, ImVec4 b, float t);
    ImVec4 GetAccentColor(float yPos);
    ImU32 GetAccentU32(float yPos, float alpha = 1.0f);
    ImVec4 HSBtoRGB(float h, float s, float b);
    float GetAnimValue(const std::string &key, float target, float speed = 12.0f);
    bool IsHover(float mx, float my, float x, float y, float w, float h);
    ImU32 ApplyAlpha(ImVec4 col, float alpha);
    ImU32 ApplyAlpha(ImU32 col, float alpha);

    SDL_Window *m_pWindow = nullptr;
    bool m_Initialized = false;
    CGameClient *m_pGameClient = nullptr;
    CAutoAnswer *m_pAutoAnswer = nullptr;
    CChatSpam *m_pChatSpam = nullptr;

    EMenuCategory m_SelectedCategory = MCAT_COMBAT;
    std::string m_SelectedModuleId;
    SModuleEntry *GetSelectedModule()
    {
        if(m_SelectedModuleId.empty())
            return nullptr;
        return CMenuRegistry::Instance().FindModule(m_SelectedModuleId);
    }
    std::string m_BindingModuleId;

    float m_MenuAlpha = 0.0f;
    float m_MenuScale = 0.9f;
    float m_BackgroundAlpha = 0.0f;
    float m_OpenAnimValue = 0.0f;
    bool m_Closing = false;
    EMenuCategory m_PreviousCategory = MCAT_COMBAT;
    float m_HeaderTransition = 1.0f;
    float m_HeaderSlideDistance = 8.0f;

    char m_SearchBuffer[128] = {0};
    bool m_SearchActive = false;
    bool m_SearchFocused = false;
    float m_SearchBoxAnim = 0.0f;
    bool m_StartMenuInitialized = false;
    float m_StartMenuUnlockTextPulse = 0.0f;
    float m_StartMenuUnlockAlpha = 1.0f;
    int m_TriggerEditIndex = -1;
    char m_aTriggerEditTrigger[128] = {0};
    char m_aTriggerEditResponse[256] = {0};
    int m_TriggerEditCaseSensitive = 0;
    int m_TriggerEditExact = 0;
    float m_TriggerListScrollTarget = 0.0f;
    float m_TriggerListScrollCurrent = 0.0f;
    float m_StartMenuMenuProgress = 0.0f;
    float m_StartMenuZoom = 1.08f;
    float m_StartMenuTargetZoom = 1.08f;
    long long m_StartMenuUnlockTime = 0;

    float m_StartMenuButtonScales[6] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
    float m_StartMenuButtonHover[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    float m_StartMenuExitRed = 0.0f;
    int m_StartMenuHoveredButton = -1;

    float m_StartMenuClockBreath = 0.0f;
    float m_StartMenuTgHover = 0.0f;

    bool m_StartMenuMusicStarted = false;
    int m_StartMenuMusicSampleId = -1;
    int m_TrollCommentIndex = -1;
    bool m_TrollCommentChosen = false;
    void RenderGameVisuals();
    void RenderStartMenuImGui();
    void RenderStartMenuUnlockScreen(float screenW, float screenH);
    void RenderStartMenuMain(float screenW, float screenH);
    void UpdateStartMenuAnimations();
    GLuint LoadTextureFromPng(const char *path);

    float m_SearchFocusAnim = 0.0f;
    float m_SearchPanelAlpha = 0.0f;
    float m_NormalPanelAlpha = 1.0f;
    float m_SearchCursorBlink = 0.0f;
    int m_SearchCursorPos = 0;
    std::vector<SModuleEntry *> m_SearchResults;
    float m_SearchScrollTarget = 0.0f;
    float m_SearchScrollCurrent = 0.0f;
    std::string m_SelectedSearchModuleId;
    SModuleEntry *GetSelectedSearchModule()
    {
        if(m_SelectedSearchModuleId.empty())
            return nullptr;
        return CMenuRegistry::Instance().FindModule(m_SelectedSearchModuleId);
    }
    std::unordered_map<std::string, float> m_SearchResultAnims;

    void RenderSearchBar(float x, float y, float w);
    void RenderSearchResults(float x, float y, float w, float h);
    void UpdateSearchResults();
    float EaseOutQuart(float t);
    void RenderKeybindsPanel(float x, float y, float w, float h, float alpha = 1.0f);
    float RenderKeybindEntry(SModuleEntry *pMod, float x, float y, float w, float alpha);
    std::string GetKeyName(int sdlKey);
    bool m_WaitingForKey = false;
    std::string m_BindingModuleForKey;
    std::unordered_map<std::string, float> m_KeybindAnims;
    float m_KeybindScrollTarget = 0.0f;
    float m_KeybindScrollCurrent = 0.0f;
public:
    CHitParticles m_HitParticles;
    CChinaHat m_ChinaHat;
    CTail m_Tail;

private:

    float m_TyanAlpha = 0.0f;
    float m_TyanBounce = 0.0f;
    float m_SnowAlpha = 0.0f;
    GLuint m_TyanTextureGL = 0;
    bool m_TyanLoaded = false;
    std::map<std::string, GLuint> m_StartMenuIconsGL;
    bool m_StartMenuIconsLoaded = false;
    void LoadStartMenuIcons();

    double m_LastTime = 0.0;
    float m_DeltaTime = 0.016f;
    long long m_Millis = 0;
    float m_Time = 0.0f;

    float m_MouseLerpX = 0.0f;
    float m_MouseLerpY = 0.0f;

    bool m_Dragging = false;
    float m_AccentHue1 = 0.6f;
    float m_AccentSat1 = 0.5f;
    float m_AccentBri1 = 1.0f;
    float m_AccentAlpha1 = 1.0f;
    float m_AccentHue2 = 0.95f;
    float m_ChinaHatHue1 = 0.95f;
    float m_ChinaHatSat1 = 0.8f;
    float m_ChinaHatBri1 = 1.0f;
    float m_ChinaHatAlpha1 = 1.0f;
    float m_ChinaHatHue2 = 0.72f;
    float m_ChinaHatSat2 = 0.8f;
    float m_ChinaHatBri2 = 1.0f;
    float m_ChinaHatAlpha2 = 1.0f;
    float m_ChinaHatHue3 = 0.8f;
    float m_ChinaHatSat3 = 0.6f;
    float m_ChinaHatBri3 = 1.0f;
    float m_ChinaHatAlpha3 = 1.0f;
    float m_AccentSat2 = 0.5f;
    float m_AccentBri2 = 1.0f;
    float m_AccentAlpha2 = 1.0f;
    float m_DragOffsetX = 0.0f;
    float m_DragOffsetY = 0.0f;
    float m_WindowOffsetX = 0.0f;
    float m_WindowOffsetY = 0.0f;
    float m_CategorySelectAnims[MCAT_COUNT] = {};
    float m_CategoryHoverAnims[MCAT_COUNT] = {};
    float m_ModuleScrollTarget = 0.0f;
    float m_ModuleScrollCurrent = 0.0f;
    SModuleEntry *m_pMenuSettingsModule = nullptr;
    float m_SettingScrollTarget = 0.0f;
    float m_SettingScrollCurrent = 0.0f;

    std::unordered_map<int, float> m_CategoryAnims;
    float m_CategorySelectorY = 0.0f;

    std::unordered_map<std::string, SCheckboxAnim> m_CheckboxAnims;
    std::unordered_map<std::string, SSliderAnim> m_SliderAnims;
    std::unordered_map<std::string, SButtonAnim> m_ButtonAnims;
    std::unordered_map<std::string, STextInputAnim> m_TextInputAnims;
    std::unordered_map<std::string, SColorPickerAnim> m_ColorPickerAnims;
    std::unordered_map<std::string, SComboAnim> m_ComboAnims;
    std::unordered_map<std::string, SMultiSelectAnim> m_MultiSelectAnims;
    std::unordered_map<std::string, SModuleAnim> m_ModuleAnims;
    std::unordered_map<std::string, float> m_AnimState;

    std::vector<SSnowflake> m_Snowflakes;

    static constexpr float BG_WIDTH = 720.0f;
    static constexpr float BG_HEIGHT = 500.0f;
    static constexpr float TAB_BAR_H = 40.0f;
    static constexpr float CARD_GAP = 8.0f;
    static constexpr float CARD_PADDING = 12.0f;
    static constexpr float CARD_COLLAPSED_H = 68.0f;
    static constexpr float CARD_CORNER_R = 14.0f;
    static constexpr float MENU_CORNER_R = 16.0f;
    static constexpr float MENU_PADDING = 12.0f;
    static constexpr float SETTING_ITEM_H = 22.0f;
};