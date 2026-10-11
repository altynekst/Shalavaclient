#pragma once

#define NOMINMAX
#include <windows.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#include "Auth.h"
#include "Config.h"

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

namespace UI
{
    namespace Colors
    {
        inline ImU32 Bg()           { return IM_COL32(13, 13, 17, 255); }
        inline ImU32 Panel()        { return IM_COL32(22, 22, 28, 255); }
        inline ImU32 PanelHover()   { return IM_COL32(30, 30, 38, 255); }
        inline ImU32 Accent()       { return IM_COL32(34, 211, 238, 255); }
        inline ImU32 AccentDim()    { return IM_COL32(20, 140, 170, 255); }
        inline ImU32 AccentGlow()   { return IM_COL32(60, 220, 250, 255); }
        inline ImU32 Text()         { return IM_COL32(240, 240, 248, 255); }
        inline ImU32 TextDim()      { return IM_COL32(130, 130, 145, 255); }
        inline ImU32 TextMuted()    { return IM_COL32(70, 70, 85, 255); }
        inline ImU32 Border()       { return IM_COL32(40, 40, 50, 255); }
        inline ImU32 Input()        { return IM_COL32(18, 18, 24, 255); }
        inline ImU32 Success()      { return IM_COL32(50, 205, 120, 255); }
        inline ImU32 Error()        { return IM_COL32(235, 80, 70, 255); }
    }

    enum class AppState { Loading, Login, Main };

    struct Anim
    {
        float val = 0.0f;
        float target = 0.0f;
        float speed = 6.0f;
        
        void Set(float t) { target = t; }
        void Update(float dt) { val += (target - val) * dt * speed; }
        float Get() const { return val; }
        float Smooth() const { float t = val; return t * t * (3.0f - 2.0f * t); }
        bool Done() const { return fabsf(val - target) < 0.001f; }
    };

    struct Toast
    {
        std::string text;
        int type;
        float life;
        float maxLife;
        Anim slideAnim;
        Anim fadeAnim;
        bool removing;
        
        Toast() : type(0), life(0), maxLife(4.0f), removing(false) {
            slideAnim.speed = 8.0f;
            fadeAnim.speed = 6.0f;
        }
    };

    struct UIState
    {
        SDL_Window* Window = nullptr;
        SDL_GLContext GLContext = nullptr;
        HWND Hwnd = nullptr;
        bool Running = true;
        bool Closing = false;

        GLuint LogoTex = 0;
        int LogoW = 0, LogoH = 0;

        ImFont* FontReg = nullptr;
        ImFont* FontMed = nullptr;
        ImFont* FontBold = nullptr;
        ImFont* FontBig = nullptr;

        AppState State = AppState::Loading;
        AppState PrevState = AppState::Loading;
        Auth::AuthState Auth;
        Config::LoaderConfig Cfg;

        char User[64] = {};
        char Pass[64] = {};
        bool Remember = false;

        std::string Status;
        int StatusType = 0;
        Anim StatusAnim;

        std::string Stage;
        bool Busy = false;
        std::atomic<long long> Done{0};
        std::atomic<long long> Total{0};

        bool Dragging = false;
        int DragX = 0, DragY = 0;
        int WinX = 0, WinY = 0;

        float Time = 0.0f;
        float Delta = 0.0f;
        
        Anim WindowAnim;
        Anim FadeAnim;
        Anim PageAnim;
        Anim HeaderAnim;
        Anim ContentAnim;
        Anim BtnAnim[4];
        Anim InputAnim[2];
        Anim CardAnim;
        Anim ProgressAnim;
        Anim SpinnerAnim;
        std::vector<Toast> Toasts;
        
        std::chrono::steady_clock::time_point LastFrame;

        std::string LoadingText;
        bool ResourcesReady = false;
        
        bool HeartbeatActive = false;
        std::string HeartbeatStatus;
    };

    bool Init(UIState& s);
    void Run(UIState& s);
    void Shutdown(UIState& s);
}