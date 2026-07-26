#include "demo.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <windowsx.h>
#include <shellapi.h>
#include <psapi.h>
#include <cstdio>
#include <algorithm>

DemoApp* DemoApp::s_inst = nullptr;

// ============================================================================
// Helpers
// ============================================================================
static constexpr ImVec4 kAccent(0.4f, 0.7f, 1.0f, 1.0f);
static constexpr ImVec4 kSubtle(0.6f, 0.6f, 0.6f, 1.0f);
static constexpr ImVec4 kOk(0.3f, 0.9f, 0.3f, 1.0f);
static constexpr ImVec4 kHint(0.5f, 0.5f, 0.5f, 0.7f);

struct BgPreset { const char* name; ImVec4 color; };
static constexpr BgPreset kPresets[] = {
    {"Red",  ImVec4(1.00f,0.71f,0.68f,1)}, {"Pink", ImVec4(1.00f,0.70f,0.75f,1)},
    {"Prpl",ImVec4(0.92f,0.71f,0.93f,1)}, {"DkPr",ImVec4(0.83f,0.74f,0.99f,1)},
    {"Indg",ImVec4(0.73f,0.76f,1.00f,1)}, {"Blue",ImVec4(0.63f,0.79f,0.99f,1)},
    {"Cyan",ImVec4(0.51f,0.83f,0.89f,1)}, {"Yell",ImVec4(0.83f,0.78f,0.44f,1)},
    {"Sky", ImVec4(0.56f,0.81f,0.95f,1)}, {"Sakr",ImVec4(1.00f,0.70f,0.73f,1)},
};

static const char* GetOSName() {
    using Fn = LONG (WINAPI*)(PRTL_OSVERSIONINFOW);
    auto f = (Fn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    RTL_OSVERSIONINFOW os = {sizeof(os)};
    if (f) f(&os);
    return os.dwBuildNumber >= 22000 ? "Windows 11" : "Windows 10";
}

void DemoApp::PrintSystemInfo() {
    MEMORYSTATUSEX m = {sizeof(m)};
    GlobalMemoryStatusEx(&m);
    printf("  RAM: %.1f/%.1f GB (%.0f%%)\n",
        (m.ullTotalPhys-m.ullAvailPhys)/1073741824.0, m.ullTotalPhys/1073741824.0,
        (1.0-(double)m.ullAvailPhys/m.ullTotalPhys)*100.0);
}

void DemoApp::PrintProcessInfo() {
    PROCESS_MEMORY_COUNTERS pmc = {sizeof(pmc)};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        printf("  WS: %.1f MB  Peak: %.1f MB  PF: %.1f MB\n",
            pmc.WorkingSetSize/1048576.0, pmc.PeakWorkingSetSize/1048576.0,
            pmc.PagefileUsage/1048576.0);
}

// ============================================================================
// Window
// ============================================================================
bool DemoApp::Init(HINSTANCE hInst, int nShow) {
    s_inst = this;
    AllocConsole();
    FILE* d; freopen_s(&d, "CONOUT$", "w", stdout);
    freopen_s(&d, "CONOUT$", "w", stderr);

    printf("========================================\n");
    printf("  Windows Liquid Glass - Demo\n");
    printf("  OS: %s\n", GetOSName());
    printf("  Screen: %dx%d  CPU: %lu cores\n",
        GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    PrintSystemInfo();
    printf("----------------------------------------\n");

    // Window
    const wchar_t* CLS = L"WindowsLiquidGlass";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc); wc.style = CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
    wc.hIcon = nullptr;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = CLS;
    RegisterClassExW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    m_w = (int)(sw*0.80f); m_h = (int)(sh*0.82f);
    m_hwnd = CreateWindowExW(0, CLS, L"Windows Liquid Glass",
        WS_OVERLAPPEDWINDOW, (sw-m_w)/2, (sh-m_h)/2, m_w, m_h,
        nullptr, nullptr, hInst, nullptr);
    if (!m_hwnd) return false;


    // Renderer
    printf("Init renderer...\n");
    if (!m_renderer.Init(m_hwnd, m_w, m_h)) { printf("ERROR\n"); return false; }

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().Fonts->AddFontDefault();
    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_renderer.GetDevice(), m_renderer.GetContext());
    m_imguiInit = true;

    Layout();
    ShowWindow(m_hwnd, nShow); UpdateWindow(m_hwnd);
    printf("========================================\n\n");
    return true;
}

void DemoApp::Layout() {
    m_cardSize = std::min(220.0f, std::min((float)m_w, (float)m_h)*0.30f);
    m_cardX = ((float)m_w - m_cardSize)*0.5f;
    m_cardY = ((float)m_h - m_cardSize)*0.42f;
    m_cfg = LiquidGlass::GlassConfig{};
}

// ============================================================================
// WndProc
// ============================================================================
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT DemoApp::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = s_inst;
    if (!self) return DefWindowProc(hwnd, msg, wp, lp);
    if (!self->m_hwnd && msg == WM_NCCREATE) self->m_hwnd = hwnd;
    if (self->m_imguiInit && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;
    return self->MsgHandler(msg, wp, lp);
}

LRESULT DemoApp::MsgHandler(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: DragAcceptFiles(m_hwnd, TRUE); return 0;
    case WM_SIZE:
        m_w = LOWORD(lp); m_h = HIWORD(lp);
        if (m_w > 0 && m_h > 0 && m_renderer.Width() > 0) {
            m_renderer.Resize(m_w, m_h); Layout();
        }
        return 0;
    case WM_MOUSEMOVE:
        m_mx = GET_X_LPARAM(lp); m_my = GET_Y_LPARAM(lp);
        if (m_dragging) { m_cardX = m_dragCX + (m_mx - m_dragSX); m_cardY = m_dragCY + (m_my - m_dragSY); }
        return 0;
    case WM_MOUSEWHEEL:
        if (!m_imguiInit || ImGui::GetIO().WantCaptureMouse) return 0;
        { float d = (float)GET_WHEEL_DELTA_WPARAM(wp)/120.0f;
          float cx = m_cardX + m_cardSize*0.5f, cy = m_cardY + m_cardSize*0.5f;
          m_cardSize = std::max(60.0f, std::min(400.0f, m_cardSize + d*15.0f));
          m_cardX = cx - m_cardSize*0.5f; m_cardY = cy - m_cardSize*0.5f; }
        return 0;
    case WM_LBUTTONDOWN:
        if (m_imguiInit && ImGui::GetIO().WantCaptureMouse) return 0;
        { int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
          if (mx >= m_cardX && mx <= m_cardX+m_cardSize && my >= m_cardY && my <= m_cardY+m_cardSize) {
              m_dragging = true; m_dragSX = (float)mx; m_dragSY = (float)my;
              m_dragCX = m_cardX; m_dragCY = m_cardY; SetCapture(m_hwnd);
          }}
        return 0;
    case WM_LBUTTONUP: m_dragging = false; ReleaseCapture(); return 0;
    case WM_DROPFILES:
        { HDROP h = (HDROP)wp; wchar_t p[MAX_PATH];
          if (DragQueryFileW(h, 0, p, MAX_PATH) > 0) m_renderer.LoadBackgroundImage(p);
          DragFinish(h); }
        return 0;
    case WM_CLOSE: printf("\n[Shutdown] Closing...\n"); DestroyWindow(m_hwnd); return 0;
    case WM_DESTROY: printf("[Shutdown] Destroying\n"); PostQuitMessage(0); return 0;
    case WM_KEYDOWN: if (wp == VK_ESCAPE) PostQuitMessage(0); return 0;
    }
    return DefWindowProc(m_hwnd, msg, wp, lp);
}

// ============================================================================
// UI
// ============================================================================
void DemoApp::DrawUI() {
    ImGui::SetNextWindowPos(ImVec2((float)m_w-320,10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300,620), ImGuiCond_FirstUseEver);
    ImGui::Begin("Control Panel", nullptr);

    ImGui::TextColored(kAccent, "Windows Liquid Glass");
    ImGui::TextColored(kSubtle, "%s", GetOSName());
    ImGui::Spacing();

    ImGui::Text("Size: %.0f", m_cardSize); ImGui::SameLine();
    if (ImGui::Button("Reset")) Layout();
    ImGui::TextColored(kHint, "Mouse wheel = resize");

    ImGui::Separator();
    ImGui::SliderFloat("Blur", &m_cfg.blurSigma, 0.1f, 30.f, "%.1f");
    ImGui::SliderFloat("Refraction", &m_cfg.refractionAmount, 4.f, 120.f, "%.0f");
    ImGui::SliderFloat("Refr Height", &m_cfg.refractionHeight, 4.f, 60.f, "%.0f");
    ImGui::SliderFloat("Corner Radius", &m_cfg.cornerRadius, 0.f, 80.f, "%.0f");
    ImGui::SliderFloat("Saturation", &m_cfg.saturation, 1.f, 2.f, "%.2f");
    ImGui::Separator();
    ImGui::SliderFloat("Dispersion", &m_cfg.dispersion, 0.f, 1.f, "%.2f");
    ImGui::SameLine();
    ImGui::Checkbox("Depth Effect", &m_cfg.depthEffect);
    // Highlight slider removed (feature not yet re-implemented)
    ImGui::Separator();

    ImGui::Text("Background:");
    for (int i = 0; i < 10; i++) {
        if (i%5) ImGui::SameLine();
        ImGui::PushID(i);
        if (ImGui::ColorButton(kPresets[i].name, kPresets[i].color,
                ImGuiColorEditFlags_NoTooltip, ImVec2(24,24)))
            m_renderer.SetBackgroundColor(kPresets[i].color.x, kPresets[i].color.y, kPresets[i].color.z);
            printf("BG color: %s\n", kPresets[i].name);
        ImGui::PopID();
    }
    if (ImGui::Button("White", ImVec2(-1,0))) m_renderer.SetBackgroundColor(1,1,1);
    if (ImGui::Button("Open Image...", ImVec2(-1,0))) {
        wchar_t p[MAX_PATH]={}; OPENFILENAMEW o={sizeof(o),m_hwnd}; o.lpstrFile=p; o.nMaxFile=MAX_PATH;
        o.lpstrFilter=L"Images\0*.jpg;*.jpeg;*.png;*.bmp\0All\0*.*\0"; o.Flags=OFN_FILEMUSTEXIST;
        if (GetOpenFileNameW(&o) && m_renderer.LoadBackgroundImage(p))
            printf("Image: %ls\n", p);
    }
    ImGui::Separator();

    if (ImGui::Button(m_consoleVisible?"Hide Console":"Show Console", ImVec2(-1,0))) {
        m_consoleVisible = !m_consoleVisible;
        ShowWindow(GetConsoleWindow(), m_consoleVisible ? SW_SHOW : SW_HIDE);
    }
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::End();
}

// ============================================================================
// Render & Run
// ============================================================================
void DemoApp::RenderFrame() {
    m_renderer.RenderGlass(m_cardX, m_cardY, m_cardSize, m_cardSize, m_cfg);
}

int DemoApp::Run() {
    printf("[Run] Starting message loop\n");
    auto prev = std::chrono::high_resolution_clock::now();
    auto fpsT = prev;
    MSG msg = {};

    while (true) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                printf("\n========================================\n");
                printf("  Exiting (code=%d)  Frames: %d\n", (int)msg.wParam, m_frameCount);
                PrintSystemInfo(); PrintProcessInfo();
                printf("  Stack: "); void* s[8];
                for (USHORT i=0,n=CaptureStackBackTrace(0,8,s,nullptr);i<n;i++) printf("%p ",s[i]);
                printf("\n========================================\n");
                if (m_consoleVisible) {
                    printf("Press Enter to exit...\n"); fflush(stdout);
                    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
                    FlushConsoleInputBuffer(h);
                    char b[4]; DWORD r; ReadConsoleA(h, b, 3, &r, nullptr);
                }
                return (int)msg.wParam;
            }
            TranslateMessage(&msg); DispatchMessage(&msg);
        }

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::min(0.1f, std::chrono::duration<float>(now-prev).count());
        prev = now;

        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame(); DrawUI();

        m_renderer.BeginFrame();
        RenderFrame();
        m_renderer.DumpDebugMessages();
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        m_renderer.EndFrame();

        m_frameCount++;
        if (std::chrono::duration<float>(now-fpsT).count() >= 1.0f) {
            wchar_t t[128]; swprintf_s(t, L"Windows Liquid Glass | FPS:%.0f", ImGui::GetIO().Framerate);
            SetWindowTextW(m_hwnd, t); fpsT = now;
        }
    }
}

void DemoApp::Shutdown() {
    printf("[Shutdown] Cleanup\n");
    if (m_imguiInit) { ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext(); }
    if (m_hwnd) { DestroyWindow(m_hwnd); m_hwnd = nullptr; }
}
