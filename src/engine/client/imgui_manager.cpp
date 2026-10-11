#include "imgui_manager.h"
#include "imgui_manage1.h"
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <SDL.h>
#include <base/system.h>
#include <engine/shared/config.h>
#include <cmath>
#include <algorithm>
#include <random>

#include <game/client/gameclient.h>
#include <game/client/components/shalawa/visuals/hud.h>
#include <game/client/components/shalawa/misc/autoanswer.h>
#include <game/client/components/shalawa/misc/chatspam.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>

CImGuiManager::CImGuiManager() { ANTI_DISASM_BLOCK(); }

CImGuiManager &CImGuiManager::Instance()
{
    ANTI_DISASM_BLOCK();
    static CImGuiManager s_Instance;
    return s_Instance;
}

float CImGuiManager::Lerp(float a, float b, float t)
{
    return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
}

float CImGuiManager::LerpHue(float current, float target, float speed)
{
    float diff = target - current;
    if(diff > 0.5f) diff -= 1.0f;
    else if(diff < -0.5f) diff += 1.0f;
    if(std::abs(diff) < 0.001f) return target;
    float result = current + diff * std::min(speed, 1.0f);
    if(result < 0.0f) result += 1.0f;
    if(result > 1.0f) result -= 1.0f;
    return result;
}

float CImGuiManager::SmoothStep(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float CImGuiManager::EaseOutBack(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    float c1 = 1.70158f;
    float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
}

float CImGuiManager::EaseOutBounce(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    float n1 = 7.5625f, d1 = 2.75f;
    if(t < 1.0f / d1) return n1 * t * t;
    else if(t < 2.0f / d1) return n1 * (t -= 1.5f / d1) * t + 0.75f;
    else if(t < 2.5f / d1) return n1 * (t -= 2.25f / d1) * t + 0.9375f;
    else return n1 * (t -= 2.625f / d1) * t + 0.984375f;
}

ImVec4 CImGuiManager::LerpColor(ImVec4 a, ImVec4 b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

ImVec4 CImGuiManager::HSBtoRGB(float h, float s, float b)
{
    if(!std::isfinite(h)) h = 0.0f;
    if(!std::isfinite(s)) s = 0.0f;
    if(!std::isfinite(b)) b = 0.0f;
    h = std::clamp(h, 0.0f, 1.0f);
    s = std::clamp(s, 0.0f, 1.0f);
    b = std::clamp(b, 0.0f, 1.0f);
    float c = b * s;
    float x = c * (1.0f - std::abs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
    float m = b - c;
    float r, g, bl;
    if(h < 1.0f/6.0f) { r=c; g=x; bl=0; }
    else if(h < 2.0f/6.0f) { r=x; g=c; bl=0; }
    else if(h < 3.0f/6.0f) { r=0; g=c; bl=x; }
    else if(h < 4.0f/6.0f) { r=0; g=x; bl=c; }
    else if(h < 5.0f/6.0f) { r=x; g=0; bl=c; }
    else { r=c; g=0; bl=x; }
    return ImVec4(r+m, g+m, bl+m, 1.0f);
}

ImVec4 CImGuiManager::GetAccentColor(float yPos)
{
    ImGuiIO &io = ImGui::GetIO();
    float t = std::clamp(yPos * 2.0f / io.DisplaySize.y, 0.0f, 1.0f);
    ImVec4 c1(m_Config.m_Color.m_SolidColor[0], m_Config.m_Color.m_SolidColor[1],
              m_Config.m_Color.m_SolidColor[2], m_Config.m_Color.m_SolidColor[3]);
    if(m_Config.m_Color.m_Easing)
    {
        ImVec4 c2(m_Config.m_Color.m_EasingColor[0], m_Config.m_Color.m_EasingColor[1],
                  m_Config.m_Color.m_EasingColor[2], m_Config.m_Color.m_EasingColor[3]);
        return LerpColor(c1, c2, t);
    }
    return c1;
}

ImU32 CImGuiManager::GetAccentU32(float yPos, float alpha)
{
    ImVec4 c = GetAccentColor(yPos);
    c.w = alpha;
    return ImGui::ColorConvertFloat4ToU32(c);
}

float CImGuiManager::GetAnimValue(const std::string &key, float target, float speed)
{
    if(key.empty() || !std::isfinite(target) || !std::isfinite(speed))
        return target;

    if(m_AnimState.size() > 5000)
    {
        std::unordered_map<std::string, float> cleaned;
        cleaned.reserve(512);
        for(auto &pair : m_AnimState)
        {
            if(std::abs(pair.second) > 0.01f && std::abs(pair.second - 1.0f) > 0.01f)
                cleaned[pair.first] = pair.second;
        }
        m_AnimState = std::move(cleaned);
    }

    auto it = m_AnimState.find(key);
    if(it == m_AnimState.end())
    {
        m_AnimState[key] = target > 0.5f ? 0.0f : 1.0f;
        it = m_AnimState.find(key);
    }
    float delta = (target - it->second) * m_DeltaTime * speed;
    if(!std::isfinite(delta))
        delta = 0.0f;
    it->second += delta;
    if(!std::isfinite(it->second))
        it->second = target;
    if(std::abs(it->second - target) < 0.001f)
        it->second = target;
    return it->second;
}

bool CImGuiManager::IsHover(float mx, float my, float x, float y, float w, float h)
{
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

ImU32 CImGuiManager::ApplyAlpha(ImVec4 col, float alpha)
{
    col.w *= alpha;
    return ImGui::ColorConvertFloat4ToU32(col);
}

ImU32 CImGuiManager::ApplyAlpha(ImU32 col, float alpha)
{
    int a = (col >> 24) & 0xFF;
    a = (int)(a * alpha);
    return (col & 0x00FFFFFF) | ((a & 0xFF) << 24);
}

void CImGuiManager::SetupStyle()
{
    ImGuiStyle &s = ImGui::GetStyle();
    s.WindowRounding = 0.0f;
    s.FrameRounding = 6.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding = 6.0f;
    s.WindowPadding = ImVec2(0, 0);
    s.FramePadding = ImVec2(6, 3);
    s.ItemSpacing = ImVec2(6, 3);
    s.WindowBorderSize = 0.0f;
    s.FrameBorderSize = 0.0f;
}

void CImGuiManager::Init(SDL_Window *pWindow, SDL_GLContext Context)
{
    ANTI_DISASM_BLOCK();
    if(m_Initialized)
        return;

    m_pWindow = pWindow;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    io.Fonts->Clear();
    ImFontConfig fontCfg;
    fontCfg.OversampleH = 2;
    fontCfg.OversampleV = 2;
    fontCfg.PixelSnapH = true;

    ImFont *font = nullptr;
    try {
        font = io.Fonts->AddFontFromFileTTF("data/fonts/Inter-Regular.ttf", 15.0f, &fontCfg);
        if(!font) font = io.Fonts->AddFontFromFileTTF("data/fonts/DejaVuSans.ttf", 15.0f, &fontCfg);
    } catch(...) {
        font = nullptr;
    }
    if(!font) io.Fonts->AddFontDefault(&fontCfg);

    SetupStyle();
    InitSnowflakes();
    RegisterModules();

    m_MenuStyle = g_Config.m_shalawaMenuStyle;
    m_Config.m_MenuStyle = g_Config.m_shalawaMenuStyle;

    ImGui_ImplSDL2_InitForOpenGL(pWindow, Context);
    ImGui_ImplOpenGL3_Init("#version 130");

    CGlassRenderer::Instance().Init();
    m_Initialized = true;
    m_LastTime = (double)SDL_GetTicks() / 1000.0;
}

void CImGuiManager::SetGameClient(CGameClient *pGameClient)
{
    ANTI_DISASM_BLOCK();
    m_pGameClient = pGameClient;
    if(!pGameClient)
    {
        m_pAutoAnswer = nullptr;
        m_pChatSpam = nullptr;
        return;
    }
    m_Watermark.Init(pGameClient);
    CShalawaHud::Instance().Init(pGameClient);
    CShalawaHud::Instance().SetWatermark(&m_Watermark);
    if(!m_HitParticles.GameClient())
    {
        m_HitParticles.OnInterfacesInit(pGameClient);
        m_HitParticles.OnInit();
    }
    if(!m_ChinaHat.GameClient())
    {
        m_ChinaHat.OnInterfacesInit(pGameClient);
        m_ChinaHat.OnInit();
    }
    if(!m_Tail.GameClient())
    {
        m_Tail.OnInterfacesInit(pGameClient);
        m_Tail.OnInit();
    }
    m_pAutoAnswer = &pGameClient->m_AutoAnswer;
    m_pChatSpam = &pGameClient->m_ChatSpam;
}

void CImGuiManager::Shutdown()
{
    ANTI_DISASM_BLOCK();
    CGlassRenderer::Instance().Shutdown();
    if(!m_Initialized) return;
    m_Initialized = false;
    m_CardAnims.clear();
    m_SelectedModuleId.clear();
    m_SelectedSearchModuleId.clear();
    m_BindingModuleId.clear();
    if(m_TyanTextureGL != 0)
    {
        glDeleteTextures(1, &m_TyanTextureGL);
        m_TyanTextureGL = 0;
    }
    for(auto &pair : m_StartMenuIconsGL)
    {
        if(pair.second != 0)
            glDeleteTextures(1, &pair.second);
    }
    m_StartMenuIconsGL.clear();
    m_AnimState.clear();
    m_CheckboxAnims.clear();
    m_SliderAnims.clear();
    m_ButtonAnims.clear();
    m_TextInputAnims.clear();
    m_ColorPickerAnims.clear();
    m_ComboAnims.clear();
    m_MultiSelectAnims.clear();
    m_ModuleAnims.clear();
    m_SearchResults.clear();
    m_SearchResultAnims.clear();
    m_KeybindAnims.clear();
    m_WaitingForKey = false;
    m_BindingModuleForKey.clear();
    m_Snowflakes.clear();
    if(ImGui::GetCurrentContext())
    {
        m_SearchResults.clear();
        m_SearchActive = false;
        m_SearchFocused = false;
        m_MenuOpen = false;
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }
    m_pGameClient = nullptr;
    m_pAutoAnswer = nullptr;
    m_pChatSpam = nullptr;
}

void CImGuiManager::BeginFrame()
{
    ANTI_DISASM_BLOCK();
    if(!m_Initialized || !ImGui::GetCurrentContext())
        return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void CImGuiManager::EndFrame()
{
    ANTI_DISASM_BLOCK();
    if(!m_Initialized || !ImGui::GetCurrentContext())
        return;
    ImGui::Render();
    ImDrawData *pDrawData = ImGui::GetDrawData();
    if(pDrawData)
        ImGui_ImplOpenGL3_RenderDrawData(pDrawData);
}

void CImGuiManager::InitSnowflakes()
{
    m_Snowflakes.clear();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distX(0.0f, 1920.0f);
    std::uniform_real_distribution<float> distY(-200.0f, 1080.0f);
    std::uniform_real_distribution<float> distSize(1.5f, 4.0f);
    std::uniform_real_distribution<float> distSpeed(25.0f, 70.0f);
    std::uniform_real_distribution<float> distAlpha(0.4f, 0.9f);
    std::uniform_real_distribution<float> distWobble(0.0f, 6.28f);
    std::uniform_real_distribution<float> distWobbleSpeed(1.0f, 3.0f);
    for(int i = 0; i < 100; i++)
    {
        SSnowflake s;
        s.x = distX(gen); s.y = distY(gen); s.size = distSize(gen);
        s.speed = distSpeed(gen); s.alpha = distAlpha(gen);
        s.wobble = distWobble(gen); s.wobbleSpeed = distWobbleSpeed(gen);
        m_Snowflakes.push_back(s);
    }
}

void CImGuiManager::UpdateSnowflakes()
{
    ImGuiIO &io = ImGui::GetIO();
    if(!m_RngInit)
    {
        std::random_device rd;
        m_Rng.seed(rd());
        m_RngInit = true;
    }
    for(auto &s : m_Snowflakes)
    {
        s.y += s.speed * m_DeltaTime;
        s.wobble += s.wobbleSpeed * m_DeltaTime;
        s.x += sinf(s.wobble) * 0.5f + (m_MouseLerpX - io.DisplaySize.x / 80.0f) * s.size * 0.2f * m_DeltaTime;
        if(s.y > io.DisplaySize.y + 20)
        {
            std::uniform_real_distribution<float> dX(0.0f, io.DisplaySize.x);
            std::uniform_real_distribution<float> dSz(1.5f, 4.0f);
            std::uniform_real_distribution<float> dSp(25.0f, 70.0f);
            std::uniform_real_distribution<float> dA(0.4f, 0.9f);
            s.x = dX(m_Rng); s.y = -10.0f;
            s.size = dSz(m_Rng); s.speed = dSp(m_Rng); s.alpha = dA(m_Rng);
        }
    }
}

void CImGuiManager::LoadTyanTexture()
{
    if(!m_pGameClient || !m_pGameClient->Graphics()) return;
    if(m_TyanTextureGL != 0 || !m_Initialized) return;

    IGraphics *pGraphics = m_pGameClient->Graphics();
    CImageInfo ImgInfo;
    if(pGraphics->LoadPng(ImgInfo, "data/okaytyan.png", IStorage::TYPE_ALL))
    {
        glGenTextures(1, &m_TyanTextureGL);
        if(m_TyanTextureGL == 0) { ImgInfo.Free(); return; }
        glBindTexture(GL_TEXTURE_2D, m_TyanTextureGL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GLenum format = ImgInfo.m_Format == CImageInfo::FORMAT_RGB ? GL_RGB : GL_RGBA;
        if(ImgInfo.m_pData && ImgInfo.m_Width > 0 && ImgInfo.m_Height > 0)
            glTexImage2D(GL_TEXTURE_2D, 0, format, ImgInfo.m_Width, ImgInfo.m_Height,
                        0, format, GL_UNSIGNED_BYTE, ImgInfo.m_pData);
        else
        {
            glDeleteTextures(1, &m_TyanTextureGL);
            m_TyanTextureGL = 0;
            ImgInfo.Free();
            return;
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        m_TyanLoaded = true;
        ImgInfo.Free();
    }
}

GLuint CImGuiManager::LoadTextureFromPng(const char *path)
{
    if(!m_pGameClient || !m_pGameClient->Graphics())
        return 0;

    IGraphics *pGraphics = m_pGameClient->Graphics();
    CImageInfo ImgInfo;

    if(!pGraphics->LoadPng(ImgInfo, path, IStorage::TYPE_ALL))
        return 0;

    GLuint texId = 0;
    glGenTextures(1, &texId);

    if(texId == 0)
    {
        ImgInfo.Free();
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLenum format = (ImgInfo.m_Format == CImageInfo::FORMAT_RGB) ? GL_RGB : GL_RGBA;
        if(ImgInfo.m_pData && ImgInfo.m_Width > 0 && ImgInfo.m_Height > 0)
            glTexImage2D(GL_TEXTURE_2D, 0, format, ImgInfo.m_Width, ImgInfo.m_Height,
                        0, format, GL_UNSIGNED_BYTE, ImgInfo.m_pData);
        else
        {
            glDeleteTextures(1, &texId);
            ImgInfo.Free();
            return 0;
        }

    glBindTexture(GL_TEXTURE_2D, 0);
    ImgInfo.Free();

    return texId;
}

void CImGuiManager::UpdateAnimations()
{
    ANTI_DISASM_BLOCK();
    double currentTime = (double)SDL_GetTicks() / 1000.0;
    m_DeltaTime = (float)(currentTime - m_LastTime);
    m_LastTime = currentTime;
    if(!std::isfinite(m_DeltaTime) || m_DeltaTime <= 0.0f)
        m_DeltaTime = 0.016f;
    m_DeltaTime = std::clamp(m_DeltaTime, 0.0001f, 0.05f);
    m_TabContentAnim = Lerp(m_TabContentAnim, 1.0f, m_DeltaTime * 8.0f);
    if(!std::isfinite(m_Time))
        m_Time = 0.0f;
    m_Millis = SDL_GetTicks();
    m_Time += m_DeltaTime;



    float targetAlpha = m_MenuOpen ? 1.0f : 0.0f;
    float alphaSpeed = m_MenuOpen ? 14.0f : 8.0f;
    float openSpeed = m_MenuOpen ? 12.0f : 7.0f;
    m_MenuAlpha = Lerp(m_MenuAlpha, targetAlpha, m_DeltaTime * alphaSpeed);
    m_OpenAnimValue = Lerp(m_OpenAnimValue, m_MenuOpen ? 1.0f : 0.0f, m_DeltaTime * openSpeed);

    ImGuiIO &io = ImGui::GetIO();
    m_MouseLerpX = Lerp(m_MouseLerpX, io.MousePos.x / 40.0f, m_DeltaTime * 8.0f);
    m_MouseLerpY = Lerp(m_MouseLerpY, io.MousePos.y / 60.0f, m_DeltaTime * 8.0f);

    m_NormalPanelAlpha = Lerp(m_NormalPanelAlpha, m_SearchActive ? 0.0f : 1.0f, m_DeltaTime * 15.0f);
    m_SearchPanelAlpha = Lerp(m_SearchPanelAlpha, m_SearchActive ? 1.0f : 0.0f, m_DeltaTime * 15.0f);
    m_SearchBoxAnim = Lerp(m_SearchBoxAnim, m_SearchActive ? 1.0f : 0.0f, m_DeltaTime * 10.0f);
    m_SearchFocusAnim = Lerp(m_SearchFocusAnim, m_SearchFocused ? 1.0f : 0.0f, m_DeltaTime * 10.0f);

    if(m_SearchActive)
    {
        m_SearchCursorBlink += m_DeltaTime * 2.0f;
        if(m_SearchCursorBlink > 1.0f) m_SearchCursorBlink -= 1.0f;
    }

    if(m_HeaderTransition < 1.0f)
    {
        m_HeaderTransition += 3.0f * m_DeltaTime;
        if(m_HeaderTransition > 1.0f) m_HeaderTransition = 1.0f;
    }
    if(!m_MenuOpen)
    {
        for(auto &pair : m_TextInputAnims)
            pair.second.m_Focused = false;
        for(auto &pair : m_SliderAnims)
        {
            pair.second.m_InputMode = false;
            pair.second.m_Dragging = false;
        }
        for(auto &pair : m_ColorPickerAnims)
        {
            pair.second.m_HexActive = false;
            pair.second.m_DraggingPalette = false;
            pair.second.m_DraggingHue = false;
            pair.second.m_DraggingAlpha = false;
        }
        for(auto &pair : m_ComboAnims)
            pair.second.m_Expanded = false;
        for(auto &pair : m_MultiSelectAnims)
            pair.second.m_Expanded = false;
        if(m_SearchActive)
        {
            m_SearchActive = false;
            m_SearchFocused = false;
        }
        m_SearchResults.clear();
        m_SelectedSearchModuleId.clear();
        m_WaitingForKey = false;
        m_BindingModuleForKey.clear();
    }

    for(auto &pair : m_TextInputAnims)
    {
        STextInputAnim &anim = pair.second;
        float targetFocus = anim.m_Focused ? 1.0f : 0.0f;
        float targetHover = anim.m_Focused ? 1.0f : 0.0f;
        anim.m_FocusAnim = Lerp(anim.m_FocusAnim, targetFocus, m_DeltaTime * 10.0f);
        anim.m_HoverAnim = Lerp(anim.m_HoverAnim, targetHover, m_DeltaTime * 10.0f);
        if(!anim.m_Focused)
            anim.m_CursorBlink = 0.0f;
    }
    m_ModuleScrollCurrent = Lerp(m_ModuleScrollCurrent, m_ModuleScrollTarget, m_DeltaTime * 12.0f);
    m_SettingScrollCurrent = Lerp(m_SettingScrollCurrent, m_SettingScrollTarget, m_DeltaTime * 12.0f);
    m_SearchScrollCurrent = Lerp(m_SearchScrollCurrent, m_SearchScrollTarget, m_DeltaTime * 12.0f);
    m_KeybindScrollCurrent = Lerp(m_KeybindScrollCurrent, m_KeybindScrollTarget, m_DeltaTime * 12.0f);
    m_CardScrollCurrent = Lerp(m_CardScrollCurrent, m_CardScrollTarget, m_DeltaTime * 12.0f);
}

float CImGuiManager::EaseOutQuart(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - powf(1.0f - t, 4.0f);
}

void CImGuiManager::UpdateSearchResults()
{
    m_SearchResults.clear();
    m_SelectedSearchModuleId.clear();

    if(m_SearchBuffer[0] == '\0')
        return;

    std::string query = m_SearchBuffer;
    for(auto &c : query) c = tolower(c);

    auto& modules = CMenuRegistry::Instance().GetModules();
    for(auto &mod : modules)
    {
        if(mod.m_Id.empty()) continue;
        std::string name = mod.m_Name;
        for(auto &c : name) c = tolower(c);
        if(name.find(query) != std::string::npos)
            m_SearchResults.push_back(&mod);
    }

    for(auto *mod : m_SearchResults)
    {
        if(m_SearchResultAnims.find(mod->m_Id) == m_SearchResultAnims.end())
            m_SearchResultAnims[mod->m_Id] = 0.0f;
    }

    if(!m_SearchResults.empty())
        m_SelectedSearchModuleId = m_SearchResults[0]->m_Id;
}

void CImGuiManager::RegisterModules()
{
    ANTI_DISASM_BLOCK();
    CMenuRegistry &reg = CMenuRegistry::Instance();
    reg.Clear();

    {
        SModuleEntry m;
        m.m_Id = "balance"; m.m_Name = "Balance Bot"; m.m_Description = "Auto balance";
        m.m_Category = MCAT_MOVEMENT; m.m_pEnabled = &g_Config.m_shalawaBalanceBot;
        static const char *balanceModes[] = {"Passive", "Aggressive", "Adaptive", "Predictive"};
        auto &c = m.m_Settings.emplace_back();
        c.m_Type = SSettingBase::COMBO; c.m_Name = "Mode"; c.m_Id = "bal_mode";
        c.m_pIntValue = &g_Config.m_shalawaBalanceMode; c.m_pComboItems = balanceModes; c.m_ComboCount = 4;
        auto addSlider = [&](const char *name, const char *id, int *val, int mn, int mx) {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::SLIDER; s.m_Name = name; s.m_Id = id;
            s.m_pIntValue = val; s.m_Min = mn; s.m_Max = mx;
        };
        addSlider("Predict Ticks", "bal_predict", &g_Config.m_shalawaBalancePredictTicks, 1, 20);
        addSlider("Sensitivity", "bal_sens", &g_Config.m_shalawaBalanceSensitivity, 1, 20);
        addSlider("Smoothing", "bal_smooth", &g_Config.m_shalawaBalanceSmoothing, 0, 99);
        addSlider("Balance Zone", "bal_zone", &g_Config.m_shalawaBalanceZone, 10, 200);
        addSlider("Panic Zone", "bal_panic", &g_Config.m_shalawaBalancePanicZone, 50, 300);
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Auto Jump", "", "bal_jump", &g_Config.m_shalawaBalanceAutoJump});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Use Hook", "", "bal_hook", &g_Config.m_shalawaBalanceUseHook});
        addSlider("Search Radius", "bal_radius", &g_Config.m_shalawaBalanceSearchRadius, 30, 300);
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Use Accel", "", "bal_accel", &g_Config.m_shalawaBalanceUseAccel});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Visuals", "", "bal_vis", &g_Config.m_shalawaBalanceVisuals});
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "hookride"; m.m_Name = "Hook Ride"; m.m_Description = "Auto hook ride";
        m.m_Category = MCAT_MOVEMENT; m.m_pEnabled = &g_Config.m_shalawaHookRide;
        auto addS = [&](const char *name, const char *id, int *val, int mn, int mx) {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::SLIDER; s.m_Name = name; s.m_Id = id;
            s.m_pIntValue = val; s.m_Min = mn; s.m_Max = mx;
        };
        addS("Speed", "hr_speed", &g_Config.m_shalawaHookRideSpeed, 1, 20);
        addS("Max Dist", "hr_dist", &g_Config.m_shalawaHookRideMaxDist, 50, 400);
        addS("Threshold", "hr_thresh", &g_Config.m_shalawaHookRideThreshold, 5, 100);
        addS("Min Ticks", "hr_mint", &g_Config.m_shalawaHookRideMinTicks, 1, 10);
        addS("Max Ticks", "hr_maxt", &g_Config.m_shalawaHookRideMaxTicks, 3, 30);
        addS("Cooldown", "hr_cd", &g_Config.m_shalawaHookRideCooldown, 1, 15);
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "quickstop"; m.m_Name = "Quick Stop"; m.m_Description = "Auto brake when releasing direction";
        m.m_Category = MCAT_MOVEMENT; m.m_pEnabled = &g_Config.m_shalawaQuickStop;
        auto &s = m.m_Settings.emplace_back();
        s.m_Type = SSettingBase::SLIDER; s.m_Name = "Brake Ticks"; s.m_Id = "qs_ticks";
        s.m_pIntValue = &g_Config.m_shalawaQuickStopTicks; s.m_Min = 1; s.m_Max = 10;
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "avoid"; m.m_Name = "Avoid Mode"; m.m_Description = "Freeze avoidance";
        m.m_Category = MCAT_MOVEMENT;
        static int avoidEnabled = 1;
        m.m_pEnabled = &avoidEnabled;
        static const char *modes[] = {"Off", "Legit", "Blatant", "AI Bot"};
        auto &c = m.m_Settings.emplace_back();
        c.m_Type = SSettingBase::COMBO; c.m_Name = "Mode"; c.m_Id = "avoid_mode";
        c.m_pIntValue = &g_Config.m_shalawaAvoidMode; c.m_pComboItems = modes; c.m_ComboCount = 4;
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "legitavoid"; m.m_Name = "Legit Settings"; m.m_Description = "Legit avoid settings";
        m.m_Category = MCAT_MOVEMENT; m.m_pEnabled = &g_Config.m_shalawaLegitAvoidFreeze;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Avoid Freeze", "", "la_freeze", &g_Config.m_shalawaLegitAvoidFreeze});
        auto &ds = m.m_Settings.emplace_back();
        ds.m_Type = SSettingBase::SLIDER; ds.m_Name = "Delay"; ds.m_Id = "la_delay";
        ds.m_pIntValue = &g_Config.m_shalawaLegitAvoidDelay; ds.m_Min = 0; ds.m_Max = 500;
        auto &ps = m.m_Settings.emplace_back();
        ps.m_Type = SSettingBase::SLIDER; ps.m_Name = "Predict Ticks"; ps.m_Id = "la_predict";
        ps.m_pIntValue = &g_Config.m_shalawaLegitPredictTicks; ps.m_Min = 0; ps.m_Max = 20;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Hook Assist", "", "la_hookassist", &g_Config.m_shalawaLegitHookAssist});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Avoid Teles", "", "la_teles", &g_Config.m_shalawaLegitAvoidTeles});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Avoid Death", "", "la_death", &g_Config.m_shalawaLegitAvoidDeath});
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "blatantavoid"; m.m_Name = "Blatant Settings"; m.m_Description = "Blatant avoid settings";
        m.m_Category = MCAT_MOVEMENT; m.m_pEnabled = &g_Config.m_shalawaAvoidFreeze;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Avoid Freeze", "", "ba_freeze", &g_Config.m_shalawaAvoidFreeze});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Block Input", "", "ba_block", &g_Config.m_shalawaAvoidBlockInput});
        auto addS = [&](const char *name, const char *id, int *val, int mn, int mx) {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::SLIDER; s.m_Name = name; s.m_Id = id;
            s.m_pIntValue = val; s.m_Min = mn; s.m_Max = mx;
        };
        addS("Min Ticks", "ba_mint", &g_Config.m_shalawaAvoidMinTicks, 0, 30);
        addS("Max Ticks", "ba_maxt", &g_Config.m_shalawaAvoidMaxTicks, 0, 30);
        addS("FOV", "ba_fov", &g_Config.m_shalawaAvoidFov, 0, 360);
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Anti Drag", "", "ba_antidrag", &g_Config.m_shalawaAvoidAntiDrag});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Safe Drag", "", "ba_safedrag", &g_Config.m_shalawaAvoidSafeDrag});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Safe Drag Friends Only", "", "ba_safedragfr", &g_Config.m_shalawaAvoidSafeDragFriends});
        addS("Safe Drag Range", "ba_safedragrange", &g_Config.m_shalawaAvoidSafeDragRange, 50, 380);
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Detect Freezes", "", "ba_detectfr", &g_Config.m_shalawaAvoidDetectFreezes});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Hook Pump", "", "ba_hookpump", &g_Config.m_shalawaAvoidHookPump});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Keep Hook", "", "ba_keephook", &g_Config.m_shalawaAvoidKeepHook});
        addS("Coarse Step", "ba_coarse", &g_Config.m_shalawaAvoidCoarseStep, 0, 45);
        addS("Stuck Ticks", "ba_stuck", &g_Config.m_shalawaAvoidStuckTicks, 1, 30);
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Debug Visuals", "", "ba_debug", &g_Config.m_shalawaAvoidDebug});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Track Point", "", "ba_trackpt", &g_Config.m_shalawaAvoidTrackPoint});
        addS("Fine Step", "ba_finestep", &g_Config.m_shalawaAvoidFineStep, 1, 10);
        addS("Top Coarse", "ba_topcoarse", &g_Config.m_shalawaAvoidTopCoarse, 2, 15);
        addS("Max Hook Hold", "ba_maxhold", &g_Config.m_shalawaAvoidMaxHookHold, 4, 50);
        addS("Predict Budget", "ba_budget", &g_Config.m_shalawaAvoidPredictBudget, 10, 120);
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "unfreeze"; m.m_Name = "Unfreeze Bot"; m.m_Description = "Auto unfreeze";
        m.m_Category = MCAT_MOVEMENT; m.m_pEnabled = &g_Config.m_shalawaUnfreezeBot;
        auto addS = [&](const char *name, const char *id, int *val, int mn, int mx) {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::SLIDER; s.m_Name = name; s.m_Id = id;
            s.m_pIntValue = val; s.m_Min = mn; s.m_Max = mx;
        };
        addS("Predict Ticks", "uf_predict", &g_Config.m_shalawaUnfreezePredictTicks, 1, 20);
        addS("FOV", "uf_fov", &g_Config.m_shalawaUnfreezeFov, 10, 360);
        addS("Max Bounces", "uf_bounces", &g_Config.m_shalawaUnfreezeMaxBounces, 1, 5);
        addS("Angle Step", "uf_angle", &g_Config.m_shalawaUnfreezeAngleStep, 1, 30);
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "watermark"; m.m_Name = "Watermark"; m.m_Description = "On-screen watermark";
        m.m_Category = MCAT_VISUAL; m.m_pEnabled = &g_Config.m_shalawaWatermark;
        static const char *styles[] = {"Modern","Rise","Gamesense","NL","Skeet","Stylo","Minimal","Pills","Claude","Gemini","GPT","Grok","Seed","DeepSeek"};
        auto &c = m.m_Settings.emplace_back();
        c.m_Type = SSettingBase::COMBO; c.m_Name = "Style"; c.m_Id = "wm_style";
        c.m_pIntValue = &g_Config.m_shalawaWatermarkStyle; c.m_pComboItems = styles; c.m_ComboCount = 14;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "FPS", "", "wm_fps", &g_Config.m_shalawaWatermarkFps});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Ping", "", "wm_ping", &g_Config.m_shalawaWatermarkPing});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Time", "", "wm_time", &g_Config.m_shalawaWatermarkTime});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Server", "", "wm_server", &g_Config.m_shalawaWatermarkServer});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Players", "", "wm_players", &g_Config.m_shalawaWatermarkPlayers});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Keybinds", "", "wm_keys", &g_Config.m_shalawaWatermarkKeybinds});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Speed", "", "wm_speed", &g_Config.m_shalawaWatermarkSpeed});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Target", "", "wm_target", &g_Config.m_shalawaWatermarkTarget});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Notifications", "", "wm_notif", &g_Config.m_shalawaWatermarkNotifications});
        reg.AddModule(m);
    }
    {
        SModuleEntry m;
        m.m_Id = "fakeaim"; m.m_Name = "Fake Aim"; m.m_Description = "Send fake cursor to server";
        m.m_Category = MCAT_MISC; m.m_pEnabled = &g_Config.m_shalawaFakeAim;
        static const char *fakeModes[] = {"Mouse Pos", "Robot Aim", "Spinbot", "Random", "Fake Angles", "Troll Aim"};
        auto &c = m.m_Settings.emplace_back();
        c.m_Type = SSettingBase::COMBO; c.m_Name = "Mode"; c.m_Id = "fa_mode";
        c.m_pIntValue = &g_Config.m_shalawaFakeAimMode; c.m_pComboItems = fakeModes; c.m_ComboCount = 6;
        auto &s = m.m_Settings.emplace_back();
        s.m_Type = SSettingBase::SLIDER; s.m_Name = "Spin Speed"; s.m_Id = "fa_speed";
        s.m_pIntValue = &g_Config.m_shalawaFakeAimSpeed; s.m_Min = 1; s.m_Max = 500;
        s.m_VisibilityCheck = []() { return g_Config.m_shalawaFakeAimMode == 2; };
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Send Always", "Send every tick", "fa_send", &g_Config.m_shalawaFakeAimSendAlways});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Visible", "Show real aim locally", "fa_visible", &g_Config.m_shalawaFakeAimVisible});
        reg.AddModule(m);
    }
    {
        SModuleEntry m;
        m.m_Id = "arraylist"; m.m_Name = "Arraylist"; m.m_Description = "Module list overlay";
        m.m_Category = MCAT_VISUAL; m.m_pEnabled = &g_Config.m_shalawaArraylist;
        static const char *arrayStyles[] = {
            "Skeet", 
            "Neverlose", 
            "Gamesense", 
            "Custom", 
            "ChatGPT", 
            "Claude", 
            "DeepSeek", 
            "GLM", 
            "Gemini"
        };
        auto &cs = m.m_Settings.emplace_back();
        cs.m_Type = SSettingBase::COMBO; cs.m_Name = "Style"; cs.m_Id = "al_style";
        cs.m_pIntValue = &g_Config.m_shalawaArraylistStyle; 
        cs.m_pComboItems = arrayStyles; 
        cs.m_ComboCount = 9;
        static const char *arrayModes[] = {"Solid","Astolfo","Rainbow","Wave","Fade"};
        auto &c = m.m_Settings.emplace_back();
        c.m_Type = SSettingBase::COMBO; c.m_Name = "Color Mode"; c.m_Id = "al_mode";
        c.m_pIntValue = &g_Config.m_shalawaArraylistMode; 
        c.m_pComboItems = arrayModes; 
        c.m_ComboCount = 5;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Shadow", "", "al_shadow", &g_Config.m_shalawaArraylistShadow});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Background", "", "al_bg", &g_Config.m_shalawaArraylistBackground});
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "hitparticles"; m.m_Name = "Hit Particles"; m.m_Description = "Custom hit effect particles";
        m.m_Category = MCAT_VISUAL; m.m_pEnabled = &g_Config.m_shalawaHitParticles;
        static const char *ptypes[] = {"Stars","Hearts","Orbits","Sparks","Diamonds","Crescents","Flowers","Swirls","Mixed"};
        auto &c = m.m_Settings.emplace_back();
        c.m_Type = SSettingBase::COMBO; c.m_Name = "Type"; c.m_Id = "hp_type";
        c.m_pIntValue = &g_Config.m_shalawaHitParticleType; c.m_pComboItems = ptypes; c.m_ComboCount = 9;
        auto addS = [&](const char *name, const char *id, int *val, int mn, int mx) {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::SLIDER; s.m_Name = name; s.m_Id = id;
            s.m_pIntValue = val; s.m_Min = mn; s.m_Max = mx;
        };
        addS("Count", "hp_count", &g_Config.m_shalawaHitParticleCount, 3, 40);
        addS("Size", "hp_size", &g_Config.m_shalawaHitParticleSize, 20, 300);
        addS("Alpha", "hp_alpha", &g_Config.m_shalawaHitParticleAlpha, 10, 100);
        addS("Speed", "hp_speed", &g_Config.m_shalawaHitParticleSpeed, 30, 300);
        addS("Gravity", "hp_grav", &g_Config.m_shalawaHitParticleGravity, 0, 300);
        addS("Lifetime", "hp_life", &g_Config.m_shalawaHitParticleLifetime, 30, 300);
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Shockwave", "", "hp_shock", &g_Config.m_shalawaHitParticleShockwave});
        addS("Wave Count", "hp_waves", &g_Config.m_shalawaHitParticleWaveCount, 1, 4);
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Glow", "", "hp_glow", &g_Config.m_shalawaHitParticleGlow});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Afterimage", "", "hp_after", &g_Config.m_shalawaHitParticleAfterimage});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Screen Flash", "", "hp_flash", &g_Config.m_shalawaHitParticleFlash});
        addS("Flash Intensity", "hp_flashi", &g_Config.m_shalawaHitParticleFlashIntensity, 10, 100);
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Connectors", "", "hp_conn", &g_Config.m_shalawaHitParticleConnectors});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Physics Debris", "", "hp_phys", &g_Config.m_shalawaHitParticlePhysics});
        reg.AddModule(m);
    }


    {
        SModuleEntry m;
        m.m_Id = "chinahat"; m.m_Name = "China Hat"; m.m_Description = "Conical hat with aura effects";
        m.m_Category = MCAT_VISUAL; m.m_pEnabled = &g_Config.m_shalawaChinaHat;
        static const char *htargets[] = {"Self","Everyone","Friends","Others"};
        auto &t = m.m_Settings.emplace_back();
        t.m_Type = SSettingBase::COMBO; t.m_Name = "Target"; t.m_Id = "ch_target";
        t.m_pIntValue = &g_Config.m_shalawaChinaHatTarget; t.m_pComboItems = htargets; t.m_ComboCount = 4;
        auto addS = [&](const char *name, const char *id, int *val, int mn, int mx) {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::SLIDER; s.m_Name = name; s.m_Id = id;
            s.m_pIntValue = val; s.m_Min = mn; s.m_Max = mx;
        };
        addS("Size", "ch_size", &g_Config.m_shalawaChinaHatSize, 8, 40);
        addS("Alpha", "ch_alpha", &g_Config.m_shalawaChinaHatAlpha, 10, 100);

        auto &col1 = m.m_Settings.emplace_back();
        col1.m_Type = SSettingBase::COLOR;
        col1.m_Name = "Color 1";
        col1.m_Id = "ch_color1";
        col1.m_pColorHue = &CImGuiManager::Instance().m_ChinaHatHue1;
        col1.m_pColorSaturation = &CImGuiManager::Instance().m_ChinaHatSat1;
        col1.m_pColorBrightness = &CImGuiManager::Instance().m_ChinaHatBri1;
        col1.m_pColorAlpha = &CImGuiManager::Instance().m_ChinaHatAlpha1;

        auto &col2 = m.m_Settings.emplace_back();
        col2.m_Type = SSettingBase::COLOR;
        col2.m_Name = "Color 2";
        col2.m_Id = "ch_color2";
        col2.m_pColorHue = &CImGuiManager::Instance().m_ChinaHatHue2;
        col2.m_pColorSaturation = &CImGuiManager::Instance().m_ChinaHatSat2;
        col2.m_pColorBrightness = &CImGuiManager::Instance().m_ChinaHatBri2;
        col2.m_pColorAlpha = &CImGuiManager::Instance().m_ChinaHatAlpha2;

        auto &col3 = m.m_Settings.emplace_back();
        col3.m_Type = SSettingBase::COLOR;
        col3.m_Name = "Color 3 (Ghost)";
        col3.m_Id = "ch_color3";
        col3.m_pColorHue = &CImGuiManager::Instance().m_ChinaHatHue3;
        col3.m_pColorSaturation = &CImGuiManager::Instance().m_ChinaHatSat3;
        col3.m_pColorBrightness = &CImGuiManager::Instance().m_ChinaHatBri3;
        col3.m_pColorAlpha = &CImGuiManager::Instance().m_ChinaHatAlpha3;
        col3.m_VisibilityCheck = []() { return g_Config.m_shalawaChinaHatGhost != 0; };

        m.m_Settings.push_back({SSettingBase::TOGGLE, "Ghost Aura", "3 spiral particle trails", "ch_ghost", &g_Config.m_shalawaChinaHatGhost});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Crystals", "Orbiting crystal diamonds", "ch_crystals", &g_Config.m_shalawaChinaHatCrystals});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Circle", "Animated circle ring", "ch_circle", &g_Config.m_shalawaChinaHatCircle});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Rhomb", "Rotating diamond shape", "ch_rhomb", &g_Config.m_shalawaChinaHatRhomb});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Pattern", "Decorative bands on hat", "ch_pattern", &g_Config.m_shalawaChinaHatPattern});
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "spoof"; m.m_Name = "Spoof"; m.m_Description = "client spoofing";
        m.m_Category = MCAT_MISC; m.m_pEnabled = &g_Config.m_shalawaSpoofVersion;

        {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::TEXT_INPUT;
            s.m_Name = "Git Hash";
            s.m_Id = "spoof_hash";
            s.m_pStringValue = g_Config.m_shalawaSpoofGitHash;
            s.m_StringMaxLen = sizeof(g_Config.m_shalawaSpoofGitHash);
        }
        {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::TEXT_INPUT;
            s.m_Name = "Version String";
            s.m_Id = "spoof_verstr";
            s.m_pStringValue = g_Config.m_shalawaSpoofVersionStr;
            s.m_StringMaxLen = sizeof(g_Config.m_shalawaSpoofVersionStr);
        }
        {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::SLIDER;
            s.m_Name = "DDNet Version";
            s.m_Id = "spoof_ddver";
            s.m_pIntValue = &g_Config.m_shalawaSpoofDDNetVersion;
            s.m_Min = 0;
            s.m_Max = 99999;
        }
        {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::SLIDER;
            s.m_Name = "Client Version 0.7";
            s.m_Id = "spoof_clver7";
            s.m_pIntValue = &g_Config.m_shalawaSpoofClientVersion7;
            s.m_Min = 0;
            s.m_Max = 9999;
        }
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "socks5"; m.m_Name = "SOCKS5 Proxy"; m.m_Description = "use proxy for connect <3";
        m.m_Category = MCAT_MISC; m.m_pEnabled = &g_Config.m_shalawaSocks5Enabled;

        {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::TEXT_INPUT;
            s.m_Name = "Address (ip:port)";
            s.m_Id = "socks5_addr";
            s.m_pStringValue = g_Config.m_shalawaSocks5Addr;
            s.m_StringMaxLen = sizeof(g_Config.m_shalawaSocks5Addr);
        }
        {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::TEXT_INPUT;
            s.m_Name = "Username";
            s.m_Id = "socks5_user";
            s.m_pStringValue = g_Config.m_shalawaSocks5User;
            s.m_StringMaxLen = sizeof(g_Config.m_shalawaSocks5User);
        }
        {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::TEXT_INPUT;
            s.m_Name = "Password";
            s.m_Id = "socks5_pass";
            s.m_pStringValue = g_Config.m_shalawaSocks5Pass;
            s.m_StringMaxLen = sizeof(g_Config.m_shalawaSocks5Pass);
        }
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "tail"; m.m_Name = "Tail"; m.m_Description = "Animated tail behind your tee";
        m.m_Category = MCAT_VISUAL; m.m_pEnabled = &g_Config.m_shalawaTail;
        static const char *tailTypes[] = {"Simple","Hearts","Stars","Weapons","Tiles","Fire","Rainbow"};
        auto &tt = m.m_Settings.emplace_back();
        tt.m_Type = SSettingBase::COMBO; tt.m_Name = "Type"; tt.m_Id = "tail_type";
        tt.m_pIntValue = &g_Config.m_shalawaTailType; tt.m_pComboItems = tailTypes; tt.m_ComboCount = 7;
        auto addS = [&](const char *name, const char *id, int *val, int mn, int mx) {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::SLIDER; s.m_Name = name; s.m_Id = id;
            s.m_pIntValue = val; s.m_Min = mn; s.m_Max = mx;
        };
        addS("Length", "tail_len", &g_Config.m_shalawaTailLength, 3, 60);
        addS("Size", "tail_size", &g_Config.m_shalawaTailSize, 20, 300);
        addS("Alpha", "tail_alpha", &g_Config.m_shalawaTailAlpha, 10, 100);
        addS("Spacing", "tail_spacing", &g_Config.m_shalawaTailSpacing, 2, 20);
        addS("Stiffness", "tail_stiff", &g_Config.m_shalawaTailStiffness, 1, 100);
        addS("Gravity", "tail_grav", &g_Config.m_shalawaTailGravity, 0, 100);
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Taper", "Segments shrink at end", "tail_taper", &g_Config.m_shalawaTailTaper});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Trail Lines", "Connect segments with lines", "tail_trail", &g_Config.m_shalawaTailTrail});
        addS("Color R", "tail_r", &g_Config.m_shalawaTailColorR, 0, 255);
        addS("Color G", "tail_g", &g_Config.m_shalawaTailColorG, 0, 255);
        addS("Color B", "tail_b", &g_Config.m_shalawaTailColorB, 0, 255);
        static const char *tileTypes[] = {"Gray","Sand","Death","Hook","Ice","Green","Gold","Purple"};
        auto &tileSel = m.m_Settings.emplace_back();
        tileSel.m_Type = SSettingBase::COMBO; tileSel.m_Name = "Tile Style"; tileSel.m_Id = "tail_tiletype";
        tileSel.m_pIntValue = &g_Config.m_shalawaTailTileType; tileSel.m_pComboItems = tileTypes; tileSel.m_ComboCount = 8;
        tileSel.m_VisibilityCheck = []() { return g_Config.m_shalawaTailType == 4 || g_Config.m_shalawaTailType == 3; };
        reg.AddModule(m);
    }
    {
        SModuleEntry m;
        m.m_Id = "nuker"; m.m_Name = "Server Nuker"; m.m_Description = "Nuke server via rcon (requires auth)";
        m.m_Category = MCAT_MISC;
        static int nukerEnabled = 0;
        m.m_pEnabled = &nukerEnabled;
        reg.AddModule(m);
    }
    {
        SModuleEntry m;
        m.m_Id = "avoidvis"; m.m_Name = "Avoid Visuals"; m.m_Description = "Avoid visual indicators";
        m.m_Category = MCAT_VISUAL; m.m_pEnabled = &g_Config.m_shalawaAvoidVisuals;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Danger Zones", "", "av_dz", &g_Config.m_shalawaAvoidDrawDangerZones});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Hitbox", "", "av_hb", &g_Config.m_shalawaAvoidDrawHitbox});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Prediction", "", "av_pred", &g_Config.m_shalawaAvoidDrawPrediction});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Escape Path", "", "av_esc", &g_Config.m_shalawaAvoidDrawEscapePath});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Hook Target", "", "av_hook", &g_Config.m_shalawaAvoidDrawHookTarget});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Escape Arrow", "", "av_arrow", &g_Config.m_shalawaAvoidDrawEscapeDir});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Danger Indicator", "", "av_ind", &g_Config.m_shalawaAvoidDrawDangerIndicator});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Status Overlay", "", "av_status", &g_Config.m_shalawaAvoidDrawStatus});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Anti Drag Visual", "", "av_antidrag", &g_Config.m_shalawaAvoidDrawAntiDrag});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Safe Drag Visual", "", "av_safedrag", &g_Config.m_shalawaAvoidDrawSafeDrag});
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "iof"; m.m_Name = "IOF Player"; m.m_Description = "Music visualizer with system audio capture";
        m.m_Category = MCAT_VISUAL; m.m_pEnabled = &g_Config.m_shalawaIOF;
        static const char *iofStyles[] = {"Bars","Wave","Circular","Dots","Pulse"};
        auto &c = m.m_Settings.emplace_back();
        c.m_Type = SSettingBase::COMBO; c.m_Name = "Style"; c.m_Id = "iof_style";
        c.m_pIntValue = &g_Config.m_shalawaIOFStyle; c.m_pComboItems = iofStyles; c.m_ComboCount = 5;
        auto addS = [&](const char *name, const char *id, int *val, int mn, int mx) {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::SLIDER; s.m_Name = name; s.m_Id = id;
            s.m_pIntValue = val; s.m_Min = mn; s.m_Max = mx;
        };
        addS("Sensitivity", "iof_sens", &g_Config.m_shalawaIOFSensitivity, 1, 100);
        addS("Smoothing", "iof_smooth", &g_Config.m_shalawaIOFSmoothing, 0, 99);
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "aimvis"; m.m_Name = "Aim Visuals"; m.m_Description = "Aim visual indicators";
        m.m_Category = MCAT_VISUAL; m.m_pEnabled = &g_Config.m_shalawaAimDrawFov;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Draw FOV", "", "aiv_fov", &g_Config.m_shalawaAimDrawFov});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Always Show", "", "aiv_always", &g_Config.m_shalawaAimVisAlways});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Priority (0=cursor, 1=us)", "", "aiv_priority", &g_Config.m_shalawaAimVisPriority});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Target Box", "", "aiv_box", &g_Config.m_shalawaAimTargetBox});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "FNG Mode", "", "aiv_fng", &g_Config.m_shalawaFNGBot});
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "tas"; m.m_Name = "TAS"; m.m_Description = "Tool-assisted speedrun";
        m.m_Category = MCAT_MISC; m.m_pEnabled = &g_Config.m_shalawaPlaying;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Record", "", "tas_rec", &g_Config.m_shalawaRecording});
        auto &ts = m.m_Settings.emplace_back();
        ts.m_Type = SSettingBase::SLIDER; ts.m_Name = "Tick Speed"; ts.m_Id = "tas_speed";
        ts.m_pIntValue = &g_Config.m_shalawaTasTickSpeed; ts.m_Min = 1; ts.m_Max = 50;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Legit Mode", "", "tas_legit", &g_Config.m_shalawaTasLegitMode});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Pause", "", "tas_pause", &g_Config.m_shalawaTasPause});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Draw Start/End", "", "tas_draw", &g_Config.m_shalawaTasDrawStartEndPos});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Draw Path", "", "tas_path", &g_Config.m_shalawaTasDrawPath});
        auto &pw = m.m_Settings.emplace_back();
        pw.m_Type = SSettingBase::SLIDER; pw.m_Name = "Path Width"; pw.m_Id = "tas_pathw";
        pw.m_pIntValue = &g_Config.m_shalawaTasDrawPathWidth; pw.m_Min = 1; pw.m_Max = 20;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Predict Line", "", "tas_predline", &g_Config.m_shalawaPredictTas});
        auto &ppw = m.m_Settings.emplace_back();
        ppw.m_Type = SSettingBase::SLIDER; ppw.m_Name = "Predict Width"; ppw.m_Id = "tas_predw";
        ppw.m_pIntValue = &g_Config.m_shalawaTasPredictWidth; ppw.m_Min = 1; ppw.m_Max = 20;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "TAS World", "", "tas_world", &g_Config.m_shalawaTasWorld});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "TAS HUD", "", "tas_hud", &g_Config.m_shalawaTasHud});
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "fast_input"; m.m_Name = "Fast Input"; m.m_Description = "Skided from Tclient";
        m.m_Category = MCAT_MISC; m.m_pEnabled = &g_Config.m_ClFastInput;
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "hide_chat_bubble"; m.m_Name = "Hide Chat Bubble"; m.m_Description = "Silent typping (or typing idknow + idc)";
        m.m_Category = MCAT_MISC; m.m_pEnabled = &g_Config.m_ClHideChatBubble;
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "autoanswer"; m.m_Name = "Auto Answer"; m.m_Description = "Auto reply on mention & triggers";
        m.m_Category = MCAT_MISC; m.m_pEnabled = &g_Config.m_shalawaAutoAnswer;

        auto &delay = m.m_Settings.emplace_back();
        delay.m_Type = SSettingBase::SLIDER; delay.m_Name = "Delay (ms)"; delay.m_Id = "aa_delay";
        delay.m_pIntValue = &g_Config.m_shalawaAutoAnswerDelay; delay.m_Min = 100; delay.m_Max = 10000;

        m.m_Settings.push_back({SSettingBase::TOGGLE, "Team Reply", "", "aa_team", &g_Config.m_shalawaAutoAnswerTeam});
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Include Name", "", "aa_name", &g_Config.m_shalawaAutoAnswerIncludeName});

        {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::TEXT_INPUT;
            s.m_Name = "Mention Response";
            s.m_Id = "aa_mention_resp";
            s.m_pStringValue = g_Config.m_shalawaAutoAnswerMentionResponse;
            s.m_StringMaxLen = sizeof(g_Config.m_shalawaAutoAnswerMentionResponse);
        }

        {
            auto &s = m.m_Settings.emplace_back();
            s.m_Type = SSettingBase::TRIGGER_LIST;
            s.m_Name = "Triggers";
            s.m_Id = "aa_triggers";
        }

        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "hide"; m.m_Name = "Hide Cheat"; m.m_Description = "Hide cheat indicators";
        m.m_Category = MCAT_CONFIG; m.m_pEnabled = &g_Config.m_shalawaHideCheat;
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "hudeditor"; m.m_Name = "Edit HUD Layout"; m.m_Description = "Drag HUD elements";
        m.m_Category = MCAT_CONFIG; m.m_pEnabled = &g_Config.m_shalawaHudEditor;
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "menusettings"; m.m_Name = "Menu Settings"; m.m_Description = "Customize menu appearance";
        m.m_Category = MCAT_CONFIG;
        static int menuSettingsEnabled = 1;
        m.m_pEnabled = &menuSettingsEnabled;

        static int snowInt = 1;
        snowInt = m_Config.m_Snow ? 1 : 0;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Snow", "Snowflake particles", "ms_snow", &snowInt});

        static int tyanInt = 1;
        tyanInt = m_Config.m_ShowTyan ? 1 : 0;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Show Tyan", "Show anime girl", "ms_tyan", &tyanInt});

        static int bgAlphaInt = 40;
        bgAlphaInt = (int)(m_Config.m_BackgroundAlpha * 100.0f);
        auto &bgSlider = m.m_Settings.emplace_back();
        bgSlider.m_Type = SSettingBase::SLIDER; bgSlider.m_Name = "BG Opacity"; bgSlider.m_Id = "ms_bgalpha";
        bgSlider.m_pIntValue = &bgAlphaInt; bgSlider.m_Min = 0; bgSlider.m_Max = 100;

        static int glowInt = 1;
        glowInt = m_Config.m_Glow ? 1 : 0;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Glow", "Glow effects", "ms_glow", &glowInt});

        static int descInt = 1;
        descInt = m_Config.m_Descriptions ? 1 : 0;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Descriptions", "Show descriptions", "ms_desc", &descInt});

        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "menustyle"; m.m_Name = "Menu Style"; m.m_Description = "Switch between menu styles";
        m.m_Category = MCAT_CONFIG;
        static int styleEnabled = 1;
        m.m_pEnabled = &styleEnabled;
        static const char *styles[] = {"ClickGUI", "Modern"};
        auto &c = m.m_Settings.emplace_back();
        c.m_Type = SSettingBase::COMBO; c.m_Name = "Style"; c.m_Id = "ms_style";
        c.m_pIntValue = &g_Config.m_shalawaMenuStyle; c.m_pComboItems = styles; c.m_ComboCount = 2;
        reg.AddModule(m);
    }

    {
        SModuleEntry m;
        m.m_Id = "menucolors"; m.m_Name = "Accent Color"; m.m_Description = "Menu color scheme";
        m.m_Category = MCAT_CONFIG;
        static int colorEnabled = 1;
        m.m_pEnabled = &colorEnabled;

        static int easingInt = 1;
        easingInt = m_Config.m_Color.m_Easing ? 1 : 0;
        m.m_Settings.push_back({SSettingBase::TOGGLE, "Gradient", "Use two-color gradient", "mc_gradient", &easingInt});

        auto &col1 = m.m_Settings.emplace_back();
        col1.m_Type = SSettingBase::COLOR;
        col1.m_Name = "Primary Color";
        col1.m_Id = "mc_color1";
        col1.m_pColorHue = &m_AccentHue1;
        col1.m_pColorSaturation = &m_AccentSat1;
        col1.m_pColorBrightness = &m_AccentBri1;
        col1.m_pColorAlpha = &m_AccentAlpha1;

        auto &col2 = m.m_Settings.emplace_back();
        col2.m_Type = SSettingBase::COLOR;
        col2.m_Name = "Secondary Color";
        col2.m_Id = "mc_color2";
        col2.m_pColorHue = &m_AccentHue2;
        col2.m_pColorSaturation = &m_AccentSat2;
        col2.m_pColorBrightness = &m_AccentBri2;
        col2.m_pColorAlpha = &m_AccentAlpha2;
        col2.m_VisibilityCheck = [this]() {
            SModuleEntry *p = CMenuRegistry::Instance().FindModule("menucolors");
            if(p) for(auto &s : p->m_Settings)
                if(s.m_Id == "mc_gradient" && s.m_pIntValue) return *s.m_pIntValue != 0;
            return false;
        };

        reg.AddModule(m);
    }

    auto mods = CMenuRegistry::Instance().GetModulesByCategory(m_SelectedCategory);
    if(!mods.empty() && m_SelectedModuleId.empty())
        m_SelectedModuleId = mods[0]->m_Id;
    m_pMenuSettingsModule = CMenuRegistry::Instance().FindModule("menusettings");
}

void CImGuiManager::RenderBackground()
{
    if(!ImGui::GetCurrentContext()) return;
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *d = ImGui::GetBackgroundDrawList();
    float targetBgAlpha = m_MenuOpen ? m_Config.m_BackgroundAlpha : 0.0f;
    m_BackgroundAlpha = Lerp(m_BackgroundAlpha, targetBgAlpha, m_DeltaTime * 8.0f);
    if(m_BackgroundAlpha > 0.01f)
    {
        d->AddRectFilled(ImVec2(0, 0), io.DisplaySize,
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, m_BackgroundAlpha)));
    }
}

void CImGuiManager::RenderTyan()
{
    if(!ImGui::GetCurrentContext()) return;
    if(!m_Config.m_ShowTyan || !m_TyanLoaded) return;
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *d = ImGui::GetBackgroundDrawList();
    if(!d) return;

    float targetAlpha = m_MenuOpen ? 1.0f : 0.0f;
    m_TyanAlpha = Lerp(m_TyanAlpha, targetAlpha, m_DeltaTime * 6.0f);
    if(m_MenuOpen && m_TyanBounce < 1.0f) m_TyanBounce += m_DeltaTime * 2.0f;
    else if(!m_MenuOpen) m_TyanBounce = 0.0f;
    if(m_TyanAlpha < 0.01f) return;
    float size = 280.0f;
    float bounceOff = EaseOutBounce(std::clamp(m_TyanBounce, 0.0f, 1.0f)) * size - size;
    float wobble = sinf(m_Time * 1.5f) * 3.0f;
    ImVec2 pos(io.DisplaySize.x - size - 15.0f, io.DisplaySize.y - size + bounceOff + wobble);
    if(m_TyanTextureGL != 0)
    {
        ImTextureID tex = (ImTextureID)(intptr_t)m_TyanTextureGL;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, m_TyanAlpha * 0.95f));
        d->AddImage(tex, pos, ImVec2(pos.x + size, pos.y + size), ImVec2(0, 0), ImVec2(1, 1), col);
    }
}

void CImGuiManager::RenderGameVisuals()
{
    if(!m_pGameClient || !m_pGameClient->Client() ||
       m_pGameClient->Client()->State() != IClient::STATE_ONLINE ||
       !m_pGameClient->m_Snap.m_pLocalCharacter)
        return;

    try { m_HitParticles.OnRender(); }
    catch(...) { m_HitParticles.OnReset(); }

    try {
        m_ChinaHat.m_pHue[0] = &m_ChinaHatHue1;
        m_ChinaHat.m_pSat[0] = &m_ChinaHatSat1;
        m_ChinaHat.m_pBri[0] = &m_ChinaHatBri1;
        m_ChinaHat.m_pHue[1] = &m_ChinaHatHue2;
        m_ChinaHat.m_pSat[1] = &m_ChinaHatSat2;
        m_ChinaHat.m_pBri[1] = &m_ChinaHatBri2;
        m_ChinaHat.m_pHue[2] = &m_ChinaHatHue3;
        m_ChinaHat.m_pSat[2] = &m_ChinaHatSat3;
        m_ChinaHat.m_pBri[2] = &m_ChinaHatBri3;
        m_ChinaHat.OnRender();
    }
    catch(...) { m_ChinaHat.OnReset(); }

    try { m_Tail.OnRender(); }
    catch(...) { m_Tail.OnReset(); }
}

void CImGuiManager::RenderSnowflakes()
{
    if(!m_Config.m_Snow) return;
    if(!ImGui::GetCurrentContext()) return;

    float targetA = m_MenuOpen ? 1.0f : 0.0f;
    m_SnowAlpha = Lerp(m_SnowAlpha, targetA, m_DeltaTime * 3.0f);
    if(m_SnowAlpha < 0.01f) return;

    UpdateSnowflakes();
    ImDrawList *d = ImGui::GetBackgroundDrawList();
    if(!d) return;

    for(const auto &s : m_Snowflakes)
    {
        float ox = m_MouseLerpX * s.size * 1.5f;
        float oy = m_MouseLerpY * s.size * 1.5f;
        float a = s.alpha * m_SnowAlpha;
        d->AddCircleFilled(ImVec2(s.x + ox, s.y + oy), s.size * 1.5f,
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.8f, 0.85f, 1.0f, a * 0.3f)), 6);
        d->AddCircleFilled(ImVec2(s.x + ox, s.y + oy), s.size,
            ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, a)), 6);
    }
}

void CImGuiManager::RenderMenuWindow()
{
	if(!ImGui::GetCurrentContext() || !m_Initialized)
		return;

	ImGuiIO &io = ImGui::GetIO();
	float bgW = BG_WIDTH, bgH = BG_HEIGHT;

	float centerX = (io.DisplaySize.x - bgW) / 2.0f + m_WindowOffsetX;
	float centerY = (io.DisplaySize.y - bgH) / 2.0f + m_WindowOffsetY;

	float easedOpen = SmoothStep(m_OpenAnimValue);
	centerY += (1.0f - easedOpen) * (m_MenuOpen ? -22.0f : 22.0f);
	float alphaM = SmoothStep(m_MenuAlpha);

	ImVec2 wp(centerX, centerY);

	ImGui::SetNextWindowPos(wp);
	ImGui::SetNextWindowSize(ImVec2(bgW, bgH));
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alphaM);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;

	if(ImGui::Begin("##shalawa_menu", nullptr, flags))
	{
		ImDrawList *d = ImGui::GetWindowDrawList();
		if(d)
		{
			CGlassRenderer::Instance().AddGlassRect(d,
				wp, ImVec2(wp.x + bgW, wp.y + bgH),
				ImVec4(0.02f, 0.02f, 0.03f, 0.45f * alphaM),
				MENU_CORNER_R, 0.06f * alphaM, 0.6f * alphaM);

			if(m_Config.m_Glow)
			{
				ImVec4 ac = GetAccentColor(wp.y + bgH * 0.5f);
				for(int i = 3; i >= 0; i--)
				{
					float t = (float)i / 3.0f;
					float sp = t * 35.0f;
					d->AddRectFilled(
						ImVec2(wp.x - sp, wp.y - sp),
						ImVec2(wp.x + bgW + sp, wp.y + bgH + sp),
						ImGui::ColorConvertFloat4ToU32(ImVec4(ac.x, ac.y, ac.z, (1.0f - t) * 0.06f * alphaM)),
						MENU_CORNER_R + sp * 0.5f);
				}
			}

			float pad = MENU_PADDING;

			RenderTabBar(wp.x + pad, wp.y + pad, bgW - pad * 2, alphaM);

			float contentY = wp.y + pad + TAB_BAR_H + 8.0f;
			float contentH = bgH - pad * 2 - TAB_BAR_H - 8.0f;
			float contentW = bgW - pad * 2;

			if(m_SearchActive && m_SearchPanelAlpha > 0.01f)
			{
				RenderSearchBar(wp.x + pad, contentY, contentW);
				float srY = contentY + 28.0f;
				float srH = contentH - 28.0f;
				if(!m_SearchResults.empty())
					RenderSearchResults(wp.x + pad, srY, contentW, srH);
			}
			else if(m_SelectedCategory == MCAT_KEYBINDS)
			{
				RenderKeybindsPanel(wp.x + pad, contentY, contentW, contentH, alphaM);
			}
			else
			{
				RenderCardGrid(wp.x + pad, contentY, contentW, contentH, alphaM);
			}
		}
	}
	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(3);
}

void CImGuiManager::RenderTabBar(float x, float y, float w, float alpha)
{
	ImDrawList *d = ImGui::GetWindowDrawList();
	if(!d) return;
	ImGuiIO &io = ImGui::GetIO();

	float barH = TAB_BAR_H;
	float innerPad = 4.0f;

	d->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + barH),
		ImGui::ColorConvertFloat4ToU32(ImVec4(0.055f, 0.055f, 0.07f, 0.75f * alpha)),
		barH / 2);
	d->AddRect(ImVec2(x, y), ImVec2(x + w, y + barH),
		ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.035f * alpha)),
		barH / 2, 0, 0.5f);

	const char *tabNames[] = {"Combat", "Movement", "Visual", "Misc", "Config", "Keybinds"};
	int tabCount = MCAT_COUNT;

	float tabsW = w - innerPad * 2;
	float singleW = tabsW / tabCount;
	float tabH = barH - innerPad * 2;

	float targetIX = x + innerPad + (int)m_SelectedCategory * singleW;
	m_TabIndicatorX = Lerp(m_TabIndicatorX, targetIX, m_DeltaTime * 10.0f);
	m_TabIndicatorW = Lerp(m_TabIndicatorW, singleW, m_DeltaTime * 10.0f);

	d->AddRectFilled(
		ImVec2(m_TabIndicatorX, y + innerPad),
		ImVec2(m_TabIndicatorX + m_TabIndicatorW, y + innerPad + tabH),
		ImGui::ColorConvertFloat4ToU32(ImVec4(0.16f, 0.16f, 0.2f, 0.75f * alpha)),
		tabH / 2);
	d->AddRect(
		ImVec2(m_TabIndicatorX, y + innerPad),
		ImVec2(m_TabIndicatorX + m_TabIndicatorW, y + innerPad + tabH),
		ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.05f * alpha)),
		tabH / 2, 0, 0.5f);

	for(int i = 0; i < tabCount; i++)
	{
		float tx = x + innerPad + i * singleW;
		float ty = y + innerPad;

		bool isActive = ((int)m_SelectedCategory == i);
		bool isHov = IsHover(io.MousePos.x, io.MousePos.y, tx, ty, singleW, tabH);

		m_TabHoverAnims[i] = Lerp(m_TabHoverAnims[i], (isActive || isHov) ? 1.0f : 0.0f, m_DeltaTime * 10.0f);

		float tA = (0.35f + m_TabHoverAnims[i] * 0.65f) * alpha;
		ImVec2 tSz = ImGui::CalcTextSize(tabNames[i]);
		d->AddText(
			ImVec2(tx + (singleW - tSz.x) / 2, ty + (tabH - tSz.y) / 2),
			ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, tA)),
			tabNames[i]);

        if(isHov && ImGui::IsMouseClicked(0) && !m_SearchActive)
        {
            if(m_SelectedCategory != (EMenuCategory)i)
            {
                m_PreviousCategory = m_SelectedCategory;
                m_HeaderTransition = 0.0f;
                m_TabContentAnim = 0.0f;
            }
            m_SelectedCategory = (EMenuCategory)i;
            m_CardScrollTarget = 0;
        }
	}
}

void CImGuiManager::RenderCardGrid(float x, float y, float w, float h, float alpha)
{
	ImDrawList *d = ImGui::GetWindowDrawList();
	if(!d) return;
	ImGuiIO &io = ImGui::GetIO();

	alpha *= std::clamp(m_TabContentAnim, 0.0f, 1.0f);
	if(alpha < 0.01f) return;

	auto modulesCopy = CMenuRegistry::Instance().GetModulesByCategory(m_SelectedCategory);
	std::vector<SModuleEntry *> modules;
	for(auto *m : modulesCopy)
		if(m && m->m_pEnabled)
			modules.push_back(m);

	d->PushClipRect(ImVec2(x, y), ImVec2(x + w, y + h), true);

	float cardW = (w - CARD_GAP) / 2.0f;
	float col1X = x;
	float col2X = x + cardW + CARD_GAP;
	float colY[2] = {y - m_CardScrollCurrent, y - m_CardScrollCurrent};

	for(size_t i = 0; i < modules.size(); i++)
	{
		SModuleEntry *pMod = modules[i];
		int col = i % 2;
		float cx = (col == 0) ? col1X : col2X;

		SCardAnim &a = m_CardAnims[pMod->m_Id];

		if(colY[col] > y + h + 50.0f || colY[col] + 300.0f < y)
		{
			colY[col] += a.m_AnimHeight + CARD_GAP;
			continue;
		}

		float cH = RenderCard(pMod, cx, colY[col], cardW, alpha);
		colY[col] += cH + CARD_GAP;
	}

	d->PopClipRect();

	float maxColY = std::max(colY[0], colY[1]) + m_CardScrollCurrent;
	float totalH = maxColY - y;
	if(IsHover(io.MousePos.x, io.MousePos.y, x, y, w, h))
	{
		float maxScroll = std::max(0.0f, totalH - h);
		m_CardScrollTarget -= io.MouseWheel * 35.0f;
		m_CardScrollTarget = std::clamp(m_CardScrollTarget, 0.0f, maxScroll);
	}
}

float CImGuiManager::RenderCard(SModuleEntry *pMod, float x, float y, float w, float alpha)
{
	ImDrawList *d = ImGui::GetWindowDrawList();
	if(!d || !pMod || !pMod->m_pEnabled)
		return CARD_COLLAPSED_H;
	ImGuiIO &io = ImGui::GetIO();

	SCardAnim &anim = m_CardAnims[pMod->m_Id];
    bool isEnabled = *pMod->m_pEnabled != 0;
	if(anim.m_AnimHeight < 1.0f)
	{
		anim.m_ToggleAnim = isEnabled ? 1.0f : 0.0f;
		anim.m_AnimHeight = CARD_COLLAPSED_H;
	}

	anim.m_ToggleAnim = Lerp(anim.m_ToggleAnim, isEnabled ? 1.0f : 0.0f, m_DeltaTime * 8.0f);
	anim.m_ExpandAnim = Lerp(anim.m_ExpandAnim, anim.m_Expanded ? 1.0f : 0.0f, m_DeltaTime * 10.0f);
	anim.m_PlusRotation = Lerp(anim.m_PlusRotation, anim.m_Expanded ? 1.0f : 0.0f, m_DeltaTime * 10.0f);
	anim.m_ContentAlpha = Lerp(anim.m_ContentAlpha, anim.m_Expanded ? 1.0f : 0.0f, m_DeltaTime * 12.0f);

	float targetH = CARD_COLLAPSED_H;
	if(anim.m_Expanded && !pMod->m_Settings.empty())
		targetH = CARD_COLLAPSED_H + 10.0f + anim.m_LastSettingsH + CARD_PADDING;

	anim.m_AnimHeight = Lerp(anim.m_AnimHeight, targetH, m_DeltaTime * 12.0f);
	if(std::abs(anim.m_AnimHeight - targetH) < 1.0f)
		anim.m_AnimHeight = targetH;
	float cardH = std::max(anim.m_AnimHeight, CARD_COLLAPSED_H);

	bool isHovered = IsHover(io.MousePos.x, io.MousePos.y, x, y, w, cardH);
	anim.m_HoverAnim = Lerp(anim.m_HoverAnim, isHovered ? 1.0f : 0.0f, m_DeltaTime * 8.0f);

	float disabledMul = 0.5f + anim.m_ToggleAnim * 0.5f;

	d->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + cardH),
		ImGui::ColorConvertFloat4ToU32(ImVec4(0.09f, 0.09f, 0.11f, 0.6f * disabledMul * alpha)),
		CARD_CORNER_R);

	if(anim.m_HoverAnim > 0.01f)
	{
		d->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + cardH),
			ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.015f * anim.m_HoverAnim * alpha)),
			CARD_CORNER_R);
	}

	d->AddRect(ImVec2(x, y), ImVec2(x + w, y + cardH),
		ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.04f * alpha)),
		CARD_CORNER_R, 0, 0.5f);


	bool needsClip = anim.m_ExpandAnim < 0.98f;
	if(needsClip)
		d->PushClipRect(ImVec2(x, y), ImVec2(x + w, y + cardH), true);

	float pad = CARD_PADDING;
	float titleY = y + pad;
	float textA = disabledMul * alpha;

	d->AddText(ImVec2(x + pad, titleY),
		ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.92f * textA)),
		pMod->m_Name.c_str());
	ImVec2 titleSz = ImGui::CalcTextSize(pMod->m_Name.c_str());

	std::string badge = pMod->m_Key != 0 ? GetKeyName(pMod->m_Key) : "NONE";
	ImVec2 badgeSz = ImGui::CalcTextSize(badge.c_str());
	float bx = x + pad + titleSz.x + 8.0f;
	float by = titleY + 1.0f;
	float bw = badgeSz.x + 12.0f;
	float bh = badgeSz.y + 4.0f;
	d->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh),
		ImGui::ColorConvertFloat4ToU32(ImVec4(0.13f, 0.13f, 0.16f, 0.6f * textA)),
		bh / 2);
	d->AddText(ImVec2(bx + 6, by + 2),
		ImGui::ColorConvertFloat4ToU32(ImVec4(0.55f, 0.55f, 0.6f, 0.65f * textA)),
		badge.c_str());

	float toggleW = 36.0f, toggleH = 18.0f;
	float toggleX = x + w - pad - toggleW;
	float toggleY2 = titleY + (titleSz.y - toggleH) / 2.0f;
	RenderPillToggle(d, toggleX, toggleY2, toggleW, toggleH, isEnabled, anim, textA);
	if(IsHover(io.MousePos.x, io.MousePos.y, toggleX - 4, toggleY2 - 4, toggleW + 8, toggleH + 8)
		&& ImGui::IsMouseClicked(0))
		*pMod->m_pEnabled = isEnabled ? 0 : 1;

	float row2Y = titleY + titleSz.y + 12.0f;
	float btnSz = 22.0f;
	float btnX = x + pad;
	float btnY = row2Y;

	bool expandHov = IsHover(io.MousePos.x, io.MousePos.y, btnX - 2, btnY - 2, btnSz + 4, btnSz + 4);
	float expandHA = GetAnimValue("exp_" + pMod->m_Id, expandHov ? 1.0f : 0.0f, 12.0f);

	d->AddRectFilled(ImVec2(btnX, btnY), ImVec2(btnX + btnSz, btnY + btnSz),
		ImGui::ColorConvertFloat4ToU32(LerpColor(
			ImVec4(0.12f, 0.12f, 0.15f, 0.7f * alpha),
			ImVec4(0.18f, 0.18f, 0.22f, 0.85f * alpha), expandHA)),
		6.0f);

	float ccx = btnX + btnSz / 2.0f, ccy = btnY + btnSz / 2.0f;
	float ll = 5.0f;
	float rot = anim.m_PlusRotation * 0.7854f;
	float cr = std::cos(rot), sr = std::sin(rot);
	ImU32 pc = ImGui::ColorConvertFloat4ToU32(ImVec4(0.65f, 0.65f, 0.7f, 0.8f * alpha));
	d->AddLine(ImVec2(ccx - ll * cr, ccy - ll * sr), ImVec2(ccx + ll * cr, ccy + ll * sr), pc, 1.5f);
	d->AddLine(ImVec2(ccx + ll * sr, ccy - ll * cr), ImVec2(ccx - ll * sr, ccy + ll * cr), pc, 1.5f);

	if(expandHov && ImGui::IsMouseClicked(0) && !pMod->m_Settings.empty())
		anim.m_Expanded = !anim.m_Expanded;

	if(!pMod->m_Description.empty())
	{
		d->AddText(ImVec2(btnX + btnSz + 8.0f, row2Y + (btnSz - 12) / 2.0f),
			ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.55f, 0.5f * textA)),
			pMod->m_Description.c_str());
	}

	if(anim.m_ExpandAnim > 0.01f && !pMod->m_Settings.empty())
	{
		float sepY = y + CARD_COLLAPSED_H;
		d->AddLine(ImVec2(x + pad, sepY), ImVec2(x + w - pad, sepY),
			ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.05f * anim.m_ContentAlpha * alpha)));

		float sY = sepY + 8.0f;
		float sW = w - pad * 2;
		float sStartY = sY;
		float sa = alpha * anim.m_ContentAlpha;

		for(auto &setting : pMod->m_Settings)
		{
			if(!setting.IsVisible()) continue;
			float sH = 0;

			bool isExpandable = (setting.m_Type == SSettingBase::COMBO ||
				setting.m_Type == SSettingBase::COLOR ||
				setting.m_Type == SSettingBase::MULTI_SELECT);
			if(isExpandable && needsClip) { d->PopClipRect(); needsClip = false; }

			try {
				switch(setting.m_Type) {
				case SSettingBase::TOGGLE: if(setting.m_pIntValue) sH = RenderCheckbox(setting, x + pad, sY, sW, sa); break;
				case SSettingBase::SLIDER: if(setting.m_pIntValue && setting.m_Max > setting.m_Min) sH = RenderSlider(setting, x + pad, sY, sW, sa); break;
				case SSettingBase::COMBO: if(setting.m_pIntValue && setting.m_pComboItems) sH = RenderCombo(setting, x + pad, sY, sW, sa); break;
				case SSettingBase::TEXT_INPUT: sH = RenderTextInput(setting, x + pad, sY, sW, sa); break;
				case SSettingBase::BUTTON: sH = RenderButton(setting, x + pad, sY, sW, sa); break;
				case SSettingBase::COLOR: if(setting.m_pColorHue) sH = RenderColorPicker(setting, x + pad, sY, sW, sa); break;
				case SSettingBase::MULTI_SELECT: sH = RenderMultiSelect(setting, x + pad, sY, sW, sa); break;
				case SSettingBase::TRIGGER_LIST: sH = RenderTriggerList(setting, x + pad, sY, sW, sa); break;
				default: break;
				}
			} catch(...) { sH = SETTING_ITEM_H; }
			sY += sH + 3.0f;
		}

		float newH = sY - sStartY;
		if(std::abs(newH - anim.m_LastSettingsH) > 2.0f)
			anim.m_LastSettingsH = newH;
	}
	else
	{
		if(!anim.m_Expanded)
			anim.m_LastSettingsH = 0;
	}

	if(needsClip)
		d->PopClipRect();

	return cardH;
}

void CImGuiManager::RenderPillToggle(ImDrawList *d, float x, float y, float tw, float th,
	bool isOn, SCardAnim &anim, float alpha)
{
	ImVec4 ac = GetAccentColor(y);

	ImVec4 trackOff(0.16f, 0.16f, 0.2f, 0.85f * alpha);
	ImVec4 trackOn(ac.x * 0.55f, ac.y * 0.55f, ac.z * 0.55f, 0.9f * alpha);
	d->AddRectFilled(ImVec2(x, y), ImVec2(x + tw, y + th),
		ImGui::ColorConvertFloat4ToU32(LerpColor(trackOff, trackOn, anim.m_ToggleAnim)),
		th / 2);

	float knobR = (th - 4.0f) / 2.0f;
	float knobMinX = x + 2.0f + knobR;
	float knobMaxX = x + tw - 2.0f - knobR;
	float knobX = knobMinX + (knobMaxX - knobMinX) * anim.m_ToggleAnim;
	float knobY = y + th / 2.0f;

	ImVec4 knobOff(0.4f, 0.4f, 0.45f, alpha);
	ImVec4 knobOn(1.0f, 1.0f, 1.0f, alpha);
	d->AddCircleFilled(ImVec2(knobX, knobY), knobR,
		ImGui::ColorConvertFloat4ToU32(LerpColor(knobOff, knobOn, anim.m_ToggleAnim)));
}

void CImGuiManager::RenderSearchBar(float x, float y, float w)
{
    ImDrawList *d = ImGui::GetWindowDrawList();
    if(!d) return;

    ImGuiIO &io = ImGui::GetIO();
    float h = 20.0f;

    bool isHovered = IsHover(io.MousePos.x, io.MousePos.y, x, y, w, h);
    float hoverAnim = GetAnimValue("searchbar_hover", isHovered ? 1.0f : 0.0f, 10.0f);
    float activeAnim = GetAnimValue("searchbar_active", m_SearchActive ? 1.0f : 0.0f, 10.0f);

    ImVec4 bgCol = LerpColor(
        ImVec4(0.1f, 0.1f, 0.12f, 0.6f),
        ImVec4(0.14f, 0.14f, 0.17f, 0.9f),
        std::max(hoverAnim, activeAnim));
    d->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
        ImGui::ColorConvertFloat4ToU32(bgCol), 4.0f);

    ImVec4 accent = GetAccentColor(y);
    ImVec4 borderCol = LerpColor(
        ImVec4(0.2f, 0.2f, 0.22f, 0.4f),
        ImVec4(accent.x, accent.y, accent.z, 0.6f),
        activeAnim);
    d->AddRect(ImVec2(x, y), ImVec2(x + w, y + h),
        ImGui::ColorConvertFloat4ToU32(borderCol), 4.0f, 0, 0.5f);

    float iconSize = 4.0f;
    float iconX = x + 8;
    float iconY = y + h / 2;
    d->AddCircle(ImVec2(iconX, iconY - 1), iconSize,
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.55f, 0.7f)), 12, 1.0f);
    d->AddLine(ImVec2(iconX + 3, iconY + 2), ImVec2(iconX + 5, iconY + 4),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.55f, 0.7f)), 1.0f);

    float textX = x + 18;

    d->PushClipRect(ImVec2(textX, y), ImVec2(x + w - 4, y + h), true);

    if(m_SearchActive && m_SearchBuffer[0])
    {
        d->AddText(ImVec2(textX, y + (h - 12) / 2),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.9f, 0.9f, 0.93f, 1.0f)), m_SearchBuffer);
    }
    else if(m_SearchActive)
    {
        d->AddText(ImVec2(textX, y + (h - 12) / 2),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 0.4f, 0.42f, 0.5f)), "Search...");
    }
    else
    {
        d->AddText(ImVec2(textX, y + (h - 12) / 2),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 0.4f, 0.42f, 0.4f)), "Ctrl+F");
    }

    d->PopClipRect();

    if(m_SearchActive)
    {
        float cursorAlpha = sinf(m_SearchCursorBlink * 3.14159f * 2.0f) * 0.5f + 0.5f;
        if(cursorAlpha > 0.3f)
        {
            float cursorX = textX;
            if(m_SearchBuffer[0])
            {
                int bufLen = (int)strlen(m_SearchBuffer);
                int safePos = std::clamp(m_SearchCursorPos, 0, bufLen);
                std::string beforeCursor(m_SearchBuffer, safePos);
                ImVec2 tSize = ImGui::CalcTextSize(beforeCursor.c_str());
                cursorX = textX + tSize.x;
            }
            d->AddLine(ImVec2(cursorX, y + 4), ImVec2(cursorX, y + h - 4),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.8f, 0.8f, 0.82f, cursorAlpha)), 1.0f);
        }
    }

    if(isHovered && ImGui::IsMouseClicked(0) && !m_SearchActive)
    {
        m_SearchActive = true;
        m_SearchFocused = true;
        m_SearchBuffer[0] = '\0';
        m_SearchCursorPos = 0;
        m_SearchResults.clear();
        m_SelectedSearchModuleId.clear();
        m_SearchScrollTarget = 0;
        for(auto &p : m_SliderAnims) { p.second.m_InputMode = false; p.second.m_InputText.clear(); }
        for(auto &p : m_TextInputAnims) p.second.m_Focused = false;
        for(auto &p : m_ColorPickerAnims) { p.second.m_HexActive = false; p.second.m_Expanded = false; }
        SDL_StartTextInput();
    }
}

void CImGuiManager::RenderSearchResults(float x, float y, float w, float h)
{
    ImDrawList *d = ImGui::GetWindowDrawList();
    if(!d) return;

    ImGuiIO &io = ImGui::GetIO();
    float panelAlpha = m_SearchPanelAlpha;
    if(panelAlpha < 0.01f) return;

    d->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.06f, 0.06f, 0.07f, 0.95f * panelAlpha)), 6.0f);
    d->AddRect(ImVec2(x, y), ImVec2(x + w, y + h),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.15f, 0.15f, 0.17f, 0.5f * panelAlpha)), 6.0f, 0, 0.5f);

    if(m_SearchResults.empty())
    {
        const char *emptyText = m_SearchBuffer[0] ? "No modules found" : "Start typing to search...";
        ImVec2 tSize = ImGui::CalcTextSize(emptyText);
        float emptyY = y + h / 2 - tSize.y / 2;
        d->AddText(ImVec2(x + (w - tSize.x) / 2, emptyY),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 0.4f, 0.45f, 0.6f * panelAlpha)), emptyText);
        return;
    }

    d->PushClipRect(ImVec2(x + 2, y + 2), ImVec2(x + w - 2, y + h - 2), true);

    float itemH = 32.0f;
    float itemSpacing = 2.0f;
    float padX = 6.0f;
    float startY = y + padX - m_SearchScrollCurrent;

    size_t maxResults = std::min(m_SearchResults.size(), (size_t)100);
    for(size_t i = 0; i < maxResults; i++)
    {
        if(i >= m_SearchResults.size()) break;
        SModuleEntry *mod = m_SearchResults[i];
        if(!mod) continue;

        std::string safeId;
        try {
            safeId = mod->m_Id;
        } catch(...) {
            continue;
        }
        if(safeId.empty()) continue;

        SModuleEntry *validMod = CMenuRegistry::Instance().FindModule(safeId);
        if(!validMod) continue;
        mod = validMod;

        if(!mod->m_pEnabled) continue;

        float iy = startY + i * (itemH + itemSpacing);
        if(iy + itemH < y || iy > y + h) continue;

        std::string animKey = "sr_" + mod->m_Id;
        if(m_SearchResultAnims.find(animKey) == m_SearchResultAnims.end())
            m_SearchResultAnims[animKey] = 0.0f;

        float &itemAnim = m_SearchResultAnims[animKey];
        float targetAnim = 1.0f;
        float delay = i * 0.06f;
        if(itemAnim < targetAnim)
            itemAnim = Lerp(itemAnim, targetAnim, m_DeltaTime * 8.0f);

        float itemAlpha = itemAnim * panelAlpha;
        if(itemAlpha < 0.01f) continue;

        float offsetX = (1.0f - EaseOutQuart(itemAnim)) * 30.0f;

        bool isHovered = IsHover(io.MousePos.x, io.MousePos.y, x + padX, iy, w - padX * 2, itemH);
        bool isSelected = (mod->m_Id == m_SelectedSearchModuleId);
        bool isEnabled = mod->m_pEnabled && *mod->m_pEnabled;

        float hoverTarget = isHovered ? 1.0f : 0.0f;
        std::string hoverKey = "srh_" + mod->m_Id;
        float hoverAnim = GetAnimValue(hoverKey, hoverTarget, 12.0f);

        ImVec4 accent = GetAccentColor(iy);

        ImVec4 bgClr;
        if(isSelected)
            bgClr = ImVec4(accent.x * 0.15f, accent.y * 0.15f, accent.z * 0.15f, 0.4f * itemAlpha);
        else if(hoverAnim > 0.01f)
            bgClr = ImVec4(0.12f, 0.12f, 0.14f, hoverAnim * 0.5f * itemAlpha);
        else
            bgClr = ImVec4(0.08f, 0.08f, 0.09f, 0.3f * itemAlpha);

        d->AddRectFilled(
            ImVec2(x + padX + offsetX, iy),
            ImVec2(x + w - padX, iy + itemH),
            ImGui::ColorConvertFloat4ToU32(bgClr), 4.0f);

        if(isSelected)
        {
            d->AddRectFilled(
                ImVec2(x + padX + offsetX, iy),
                ImVec2(x + padX + offsetX + 2.5f, iy + itemH),
                GetAccentU32(iy, 0.8f * itemAlpha), 1.0f);
        }

        if(isEnabled)
        {
            float dotX = x + padX + offsetX + 8;
            float dotY = iy + 8;
            d->AddCircleFilled(ImVec2(dotX, dotY), 3.0f,
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.8f, 0.4f, 0.8f * itemAlpha)));
        }
        else
        {
            float dotX = x + padX + offsetX + 8;
            float dotY = iy + 8;
            d->AddCircle(ImVec2(dotX, dotY), 3.0f,
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.5f, 0.4f * itemAlpha)), 12, 1.0f);
        }

        float textX = x + padX + offsetX + 18;
        ImVec4 nameClr = isEnabled ?
            ImVec4(0.92f, 0.92f, 0.95f, itemAlpha) :
            ImVec4(0.6f, 0.6f, 0.65f, 0.8f * itemAlpha);
        d->AddText(ImVec2(textX, iy + 4),
            ImGui::ColorConvertFloat4ToU32(nameClr), mod->m_Name.c_str());

        const char *catNames[] = {"Combat", "Movement", "Visual", "Misc", "Config", "Keybinds"};
        if(mod->m_Category < MCAT_COUNT)
        {
            ImVec4 catClr(0.45f, 0.45f, 0.5f, 0.6f * itemAlpha);
            d->AddText(ImVec2(textX, iy + 18),
                ImGui::ColorConvertFloat4ToU32(catClr), catNames[(int)mod->m_Category]);
        }

        if(!mod->m_Description.empty())
        {
            ImVec2 descSize = ImGui::CalcTextSize(mod->m_Description.c_str());
            float descX = x + w - padX - descSize.x - 4;
            if(descX > textX + 60)
            {
                d->AddText(ImVec2(descX, iy + 10),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.35f, 0.35f, 0.4f, 0.5f * itemAlpha)),
                    mod->m_Description.c_str());
            }
        }

        if(isHovered && ImGui::IsMouseClicked(0))
        {
            if(mod->m_pEnabled) *mod->m_pEnabled = *mod->m_pEnabled ? 0 : 1;
        }
        if(isHovered && ImGui::IsMouseClicked(1))
        {
            m_SearchActive = false;
            m_SearchFocused = false;
            m_PreviousCategory = m_SelectedCategory;
            m_SelectedCategory = mod->m_Category;
            m_SelectedModuleId = mod->m_Id;
            m_SettingScrollTarget = 0;
            m_HeaderTransition = 0;
            m_SelectedSearchModuleId.clear();
            d->PopClipRect();
            m_SearchResults.clear();
            return;
        }
    }

    d->PopClipRect();

    if(IsHover(io.MousePos.x, io.MousePos.y, x, y, w, h))
    {
        float totalH = (float)m_SearchResults.size() * (itemH + itemSpacing);
        float maxScroll = std::max(0.0f, totalH - h + padX * 2);
        m_SearchScrollTarget -= io.MouseWheel * 30.0f;
        m_SearchScrollTarget = std::clamp(m_SearchScrollTarget, 0.0f, maxScroll);
    }
}



float CImGuiManager::RenderCheckbox(SSettingBase &setting, float x, float y, float w, float alpha)
{
	if(!setting.m_pIntValue || !ImGui::GetCurrentContext())
		return 0;

	ImDrawList *d = ImGui::GetWindowDrawList();
	if(!d) return 0;
	ImGuiIO &io = ImGui::GetIO();
	float h = SETTING_ITEM_H;

	bool isOn = *setting.m_pIntValue != 0;

	SCheckboxAnim &anim = m_CheckboxAnims[setting.m_Id];
	anim.m_CheckAnim = Lerp(anim.m_CheckAnim, isOn ? 1.0f : 0.0f, m_DeltaTime * 10.0f);

	float toggleW = 30.0f, toggleH = 15.0f;
	float toggleX = x + w - toggleW;
	float toggleY = y + (h - toggleH) / 2.0f;

	bool isHov = IsHover(io.MousePos.x, io.MousePos.y, x, y, w, h);
	anim.m_HoverAnim = Lerp(anim.m_HoverAnim, isHov ? 1.0f : 0.0f, m_DeltaTime * 10.0f);

	float textAlpha = (0.7f + anim.m_HoverAnim * 0.2f) * alpha;
	d->AddText(ImVec2(x, y + (h - 13) / 2),
		ImGui::ColorConvertFloat4ToU32(ImVec4(0.85f, 0.85f, 0.9f, textAlpha)),
		setting.m_Name.c_str());

	ImVec4 ac = GetAccentColor(y);
	ImVec4 trackOff(0.16f, 0.16f, 0.2f, 0.75f * alpha);
	ImVec4 trackOn(ac.x * 0.45f, ac.y * 0.45f, ac.z * 0.45f, 0.85f * alpha);
	d->AddRectFilled(ImVec2(toggleX, toggleY), ImVec2(toggleX + toggleW, toggleY + toggleH),
		ImGui::ColorConvertFloat4ToU32(LerpColor(trackOff, trackOn, anim.m_CheckAnim)),
		toggleH / 2);

	if(anim.m_CheckAnim > 0.01f)
	{
		d->AddRect(ImVec2(toggleX, toggleY), ImVec2(toggleX + toggleW, toggleY + toggleH),
			ImGui::ColorConvertFloat4ToU32(ImVec4(ac.x, ac.y, ac.z, 0.15f * anim.m_CheckAnim * alpha)),
			toggleH / 2, 0, 0.5f);
	}

	float knobR = (toggleH - 4.0f) / 2.0f;
	float knobMinX = toggleX + 2.0f + knobR;
	float knobMaxX = toggleX + toggleW - 2.0f - knobR;
	float knobX = knobMinX + (knobMaxX - knobMinX) * anim.m_CheckAnim;
	float knobCY = toggleY + toggleH / 2.0f;

	ImVec4 knobCol = LerpColor(ImVec4(0.4f, 0.4f, 0.45f, alpha), ImVec4(1, 1, 1, alpha), anim.m_CheckAnim);

	if(anim.m_CheckAnim > 0.3f)
	{
		d->AddCircleFilled(ImVec2(knobX, knobCY), knobR + 2.0f,
			ImGui::ColorConvertFloat4ToU32(ImVec4(ac.x, ac.y, ac.z, 0.15f * anim.m_CheckAnim * alpha)));
	}
	d->AddCircleFilled(ImVec2(knobX, knobCY), knobR,
		ImGui::ColorConvertFloat4ToU32(knobCol));

	if(isHov && ImGui::IsMouseClicked(0))
		*setting.m_pIntValue = isOn ? 0 : 1;

	return h;
}

float CImGuiManager::RenderSlider(SSettingBase &setting, float x, float y, float w, float alpha)
{
	if(!setting.m_pIntValue || !ImGui::GetCurrentContext())
		return 0;
	if(setting.m_Max <= setting.m_Min)
		return 0;

	ImDrawList *d = ImGui::GetWindowDrawList();
	if(!d) return 0;
	ImGuiIO &io = ImGui::GetIO();
	float totalH = 30.0f;

	SSliderAnim &anim = m_SliderAnims[setting.m_Id];
	float range = (float)(setting.m_Max - setting.m_Min);
	float targetPct = std::clamp((float)(*setting.m_pIntValue - setting.m_Min) / range, 0.0f, 1.0f);
	anim.m_AnimatedPercentage += (targetPct - anim.m_AnimatedPercentage) * 0.25f;

	float labelY = y + 1.0f;
	d->AddText(ImVec2(x, labelY),
		ImGui::ColorConvertFloat4ToU32(ImVec4(0.82f, 0.82f, 0.86f, 0.8f * alpha)),
		setting.m_Name.c_str());

	char valBuf[16];
	snprintf(valBuf, sizeof(valBuf), "%d", *setting.m_pIntValue);
	ImVec2 valSz = ImGui::CalcTextSize(valBuf);

	float valX = x + w - valSz.x;
	bool valHov = IsHover(io.MousePos.x, io.MousePos.y, valX - 4, labelY - 2, valSz.x + 8, 16);
	anim.m_InputAnim = Lerp(anim.m_InputAnim, anim.m_InputMode ? 1.0f : 0.0f, m_DeltaTime * 15.0f);

	if(anim.m_InputMode && anim.m_InputAnim > 0.5f)
	{
		float boxW = std::max(valSz.x + 12.0f, 40.0f);
		float boxX = x + w - boxW;
		d->AddRectFilled(ImVec2(boxX, labelY - 2), ImVec2(boxX + boxW, labelY + 14),
			ImGui::ColorConvertFloat4ToU32(ImVec4(0.14f, 0.14f, 0.17f, 0.9f * alpha)), 4.0f);
		d->AddRect(ImVec2(boxX, labelY - 2), ImVec2(boxX + boxW, labelY + 14),
			ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.55f, 0.5f * alpha)), 4.0f, 0, 0.5f);

		ImVec2 inputSz = ImGui::CalcTextSize(anim.m_InputText.c_str());
		float centX = boxX + (boxW - inputSz.x) / 2.0f;
		d->AddText(ImVec2(centX, labelY),
			ImGui::ColorConvertFloat4ToU32(ImVec4(0.9f, 0.9f, 0.93f, alpha)),
			anim.m_InputText.c_str());

		float blink = fmodf(m_Time * 2.0f, 1.0f) < 0.5f ? 1.0f : 0.0f;
		int cp = std::clamp(anim.m_CursorPos, 0, (int)anim.m_InputText.length());
		std::string before = anim.m_InputText.substr(0, cp);
		float curX = centX + ImGui::CalcTextSize(before.c_str()).x;
		d->AddLine(ImVec2(curX, labelY), ImVec2(curX, labelY + 12),
			ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.7f, 0.7f, blink * alpha)), 0.5f);
	}
	else
	{
		ImVec4 valCol = LerpColor(ImVec4(0.45f, 0.45f, 0.5f, alpha),
			ImVec4(0.7f, 0.7f, 0.75f, alpha), valHov ? 1.0f : 0.0f);
		d->AddText(ImVec2(valX, labelY), ImGui::ColorConvertFloat4ToU32(valCol), valBuf);
	}

	if(valHov && ImGui::IsMouseClicked(0) && !anim.m_InputMode)
	{
		for(auto &o : m_SliderAnims) { o.second.m_InputMode = false; o.second.m_InputText.clear(); }
		anim.m_InputMode = true;
		anim.m_InputText = valBuf;
		anim.m_CursorPos = (int)anim.m_InputText.length();
	}
	if(anim.m_InputMode && !valHov && ImGui::IsMouseClicked(0))
	{
		if(!anim.m_InputText.empty())
		{
			try { int v = std::stoi(anim.m_InputText);
				*setting.m_pIntValue = std::clamp(v, setting.m_Min, setting.m_Max);
			} catch(...) {}
		}
		anim.m_InputMode = false;
		anim.m_InputText.clear();
	}

	float trackY = y + 18.0f;
	float trackH = 4.0f;
	float trackW = w;
	float trackR = trackH / 2.0f;

	bool trackHov = IsHover(io.MousePos.x, io.MousePos.y, x, trackY - 4, trackW, trackH + 8);
	anim.m_KnobAnim = Lerp(anim.m_KnobAnim, (trackHov || anim.m_Dragging) ? 1.0f : 0.0f, m_DeltaTime * 12.0f);

	d->AddRectFilled(ImVec2(x, trackY), ImVec2(x + trackW, trackY + trackH),
		ImGui::ColorConvertFloat4ToU32(ImVec4(0.14f, 0.14f, 0.17f, 0.8f * alpha)), trackR);

	float filledW = trackW * anim.m_AnimatedPercentage;
	if(filledW > 1.0f)
	{
		ImVec4 ac = GetAccentColor(y);
		d->AddRectFilled(ImVec2(x, trackY), ImVec2(x + filledW, trackY + trackH),
			ImGui::ColorConvertFloat4ToU32(ImVec4(ac.x * 0.6f, ac.y * 0.6f, ac.z * 0.6f, 0.85f * alpha)), trackR);
	}

	float knobR = 5.0f + anim.m_KnobAnim * 1.5f;
	float knobX = x + trackW * anim.m_AnimatedPercentage;
	knobX = std::clamp(knobX, x + knobR, x + trackW - knobR);
	float knobCY = trackY + trackH / 2.0f;

	ImVec4 ac = GetAccentColor(y);

	if(anim.m_KnobAnim > 0.01f)
	{
		d->AddCircleFilled(ImVec2(knobX, knobCY), knobR + 3.0f,
			ImGui::ColorConvertFloat4ToU32(ImVec4(ac.x, ac.y, ac.z, 0.12f * anim.m_KnobAnim * alpha)));
	}

	d->AddCircleFilled(ImVec2(knobX, knobCY), knobR,
		ImGui::ColorConvertFloat4ToU32(ImVec4(0.92f, 0.92f, 0.95f, alpha)));

	d->AddCircle(ImVec2(knobX, knobCY), knobR,
		ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.15f * alpha)), 0, 0.5f);

	if(!anim.m_InputMode)
	{
		if(trackHov && ImGui::IsMouseClicked(0))
			anim.m_Dragging = true;
		if(anim.m_Dragging)
		{
			if(ImGui::IsMouseDown(0))
			{
				float newT = std::clamp((io.MousePos.x - x) / trackW, 0.0f, 1.0f);
				*setting.m_pIntValue = setting.m_Min + (int)(newT * range + 0.5f);
			}
			else
				anim.m_Dragging = false;
		}
	}

	return totalH;
}

float CImGuiManager::RenderCombo(SSettingBase &setting, float x, float y, float w, float alpha)
{
    if(!setting.m_pIntValue || !setting.m_pComboItems || setting.m_ComboCount <= 0 || !ImGui::GetCurrentContext()) return 0;
    for(int i = 0; i < setting.m_ComboCount; i++)
        if(!setting.m_pComboItems[i]) return 0;
    if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || w <= 0) return 0;
    if(setting.m_ComboCount > 256) return 0;

    ImDrawList *d = ImGui::GetWindowDrawList();
    if(!d) return 0;
    ImGuiIO &io = ImGui::GetIO();
    float h = SETTING_ITEM_H;

    int idx = std::clamp(*setting.m_pIntValue, 0, setting.m_ComboCount - 1);
    *setting.m_pIntValue = idx;
    const char *current = setting.m_pComboItems[idx];

    float comboW = 65.0f, comboH = 18.0f;
    float comboX = x + w - comboW;
    float comboY = y + (h - comboH) / 2;

    SComboAnim &anim = m_ComboAnims[setting.m_Id];
    bool isHovered = IsHover(io.MousePos.x, io.MousePos.y, comboX, comboY, comboW, comboH);
    anim.m_HoverAnim = Lerp(anim.m_HoverAnim, isHovered ? 1.0f : 0.0f, m_DeltaTime * 12.0f);
    anim.m_ExpandAnim = Lerp(anim.m_ExpandAnim, anim.m_Expanded ? 1.0f : 0.0f, m_DeltaTime * 12.0f);

    d->AddText(ImVec2(x, y + (h - 13) / 2),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.7f, 0.75f, alpha)), setting.m_Name.c_str());

    ImVec4 comboBg = LerpColor(ImVec4(0.12f, 0.12f, 0.15f, alpha),
        ImVec4(0.18f, 0.18f, 0.22f, alpha), anim.m_HoverAnim);
    d->AddRectFilled(ImVec2(comboX, comboY), ImVec2(comboX + comboW, comboY + comboH),
        ImGui::ColorConvertFloat4ToU32(comboBg), 4.0f);
    ImVec4 comboAccent = GetAccentColor(y);
    ImVec4 comboBorder = LerpColor(ImVec4(0.25f, 0.25f, 0.28f, 0.5f * alpha),
        ImVec4(comboAccent.x, comboAccent.y, comboAccent.z, 0.4f * alpha), anim.m_HoverAnim);
    d->AddRect(ImVec2(comboX, comboY), ImVec2(comboX + comboW, comboY + comboH),
        ImGui::ColorConvertFloat4ToU32(comboBorder), 4.0f, 0, 0.5f);

    d->PushClipRect(ImVec2(comboX + 4, comboY), ImVec2(comboX + comboW - 14, comboY + comboH), true);
    ImVec2 textSize = ImGui::CalcTextSize(current);
    d->AddText(ImVec2(comboX + 5, comboY + (comboH - textSize.y) / 2),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.85f, 0.85f, 0.9f, alpha)), current);
    d->PopClipRect();

    float arrowX = comboX + comboW - 9;
    float arrowY = comboY + comboH / 2;
    d->AddTriangleFilled(ImVec2(arrowX - 3, arrowY - 2), ImVec2(arrowX + 3, arrowY - 2),
        ImVec2(arrowX, arrowY + 2),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.55f, 0.8f * alpha)));

    float totalH = h;

    if(anim.m_Expanded && anim.m_ExpandAnim > 0.01f)
    {
        float optH = 16.0f;
        float panelH = setting.m_ComboCount * optH;
        float panelY = comboY + comboH + 2;
        float visH = panelH * anim.m_ExpandAnim;

        d->AddRectFilled(ImVec2(comboX, panelY), ImVec2(comboX + comboW, panelY + visH),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.1f, 0.1f, 0.12f, 0.95f * alpha)), 4.0f);
        d->AddRect(ImVec2(comboX, panelY), ImVec2(comboX + comboW, panelY + visH),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.2f, 0.2f, 0.22f, 0.5f * alpha)), 4.0f);

        d->PushClipRect(ImVec2(comboX, panelY), ImVec2(comboX + comboW, panelY + visH), true);

        for(int i = 0; i < setting.m_ComboCount; i++)
        {
            float oy = panelY + i * optH;
            bool optHov = IsHover(io.MousePos.x, io.MousePos.y, comboX, oy, comboW, optH) && anim.m_ExpandAnim > 0.8f;

            std::string optKey = setting.m_Id + "_opt_" + std::to_string(i);
            float &optHoverAnim = anim.m_OptionHovers[optKey];
            optHoverAnim = Lerp(optHoverAnim, optHov ? 1.0f : 0.0f, m_DeltaTime * 12.0f);

            if(optHoverAnim > 0.01f)
            {
                d->AddRectFilled(ImVec2(comboX + 2, oy), ImVec2(comboX + comboW - 2, oy + optH),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.2f, 0.2f, 0.22f, optHoverAnim * 0.5f * alpha)), 3.0f);
            }

            bool isCurrent = (i == idx);
            ImVec4 optTextClr = isCurrent ? GetAccentColor(oy) : ImVec4(0.7f, 0.7f, 0.75f, alpha);
            if(isCurrent) optTextClr.w = alpha;
            d->AddText(ImVec2(comboX + 6, oy + (optH - 12) / 2),
                ImGui::ColorConvertFloat4ToU32(optTextClr), setting.m_pComboItems[i]);

            if(optHov && ImGui::IsMouseClicked(0))
            {
                *setting.m_pIntValue = i;
                anim.m_Expanded = false;
            }
        }

        d->PopClipRect();
        totalH += visH + 4;
    }

    if(isHovered && ImGui::IsMouseClicked(0))
        anim.m_Expanded = !anim.m_Expanded;

    if(anim.m_Expanded && ImGui::IsMouseClicked(0) && !isHovered)
    {
        float panelY = comboY + comboH + 2;
        float panelH = setting.m_ComboCount * 16.0f;
        if(!IsHover(io.MousePos.x, io.MousePos.y, comboX, panelY, comboW, panelH))
            anim.m_Expanded = false;
    }

    return totalH;
}

float CImGuiManager::RenderTextInput(SSettingBase &setting, float x, float y, float w, float alpha)
{
    if(!ImGui::GetCurrentContext()) return 0;

    char *pBuf = setting.m_pStringValue ? setting.m_pStringValue : setting.m_pTextBuffer;
    int bufSize = setting.m_pStringValue ? setting.m_StringMaxLen : setting.m_TextBufferSize;
    if(!pBuf || bufSize <= 0) return 0;
    if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || w <= 0) return 0;

    ImDrawList *d = ImGui::GetWindowDrawList();
    if(!d) return 0;
    ImGuiIO &io = ImGui::GetIO();
    float inputH = 22.0f;

    STextInputAnim &anim = m_TextInputAnims[setting.m_Id];

    bool isHovered = IsHover(io.MousePos.x, io.MousePos.y, x, y, w, inputH);
    anim.m_HoverAnim = Lerp(anim.m_HoverAnim, (isHovered || anim.m_Focused) ? 1.0f : 0.0f, m_DeltaTime * 10.0f);
    anim.m_FocusAnim = Lerp(anim.m_FocusAnim, anim.m_Focused ? 1.0f : 0.0f, m_DeltaTime * 10.0f);

    if(anim.m_Focused)
    {
        anim.m_CursorBlink += m_DeltaTime * 2.0f;
        if(anim.m_CursorBlink > 1.0f) anim.m_CursorBlink -= 1.0f;
    }

    ImVec4 inputBg = LerpColor(ImVec4(0.1f, 0.1f, 0.12f, alpha),
        ImVec4(0.15f, 0.15f, 0.18f, alpha), anim.m_HoverAnim);
    ImVec4 accentCol = GetAccentColor(y);
    ImVec4 borderCol = LerpColor(
        ImVec4(0.25f, 0.25f, 0.28f, 0.5f * alpha),
        ImVec4(accentCol.x, accentCol.y, accentCol.z, 0.8f * alpha),
        anim.m_FocusAnim);

    d->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + inputH),
        ImGui::ColorConvertFloat4ToU32(inputBg), 4.0f);
    d->AddRect(ImVec2(x, y), ImVec2(x + w, y + inputH),
        ImGui::ColorConvertFloat4ToU32(borderCol), 4.0f, 0, 0.5f + anim.m_FocusAnim * 0.5f);

    bool hasText = pBuf[0] != '\0';

    d->PushClipRect(ImVec2(x + 6, y), ImVec2(x + w - 6, y + inputH), true);
    if(anim.m_Focused)
    {
        if(hasText)
        {
            d->AddText(ImVec2(x + 8, y + (inputH - 14) / 2),
                ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, alpha)), pBuf);
        }
        else
        {
            d->AddText(ImVec2(x + 8, y + (inputH - 14) / 2),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.55f, 0.4f * alpha)), setting.m_Name.c_str());
        }
    }
    else
    {
        const char *displayText = hasText ? pBuf : setting.m_Name.c_str();
        float textAlpha = hasText ? alpha : 0.7f * alpha;
        ImVec4 textColor = hasText ? ImVec4(1, 1, 1, textAlpha) : ImVec4(0.5f, 0.5f, 0.55f, textAlpha);
        d->AddText(ImVec2(x + 8, y + (inputH - 14) / 2),
            ImGui::ColorConvertFloat4ToU32(textColor), displayText);
    }
    d->PopClipRect();

    if(anim.m_Focused)
    {
        float cursorAlpha = sinf(anim.m_CursorBlink * 3.14159f * 2.0f) * 0.5f + 0.5f;
        if(cursorAlpha > 0.3f)
        {
            ImVec2 textSize = ImGui::CalcTextSize(pBuf);
            float cursorX = x + 8 + std::min(textSize.x, w - 16);
            d->AddLine(ImVec2(cursorX, y + 4), ImVec2(cursorX, y + inputH - 4),
                ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, cursorAlpha * anim.m_FocusAnim * alpha)), 1.0f);
        }
    }

    if(isHovered && ImGui::IsMouseClicked(0))
    {
        for(auto &pair : m_TextInputAnims)
            pair.second.m_Focused = false;
        anim.m_Focused = true;
        SDL_StartTextInput();
    }

    if(anim.m_Focused && !isHovered && ImGui::IsMouseClicked(0))
        anim.m_Focused = false;

    return inputH;
}

float CImGuiManager::RenderTriggerList(SSettingBase &setting, float x, float y, float w, float alpha)
{
    if(!ImGui::GetCurrentContext() || !m_pAutoAnswer) return 0;
    ImDrawList *d = ImGui::GetWindowDrawList();
    if(!d) return 0;
    ImGuiIO &io = ImGui::GetIO();

    float totalH = 0.0f;
    float curY = y;
    float padX = 4.0f;

    d->AddText(ImVec2(x, curY),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.7f, 0.75f, 0.9f * alpha)), "Triggers");
    curY += 16.0f;
    totalH += 16.0f;

    auto &triggers = m_pAutoAnswer->GetTriggers();

    float itemH = 38.0f;
    float listMaxH = 120.0f;
    float listContentH = triggers.size() * (itemH + 2.0f);
    float listH = std::min(listMaxH, std::max(listContentH, 20.0f));

    d->AddRectFilled(ImVec2(x, curY), ImVec2(x + w, curY + listH),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.08f, 0.08f, 0.09f, 0.8f * alpha)), 4.0f);
    d->AddRect(ImVec2(x, curY), ImVec2(x + w, curY + listH),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.18f, 0.18f, 0.2f, 0.5f * alpha)), 4.0f);

    if(triggers.empty())
    {
        d->AddText(ImVec2(x + 8, curY + (listH - 12) / 2),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 0.4f, 0.45f, 0.6f * alpha)), "No triggers added");
    }
    else
    {
        d->PushClipRect(ImVec2(x + 1, curY + 1), ImVec2(x + w - 1, curY + listH - 1), true);

        m_TriggerListScrollCurrent = Lerp(m_TriggerListScrollCurrent, m_TriggerListScrollTarget, m_DeltaTime * 12.0f);

        for(int i = 0; i < (int)triggers.size(); i++)
        {
            float iy = curY + 2 + i * (itemH + 2.0f) - m_TriggerListScrollCurrent;
            if(iy + itemH < curY || iy > curY + listH)
                continue;

            const auto &t = triggers[i];

            bool isEditing = (m_TriggerEditIndex == i);

            ImVec4 bgClr = isEditing ?
                ImVec4(0.15f, 0.12f, 0.08f, 0.6f * alpha) :
                ImVec4(0.1f, 0.1f, 0.12f, 0.5f * alpha);
            d->AddRectFilled(ImVec2(x + 2, iy), ImVec2(x + w - 2, iy + itemH),
                ImGui::ColorConvertFloat4ToU32(bgClr), 3.0f);

            float textX = x + 6;

            d->PushClipRect(ImVec2(x + 2, iy), ImVec2(x + w - 40, iy + itemH), true);
            d->AddText(ImVec2(textX, iy + 3),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.9f, 0.7f, 0.4f, 0.9f * alpha)),
                t.m_Trigger.c_str());

            std::string arrow = " -> ";
            ImVec2 trigSize = ImGui::CalcTextSize(t.m_Trigger.c_str());
            d->AddText(ImVec2(textX + trigSize.x, iy + 3),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.5f, 0.7f * alpha)),
                arrow.c_str());

            ImVec2 arrowSize = ImGui::CalcTextSize(arrow.c_str());
            d->AddText(ImVec2(textX + trigSize.x + arrowSize.x, iy + 3),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.85f, 0.5f, 0.9f * alpha)),
                t.m_Response.c_str());
            d->PopClipRect();

            std::string flags;
            if(t.m_CaseSensitive) flags += "CS ";
            if(t.m_ExactMatch) flags += "EX";
            if(!flags.empty())
            {
                d->AddText(ImVec2(textX, iy + 20),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 0.4f, 0.5f, 0.5f * alpha)),
                    flags.c_str());
            }

            float btnSize = 14.0f;
            float delBtnX = x + w - btnSize - 6;
            float delBtnY = iy + (itemH - btnSize) / 2;

            bool delHov = IsHover(io.MousePos.x, io.MousePos.y, delBtnX, delBtnY, btnSize, btnSize);
            ImVec4 delClr = delHov ? ImVec4(1.0f, 0.3f, 0.3f, 0.9f * alpha) : ImVec4(0.6f, 0.3f, 0.3f, 0.6f * alpha);
            d->AddRectFilled(ImVec2(delBtnX, delBtnY), ImVec2(delBtnX + btnSize, delBtnY + btnSize),
                ImGui::ColorConvertFloat4ToU32(delClr), 3.0f);
            d->AddLine(ImVec2(delBtnX + 3, delBtnY + 3), ImVec2(delBtnX + btnSize - 3, delBtnY + btnSize - 3),
                ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.8f * alpha)), 1.5f);
            d->AddLine(ImVec2(delBtnX + btnSize - 3, delBtnY + 3), ImVec2(delBtnX + 3, delBtnY + btnSize - 3),
                ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.8f * alpha)), 1.5f);

            if(delHov && ImGui::IsMouseClicked(0))
            {
                m_pAutoAnswer->RemoveTrigger(i);
                if(m_TriggerEditIndex == i)
                    m_TriggerEditIndex = -1;
                else if(m_TriggerEditIndex > i)
                    m_TriggerEditIndex--;
                break;
            }

            float editBtnX = delBtnX - btnSize - 4;
            bool editHov = IsHover(io.MousePos.x, io.MousePos.y, editBtnX, delBtnY, btnSize, btnSize);
            ImVec4 editClr = editHov ? ImVec4(0.3f, 0.5f, 0.9f, 0.9f * alpha) : ImVec4(0.3f, 0.3f, 0.6f, 0.6f * alpha);
            d->AddRectFilled(ImVec2(editBtnX, delBtnY), ImVec2(editBtnX + btnSize, delBtnY + btnSize),
                ImGui::ColorConvertFloat4ToU32(editClr), 3.0f);
            d->AddText(ImVec2(editBtnX + 3, delBtnY + 1),
                ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.8f * alpha)), "E");

            if(editHov && ImGui::IsMouseClicked(0))
            {
                m_TriggerEditIndex = i;
                str_copy(m_aTriggerEditTrigger, t.m_Trigger.c_str(), sizeof(m_aTriggerEditTrigger));
                str_copy(m_aTriggerEditResponse, t.m_Response.c_str(), sizeof(m_aTriggerEditResponse));
                m_TriggerEditCaseSensitive = t.m_CaseSensitive ? 1 : 0;
                m_TriggerEditExact = t.m_ExactMatch ? 1 : 0;
            }
        }

        d->PopClipRect();

        if(IsHover(io.MousePos.x, io.MousePos.y, x, curY, w, listH))
        {
            float maxScroll = std::max(0.0f, listContentH - listH);
            m_TriggerListScrollTarget -= io.MouseWheel * 20.0f;
            m_TriggerListScrollTarget = std::clamp(m_TriggerListScrollTarget, 0.0f, maxScroll);
        }
    }

    curY += listH + 4.0f;
    totalH += listH + 4.0f;

    d->AddLine(ImVec2(x, curY), ImVec2(x + w, curY),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.15f, 0.15f, 0.18f, 0.4f * alpha)));
    curY += 4.0f;
    totalH += 4.0f;

    bool isEditMode = (m_TriggerEditIndex >= 0 && m_TriggerEditIndex < (int)triggers.size());

    d->AddText(ImVec2(x, curY),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 0.6f, 0.65f, 0.8f * alpha)),
        isEditMode ? "Edit Trigger" : "Add Trigger");
    curY += 14.0f;
    totalH += 14.0f;

    {
        SSettingBase triggerInput;
        triggerInput.m_Type = SSettingBase::TEXT_INPUT;
        triggerInput.m_Name = "Trigger text";
        triggerInput.m_Id = "aa_trig_input";
        triggerInput.m_pStringValue = m_aAutoAnswerTrigger;
        triggerInput.m_StringMaxLen = sizeof(m_aAutoAnswerTrigger);
        float h = RenderTextInput(triggerInput, x, curY, w, alpha);
        curY += h + 2.0f;
        totalH += h + 2.0f;
    }

    {
        SSettingBase responseInput;
        responseInput.m_Type = SSettingBase::TEXT_INPUT;
        responseInput.m_Name = "Response text";
        responseInput.m_Id = "aa_resp_input";
        responseInput.m_pStringValue = m_aAutoAnswerResponse;
        responseInput.m_StringMaxLen = sizeof(m_aAutoAnswerResponse);
        float h = RenderTextInput(responseInput, x, curY, w, alpha);
        curY += h + 2.0f;
        totalH += h + 2.0f;
    }

    {
        SSettingBase csToggle;
        csToggle.m_Type = SSettingBase::TOGGLE;
        csToggle.m_Name = "Case Sensitive";
        csToggle.m_Id = "aa_trig_cs";
        csToggle.m_pIntValue = &m_TriggerEditCaseSensitive;
        float h = RenderCheckbox(csToggle, x, curY, w, alpha);
        curY += h + 2.0f;
        totalH += h + 2.0f;
    }

    {
        SSettingBase exToggle;
        exToggle.m_Type = SSettingBase::TOGGLE;
        exToggle.m_Name = "Exact Match";
        exToggle.m_Id = "aa_trig_ex";
        exToggle.m_pIntValue = &m_TriggerEditExact;
        float h = RenderCheckbox(exToggle, x, curY, w, alpha);
        curY += h + 2.0f;
        totalH += h + 2.0f;
    }

    float btnH = 20.0f;
    float btnW1 = isEditMode ? (w - 4) / 2 : w;

    {
        bool hasContent = (m_aAutoAnswerTrigger[0] != '\0' && m_aAutoAnswerResponse[0] != '\0');
        ImVec4 accent = GetAccentColor(curY);
        bool addHov = IsHover(io.MousePos.x, io.MousePos.y, x, curY, btnW1, btnH);
        float addHoverAnim = GetAnimValue("aa_addbtn", addHov ? 1.0f : 0.0f, 10.0f);

        ImVec4 btnBg = hasContent ?
            LerpColor(ImVec4(accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 0.8f * alpha),
                      ImVec4(accent.x * 0.5f, accent.y * 0.5f, accent.z * 0.5f, 0.9f * alpha), addHoverAnim) :
            ImVec4(0.12f, 0.12f, 0.14f, 0.5f * alpha);

        d->AddRectFilled(ImVec2(x, curY), ImVec2(x + btnW1, curY + btnH),
            ImGui::ColorConvertFloat4ToU32(btnBg), 4.0f);
        d->AddRect(ImVec2(x, curY), ImVec2(x + btnW1, curY + btnH),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.33f, 0.4f * alpha)), 4.0f);

        const char *btnText = isEditMode ? "Save" : "Add";
        ImVec2 textSize = ImGui::CalcTextSize(btnText);
        ImVec4 textClr = hasContent ? ImVec4(0.9f, 0.9f, 0.93f, alpha) : ImVec4(0.5f, 0.5f, 0.5f, 0.5f * alpha);
        d->AddText(ImVec2(x + (btnW1 - textSize.x) / 2, curY + (btnH - textSize.y) / 2),
            ImGui::ColorConvertFloat4ToU32(textClr), btnText);

        if(addHov && ImGui::IsMouseClicked(0) && hasContent)
        {
            if(isEditMode)
            {
                m_pAutoAnswer->UpdateTrigger(m_TriggerEditIndex,
                    m_aAutoAnswerTrigger, m_aAutoAnswerResponse,
                    m_TriggerEditCaseSensitive != 0, m_TriggerEditExact != 0);
                m_TriggerEditIndex = -1;
            }
            else
            {
                m_pAutoAnswer->AddTrigger(m_aAutoAnswerTrigger, m_aAutoAnswerResponse,
                    m_TriggerEditCaseSensitive != 0, m_TriggerEditExact != 0);
            }
            m_aAutoAnswerTrigger[0] = '\0';
            m_aAutoAnswerResponse[0] = '\0';
            m_TriggerEditCaseSensitive = 0;
            m_TriggerEditExact = 0;
        }
    }

    if(isEditMode)
    {
        float cancelX = x + btnW1 + 4;
        float cancelW = w - btnW1 - 4;
        bool cancelHov = IsHover(io.MousePos.x, io.MousePos.y, cancelX, curY, cancelW, btnH);
        float cancelAnim = GetAnimValue("aa_cancelbtn", cancelHov ? 1.0f : 0.0f, 10.0f);

        ImVec4 cancelBg = LerpColor(ImVec4(0.3f, 0.12f, 0.12f, 0.6f * alpha),
            ImVec4(0.5f, 0.15f, 0.15f, 0.8f * alpha), cancelAnim);
        d->AddRectFilled(ImVec2(cancelX, curY), ImVec2(cancelX + cancelW, curY + btnH),
            ImGui::ColorConvertFloat4ToU32(cancelBg), 4.0f);
        d->AddRect(ImVec2(cancelX, curY), ImVec2(cancelX + cancelW, curY + btnH),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.33f, 0.4f * alpha)), 4.0f);

        const char *cancelText = "Cancel";
        ImVec2 cancelSize = ImGui::CalcTextSize(cancelText);
        d->AddText(ImVec2(cancelX + (cancelW - cancelSize.x) / 2, curY + (btnH - cancelSize.y) / 2),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.9f, 0.6f, 0.6f, alpha)), cancelText);

        if(cancelHov && ImGui::IsMouseClicked(0))
        {
            m_TriggerEditIndex = -1;
            m_aAutoAnswerTrigger[0] = '\0';
            m_aAutoAnswerResponse[0] = '\0';
            m_TriggerEditCaseSensitive = 0;
            m_TriggerEditExact = 0;
        }
    }

    curY += btnH + 2.0f;
    totalH += btnH + 2.0f;

    if(!triggers.empty())
    {
        bool clearHov = IsHover(io.MousePos.x, io.MousePos.y, x, curY, w, btnH);
        float clearAnim = GetAnimValue("aa_clearbtn", clearHov ? 1.0f : 0.0f, 10.0f);

        ImVec4 clearBg = LerpColor(ImVec4(0.2f, 0.08f, 0.08f, 0.5f * alpha),
            ImVec4(0.4f, 0.1f, 0.1f, 0.7f * alpha), clearAnim);
        d->AddRectFilled(ImVec2(x, curY), ImVec2(x + w, curY + btnH),
            ImGui::ColorConvertFloat4ToU32(clearBg), 4.0f);

        const char *clearText = "Clear All";
        ImVec2 clearSize = ImGui::CalcTextSize(clearText);
        d->AddText(ImVec2(x + (w - clearSize.x) / 2, curY + (btnH - clearSize.y) / 2),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.8f, 0.4f, 0.4f, 0.8f * alpha)), clearText);

        if(clearHov && ImGui::IsMouseClicked(0))
        {
            m_pAutoAnswer->ClearTriggers();
            m_TriggerEditIndex = -1;
        }

        curY += btnH + 2.0f;
        totalH += btnH + 2.0f;
    }

    return totalH;
}

float CImGuiManager::RenderButton(SSettingBase &setting, float x, float y, float w, float alpha)
{
    if(!ImGui::GetCurrentContext()) return 0;
    if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || w <= 0) return 0;
    ImDrawList *d = ImGui::GetWindowDrawList();
    if(!d) return 0;
    ImGuiIO &io = ImGui::GetIO();
    float btnH = 22.0f;

    SButtonAnim &anim = m_ButtonAnims[setting.m_Id];

    bool isHovered = IsHover(io.MousePos.x, io.MousePos.y, x, y, w, btnH);
    anim.m_HoverAnim = Lerp(anim.m_HoverAnim, isHovered ? 1.0f : 0.0f, m_DeltaTime * 10.0f);
    anim.m_PressAnim = Lerp(anim.m_PressAnim, anim.m_WasPressed ? 1.0f : 0.0f, m_DeltaTime * 15.0f);

    if(anim.m_PressAnim < 0.05f && anim.m_WasPressed)
        anim.m_WasPressed = false;

    float scaleTarget = anim.m_WasPressed ? 0.95f : (isHovered ? 1.02f : 1.0f);
    anim.m_ScaleAnim = Lerp(anim.m_ScaleAnim, scaleTarget, m_DeltaTime * 15.0f);

    float pressOffset = anim.m_PressAnim * 1.0f;

    ImVec4 btnBg = LerpColor(ImVec4(0.15f, 0.15f, 0.18f, 1.0f),
        GetAccentColor(y), anim.m_HoverAnim * 0.4f);

    d->AddRectFilled(ImVec2(x, y + pressOffset), ImVec2(x + w, y + btnH + pressOffset),
        ImGui::ColorConvertFloat4ToU32(btnBg), 4.0f);

    int outlineAlpha = (int)(60 + anim.m_HoverAnim * 60 + anim.m_PressAnim * 40);
    d->AddRect(ImVec2(x, y + pressOffset), ImVec2(x + w, y + btnH + pressOffset),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.33f, outlineAlpha / 255.0f)), 4.0f, 0, 0.5f);

    const char *btnText = setting.m_ButtonName.empty() ? setting.m_Name.c_str() : setting.m_ButtonName.c_str();
    ImVec2 textSize = ImGui::CalcTextSize(btnText);
    d->AddText(ImVec2(x + (w - textSize.x) / 2, y + (btnH - textSize.y) / 2 + pressOffset),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.85f, 0.85f, 0.9f, 1.0f)), btnText);

    if(isHovered && ImGui::IsMouseClicked(0))
    {
        anim.m_WasPressed = true;
        anim.m_PressAnim = 1.0f;
        if(setting.m_Callback)
            setting.m_Callback();
    }

    return btnH;
}

float CImGuiManager::RenderColorPicker(SSettingBase &setting, float x, float y, float w, float alpha)
{
    if(!setting.m_pColorHue || !setting.m_pColorSaturation || !setting.m_pColorBrightness || !setting.m_pColorAlpha) return 0;
    if(!ImGui::GetCurrentContext()) return 0;
    if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || w <= 0) return 0;

    ImDrawList *d = ImGui::GetWindowDrawList();
    if(!d) return 0;
    ImGuiIO &io = ImGui::GetIO();
    float previewH = SETTING_ITEM_H;

    SColorPickerAnim &anim = m_ColorPickerAnims[setting.m_Id];

    anim.m_DisplayHue = LerpHue(anim.m_DisplayHue, *setting.m_pColorHue, m_DeltaTime * 6.0f);
    anim.m_DisplaySat = Lerp(anim.m_DisplaySat, *setting.m_pColorSaturation, m_DeltaTime * 6.0f);
    anim.m_DisplayBri = Lerp(anim.m_DisplayBri, *setting.m_pColorBrightness, m_DeltaTime * 6.0f);
    anim.m_DisplayAlpha = Lerp(anim.m_DisplayAlpha, *setting.m_pColorAlpha, m_DeltaTime * 6.0f);

    anim.m_ExpandAnim = Lerp(anim.m_ExpandAnim, anim.m_Expanded ? 1.0f : 0.0f, m_DeltaTime * 10.0f);
    anim.m_ContentAlpha = Lerp(anim.m_ContentAlpha, anim.m_Expanded ? 1.0f : 0.0f, m_DeltaTime * 15.0f);

    bool previewHov = IsHover(io.MousePos.x, io.MousePos.y, x + w - 14, y, 14, previewH);
    anim.m_PreviewHoverAnim = Lerp(anim.m_PreviewHoverAnim, previewHov ? 1.0f : 0.0f, m_DeltaTime * 10.0f);

    d->AddText(ImVec2(x, y + (previewH - 13) / 2),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.7f, 0.75f, 1.0f)), setting.m_Name.c_str());

    ImVec4 displayCol = HSBtoRGB(anim.m_DisplayHue, anim.m_DisplaySat, anim.m_DisplayBri);
    displayCol.w = anim.m_DisplayAlpha;
    float pvX = x + w - 12;
    float pvY = y + previewH / 4;
    d->AddRectFilled(ImVec2(pvX, pvY), ImVec2(pvX + 10, pvY + 10),
        ImGui::ColorConvertFloat4ToU32(displayCol), 10.0f);
    d->AddRect(ImVec2(pvX - 0.5f, pvY - 0.5f), ImVec2(pvX + 10.5f, pvY + 10.5f),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.5f, 0.6f + anim.m_PreviewHoverAnim * 0.3f)), 10.0f, 0, 1.0f);

    if(previewHov && ImGui::IsMouseClicked(0))
    {
        anim.m_Expanded = !anim.m_Expanded;
        if(!anim.m_Expanded)
        {
            anim.m_DraggingPalette = false;
            anim.m_DraggingHue = false;
            anim.m_DraggingAlpha = false;
            anim.m_HexActive = false;
        }
    }

    float totalH = previewH;

    if(anim.m_ExpandAnim > 0.01f)
    {
        float pickerSpacing = 4.0f;
        float paletteSize = 70.0f;
        float sliderW = 8.0f;
        float hexH = 16.0f;
        float expandedH = (paletteSize + pickerSpacing + hexH + pickerSpacing * 2) * anim.m_ExpandAnim;

        float pY = y + previewH + pickerSpacing;
        float contentX = x + pickerSpacing;
        float contentW = w - pickerSpacing * 2;
        float slidersW = sliderW * 2 + pickerSpacing;
        float palW = contentW - slidersW - pickerSpacing;

        if(anim.m_ExpandAnim > 0.3f && anim.m_ContentAlpha > 0.01f)
        {
            if(anim.m_DraggingPalette)
            {
                float sat = std::clamp((io.MousePos.x - (contentX + slidersW + pickerSpacing)) / palW, 0.0f, 1.0f);
                float bri = 1.0f - std::clamp((io.MousePos.y - (pY + pickerSpacing)) / paletteSize, 0.0f, 1.0f);
                *setting.m_pColorSaturation = sat;
                *setting.m_pColorBrightness = bri;
            }
            if(anim.m_DraggingHue)
            {
                float hue = std::clamp((io.MousePos.y - (pY + pickerSpacing)) / paletteSize, 0.0f, 1.0f);
                *setting.m_pColorHue = hue;
            }
            if(anim.m_DraggingAlpha)
            {
                float alpha = std::clamp((io.MousePos.y - (pY + pickerSpacing)) / paletteSize, 0.0f, 1.0f);
                *setting.m_pColorAlpha = alpha;
            }

            RenderHueSlider(contentX, pY + pickerSpacing, sliderW, paletteSize, anim, setting);
            RenderAlphaSlider(contentX + sliderW + pickerSpacing, pY + pickerSpacing, sliderW, paletteSize, anim, setting);
            RenderSatBriPalette(contentX + slidersW + pickerSpacing, pY + pickerSpacing, palW, paletteSize, anim, setting);

            float hexY = pY + pickerSpacing + paletteSize + pickerSpacing;
            RenderHexInput(contentX, hexY, contentW, hexH, anim, setting);

            if(!ImGui::IsMouseDown(0))
            {
                anim.m_DraggingPalette = false;
                anim.m_DraggingHue = false;
                anim.m_DraggingAlpha = false;
            }
        }

        totalH += expandedH;
    }

    return totalH;
}

void CImGuiManager::RenderSatBriPalette(float x, float y, float w, float h, SColorPickerAnim &anim, SSettingBase &setting)
{
    ImDrawList *d = ImGui::GetWindowDrawList();
    if(!d) return;
    ImGuiIO &io = ImGui::GetIO();
    float ca = anim.m_ContentAlpha;

    ImVec4 pureCol = HSBtoRGB(anim.m_DisplayHue, 1.0f, 1.0f);

    ImU32 white = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, ca));
    ImU32 pure = ImGui::ColorConvertFloat4ToU32(ImVec4(pureCol.x, pureCol.y, pureCol.z, ca));
    d->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + w, y + h), white, pure, pure, white);

    ImU32 black0 = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0));
    ImU32 black1 = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, ca));
    d->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + w, y + h), black0, black0, black1, black1);

    float handleX = x + anim.m_DisplaySat * w;
    float handleY = y + (1.0f - anim.m_DisplayBri) * h;
    float handleSize = 6.0f;
    d->AddCircleFilled(ImVec2(handleX, handleY), handleSize / 2 + 1,
        ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, ca)));
    ImVec4 hCol = HSBtoRGB(anim.m_DisplayHue, anim.m_DisplaySat, anim.m_DisplayBri);
    hCol.w = ca;
    d->AddCircleFilled(ImVec2(handleX, handleY), handleSize / 2 - 0.5f,
        ImGui::ColorConvertFloat4ToU32(hCol));

    if(IsHover(io.MousePos.x, io.MousePos.y, x, y, w, h) && ImGui::IsMouseClicked(0))
        anim.m_DraggingPalette = true;
}

void CImGuiManager::RenderHueSlider(float x, float y, float w, float h, SColorPickerAnim &anim, SSettingBase &setting)
{
    ImDrawList *d = ImGui::GetWindowDrawList();
    if(!d) return;
    ImGuiIO &io = ImGui::GetIO();
    float ca = anim.m_ContentAlpha;

    float segH = h / 6.0f;
    for(int i = 0; i < 6; i++)
    {
        ImVec4 c1 = HSBtoRGB(i / 6.0f, 1.0f, 1.0f); c1.w = ca;
        ImVec4 c2 = HSBtoRGB((i + 1) / 6.0f, 1.0f, 1.0f); c2.w = ca;
        ImU32 top = ImGui::ColorConvertFloat4ToU32(c1);
        ImU32 bot = ImGui::ColorConvertFloat4ToU32(c2);
        float rTL = (i == 0) ? 2.0f : 0.0f;
        float rBL = (i == 5) ? 2.0f : 0.0f;
        d->AddRectFilledMultiColor(ImVec2(x, y + i * segH), ImVec2(x + w, y + (i + 1) * segH), top, top, bot, bot);
    }

    float handleY = y + anim.m_DisplayHue * h;
    float handleH = 3.0f;
    d->AddRectFilled(ImVec2(x - 1, handleY - handleH / 2), ImVec2(x + w + 1, handleY + handleH / 2),
        ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, ca)), 1.5f);

    if(IsHover(io.MousePos.x, io.MousePos.y, x - 2, y, w + 4, h) && ImGui::IsMouseClicked(0))
        anim.m_DraggingHue = true;
}

void CImGuiManager::RenderAlphaSlider(float x, float y, float w, float h, SColorPickerAnim &anim, SSettingBase &setting)
{
    ImDrawList *d = ImGui::GetWindowDrawList();
    if(!d) return;
    ImGuiIO &io = ImGui::GetIO();
    float ca = anim.m_ContentAlpha;

    d->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.7f, 0.7f, ca * 0.6f)), 2.0f);

    ImVec4 baseCol = HSBtoRGB(anim.m_DisplayHue, anim.m_DisplaySat, anim.m_DisplayBri);
    ImU32 transparent = ImGui::ColorConvertFloat4ToU32(ImVec4(baseCol.x, baseCol.y, baseCol.z, 0));
    ImU32 opaque = ImGui::ColorConvertFloat4ToU32(ImVec4(baseCol.x, baseCol.y, baseCol.z, ca));
    d->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + w, y + h), transparent, transparent, opaque, opaque);

    float handleY = y + anim.m_DisplayAlpha * h;
    float handleH = 3.0f;
    d->AddRectFilled(ImVec2(x - 1, handleY - handleH / 2), ImVec2(x + w + 1, handleY + handleH / 2),
        ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, ca)), 1.5f);

    if(IsHover(io.MousePos.x, io.MousePos.y, x - 2, y, w + 4, h) && ImGui::IsMouseClicked(0))
        anim.m_DraggingAlpha = true;
}

void CImGuiManager::RenderHexInput(float x, float y, float w, float h, SColorPickerAnim &anim, SSettingBase &setting)
{
    ImDrawList *d = ImGui::GetWindowDrawList();
    if(!d) return;
    ImGuiIO &io = ImGui::GetIO();
    float ca = anim.m_ContentAlpha;

    bool inputHov = IsHover(io.MousePos.x, io.MousePos.y, x, y, w, h);
    anim.m_HexInputAnim = Lerp(anim.m_HexInputAnim, anim.m_HexActive ? 1.0f : 0.0f, m_DeltaTime * 15.0f);

    int bgAlpha = (int)((40 + anim.m_HexInputAnim * 20 + (inputHov ? 10 : 0)) * ca);
    d->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.14f, 0.14f, 0.16f, bgAlpha / 255.0f)), 3.0f);

    ImVec4 borderCol = anim.m_HexActive ?
        ImVec4(0.4f, 0.55f, 0.7f, ca * 0.8f) : ImVec4(0.3f, 0.3f, 0.33f, ca * 0.4f);
    d->AddRect(ImVec2(x, y), ImVec2(x + w, y + h),
        ImGui::ColorConvertFloat4ToU32(borderCol), 3.0f, 0, 0.5f);

    ImVec4 displayCol = HSBtoRGB(anim.m_DisplayHue, anim.m_DisplaySat, anim.m_DisplayBri);
    int r = (int)(displayCol.x * 255); int g = (int)(displayCol.y * 255); int b = (int)(displayCol.z * 255);
    int a = (int)(anim.m_DisplayAlpha * 255);
    char hexStr[16];
    snprintf(hexStr, sizeof(hexStr), "#%02X%02X%02X%02X", r, g, b, a);

    const char *displayText = anim.m_HexActive ? anim.m_HexText.c_str() : hexStr;
    if(anim.m_HexActive)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "#%s", anim.m_HexText.c_str());
        displayText = buf;
    }

    d->AddText(ImVec2(x + 6, y + (h - 12) / 2),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.8f, 0.8f, 0.85f, ca)), anim.m_HexActive ? "" : hexStr);

    if(anim.m_HexActive)
    {
        char buf2[16];
        snprintf(buf2, sizeof(buf2), "#%s", anim.m_HexText.c_str());
        d->AddText(ImVec2(x + 6, y + (h - 12) / 2),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.9f, 0.9f, 0.95f, ca)), buf2);
    }

    float pvSize = h - 4;
    float pvX = x + w - pvSize - 3;
    float pvY2 = y + 2;
    displayCol.w = anim.m_DisplayAlpha;
    d->AddRectFilled(ImVec2(pvX, pvY2), ImVec2(pvX + pvSize, pvY2 + pvSize),
        ImGui::ColorConvertFloat4ToU32(displayCol), 3.0f);

    if(inputHov && ImGui::IsMouseClicked(0) && !anim.m_HexActive)
    {
        anim.m_HexActive = true;
        snprintf(hexStr, sizeof(hexStr), "%02X%02X%02X%02X", r, g, b, a);
        anim.m_HexText = hexStr;
        anim.m_HexCursor = (int)anim.m_HexText.length();
    }
    else if(anim.m_HexActive && !inputHov && ImGui::IsMouseClicked(0))
    {
        anim.m_HexActive = false;
    }
}

std::string CImGuiManager::GetKeyName(int sdlKey)
{
    if(sdlKey == 0)
        return "None";
    const char *name = SDL_GetKeyName(SDL_GetKeyFromScancode((SDL_Scancode)sdlKey));
    if(name && name[0])
        return name;
    char buf[32];
    snprintf(buf, sizeof(buf), "Key %d", sdlKey);
    return buf;
}

void CImGuiManager::RenderKeybindsPanel(float x, float y, float w, float h, float alpha)
{
    ImDrawList *d = ImGui::GetWindowDrawList();
    if(!d) return;
    ImGuiIO &io = ImGui::GetIO();

    d->PushClipRect(ImVec2(x, y), ImVec2(x + w, y + h), true);

    float padX = 10.0f;
    float padY = 8.0f;
    float curY = y + padY - m_KeybindScrollCurrent;

    d->AddText(ImVec2(x + padX, curY),
        GetAccentU32(curY, 0.95f * alpha), "Keybinds");
    curY += 18.0f;

    d->AddText(ImVec2(x + padX, curY),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.5f, 0.55f, 0.7f * alpha)),
        "Click to bind, right-click to clear");
    curY += 16.0f;

    d->AddLine(ImVec2(x + padX, curY + 2), ImVec2(x + w - padX, curY + 2),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.15f, 0.15f, 0.18f, 0.5f * alpha)));
    curY += 8.0f;

    auto modules = CMenuRegistry::Instance().GetModules();

    for(auto &mod : modules)
    {
        if(!mod.m_pEnabled) continue;

        if(curY + 30.0f < y)
        {
            curY += 26.0f;
            continue;
        }
        if(curY > y + h + 30.0f)
            break;

        float entryH = RenderKeybindEntry(&mod, x + padX, curY, w - padX * 2, alpha);
        curY += entryH + 3.0f;
    }

    float totalContentH = curY - (y + padY - m_KeybindScrollCurrent) + 10.0f;

    d->PopClipRect();

    if(IsHover(io.MousePos.x, io.MousePos.y, x, y, w, h))
    {
        float maxScroll = std::max(0.0f, totalContentH - h);
        m_KeybindScrollTarget -= io.MouseWheel * 20.0f;
        m_KeybindScrollTarget = std::clamp(m_KeybindScrollTarget, 0.0f, maxScroll);
    }
}

float CImGuiManager::RenderKeybindEntry(SModuleEntry *pMod, float x, float y, float w, float alpha)
{
    if(!pMod) return 0;
    ImDrawList *d = ImGui::GetWindowDrawList();
    if(!d) return 0;
    ImGuiIO &io = ImGui::GetIO();

    float h = 22.0f;
    float btnW = 80.0f;
    float btnH = 16.0f;
    float btnX = x + w - btnW;
    float btnY = y + (h - btnH) / 2;

    std::string animKey = "kb_" + pMod->m_Id;
    float &hoverAnim = m_KeybindAnims[animKey];
    bool isHovered = IsHover(io.MousePos.x, io.MousePos.y, btnX, btnY, btnW, btnH);
    hoverAnim = Lerp(hoverAnim, isHovered ? 1.0f : 0.0f, m_DeltaTime * 12.0f);

    bool isBinding = (m_WaitingForKey && m_BindingModuleForKey == pMod->m_Id);
    bool isEnabled = pMod->m_pEnabled && *pMod->m_pEnabled;

    ImVec4 textClr = isEnabled ?
        ImVec4(0.9f, 0.9f, 0.93f, 0.9f * alpha) :
        ImVec4(0.6f, 0.6f, 0.65f, 0.7f * alpha);
    d->AddText(ImVec2(x, y + (h - 13) / 2),
        ImGui::ColorConvertFloat4ToU32(textClr), pMod->m_Name.c_str());

    if(isEnabled)
    {
        ImVec4 accent = GetAccentColor(y);
        d->AddRectFilled(ImVec2(x - 4, y + (h - 4) / 2),
            ImVec2(x - 1, y + (h + 4) / 2),
            ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.7f * alpha)), 1.5f);
    }

    ImVec4 btnBg;
    if(isBinding)
        btnBg = ImVec4(0.3f, 0.15f, 0.15f, 0.8f * alpha);
    else
    {
        ImVec4 accent = GetAccentColor(y);
        btnBg = LerpColor(
            ImVec4(0.12f, 0.12f, 0.15f, 0.9f * alpha),
            ImVec4(accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 0.9f * alpha),
            hoverAnim * 0.5f);
    }
    d->AddRectFilled(ImVec2(btnX, btnY), ImVec2(btnX + btnW, btnY + btnH),
        ImGui::ColorConvertFloat4ToU32(btnBg), 4.0f);

    float borderAlpha = (0.3f + hoverAnim * 0.3f) * alpha;
    if(isBinding) borderAlpha = 0.7f * alpha;
    ImVec4 borderClr = isBinding ?
        ImVec4(0.8f, 0.3f, 0.3f, borderAlpha) :
        ImVec4(0.4f, 0.4f, 0.45f, borderAlpha);
    d->AddRect(ImVec2(btnX, btnY), ImVec2(btnX + btnW, btnY + btnH),
        ImGui::ColorConvertFloat4ToU32(borderClr), 4.0f, 0, 0.5f);

    std::string keyText;
    if(isBinding)
    {
        float blink = sinf(m_Time * 4.0f) * 0.5f + 0.5f;
        keyText = "Press key...";
        ImVec4 bindTextClr(1.0f, 0.6f, 0.6f, (0.6f + blink * 0.4f) * alpha);
        ImVec2 textSize = ImGui::CalcTextSize(keyText.c_str());
        d->AddText(ImVec2(btnX + (btnW - textSize.x) / 2, btnY + (btnH - textSize.y) / 2),
            ImGui::ColorConvertFloat4ToU32(bindTextClr), keyText.c_str());
    }
    else
    {
        keyText = GetKeyName(pMod->m_Key);
        ImVec4 keyTextClr = (pMod->m_Key != 0) ?
            ImVec4(0.85f, 0.85f, 0.9f, 0.9f * alpha) :
            ImVec4(0.45f, 0.45f, 0.5f, 0.6f * alpha);
        ImVec2 textSize = ImGui::CalcTextSize(keyText.c_str());
        d->AddText(ImVec2(btnX + (btnW - textSize.x) / 2, btnY + (btnH - textSize.y) / 2),
            ImGui::ColorConvertFloat4ToU32(keyTextClr), keyText.c_str());
    }

    if(isHovered && ImGui::IsMouseClicked(0) && !isBinding)
    {
        m_WaitingForKey = true;
        m_BindingModuleForKey = pMod->m_Id;
    }

    if(isHovered && ImGui::IsMouseClicked(1))
    {
        pMod->m_Key = 0;
        if(isBinding)
        {
            m_WaitingForKey = false;
            m_BindingModuleForKey.clear();
        }
    }

    return h;
}

float CImGuiManager::RenderMultiSelect(SSettingBase &setting, float x, float y, float w, float alpha)
{
    if(!setting.m_pMultiSelected || setting.m_MultiSelectOptions.empty() || !ImGui::GetCurrentContext()) return 0;
    if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || w <= 0) return 0;

    ImDrawList *d = ImGui::GetWindowDrawList();
    if(!d) return 0;
    ImGuiIO &io = ImGui::GetIO();
    float h = SETTING_ITEM_H;

    float boxW = 65.0f, boxH = 10.0f;
    float boxX = x + w - boxW - 2;
    float boxY = y + h / 2 - boxH / 2;

    SMultiSelectAnim &anim = m_MultiSelectAnims[setting.m_Id];
    bool mainHov = IsHover(io.MousePos.x, io.MousePos.y, boxX, boxY, boxW, boxH);
    anim.m_HoverAnim = Lerp(anim.m_HoverAnim, mainHov ? 1.0f : 0.0f, m_DeltaTime * 10.0f);
    anim.m_ExpandAnim = Lerp(anim.m_ExpandAnim, anim.m_Expanded ? 1.0f : 0.0f,
        m_DeltaTime * (anim.m_Expanded ? 10.0f : 15.0f));

    d->AddText(ImVec2(x, y + (h - 13) / 2),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.7f, 0.75f, 1.0f)), setting.m_Name.c_str());

    int bgAlpha = 25 + (int)(anim.m_HoverAnim * 15);
    d->AddRectFilled(ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.22f, 0.22f, 0.22f, bgAlpha / 255.0f)), 3.0f);
    d->AddRect(ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 0.6f, 0.6f, (60 + (int)(anim.m_HoverAnim * 40)) / 255.0f)), 3.0f, 0, 0.5f);

    if(setting.m_pMultiSelected->empty())
    {
        d->AddText(ImVec2(boxX + 4, boxY + (boxH - 12) / 2),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 0.6f, 0.65f, 0.8f)), "None");
    }
    else
    {
        std::string selectedStr;
        for(auto &opt : setting.m_MultiSelectOptions)
        {
            if(setting.m_pMultiSelected->count(opt))
            {
                if(!selectedStr.empty()) selectedStr += ", ";
                selectedStr += opt;
            }
        }
        d->PushClipRect(ImVec2(boxX + 2, boxY), ImVec2(boxX + boxW - 2, boxY + boxH), true);
        d->AddText(ImVec2(boxX + 4, boxY + (boxH - 12) / 2),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 0.6f, 0.65f, 0.8f)), selectedStr.c_str());
        d->PopClipRect();
    }

    float totalH = h;

    if(anim.m_ExpandAnim > 0.01f)
    {
        float optH = 14.0f;
        float panelH = setting.m_MultiSelectOptions.size() * optH;
        float panelY = boxY + boxH + 2;
        float visH = panelH * anim.m_ExpandAnim;

        d->AddRectFilled(ImVec2(boxX, panelY), ImVec2(boxX + boxW, panelY + visH),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.12f, 0.12f, 0.12f, 0.95f)), 3.0f);
        d->AddRect(ImVec2(boxX, panelY), ImVec2(boxX + boxW, panelY + visH),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.33f, 0.4f)), 3.0f);

        d->PushClipRect(ImVec2(boxX, panelY), ImVec2(boxX + boxW, panelY + visH), true);

        float oy = panelY;
        for(size_t i = 0; i < setting.m_MultiSelectOptions.size(); i++)
        {
            const std::string &opt = setting.m_MultiSelectOptions[i];
            bool optHov = IsHover(io.MousePos.x, io.MousePos.y, boxX, oy, boxW, optH) && anim.m_ExpandAnim > 0.8f;
            bool isSelected = setting.m_pMultiSelected->count(opt) > 0;

            std::string cKey = setting.m_Id + "_msc_" + opt;
            float &checkAnim = anim.m_CheckAnims[cKey];
            checkAnim = Lerp(checkAnim, isSelected ? 1.0f : 0.0f, m_DeltaTime * 10.0f);

            float &optHoverAnim = anim.m_OptionHovers[cKey];
            optHoverAnim = Lerp(optHoverAnim, optHov ? 1.0f : 0.0f, m_DeltaTime * 12.0f);

            if(optHoverAnim > 0.01f)
            {
                d->AddRectFilled(ImVec2(boxX + 2, oy), ImVec2(boxX + boxW - 2, oy + optH),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.35f, 0.35f, 0.38f, optHoverAnim * 0.12f)), 2.0f);
            }

            float ckSz = 6.0f;
            float ckX = boxX + 5;
            float ckY = oy + optH / 2 - ckSz / 2;
            d->AddRectFilled(ImVec2(ckX, ckY), ImVec2(ckX + ckSz, ckY + ckSz),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.22f, 0.22f, 0.24f, 0.7f)), 2.0f);

            if(checkAnim > 0.01f)
            {
                float innerSz = (ckSz - 2) * checkAnim;
                float innerX = ckX + (ckSz - innerSz) / 2;
                float innerY = ckY + (ckSz - innerSz) / 2;
                ImVec4 accent = GetAccentColor(oy);
                d->AddRectFilled(ImVec2(innerX, innerY), ImVec2(innerX + innerSz, innerY + innerSz),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, checkAnim * 0.85f)), 1.5f);
            }

            int textGray = (int)(140 + checkAnim * 40 + optHoverAnim * 20);
            d->AddText(ImVec2(ckX + ckSz + 4, oy + optH / 2 - 5),
                ImGui::ColorConvertFloat4ToU32(ImVec4(textGray / 255.0f, textGray / 255.0f, (textGray + 5) / 255.0f, 0.9f)),
                opt.c_str());

            if(optHov && ImGui::IsMouseClicked(0))
            {
                if(isSelected)
                    setting.m_pMultiSelected->erase(opt);
                else
                    setting.m_pMultiSelected->insert(opt);
            }

            oy += optH;
        }

        d->PopClipRect();
        totalH += visH + 4;
    }

    if(mainHov && ImGui::IsMouseClicked(0))
        anim.m_Expanded = !anim.m_Expanded;

    if(anim.m_Expanded && ImGui::IsMouseClicked(0) && !mainHov)
    {
        float panelY = boxY + boxH + 2;
        float panelH = setting.m_MultiSelectOptions.size() * 14.0f;
        if(!IsHover(io.MousePos.x, io.MousePos.y, boxX, panelY, boxW, panelH))
            anim.m_Expanded = false;
    }

    return totalH;
}

bool CImGuiManager::ProcessEvent(SDL_Event *event)
{
    if(!m_Initialized || !event) return false;
    if(!ImGui::GetCurrentContext()) return false;
    m_TabContentAnim = 0.0f;
    try
    {
        ImGui_ImplSDL2_ProcessEvent(event);
    }
    catch(...)
    {
        return false;
    }

    if(m_ShowStartMenu)
    {
        if(event->type == SDL_KEYDOWN || event->type == SDL_MOUSEBUTTONDOWN)
        {
            if(!m_StartMenuUnlocked)
            {
                m_StartMenuUnlocked = true;
                m_StartMenuUnlockTime = SDL_GetTicks();
                m_StartMenuTargetZoom = 1.0f;
                return true;
            }
        }

        if(event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
        {
            if(m_StartMenuUnlocked && m_StartMenuMenuProgress > 0.8f && m_StartMenuHoveredButton >= 0)
            {
                m_StartMenuClickedButton = m_StartMenuHoveredButton;
                m_ShowStartMenu = false;
                SDL_SetRelativeMouseMode(SDL_TRUE);
                SDL_ShowCursor(SDL_DISABLE);
                return true;
            }
        }

        return true;
    }
    if(CShalawaHud::Instance().GetEditor().IsActive())
    {
        if(CShalawaHud::Instance().ProcessEvent(event))
            return true;
        if(event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP ||
           event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEWHEEL)
            return true;
    }

    if(event->type == SDL_KEYDOWN && !m_MenuOpen && !m_ShowStartMenu)
    {
        int scancode = event->key.keysym.scancode;
        auto& modulesRef = CMenuRegistry::Instance().GetModules();
        for(auto &mod : modulesRef)
        {
            if(mod.m_Key != 0 && mod.m_Key == scancode && mod.m_pEnabled)
            {
                *mod.m_pEnabled = (*mod.m_pEnabled) ? 0 : 1;
            }
        }
    }

    if(event->type == SDL_KEYDOWN)
    {
        if(event->key.keysym.scancode == SDL_SCANCODE_DELETE && g_Config.m_shalawaHideCheat)
        {
            g_Config.m_shalawaHideCheat = 0;
            m_MenuOpen = true;
            SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_ShowCursor(SDL_ENABLE);
            return true;
        }

        if(event->key.keysym.scancode == SDL_SCANCODE_INSERT)
        {
            m_MenuOpen = !m_MenuOpen;
            if(m_MenuOpen)
            {
                g_Config.m_shalawaHideCheat = 0;
                SDL_SetRelativeMouseMode(SDL_FALSE);
                SDL_ShowCursor(SDL_ENABLE);
            }
            else
            {
                if(g_Config.m_shalawaHudEditor)
                {
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    SDL_ShowCursor(SDL_ENABLE);
                }
                else
                {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    SDL_ShowCursor(SDL_DISABLE);
                }
            }
            return true;
        }
    }
    if(event->type == SDL_KEYDOWN && m_WaitingForKey && m_MenuOpen && event->key.keysym.scancode != SDL_SCANCODE_INSERT)
    {
        if(event->key.keysym.scancode == SDL_SCANCODE_ESCAPE)
        {
            m_WaitingForKey = false;
            m_BindingModuleForKey.clear();
        }
        else
        {
            SModuleEntry *pBindMod = CMenuRegistry::Instance().FindModule(m_BindingModuleForKey);
            if(pBindMod)
            {
                int scancode = event->key.keysym.scancode;
                bool alreadyUsed = false;
                for(auto &mod : CMenuRegistry::Instance().GetModules())
                {
                    if(mod.m_Id != m_BindingModuleForKey && mod.m_Key == scancode)
                    {
                        mod.m_Key = 0;
                    }
                }
                pBindMod->m_Key = scancode;
            }
            m_WaitingForKey = false;
            m_BindingModuleForKey.clear();
        }
        return true;
    }
    if(event->type == SDL_KEYDOWN && m_MenuOpen)
    {
        SDL_Keycode key = event->key.keysym.sym;
        bool anyInputActive = m_SearchActive;
        for(auto &p : m_SliderAnims) if(p.second.m_InputMode) anyInputActive = true;
        for(auto &p : m_TextInputAnims) if(p.second.m_Focused) anyInputActive = true;
        for(auto &p : m_ColorPickerAnims) if(p.second.m_HexActive) anyInputActive = true;

        if(m_SearchActive && key == SDLK_ESCAPE)
        {
            m_SearchActive = false;
            m_SearchFocused = false;
            m_SearchBuffer[0] = '\0';
            m_SearchCursorPos = 0;
            m_SearchResults.clear();
            m_SelectedSearchModuleId.clear();
            return true;
        }

        if(!anyInputActive)
        {
            if(key == SDLK_f && (event->key.keysym.mod & KMOD_CTRL))
            {
                m_SearchActive = true;
                m_SearchFocused = true;
                m_SearchBuffer[0] = '\0';
                m_SearchCursorPos = 0;
                m_SearchResults.clear();
                m_SelectedSearchModuleId.clear();
                for(auto &p : m_SliderAnims) p.second.m_InputMode = false;
                for(auto &p : m_TextInputAnims) p.second.m_Focused = false;
                for(auto &p : m_ColorPickerAnims) { p.second.m_HexActive = false; p.second.m_DraggingPalette = false; p.second.m_DraggingHue = false; p.second.m_DraggingAlpha = false; }
                SDL_StartTextInput();
                return true;
            }
            if(key == SDLK_TAB)
            {
                m_PreviousCategory = m_SelectedCategory;
                m_HeaderTransition = 0.0f;
                bool shift = (event->key.keysym.mod & KMOD_SHIFT) != 0;
                int next = ((int)m_SelectedCategory + (shift ? MCAT_COUNT - 1 : 1)) % MCAT_COUNT;
                m_SelectedCategory = (EMenuCategory)next;
                m_SelectedModuleId.clear();
                m_ModuleScrollTarget = 0;
                m_SettingScrollTarget = 0;
                auto mods = CMenuRegistry::Instance().GetModulesByCategory(m_SelectedCategory);
                return true;
            }
            if(key == SDLK_ESCAPE)
            {
                bool closedSomething = false;
                for(auto &p : m_ComboAnims) if(p.second.m_Expanded) { p.second.m_Expanded = false; closedSomething = true; }
                for(auto &p : m_MultiSelectAnims) if(p.second.m_Expanded) { p.second.m_Expanded = false; closedSomething = true; }
                if(!closedSomething)
                {
                    m_MenuOpen = false;
                    if(g_Config.m_shalawaHudEditor)
                    {
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                        SDL_ShowCursor(SDL_ENABLE);
                    }
                    else
                    {
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                        SDL_ShowCursor(SDL_DISABLE);
                    }
                }
                return true;
            }
        }
    }
    m_CardScrollTarget = 0;
    if(event->type == SDL_KEYDOWN && m_MenuOpen && m_SearchActive)
    {
        SDL_Keycode key = event->key.keysym.sym;
        if(key == SDLK_BACKSPACE)
        {
            int len = (int)strlen(m_SearchBuffer);
            int pos = std::clamp(m_SearchCursorPos, 0, len);
            if(pos > 0)
            {
                memmove(&m_SearchBuffer[pos - 1], &m_SearchBuffer[pos], len - pos + 1);
                m_SearchCursorPos = pos - 1;
                UpdateSearchResults();
            }
            return true;
        }
        if(key == SDLK_DELETE)
        {
            int len = (int)strlen(m_SearchBuffer);
            int pos = std::clamp(m_SearchCursorPos, 0, len);
            if(pos < len)
            {
                memmove(&m_SearchBuffer[pos], &m_SearchBuffer[pos + 1], len - pos);
                UpdateSearchResults();
            }
            return true;
        }
        if(key == SDLK_LEFT) { if(m_SearchCursorPos > 0) m_SearchCursorPos--; return true; }
        if(key == SDLK_RIGHT) { int len = (int)strlen(m_SearchBuffer); if(m_SearchCursorPos < len) m_SearchCursorPos++; return true; }
        if(key == SDLK_HOME) { m_SearchCursorPos = 0; return true; }
        if(key == SDLK_END) { m_SearchCursorPos = (int)strlen(m_SearchBuffer); return true; }
        if(key == SDLK_DOWN && !m_SearchResults.empty())
        {
            for(size_t i = 0; i + 1 < m_SearchResults.size(); i++)
                if(m_SearchResults[i]->m_Id == m_SelectedSearchModuleId) { m_SelectedSearchModuleId = m_SearchResults[i + 1]->m_Id; break; }
            return true;
        }
        if(key == SDLK_UP && !m_SearchResults.empty())
        {
            for(size_t i = 1; i < m_SearchResults.size(); i++)
                if(m_SearchResults[i]->m_Id == m_SelectedSearchModuleId) { m_SelectedSearchModuleId = m_SearchResults[i - 1]->m_Id; break; }
            return true;
        }
        if(key == SDLK_RETURN && !m_SelectedSearchModuleId.empty())
        {
            SModuleEntry *pSearchMod = CMenuRegistry::Instance().FindModule(m_SelectedSearchModuleId);
            if(pSearchMod)
            {
                m_SearchActive = false;
                m_PreviousCategory = m_SelectedCategory;
                m_SelectedCategory = pSearchMod->m_Category;
                m_SelectedModuleId = pSearchMod->m_Id;
                m_SettingScrollTarget = 0;
                m_ModuleScrollTarget = 0;
                m_HeaderTransition = 0;
            }
            return true;
        }
        return true;
    }

    if(event->type == SDL_KEYDOWN && m_MenuOpen && !m_SearchActive)
    {
        SDL_Keycode key = event->key.keysym.sym;

        for(auto &pair : m_TextInputAnims)
        {
            STextInputAnim &anim = pair.second;
            if(!anim.m_Focused) continue;

            std::string settingId = pair.first;
            SSettingBase *pSetting = nullptr;
            SModuleEntry *pCurMod = GetSelectedModule();
            if(pCurMod)
                for(auto &s : pCurMod->m_Settings)
                    if(s.m_Id == settingId) { pSetting = &s; break; }

            char *pBuf = nullptr;
            int maxLen = 0;
            if(pSetting)
            {
                pBuf = pSetting->m_pStringValue ? pSetting->m_pStringValue : pSetting->m_pTextBuffer;
                maxLen = pSetting->m_pStringValue ? pSetting->m_StringMaxLen : pSetting->m_TextBufferSize;
            }
            if(!pBuf || maxLen <= 0) { anim.m_Focused = false; continue; }

            if(key == SDLK_ESCAPE) { anim.m_Focused = false; return true; }
            if(key == SDLK_BACKSPACE)
            {
                int len = (int)strlen(pBuf);
                if(len > 0) pBuf[len - 1] = '\0';
                return true;
            }
            if(key == SDLK_RETURN) { anim.m_Focused = false; return true; }
            return true;
        }

        for(auto &pair : m_ColorPickerAnims)
        {
            SColorPickerAnim &anim = pair.second;
            if(!anim.m_HexActive) continue;

            if(key == SDLK_ESCAPE)
            {
                bool closedSomething = false;
                for(auto &p : m_ComboAnims)
                    if(p.second.m_Expanded) { p.second.m_Expanded = false; closedSomething = true; }
                for(auto &p : m_MultiSelectAnims)
                    if(p.second.m_Expanded) { p.second.m_Expanded = false; closedSomething = true; }
                if(!closedSomething)
                {
                    m_MenuOpen = false;
                    if(g_Config.m_shalawaHudEditor)
                    {
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                        SDL_ShowCursor(SDL_ENABLE);
                    }
                    else
                    {
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                        SDL_ShowCursor(SDL_DISABLE);
                    }
                }
                return true;
            }
            if(key == SDLK_BACKSPACE)
            {
                if(anim.m_HexCursor > 0 && !anim.m_HexText.empty())
                {
                    int pos = std::clamp(anim.m_HexCursor, 0, (int)anim.m_HexText.length());
                    if(pos > 0)
                    {
                        anim.m_HexText.erase(pos - 1, 1);
                        anim.m_HexCursor = pos - 1;
                    }
                }
                return true;
            }
            if(key == SDLK_DELETE)
            {
                int pos = std::clamp(anim.m_HexCursor, 0, (int)anim.m_HexText.length());
                if(pos < (int)anim.m_HexText.length())
                    anim.m_HexText.erase(pos, 1);
                return true;
            }
            if(key == SDLK_LEFT) { if(anim.m_HexCursor > 0) anim.m_HexCursor--; return true; }
            if(key == SDLK_RIGHT) { if(anim.m_HexCursor < (int)anim.m_HexText.length()) anim.m_HexCursor++; return true; }
            if(key == SDLK_RETURN)
            {
                if(anim.m_HexText.length() >= 6)
                {
                    std::string settingId = pair.first;
                    SSettingBase *pSetting = nullptr;
                    SModuleEntry *pCurMod = GetSelectedModule();
                    if(pCurMod)
                        for(auto &s : pCurMod->m_Settings)
                            if(s.m_Id == settingId) { pSetting = &s; break; }

                    if(pSetting && pSetting->m_pColorHue)
                    {
                        unsigned int hex = 0;
                        try { hex = std::stoul(anim.m_HexText.substr(0, std::min((int)anim.m_HexText.length(), 8)), nullptr, 16); } catch(...) {}
                        int r = (hex >> 24) & 0xFF, g = (hex >> 16) & 0xFF, b = (hex >> 8) & 0xFF, a = hex & 0xFF;
                        if(anim.m_HexText.length() <= 6) { r = (hex >> 16) & 0xFF; g = (hex >> 8) & 0xFF; b = hex & 0xFF; a = 255; }
                        float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
                        float maxC = std::max({rf, gf, bf}), minC = std::min({rf, gf, bf});
                        float delta = maxC - minC;
                        float hue = 0, sat = 0, bri = maxC;
                        if(delta > 0.001f)
                        {
                            sat = delta / maxC;
                            if(maxC == rf) hue = fmodf((gf - bf) / delta, 6.0f) / 6.0f;
                            else if(maxC == gf) hue = ((bf - rf) / delta + 2.0f) / 6.0f;
                            else hue = ((rf - gf) / delta + 4.0f) / 6.0f;
                            if(hue < 0) hue += 1.0f;
                        }
                        *pSetting->m_pColorHue = hue;
                        *pSetting->m_pColorSaturation = sat;
                        *pSetting->m_pColorBrightness = bri;
                        *pSetting->m_pColorAlpha = a / 255.0f;
                    }
                }
                anim.m_HexActive = false;
                return true;
            }
            return true;
        }
    }

    if(event->type == SDL_TEXTINPUT && m_MenuOpen && !m_SearchActive)
    {
        for(auto &pair : m_SliderAnims)
        {
            SSliderAnim &anim = pair.second;
            if(!anim.m_InputMode) continue;

            const char *text = event->text.text;
            for(int i = 0; text[i]; i++)
            {
                char c = text[i];
                bool valid = (c >= '0' && c <= '9') || c == '-';
                if(c == '-' && anim.m_CursorPos != 0) valid = false;
                if(c == '-')
                {
                    for(char ch : anim.m_InputText)
                        if(ch == '-') { valid = false; break; }
                }

                if(valid && anim.m_InputText.length() < 10)
                {
                    anim.m_CursorPos = std::clamp(anim.m_CursorPos, 0, (int)anim.m_InputText.length());
                    anim.m_InputText.insert(anim.m_CursorPos, 1, c);
                    anim.m_CursorPos++;
                }
            }
            return true;
        }

        for(auto &pair : m_TextInputAnims)
        {
            STextInputAnim &anim = pair.second;
            if(!anim.m_Focused) continue;

            std::string settingId = pair.first;
            SSettingBase *pSetting = nullptr;
            SModuleEntry *pCurMod = GetSelectedModule();
            if(pCurMod)
                for(auto &s : pCurMod->m_Settings)
                    if(s.m_Id == settingId) { pSetting = &s; break; }
            if(!pSetting) { anim.m_Focused = false; continue; }

            char *pBuf = pSetting->m_pStringValue ? pSetting->m_pStringValue : pSetting->m_pTextBuffer;
            int maxLen = pSetting->m_pStringValue ? pSetting->m_StringMaxLen : pSetting->m_TextBufferSize;
            if(!pBuf || maxLen <= 0) { anim.m_Focused = false; continue; }

            int len = (int)strlen(pBuf);
            int inputLen = (int)strlen(event->text.text);
            if(len + inputLen < maxLen - 1)
                strcat(pBuf, event->text.text);
            return true;
        }

        for(auto &pair : m_ColorPickerAnims)
        {
            SColorPickerAnim &anim = pair.second;
            if(!anim.m_HexActive) continue;

            const char *text = event->text.text;
            for(int i = 0; text[i]; i++)
            {
                char c = text[i];
                bool isHexChar = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
                if(isHexChar && anim.m_HexText.length() < 8)
                {
                    char upper = (c >= 'a' && c <= 'f') ? c - 32 : c;
                    anim.m_HexCursor = std::clamp(anim.m_HexCursor, 0, (int)anim.m_HexText.length());
                    anim.m_HexText.insert(anim.m_HexCursor, 1, upper);
                    anim.m_HexCursor++;
                }
            }
            return true;
        }
    }

    if(event->type == SDL_TEXTINPUT && m_MenuOpen && m_SearchActive)
    {
        int len = (int)strlen(m_SearchBuffer);
        int inputLen = (int)strlen(event->text.text);
        if(len + inputLen < (int)sizeof(m_SearchBuffer) - 1)
        {
            int pos = std::clamp(m_SearchCursorPos, 0, len);
            memmove(&m_SearchBuffer[pos + inputLen], &m_SearchBuffer[pos], len - pos + 1);
            memcpy(&m_SearchBuffer[pos], event->text.text, inputLen);
            m_SearchCursorPos = pos + inputLen;
            UpdateSearchResults();
        }
        return true;
    }

    if(event->type == SDL_MOUSEBUTTONDOWN && m_MenuOpen)
    {
        if(event->button.button == SDL_BUTTON_MIDDLE)
        {
            ImGuiIO &io = ImGui::GetIO();
            float bgW = BG_WIDTH;
            float bgH = BG_HEIGHT;
            float cx = (io.DisplaySize.x - bgW) / 2.0f + m_WindowOffsetX;
            float cy = (io.DisplaySize.y - bgH) / 2.0f + m_WindowOffsetY;
        }
    }

    if(event->type == SDL_MOUSEBUTTONUP)
    {
        if(event->button.button == SDL_BUTTON_MIDDLE)
            m_Dragging = false;
    }

    if(event->type == SDL_MOUSEMOTION && m_Dragging)
    {
        m_WindowOffsetX = event->motion.x - m_DragOffsetX;
        m_WindowOffsetY = event->motion.y - m_DragOffsetY;
    }

    if(!m_MenuOpen)
        return false;

    return true;
}

void CImGuiManager::Render()
{
    ANTI_DISASM_BLOCK();
    if(!m_Initialized) return;

    if(!m_TyanLoaded && m_pGameClient) LoadTyanTexture();
    UpdateAnimations();
    m_MenuStyle = g_Config.m_shalawaMenuStyle;
    m_Config.m_MenuStyle = g_Config.m_shalawaMenuStyle;

    if(m_ShowStartMenu)
    {
        BeginFrame();

        if(!ImGui::GetCurrentContext())
        {
            EndFrame();
            return;
        }
        RenderStartMenuImGui();
        EndFrame();
        return;
    }

    if(g_Config.m_shalawaHideCheat) return;

    if(m_MenuStyle == 1)
    {
        BeginFrame();

        if(!ImGui::GetCurrentContext())
        {
            EndFrame();
            return;
        }

        try
        {
            CShalawaHud::Instance().Update();
            RenderGameVisuals();

            if(CShalawaHud::Instance().GetEditor().IsActive() && m_MenuOpen)
            {
                m_MenuOpen = false;
                SDL_SetRelativeMouseMode(SDL_FALSE);
                SDL_ShowCursor(SDL_ENABLE);
            }

            CShalawaHud::Instance().Render();
            m_Watermark.Render();

            if(m_MenuOpen || m_MenuAlpha >= 0.01f)
            {
                CImGuiManagerModern &modern = CImGuiManagerModern::Instance();
                modern.m_MenuOpen = m_MenuOpen;
                modern.RenderContent_Internal();
            }
        }
        catch(...)
        {
        }

        EndFrame();
        return;
    }

    // --- Style 0 (ClickGUI) ---

    {
        SModuleEntry *pMenuSettings = m_pMenuSettingsModule;
        if(pMenuSettings)
        {
            for(auto &s : pMenuSettings->m_Settings)
            {
                if(s.m_Id == "ms_snow" && s.m_pIntValue)
                    m_Config.m_Snow = (*s.m_pIntValue != 0);
                else if(s.m_Id == "ms_tyan" && s.m_pIntValue)
                    m_Config.m_ShowTyan = (*s.m_pIntValue != 0);
                else if(s.m_Id == "ms_bgalpha" && s.m_pIntValue)
                    m_Config.m_BackgroundAlpha = *s.m_pIntValue / 100.0f;
                else if(s.m_Id == "ms_glow" && s.m_pIntValue)
                    m_Config.m_Glow = (*s.m_pIntValue != 0);
                else if(s.m_Id == "ms_desc" && s.m_pIntValue)
                    m_Config.m_Descriptions = (*s.m_pIntValue != 0);
                else if(s.m_Id == "mc_gradient" && s.m_pIntValue)
                    m_Config.m_Color.m_Easing = (*s.m_pIntValue != 0);
            }
        }
    }

    {
        ImVec4 c1 = HSBtoRGB(m_AccentHue1, m_AccentSat1, m_AccentBri1);
        m_Config.m_Color.m_SolidColor[0] = c1.x;
        m_Config.m_Color.m_SolidColor[1] = c1.y;
        m_Config.m_Color.m_SolidColor[2] = c1.z;
        m_Config.m_Color.m_SolidColor[3] = m_AccentAlpha1;

        ImVec4 c2 = HSBtoRGB(m_AccentHue2, m_AccentSat2, m_AccentBri2);
        m_Config.m_Color.m_EasingColor[0] = c2.x;
        m_Config.m_Color.m_EasingColor[1] = c2.y;
        m_Config.m_Color.m_EasingColor[2] = c2.z;
        m_Config.m_Color.m_EasingColor[3] = m_AccentAlpha2;
    }

    if(m_MenuOpen || m_MenuAlpha > 0.01f)
    {
        int GlW, GlH;
        SDL_GL_GetDrawableSize(m_pWindow, &GlW, &GlH);
        CGlassRenderer::Instance().CaptureScreen(GlW, GlH);
        CGlassRenderer::Instance().GenerateBlur(4);
    }

    BeginFrame();

    if(!ImGui::GetCurrentContext())
    {
        EndFrame();
        return;
    }

        try
        {
            CShalawaHud::Instance().Update();
            RenderGameVisuals();

            if(CShalawaHud::Instance().GetEditor().IsActive() && m_MenuOpen)
            {
                m_MenuOpen = false;
                SDL_SetRelativeMouseMode(SDL_FALSE);
                SDL_ShowCursor(SDL_ENABLE);
            }

            CShalawaHud::Instance().Render();
            m_Watermark.Render();

            RenderSnowflakes();
            if(m_MenuOpen || m_MenuAlpha >= 0.01f)
            {
                RenderBackground();
                RenderTyan();
                RenderMenuWindow();
            }
        }
        catch(...)
        {
        }

        EndFrame();
}
