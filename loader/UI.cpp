#include "UI.h"
#include "Utils.h"
#include "Network.h"
#include "Config.h"
#include "Client.h"
#include "Auth.h"
#include "AntiDebug.h"

#include <dwmapi.h>
#include <wincodec.h>
#include "Heartbeat.h"
#include <SDL_syswm.h>
#include <shellapi.h>
#include <thread>
#include <cmath>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace UI
{

static const float W = 440.0f;
static const float H = 540.0f;
static const float HDR = 56.0f;
static const float PAD = 32.0f;
static const float RAD = 10.0f;

static float Lerp(float a, float b, float t) { return a + (b - a) * t; }
static float Clamp(float v, float mn, float mx) { return v < mn ? mn : v > mx ? mx : v; }
static float EaseOut(float t) { return 1.0f - powf(1.0f - t, 3.0f); }
static float EaseInOut(float t) { return t < 0.5f ? 4*t*t*t : 1 - powf(-2*t+2, 3)/2; }

static bool LoadWIC(const std::wstring& path, std::vector<unsigned char>& px, int& w, int& h)
{
    IWICImagingFactory* fac = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fac)))) return false;
    
    IWICBitmapDecoder* dec = nullptr;
    if (FAILED(fac->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &dec))) 
    { fac->Release(); return false; }
    
    IWICBitmapFrameDecode* frm = nullptr;
    if (FAILED(dec->GetFrame(0, &frm))) { dec->Release(); fac->Release(); return false; }
    
    IWICFormatConverter* conv = nullptr;
    if (FAILED(fac->CreateFormatConverter(&conv))) { frm->Release(); dec->Release(); fac->Release(); return false; }
    
    if (FAILED(conv->Initialize(frm, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
    { conv->Release(); frm->Release(); dec->Release(); fac->Release(); return false; }
    
    UINT uw, uh; conv->GetSize(&uw, &uh);
    w = (int)uw; h = (int)uh;
    px.resize(w * h * 4);
    HRESULT hr = conv->CopyPixels(nullptr, w * 4, (UINT)px.size(), px.data());
    
    conv->Release(); frm->Release(); dec->Release(); fac->Release();
    return SUCCEEDED(hr);
}

static GLuint MakeTex(const unsigned char* px, int w, int h)
{
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

static HICON CreateIconFromPNG(const std::wstring& path)
{
    std::vector<unsigned char> px;
    int w, h;
    if (!LoadWIC(path, px, w, h)) return nullptr;
    
    BITMAPV5HEADER bi = {};
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = w;
    bi.bV5Height = -h;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    HDC hdc = GetDC(nullptr);
    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    
    if (!hBmp) return nullptr;

    for (int i = 0; i < w * h; i++) {
        unsigned char* src = &px[i * 4];
        unsigned char* dst = &((unsigned char*)bits)[i * 4];
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
        dst[3] = src[3];
    }

    HBITMAP hMask = CreateBitmap(w, h, 1, 1, nullptr);
    
    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmMask = hMask;
    ii.hbmColor = hBmp;
    
    HICON icon = CreateIconIndirect(&ii);
    
    DeleteObject(hBmp);
    DeleteObject(hMask);
    
    return icon;
}

static void RoundCorners(HWND hwnd)
{
    DWORD pref = 2;
    DwmSetWindowAttribute(hwnd, 33, &pref, sizeof(pref));
}

static void DownloadResources(UIState& s)
{
    std::wstring dir = Utils::GetAppDataPath();

    struct Res { const wchar_t* url; const wchar_t* file; };
    Res files[] = {
        { L"https://raw.githubusercontent.com/sisubak/real_ueh/refs/heads/main/Inter-Regular.ttf", L"Inter-Regular.ttf" },
        { L"https://raw.githubusercontent.com/sisubak/real_ueh/refs/heads/main/Inter-Medium.ttf", L"Inter-Medium.ttf" },
        { L"https://raw.githubusercontent.com/sisubak/real_ueh/refs/heads/main/Inter-Bold.ttf", L"Inter-Bold.ttf" },
        { L"https://i.postimg.cc/NjsxB8q0/izobrazenie.png", L"logo.png" },
    };

    for (int i = 0; i < 4; i++) {
        std::wstring path = Utils::JoinPath(dir, files[i].file);
        if (!Utils::FileExists(path)) {
            s.LoadingText = "Downloading resources...";
            Network::DownloadFile(files[i].url, path);
        }
    }
}

bool Init(UIState& s)
{
    // Инициализация антиотладки один раз при старте
    AntiDebug::Init();
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return false;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    s.Window = SDL_CreateWindow("Shalawa", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (int)W, (int)H, SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
    if (!s.Window) return false;

    s.GLContext = SDL_GL_CreateContext(s.Window);
    SDL_GL_SetSwapInterval(1);

    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (SDL_GetWindowWMInfo(s.Window, &wmi)) {
        s.Hwnd = wmi.info.win.window;
        RoundCorners(s.Hwnd);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;

    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 12.0f;
    st.FrameRounding = RAD;
    st.ScrollbarRounding = 6.0f;
    st.GrabRounding = 6.0f;
    st.WindowBorderSize = 0.0f;
    st.FrameBorderSize = 1.0f;
    st.WindowPadding = ImVec2(0, 0);
    st.FramePadding = ImVec2(16, 12);
    st.ItemSpacing = ImVec2(8, 8);

    auto C = [](ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); };
    ImVec4* col = st.Colors;
    col[ImGuiCol_WindowBg] = C(Colors::Bg());
    col[ImGuiCol_FrameBg] = C(Colors::Input());
    col[ImGuiCol_FrameBgHovered] = C(IM_COL32(24, 24, 32, 255));
    col[ImGuiCol_FrameBgActive] = C(IM_COL32(28, 28, 36, 255));
    col[ImGuiCol_Border] = C(Colors::Border());
    col[ImGuiCol_Text] = C(Colors::Text());
    col[ImGuiCol_CheckMark] = C(Colors::Accent());
    col[ImGuiCol_Button] = C(Colors::Accent());
    col[ImGuiCol_ButtonHovered] = C(Colors::AccentGlow());
    col[ImGuiCol_ButtonActive] = C(Colors::AccentDim());

    s.WindowAnim.speed = 4.0f;
    s.FadeAnim.speed = 5.0f;
    s.PageAnim.speed = 6.0f;
    s.HeaderAnim.speed = 5.0f;
    s.ContentAnim.speed = 4.0f;
    s.CardAnim.speed = 5.0f;
    s.StatusAnim.speed = 8.0f;
    
    for (int i = 0; i < 4; i++) s.BtnAnim[i].speed = 12.0f;
    for (int i = 0; i < 2; i++) s.InputAnim[i].speed = 10.0f;

    s.LastFrame = std::chrono::steady_clock::now();
    return true;
}

static void LoadFontsAndLogo(UIState& s)
{
    std::wstring dir = Utils::GetAppDataPath();
    ImGuiIO& io = ImGui::GetIO();

    std::string fReg = Utils::WideToUtf8(Utils::JoinPath(dir, L"Inter-Regular.ttf"));
    std::string fMed = Utils::WideToUtf8(Utils::JoinPath(dir, L"Inter-Medium.ttf"));
    std::string fBold = Utils::WideToUtf8(Utils::JoinPath(dir, L"Inter-Bold.ttf"));

    s.FontReg = io.Fonts->AddFontFromFileTTF(fReg.c_str(), 15.0f);
    s.FontMed = io.Fonts->AddFontFromFileTTF(fMed.c_str(), 15.0f);
    s.FontBold = io.Fonts->AddFontFromFileTTF(fBold.c_str(), 16.0f);
    s.FontBig = io.Fonts->AddFontFromFileTTF(fBold.c_str(), 22.0f);

    if (!s.FontReg) s.FontReg = io.Fonts->AddFontDefault();
    if (!s.FontMed) s.FontMed = s.FontReg;
    if (!s.FontBold) s.FontBold = s.FontReg;
    if (!s.FontBig) s.FontBig = s.FontBold;

    ImGui_ImplSDL2_InitForOpenGL(s.Window, s.GLContext);
    ImGui_ImplOpenGL3_Init("#version 130");

    std::wstring logoPath = Utils::JoinPath(dir, L"logo.png");
    std::vector<unsigned char> px;
    if (LoadWIC(logoPath, px, s.LogoW, s.LogoH)) {
        s.LogoTex = MakeTex(px.data(), s.LogoW, s.LogoH);
    }

    if (s.Hwnd) {
        HICON icon = CreateIconFromPNG(logoPath);
        if (icon) {
            SendMessage(s.Hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
            SendMessage(s.Hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
        }
    }

    s.ResourcesReady = true;
}

static void DrawSpinner(ImDrawList* dl, ImVec2 c, float r, float t, float alpha = 1.0f)
{
    int segs = 30;
    float start = fmodf(t * 5.0f, 6.28318f);
    float len = 2.0f + 0.8f * sinf(t * 4.0f);
    
    for (int i = 0; i < segs; i++) {
        float a1 = start + (float)i / segs * len;
        float a2 = start + (float)(i + 1) / segs * len;
        float segAlpha = (float)i / segs;
        ImU32 col = IM_COL32(34, 211, 238, (int)(segAlpha * 255 * alpha));
        dl->AddLine(
            ImVec2(c.x + cosf(a1) * r, c.y + sinf(a1) * r),
            ImVec2(c.x + cosf(a2) * r, c.y + sinf(a2) * r),
            col, 2.5f
        );
    }
}

static void AddToast(UIState& s, const std::string& text, int type)
{
    Toast t;
    t.text = text;
    t.type = type;
    t.life = 0.0f;
    t.maxLife = 4.0f;
    t.removing = false;
    t.slideAnim.val = 0.0f;
    t.slideAnim.Set(1.0f);
    t.fadeAnim.val = 1.0f;
    t.fadeAnim.Set(1.0f);
    s.Toasts.push_back(t);
}

static void SetStatus(UIState& s, const std::string& t, int type)
{
    AddToast(s, t, type);
}

static void UpdateToasts(UIState& s)
{
    for (auto it = s.Toasts.begin(); it != s.Toasts.end();) {
        it->life += s.Delta;
        it->slideAnim.Update(s.Delta);
        it->fadeAnim.Update(s.Delta);
        
        if (it->life > it->maxLife - 0.5f && !it->removing) {
            it->removing = true;
            it->slideAnim.Set(0.0f);
            it->fadeAnim.Set(0.0f);
        }
        
        if (it->removing && it->slideAnim.Get() < 0.02f) {
            it = s.Toasts.erase(it);
        } else {
            ++it;
        }
    }
}

static void RenderToasts(UIState& s, ImDrawList* dl)
{
    float startX = W - 10;
    float startY = 20;
    float toastW = 200.0f;
    float toastH = 45.0f;
    float gap = 8.0f;
    
    int idx = 0;
    for (auto& t : s.Toasts) {
        float slide = EaseOut(t.slideAnim.Get());
        float fade = t.fadeAnim.Get();
        
        float x = startX - (toastW + 10) * slide;
        float y = startY + idx * (toastH + gap);
        
        ImU32 accentCol;
        switch (t.type) {
            case 1: accentCol = Colors::Success(); break;
            case 2: accentCol = Colors::Error(); break;
            default: accentCol = Colors::Accent(); break;
        }
        
        int alpha = (int)(fade * 255);
        int bgAlpha = (int)(fade * 200);
        
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + toastW, y + toastH), 
            IM_COL32(20, 20, 26, bgAlpha), 8.0f);
        
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 4, y + toastH), 
            IM_COL32((accentCol >> 0) & 0xFF, (accentCol >> 8) & 0xFF, 
                (accentCol >> 16) & 0xFF, alpha), 8.0f, ImDrawFlags_RoundCornersLeft);
        
        ImGui::PushFont(s.FontMed);
        ImVec2 textSize = ImGui::CalcTextSize(t.text.c_str());
        float textX = x + 14;
        float textY = y + (toastH - textSize.y) * 0.5f;
        dl->AddText(ImVec2(textX, textY), IM_COL32(255, 255, 255, alpha), t.text.c_str());
        ImGui::PopFont();
        
        idx++;
    }
}

static void UpdateAnims(UIState& s)
{
    s.WindowAnim.Update(s.Delta);
    s.FadeAnim.Update(s.Delta);
    s.PageAnim.Update(s.Delta);
    s.HeaderAnim.Update(s.Delta);
    s.ContentAnim.Update(s.Delta);
    s.CardAnim.Update(s.Delta);
    s.StatusAnim.Update(s.Delta);
    
    for (int i = 0; i < 4; i++) s.BtnAnim[i].Update(s.Delta);
    for (int i = 0; i < 2; i++) s.InputAnim[i].Update(s.Delta);
    
    UpdateToasts(s);
}

static void RenderHeader(UIState& s, ImDrawList* dl)
{
    float anim = EaseOut(s.HeaderAnim.Get());
    float slideY = (1.0f - anim) * -HDR;
    
    dl->AddRectFilled(ImVec2(0, slideY), ImVec2(W, HDR + slideY), Colors::Panel());
    dl->AddLine(ImVec2(0, HDR + slideY), ImVec2(W, HDR + slideY), Colors::Border());

    float logoSz = 32.0f;
    float logoX = 20.0f;
    float logoY = (HDR - logoSz) * 0.5f + slideY;

    if (s.LogoTex) {
        dl->AddImageRounded((void*)(intptr_t)s.LogoTex,
            ImVec2(logoX, logoY), ImVec2(logoX + logoSz, logoY + logoSz),
            ImVec2(0, 0), ImVec2(1, 1), 
            IM_COL32(255, 255, 255, (int)(anim * 255)), 6.0f);
    }

    ImGui::PushFont(s.FontBig);
    dl->AddText(ImVec2(logoX + logoSz + 12, (HDR - 22) * 0.5f + slideY), 
        IM_COL32(240, 240, 248, (int)(anim * 255)), "Shalawa");
    ImGui::PopFont();

    ImVec2 m = ImGui::GetMousePos();
    float btnSz = 22.0f;
    float btnY = (HDR - btnSz) * 0.5f + slideY;

    // Minimize button
    ImVec2 minP(W - btnSz * 2 - 28, btnY);
    bool minH = m.x >= minP.x && m.x < minP.x + btnSz && m.y >= minP.y && m.y < minP.y + btnSz;
    s.BtnAnim[0].Set(minH ? 1.0f : 0.0f);
    float minA = s.BtnAnim[0].Smooth();
    ImU32 minC = IM_COL32(50 + (int)(minA * 30), 50 + (int)(minA * 30), 60 + (int)(minA * 20), (int)(anim * 255));
    dl->AddRectFilled(minP, ImVec2(minP.x + btnSz, minP.y + btnSz), minC, 5.0f);
    dl->AddLine(ImVec2(minP.x + 6, minP.y + btnSz * 0.5f), 
        ImVec2(minP.x + btnSz - 6, minP.y + btnSz * 0.5f), 
        IM_COL32(130, 130, 145, (int)(anim * 255)), 1.5f);
    if (minH && ImGui::IsMouseClicked(0)) SDL_MinimizeWindow(s.Window);

    // Close button
    ImVec2 clP(W - btnSz - 16, btnY);
    bool clH = m.x >= clP.x && m.x < clP.x + btnSz && m.y >= clP.y && m.y < clP.y + btnSz;
    s.BtnAnim[1].Set(clH ? 1.0f : 0.0f);
    float clA = s.BtnAnim[1].Smooth();
    ImU32 clC = IM_COL32(80 + (int)(clA * 120), 40, 40, (int)(anim * 255));
    dl->AddRectFilled(clP, ImVec2(clP.x + btnSz, clP.y + btnSz), clC, 5.0f);
    float cx = clP.x + btnSz * 0.5f, cy = clP.y + btnSz * 0.5f;
    dl->AddLine(ImVec2(cx - 4, cy - 4), ImVec2(cx + 4, cy + 4), 
        IM_COL32(240, 240, 248, (int)(anim * 255)), 1.5f);
    dl->AddLine(ImVec2(cx + 4, cy - 4), ImVec2(cx - 4, cy + 4), 
        IM_COL32(240, 240, 248, (int)(anim * 255)), 1.5f);
    if (clH && ImGui::IsMouseClicked(0) && !s.Closing) {
        s.Closing = true;
        s.WindowAnim.Set(0.0f);
        s.WindowAnim.speed = 6.0f;
    }

    // Drag
    bool dragZone = m.x >= 0 && m.x < W - 80 && m.y >= 0 && m.y < HDR;
    if (dragZone && ImGui::IsMouseClicked(0) && !s.Dragging) {
        s.Dragging = true;
        SDL_GetGlobalMouseState(&s.DragX, &s.DragY);
        SDL_GetWindowPosition(s.Window, &s.WinX, &s.WinY);
    }
    if (s.Dragging) {
        if (ImGui::IsMouseDown(0)) {
            int mx, my;
            SDL_GetGlobalMouseState(&mx, &my);
            SDL_SetWindowPosition(s.Window, s.WinX + mx - s.DragX, s.WinY + my - s.DragY);
        } else {
            s.Dragging = false;
        }
    }
}

static void RenderInput(UIState& s, const char* lbl, char* buf, int sz, bool pwd, int idx, float anim)
{
    float slideX = (1.0f - EaseOut(anim)) * 30.0f;
    float alpha = anim;
    
    ImGui::SetCursorPosX(PAD + slideX);
    
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::PushFont(s.FontReg);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Colors::TextDim()), "%s", lbl);
    ImGui::PopFont();
    
    ImGui::Spacing();
    ImGui::SetCursorPosX(PAD + slideX);
    
    float focusAnim = s.InputAnim[idx].Smooth();
    
    ImU32 borderCol = IM_COL32(
        40 + (int)(focusAnim * 20), 
        40 + (int)(focusAnim * 80), 
        50 + (int)(focusAnim * 100), 
        (int)(alpha * 255)
    );
    
    ImGui::PushFont(s.FontMed);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14, 12));
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(borderCol));
    ImGui::SetNextItemWidth(W - PAD * 2 - slideX);
    char id[32]; snprintf(id, 32, "##%s", lbl);
    ImGui::InputText(id, buf, sz, pwd ? ImGuiInputTextFlags_Password : 0);
    
    bool focused = ImGui::IsItemActive() || ImGui::IsItemFocused();
    s.InputAnim[idx].Set(focused ? 1.0f : 0.0f);
    
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopFont();
    ImGui::PopStyleVar();
}
    
static bool RenderButton(UIState& s, const char* txt, float w, float h, bool primary, bool enabled, int idx, float anim)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float slideY = (1.0f - EaseOut(anim)) * 20.0f;
    p.y += slideY;
    
    ImVec2 m = ImGui::GetMousePos();
    bool hover = enabled && m.x >= p.x && m.x < p.x + w && m.y >= p.y && m.y < p.y + h;
    
    s.BtnAnim[idx].Set(hover ? 1.0f : 0.0f);
    float hoverAnim = s.BtnAnim[idx].Smooth();
    
    ImDrawList* dl = ImGui::GetWindowDrawList();
    
    float scale = 1.0f + hoverAnim * 0.02f;
    float offsetX = w * (1.0f - scale) * 0.5f;
    float offsetY = h * (1.0f - scale) * 0.5f;
    
    ImU32 bg, fg;
    if (primary) {
        int r = (int)Lerp(34, 60, hoverAnim);
        int g = (int)Lerp(211, 225, hoverAnim);
        int b = (int)Lerp(238, 255, hoverAnim);
        bg = IM_COL32(r, g, b, (int)((enabled ? 255 : 120) * anim));
        fg = IM_COL32(5, 5, 15, (int)(255 * anim));
    } else {
        int v = (int)Lerp(28, 40, hoverAnim);
        bg = IM_COL32(v, v, v + 8, (int)((enabled ? 255 : 120) * anim));
        fg = IM_COL32(240, 240, 248, (int)(255 * anim));
    }
    
    dl->AddRectFilled(
        ImVec2(p.x + offsetX, p.y + offsetY), 
        ImVec2(p.x + w - offsetX, p.y + h - offsetY), 
        bg, RAD);
    
    ImGui::PushFont(s.FontMed);
    ImVec2 tsz = ImGui::CalcTextSize(txt);
    dl->AddText(ImVec2(p.x + (w - tsz.x) * 0.5f, p.y + (h - tsz.y) * 0.5f + offsetY), fg, txt);
    ImGui::PopFont();
    
    ImGui::Dummy(ImVec2(w, h + slideY));
    return hover && enabled && ImGui::IsMouseClicked(0);
}

static void RenderProgress(UIState& s, float prog, float anim)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = W - PAD * 2;
    float h = 44.0f;
    
    s.ProgressAnim.Set(prog);
    s.ProgressAnim.speed = 8.0f;
    s.ProgressAnim.Update(s.Delta);
    float smoothProg = s.ProgressAnim.Get();
    
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), 
        IM_COL32(18, 18, 24, (int)(255 * anim)), RAD);
    
    float fill = w * Clamp(smoothProg, 0.0f, 1.0f);
    if (fill > 2) {
        dl->AddRectFilled(p, ImVec2(p.x + fill, p.y + h), 
            IM_COL32(34, 211, 238, (int)(255 * anim)), RAD);
    }
    
    ImGui::PushFont(s.FontMed);
    char txt[16]; snprintf(txt, 16, "%d%%", (int)(smoothProg * 100));
    ImVec2 tsz = ImGui::CalcTextSize(txt);
    dl->AddText(ImVec2(p.x + (w - tsz.x) * 0.5f, p.y + (h - tsz.y) * 0.5f), 
        IM_COL32(240, 240, 248, (int)(255 * anim)), txt);
    ImGui::PopFont();
    
    ImGui::Dummy(ImVec2(w, h));
}

static void RenderLogin(UIState& s, float pageAnim)
{
    float cw = W - PAD * 2;
    float y = HDR + 32;
    
    float titleAnim = Clamp((pageAnim - 0.0f) * 2.0f, 0.0f, 1.0f);
    float inputAnim1 = Clamp((pageAnim - 0.1f) * 2.0f, 0.0f, 1.0f);
    float inputAnim2 = Clamp((pageAnim - 0.15f) * 2.0f, 0.0f, 1.0f);
    float checkAnim = Clamp((pageAnim - 0.2f) * 2.0f, 0.0f, 1.0f);
    float btnAnim = Clamp((pageAnim - 0.25f) * 2.0f, 0.0f, 1.0f);
    
    ImGui::SetCursorPos(ImVec2(PAD, y));
    ImGui::BeginGroup();
    
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, titleAnim);
    ImGui::PushFont(s.FontBig);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Colors::Text()), "Welcome to Shalawa");
    ImGui::PopFont();
    
    ImGui::Spacing();
    ImGui::PushFont(s.FontReg);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Colors::TextDim()), "Введи данные об аккаунте");
    ImGui::PopFont();
    ImGui::PopStyleVar();
    
    ImGui::Dummy(ImVec2(0, 20));
    
    RenderInput(s, "Юзернейм", s.User, sizeof(s.User), false, 0, inputAnim1);
    ImGui::Dummy(ImVec2(0, 8));
    RenderInput(s, "Пароль", s.Pass, sizeof(s.Pass), true, 1, inputAnim2);
    
    ImGui::Dummy(ImVec2(0, 12));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, checkAnim);
    ImGui::PushFont(s.FontReg);
    ImGui::Checkbox("  Запомнить меня", &s.Remember);
    ImGui::PopFont();
    ImGui::PopStyleVar();
    
    ImGui::Dummy(ImVec2(0, 20));
    
    bool canLogin = strlen(s.User) > 0 && strlen(s.Pass) > 0 && !s.Busy;
    ImGui::SetCursorPosX(PAD);
    if (RenderButton(s, s.Busy ? "Вход..." : "Войти", cw, 48, true, canLogin, 2, btnAnim)) {
        AntiDebug::PerformAdvancedAntiDebug();
        s.Busy = true;
        std::string user = s.User, pass = s.Pass;
        std::thread([&s, user, pass]() {
            s.Auth = Auth::Login(user, pass, Utils::GetHwid());
            if (s.Auth.HasValidSession) {
                if (s.Auth.SubscriptionActive) {
                    SetStatus(s, "Welcome, " + s.Auth.Username + "!", 1);
                    s.Cfg.Remember = s.Remember;
                    s.Cfg.Token = s.Remember ? s.Auth.Token : "";
                    s.Cfg.Username = user;
                    Config::Save(s.Cfg);

                    std::string hwid = Utils::GetHwid();
                    Heartbeat::Start(s.Auth.Token, hwid, [&s](Heartbeat::HeartbeatResult result) {
                        if (result.ServerAction == Heartbeat::Action::Terminate) {
                            SetStatus(s, result.Message.empty() ? "Session terminated" : result.Message, 2);
                            s.Auth = Auth::AuthState();
                            s.Cfg.Token.clear();
                            Config::Save(s.Cfg);
                            s.PrevState = s.State;
                            s.State = AppState::Login;
                            s.PageAnim.val = 0.0f;
                            s.PageAnim.Set(1.0f);
                            Heartbeat::Stop();
                        }
                        else if (result.ServerAction == Heartbeat::Action::UpdateRequired) {
                            SetStatus(s, "Update required, downloading...", 0);
                        }
                    });
                    
                    s.PrevState = s.State;
                    s.State = AppState::Main;
                    s.PageAnim.val = 0.0f;
                    s.PageAnim.Set(1.0f);
                } else {
                    SetStatus(s, "Подписка закончилась..", 2);
                    s.Auth = Auth::AuthState();
                }
            } else {
                SetStatus(s, s.Auth.Status.empty() ? "Логин не удался" : s.Auth.Status, 2);
            }
            s.Busy = false;
        }).detach();
    }
    
    ImGui::EndGroup();
}

static void RenderMain(UIState& s, float pageAnim)
{
    float cw = W - PAD * 2;
    float y = HDR + 32;
    
    float titleAnim = Clamp((pageAnim - 0.0f) * 2.0f, 0.0f, 1.0f);
    float cardAnim = Clamp((pageAnim - 0.1f) * 2.0f, 0.0f, 1.0f);
    float btnAnim = Clamp((pageAnim - 0.2f) * 2.0f, 0.0f, 1.0f);
    float logoutAnim = Clamp((pageAnim - 0.25f) * 2.0f, 0.0f, 1.0f);
    
    ImGui::SetCursorPos(ImVec2(PAD, y));
    ImGui::BeginGroup();
    
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, titleAnim);
    ImGui::PushFont(s.FontBig);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Colors::Text()), "Добро пожаловать!");
    ImGui::PopFont();
    
    ImGui::Spacing();
    ImGui::PushFont(s.FontReg);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Colors::TextDim()), "Вошел как");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Colors::Accent()), "%s", s.Auth.Username.c_str());
    ImGui::PopFont();
    ImGui::PopStyleVar();
    
    ImGui::Dummy(ImVec2(0, 16));
    
    // Subscription card
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 cardP = ImGui::GetCursorScreenPos();
    float cardH = 64;
    
    dl->AddRectFilled(cardP, ImVec2(cardP.x + cw, cardP.y + cardH), 
        IM_COL32(22, 22, 28, (int)(255 * cardAnim)), RAD);
    dl->AddRect(cardP, ImVec2(cardP.x + cw, cardP.y + cardH), 
        IM_COL32(40, 40, 50, (int)(255 * cardAnim)), RAD);
    
    ImGui::SetCursorPosX(PAD + 16);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, cardAnim);
    ImGui::PushFont(s.FontMed);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Colors::TextDim()), "Подписка");
    ImGui::PopFont();
    
    ImGui::PushFont(s.FontBold);
    ImVec2 statusSize = ImGui::CalcTextSize("АКТИВНА");
    ImGui::SameLine(cw - statusSize.x - 16);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Colors::Success()), "АКТИВНА");
    ImGui::PopFont();
    
    ImGui::SetCursorPosX(PAD + 16);
    ImGui::PushFont(s.FontReg);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Colors::TextMuted()), 
        "До: %s", s.Auth.SubscriptionUntil.c_str());
    ImGui::PopFont();
    ImGui::PopStyleVar();
    
    ImGui::Dummy(ImVec2(0, 24));
    
    if (s.Busy) {
        ImGui::SetCursorPosX(PAD);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, btnAnim);
        ImGui::PushFont(s.FontMed);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Colors::TextDim()), "%s", s.Stage.c_str());
        ImGui::PopFont();
        ImGui::PopStyleVar();
        ImGui::Spacing();
        ImGui::SetCursorPosX(PAD);
        float prog = s.Total > 0 ? (float)s.Done.load() / (float)s.Total.load() : 0.0f;
        RenderProgress(s, prog, btnAnim);
    } else {
        std::wstring dir = Utils::GetAppDataPath();
        std::wstring exe = Utils::JoinPath(Utils::JoinPath(dir, L"client"), L"DDNet.exe");
        bool hasClient = Utils::FileExists(exe);
        
        ImGui::SetCursorPosX(PAD);
        if (RenderButton(s, hasClient ? "Запуск" : "Скачать", cw, 48, true, true, 2, btnAnim)) {
            AntiDebug::PerformAdvancedAntiDebug();
            s.Busy = true;
            s.Done = 0;
            s.Total = 0;
            
            std::thread([&s, dir]() {
                std::wstring clientDir = Utils::JoinPath(dir, L"client");
                std::wstring exe = Utils::JoinPath(clientDir, L"DDNet.exe");
                bool hasClient = Utils::FileExists(exe);
                
                std::string ver = Client::FetchArchiveVersion(s.Auth.Token);
                if (hasClient && !ver.empty() && ver == s.Cfg.LastVersion) {
                    s.Stage = "Запускаю...";
                    Auth::WriteSessionFile(s.Auth.Token, s.Auth.SubscriptionUntil);
                    std::string err;
                    if (Client::LaunchClient(clientDir, err)) {
                        s.Closing = true;
                        s.WindowAnim.Set(0.0f);
                    } else {
                        SetStatus(s, err, 2);
                    }
                    s.Busy = false;
                    s.Stage.clear();
                    return;
                }
                
                s.Stage = "Скачиваю...";
                std::wstring arc = Utils::JoinPath(dir, Config::GetClientArchive());
                long long wr = 0, tot = 0;
                if (!Client::DownloadClientArchive(s.Auth.Token, arc, wr, tot, [&s, &tot](long long b) {
                    if (tot > 0) s.Total = tot;
                    s.Done = b;
                })) {
                    SetStatus(s, "Ошибка скачивания", 2);
                    s.Busy = false;
                    s.Stage.clear();
                    return;
                }
                
                if (!ver.empty()) { s.Cfg.LastVersion = ver; Config::Save(s.Cfg); }
                
                s.Stage = "Распаковываю...";
                CreateDirectoryW(clientDir.c_str(), nullptr);
                std::string err;
                if (!Client::ExtractArchive(arc, clientDir, err)) {
                    SetStatus(s, err, 2);
                    s.Busy = false;
                    s.Stage.clear();
                    return;
                }
                
                s.Stage = "Запускаю...";
                Auth::WriteSessionFile(s.Auth.Token, s.Auth.SubscriptionUntil);
                if (Client::LaunchClient(clientDir, err)) {
                    s.Closing = true;
                    s.WindowAnim.Set(0.0f);
                } else {
                    SetStatus(s, err, 2);
                }
                s.Busy = false;
                s.Stage.clear();
            }).detach();
        }
    }
    
    ImGui::Dummy(ImVec2(0, 16));
    ImGui::SetCursorPosX(PAD);
    if (RenderButton(s, "Выйти", cw, 42, false, !s.Busy, 3, logoutAnim)) {
        Heartbeat::Stop();
        s.Auth = Auth::AuthState();
        s.Cfg.Token.clear();
        Config::Save(s.Cfg);
        s.User[0] = s.Pass[0] = 0;
        s.PrevState = s.State;
        s.State = AppState::Login;
        s.PageAnim.val = 0.0f;
        s.PageAnim.Set(1.0f);
    }
    
    ImGui::EndGroup();
}

static void RenderFooter(UIState& s, ImDrawList* dl, float anim)
{
    ImGui::PushFont(s.FontReg);
    const char* txt = "made with <3 by Soup & ueh";
    ImVec2 sz = ImGui::CalcTextSize(txt);
    dl->AddText(ImVec2((W - sz.x) * 0.5f, H - 28), 
        IM_COL32(70, 70, 85, (int)(255 * anim)), txt);
    ImGui::PopFont();
}

void Run(UIState& s)
{
    DownloadResources(s);
    LoadFontsAndLogo(s);
    
    Config::Load(s.Cfg);
    s.Remember = s.Cfg.Remember;
    if (!s.Cfg.Username.empty()) strncpy_s(s.User, s.Cfg.Username.c_str(), sizeof(s.User) - 1);
    
    if (s.Cfg.Remember && !s.Cfg.Token.empty()) {
        s.LoadingText = "Валидация сессии...";
        s.Auth = Auth::ValidateToken(s.Cfg.Token);
        if (s.Auth.HasValidSession && s.Auth.SubscriptionActive) s.State = AppState::Main;
        else s.State = AppState::Login;
    } else {
        s.State = AppState::Login;
    }
    
    s.LoadingText.clear();
    s.ResourcesReady = true;
    
    s.WindowAnim.val = 0.0f;
    s.WindowAnim.Set(1.0f);
    s.FadeAnim.val = 0.0f;
    s.FadeAnim.Set(1.0f);
    s.HeaderAnim.val = 0.0f;
    s.HeaderAnim.Set(1.0f);
    s.PageAnim.val = 0.0f;
    s.PageAnim.Set(1.0f);
    
    SDL_ShowWindow(s.Window);

    while (s.Running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) {
                s.Closing = true;
                s.WindowAnim.Set(0.0f);
            }
        }

        auto now = std::chrono::steady_clock::now();
        s.Delta = std::chrono::duration<float>(now - s.LastFrame).count();
        s.LastFrame = now;
        s.Time += s.Delta;
        
        UpdateAnims(s);
        
        if (s.Closing && s.WindowAnim.Get() < 0.01f) {
            s.Running = false;
            continue;
        }

        float winAnim = EaseInOut(s.WindowAnim.Get());
        SDL_SetWindowOpacity(s.Window, winAnim);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        bg->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), IM_COL32(13, 13, 17, 255));
        
        float accentPulse = 0.8f + 0.2f * sinf(s.Time * 2.0f);
        bg->AddRectFilled(ImVec2(0, 0), ImVec2(W, 2), 
            IM_COL32(34, (int)(211 * accentPulse), (int)(238 * accentPulse), 255));

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(W, H));
        ImGui::Begin("##m", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        float pageAnim = EaseOut(s.PageAnim.Get());
        
        RenderHeader(s, dl);
        if (s.State == AppState::Login) RenderLogin(s, pageAnim);
        else if (s.State == AppState::Main) RenderMain(s, pageAnim);
        RenderFooter(s, dl, s.FadeAnim.Get());
        RenderToasts(s, dl);
        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, (int)W, (int)H);
        glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(s.Window);
    }
}

void Shutdown(UIState& s)
{
    Heartbeat::Stop();
    if (s.LogoTex) glDeleteTextures(1, &s.LogoTex);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    if (s.GLContext) SDL_GL_DeleteContext(s.GLContext);
    if (s.Window) SDL_DestroyWindow(s.Window);
    SDL_Quit();
}

}