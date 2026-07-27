// Windows Liquid Glass -- ImGui Controls Demo (D3D11 + Dear ImGui)
#ifndef UNICODE
#define UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "LiquidGlass.h"
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <psapi.h>
#include <cstdio>
#include <chrono>
#include <thread>
#include <algorithm>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// ============================================================================
// Logging �� outputs to both console (wprintf) and VS debug window (OutputDebugStringW)
// ============================================================================
#define APP_LOG(fmt, ...) do { \
    wchar_t _b[512]; swprintf_s(_b, L"[APP] " fmt L"\n", ##__VA_ARGS__); \
    wprintf(L"%s", _b); OutputDebugStringW(_b); \
} while(0)
#define APP_WARN(fmt, ...) do { \
    wchar_t _b[512]; swprintf_s(_b, L"[APP] WARN: " fmt L"\n", ##__VA_ARGS__); \
    wprintf(L"%s", _b); OutputDebugStringW(_b); \
} while(0)
#define APP_ERR(fmt, ...) do { \
    wchar_t _b[512]; swprintf_s(_b, L"[APP] ERROR: " fmt L"\n", ##__VA_ARGS__); \
    wprintf(L"%s", _b); OutputDebugStringW(_b); \
    MessageBoxW(nullptr, _b, L"Windows Liquid Glass - Error", MB_ICONERROR); \
} while(0)

// ============================================================================
// Console control handler — prevent Ctrl+C / close from killing the process
// ============================================================================
static BOOL WINAPI ConsoleCtrlHandler(DWORD dwType) {
    switch (dwType) {
    case CTRL_C_EVENT:
        return TRUE; // ignore Ctrl+C
    case CTRL_CLOSE_EVENT:
        wprintf(L"\nWhen the debug window shows up, you should hide it from the\n"
                L"\"Hide Console\" option in the Control Panel!\n");
        return TRUE;
    default:
        return FALSE;
    }
}

// ============================================================================
// Globals
// ============================================================================
static LiquidGlass::Renderer     gR;
static LiquidGlass::GlassConfig  gCfg;
static float    gGX, gGY, gGW, gGH;    // glass position & size
static HWND     gMainWnd;
static int      gW, gH, gMx, gMy;
static bool     gDrag, gConVis = true;
static float    gDsx, gDsy, gDcx, gDcy;

static const float kColors[10][3] = {
    {1,.71f,.68f}, {1,.70f,.75f}, {.92f,.71f,.93f}, {.83f,.74f,.99f}, {.73f,.76f,1},
    {.63f,.79f,.99f}, {.51f,.83f,.89f}, {.83f,.78f,.44f}, {.56f,.81f,.95f}, {1,.70f,.73f}
};
static const char* kNames[10] = {
    "Red", "Pink", "Purple", "DkPurple", "Indigo",
    "Blue", "Cyan", "Yellow", "Sky", "Sakura"
};

// ============================================================================
// System info
// ============================================================================
static void PrintSystemInfo() {
    // OS
    using RtlGetVersionFn = LONG (WINAPI*)(PRTL_OSVERSIONINFOW);
    auto RtlGetVersion = (RtlGetVersionFn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    RTL_OSVERSIONINFOW os = {sizeof(os)};
    if (RtlGetVersion) RtlGetVersion(&os);
    const wchar_t* osName = os.dwBuildNumber >= 22000 ? L"Windows 11" : L"Windows 10";
    APP_LOG(L"OS: %s (Build %lu.%lu.%lu)", osName, os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber);

    // CPU
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const wchar_t* archNames[] = {L"x86",L"",L"",L"",L"",L"",L"",L"",L"AMD64",L"ARM64"};
    const wchar_t* archName = (si.wProcessorArchitecture <= 12) ? archNames[si.wProcessorArchitecture] : L"???";
    if(!archName||!archName[0])archName=L"???";
    APP_LOG(L"CPU: %lu logical cores, arch=%s", si.dwNumberOfProcessors, archName);

    // RAM
    MEMORYSTATUSEX ms = {sizeof(ms)};
    GlobalMemoryStatusEx(&ms);
    APP_LOG(L"RAM: %.1f GB used / %.1f GB total",
        (ms.ullTotalPhys-ms.ullAvailPhys)/1073741824.0,
        ms.ullTotalPhys/1073741824.0);

    // Screen
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    APP_LOG(L"Screen: %dx%d @ %d DPI", sw, sh, GetDpiForSystem());

    // Process
    APP_LOG(L"Process ID: %lu", GetCurrentProcessId());
    SetProcessDPIAware();
}

// ============================================================================
// ImGui DrawUI
// ============================================================================
static void DrawUI() {
    ImGui::SetNextWindowPos(ImVec2((float)gW - 360, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 640), ImGuiCond_FirstUseEver);
    ImGui::Begin("Control Panel", nullptr);

    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Windows Liquid Glass");
    ImGui::Spacing();

    ImGui::Text("Size: %.0f", gGW); ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        gGW = gGH = 220;
        gGX = (gW - 220.0f) * 0.5f; gGY = (gH - 220.0f) * 0.42f;
        APP_LOG(L"Reset pos=(%.0f,%.0f) size=%.0f", gGX, gGY, gGW);
    }
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.7f), "Mouse wheel = resize | Drag = move");

    ImGui::Separator();
    ImGui::SliderFloat("Blur", &gCfg.blurSigma, 0.1f, 30.f, "%.1f");
    static int refrMode = 0;
    ImGui::Combo("Refr Mode", &refrMode, "Correct (Convex)\0Negative (Concave)\0");
    if (refrMode == 0) {
        ImGui::SliderFloat("Refr Amount", &gCfg.refractionCorrect, 0.00f, 0.30f, "%.2f");
        gCfg.refractionNegative = 0.00f;
    } else {
        ImGui::SliderFloat("Refr Amount", &gCfg.refractionNegative, 0.00f, 0.30f, "%.2f");
        gCfg.refractionCorrect = 0.00f;
    }
    ImGui::SliderFloat("Refr Height", &gCfg.refractionHeight, 0.00f, 0.30f, "%.2f");
    ImGui::SliderFloat("Corner Radius", &gCfg.cornerRadius, 0.f, 80.f, "%.0f");
    ImGui::SliderFloat("Saturation", &gCfg.saturation, 1.f, 2.f, "%.2f");
    ImGui::SliderFloat("Dispersion", &gCfg.dispersion, 0.f, 1.f, "%.2f");
    ImGui::SliderFloat("Highlight", &gCfg.highlightAlpha, 0.f, 0.5f, "%.2f");
    ImGui::SliderFloat("Darkening", &gCfg.darkening, 0.50f, 1.00f, "%.2f");
    ImGui::SliderFloat("Shadow Alpha", &gCfg.shadowAlpha, 0.00f, 0.35f, "%.2f");
    ImGui::Checkbox("Depth Effect", &gCfg.depthEffect);

    ImGui::Separator();
    ImGui::Text("Glass Tint:");
    for (int i = 0; i < 10; i++) {
        if (i % 5) ImGui::SameLine();
        ImGui::PushID(i);
        float r = kColors[i][0], g = kColors[i][1], b = kColors[i][2];
        ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoTooltip;
        if (ImGui::ColorButton(kNames[i], ImVec4(r, g, b, 1), flags, ImVec2(24, 24))) {
            gR.GlassTint(r, g, b, 0.6f);
            gCfg.glassTintR = r; gCfg.glassTintG = g; gCfg.glassTintB = b; gCfg.glassTintA = 0.6f;
            APP_LOG(L"Glass tint: %S (%.2f,%.2f,%.2f)", kNames[i], r, g, b);
        }
        ImGui::PopID();
    }
    if (ImGui::Button("Clear Tint", ImVec2(-1, 0))) {
        gR.GlassTint(1, 1, 1, 0);
        gCfg.glassTintR = 1; gCfg.glassTintG = 1; gCfg.glassTintB = 1; gCfg.glassTintA = 0;
        APP_LOG(L"Glass tint: cleared");
    }
    if (ImGui::Button("Open Image...", ImVec2(-1, 0))) {
        wchar_t path[MAX_PATH] = {};
        OPENFILENAMEW ofn = {sizeof(ofn), gMainWnd};
        ofn.lpstrFile = path; ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = L"Images\0*.jpg;*.jpeg;*.png;*.bmp\0All\0*.*\0";
        ofn.Flags = OFN_FILEMUSTEXIST;
        if (GetOpenFileNameW(&ofn)) {
            APP_LOG(L"LoadBackgroundImage: %s", path);
            bool ok = gR.LoadBackgroundImage(path);
            APP_LOG(L"Result: %s", ok ? L"OK" : L"FAILED");
        }
    }
    ImGui::Separator();

    if (ImGui::Button(gConVis ? "Hide Console" : "Show Console", ImVec2(-1, 0))) {
        gConVis = !gConVis;
        ShowWindow(GetConsoleWindow(), gConVis ? SW_SHOW : SW_HIDE);
        APP_LOG(L"Console %s", gConVis ? L"shown" : L"hidden");
    }
    ImGui::Separator();
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Visit GitHub: https://github.com/JetComX/WindowsLiquidGlass");
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsItemClicked()) {
        ShellExecuteW(nullptr, L"open", L"https://github.com/JetComX/WindowsLiquidGlass", nullptr, nullptr, SW_SHOW);
    }
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "If you think this project is pretty good,");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "please give it a Star on GitHub :D");
    ImGui::End();
}

// ============================================================================
// Main window procedure
// ============================================================================
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;
    switch (msg) {
    case WM_CREATE:
        APP_LOG(L"MainWnd WM_CREATE hwnd=%p", hwnd);
        DragAcceptFiles(hwnd, TRUE);
        return 0;

    case WM_SIZE:
        gW = LOWORD(lp); gH = HIWORD(lp);
        if (gW > 0 && gH > 0) APP_LOG(L"WM_SIZE %dx%d", gW, gH);
        if (gW > 0 && gH > 0 && gR.Width() > 0) gR.Resize(gW, gH);
        return 0;

    case WM_MOUSEMOVE:
        gMx = GET_X_LPARAM(lp); gMy = GET_Y_LPARAM(lp);
        if (gDrag) { gGX = gDcx + (gMx - gDsx); gGY = gDcy + (gMy - gDsy); }
        return 0;

    case WM_MOUSEWHEEL:
        if (ImGui::GetIO().WantCaptureMouse) return 0;
        { float d = (float)GET_WHEEL_DELTA_WPARAM(wp) / 120.0f;
        float cx = gGX + gGW * 0.5f, cy = gGY + gGH * 0.5f;
        gGW = gGH = std::max(60.0f, std::min(400.0f, gGW + d * 15.0f));
        gGX = cx - gGW * 0.5f; gGY = cy - gGH * 0.5f;
        return 0;
    }

    case WM_LBUTTONDOWN:
        if (ImGui::GetIO().WantCaptureMouse) return 0;
        { int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        bool hit = (mx >= gGX && mx <= gGX + gGW && my >= gGY && my <= gGY + gGH);
        if (hit) {
            APP_LOG(L"Glass drag START pos=(%.0f,%.0f) size=%.0f mouse=(%d,%d)", gGX, gGY, gGW, mx, my);
            gDrag = true; gDsx = (float)mx; gDsy = (float)my;
            gDcx = gGX; gDcy = gGY; SetCapture(hwnd);
        }
        return 0;
    }

    case WM_LBUTTONUP:
        if (gDrag) APP_LOG(L"Glass drag END pos=(%.0f,%.0f)", gGX, gGY);
        gDrag = false; ReleaseCapture(); return 0;

    case WM_DROPFILES: {
        HDROP drop = (HDROP)wp; wchar_t path[MAX_PATH];
        if (DragQueryFileW(drop, 0, path, MAX_PATH) > 0) {
            APP_LOG(L"Drop file: %s", path);
            bool ok = gR.LoadBackgroundImage(path);
            APP_LOG(L"Drop result: %s", ok ? L"OK" : L"FAILED");
        }
        DragFinish(drop);
        return 0;
    }

    case WM_KEYDOWN: if (wp == VK_ESCAPE) { APP_LOG(L"ESC -> quit"); PostQuitMessage(0); } return 0;
    case WM_CLOSE:   APP_LOG(L"MainWnd WM_CLOSE"); DestroyWindow(hwnd); return 0;
    case WM_DESTROY: APP_LOG(L"MainWnd WM_DESTROY -> PostQuitMessage"); PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ============================================================================
// WinMain
// ============================================================================
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nShow) {
    // Earliest possible trace (before any init)
    OutputDebugStringW(L"[APP] wWinMain entry\n");

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        wchar_t buf[128]; swprintf_s(buf, L"CoInitializeEx failed: 0x%08X", hr);
        OutputDebugStringW(buf); MessageBoxW(nullptr, buf, L"Fatal Error", MB_ICONERROR);
        return 1;
    }

    if (!AllocConsole()) {
        DWORD err = GetLastError();
        wchar_t buf[128]; swprintf_s(buf, L"AllocConsole failed: error=%lu", err);
        OutputDebugStringW(buf);
        // Non-fatal: OutputDebugString still works
    }
    FILE* f = nullptr;
    errno_t e1 = freopen_s(&f, "CONOUT$", "w", stdout);
    errno_t e2 = freopen_s(&f, "CONOUT$", "w", stderr);
    if (e1 || e2) {
        wchar_t buf[128]; swprintf_s(buf, L"freopen_s failed: e1=%d e2=%d", e1, e2);
        OutputDebugStringW(buf);
    }

    APP_LOG(L"========================================");
    APP_LOG(L"  Windows Liquid Glass - Starting");
    APP_LOG(L"========================================");
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    PrintSystemInfo();
    // Disable console close button (CTRL_CLOSE_EVENT kills process on Win10+ regardless of handler)
    { HWND hc=GetConsoleWindow(); if(hc){ HMENU hm=GetSystemMenu(hc,FALSE); DeleteMenu(hm,SC_CLOSE,MF_BYCOMMAND); } }
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    gConVis = false;

    // Main window
    WNDCLASSEXW wc = {sizeof(wc), CS_HREDRAW | CS_VREDRAW, MainWndProc, 0, 0, hInst,
        nullptr, LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1),
        nullptr, L"WLGMain", nullptr};
    if (!RegisterClassExW(&wc)) { APP_ERR(L"RegisterClass MainWnd FAILED"); return 1; }
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    gW = (int)(sw * 0.80f); gH = (int)(sh * 0.82f);
    APP_LOG(L"Window size: %dx%d", gW, gH);

    gMainWnd = CreateWindowExW(0, L"WLGMain", L"Windows Liquid Glass",
        WS_OVERLAPPEDWINDOW, (sw - gW) / 2, (sh - gH) / 2, gW, gH,
        nullptr, nullptr, hInst, nullptr);
    if (!gMainWnd) { APP_ERR(L"CreateWindow MainWnd FAILED"); return 1; }
    APP_LOG(L"MainWnd created: hwnd=%p", gMainWnd);


    // Renderer
    APP_LOG(L"Initializing renderer...");
    if (!gR.Init(gMainWnd, gW, gH)) { APP_ERR(L"Renderer Init FAILED"); return 1; }
    APP_LOG(L"Renderer initialized OK");

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplWin32_Init(gMainWnd);
    ImGui_ImplDX11_Init(gR.GetDevice(), gR.GetContext());
    APP_LOG(L"ImGui initialized OK");

    // Default background image
    APP_LOG(L"Loading default background...");
    if (!gR.LoadBackgroundImage(L"examples/demo/Windows11.png")) {
        APP_WARN(L"Default image not found, using white background");
        gR.SetBackgroundColor(1, 1, 1);
    }

    gGW = gGH = 220;
    gGX = (gW - 220.0f) * 0.5f;
    gGY = (gH - 220.0f) * 0.42f;
    APP_LOG(L"Glass initial: pos=(%.0f,%.0f) size=%.0f", gGX, gGY, gGW);
    APP_LOG(L"GlassConfig: blur=%.1f refrCorrect=%.2f refrNegative=%.2f refrH=%.2f r=%.0f sat=%.2f disp=%.2f depth=%d",
        gCfg.blurSigma, gCfg.refractionCorrect, gCfg.refractionNegative, gCfg.refractionHeight, gCfg.cornerRadius,
        gCfg.saturation, gCfg.dispersion, gCfg.depthEffect);

    ShowWindow(gMainWnd, nShow);
    UpdateWindow(gMainWnd);
    APP_LOG(L"========================================");
    APP_LOG(L"  Entering main loop");
    APP_LOG(L"========================================");

    auto fpsT = std::chrono::high_resolution_clock::now();
    int frames = 0;
    MSG msg;
    auto constexpr frameTarget = std::chrono::microseconds(16667); // 60 FPS
    auto lastFrame = std::chrono::high_resolution_clock::now();

    while (true) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                APP_LOG(L"WM_QUIT received after %d frames", frames);
                goto done;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        auto now = std::chrono::high_resolution_clock::now();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        DrawUI();

        gCfg.highlightMouseX = (float)gMx;
        gCfg.highlightMouseY = (float)gMy;

        gR.BeginFrame();
        gR.RenderGlass(gGX, gGY, gGW, gGH, gCfg);
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        gR.EndFrame();

        frames++;
        if (std::chrono::duration<float>(now-fpsT).count() >= 1.0f) {
            float fps = (float)frames / std::chrono::duration<float>(now - fpsT).count();
            wchar_t title[128];
            swprintf_s(title, L"WLG | %.0f FPS", fps);
            SetWindowTextW(gMainWnd, title);
            frames = 0;
            fpsT = now;
        }
        auto now2 = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now2 - lastFrame);
        if (elapsed < frameTarget) std::this_thread::sleep_for(frameTarget - elapsed);
        lastFrame = std::chrono::high_resolution_clock::now();
    }

done:
    APP_LOG(L"========================================");
    APP_LOG(L"  Shutting down... (%d frames)", frames);
    // Memory usage
    PROCESS_MEMORY_COUNTERS pmc={sizeof(pmc)};
    if(GetProcessMemoryInfo(GetCurrentProcess(),&pmc,sizeof(pmc)))
        APP_LOG(L"  Memory: WS=%.1fMB Peak=%.1fMB",
            pmc.WorkingSetSize/1048576.0,pmc.PeakWorkingSetSize/1048576.0);
    // Stack trace
    APP_LOG(L"  Stack trace:");
    void* stack[16]; USHORT n=CaptureStackBackTrace(0,16,stack,nullptr);
    for(USHORT i=0;i<n;i++){wchar_t b[32];swprintf_s(b,L"  [%d] %p",i,stack[i]);OutputDebugStringW(b);}
    APP_LOG(L"  %d frames captured",n);
    APP_LOG(L"========================================");

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    gR.Shutdown();
    CoUninitialize();

    if (gConVis) {
        wprintf(L"\nPress Enter to exit...\n");
        fflush(stdout);
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        FlushConsoleInputBuffer(hIn);
        char buf[4]; DWORD read;
        ReadConsoleA(hIn, buf, 3, &read, nullptr);
    }
    return 0;
}
