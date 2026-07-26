// Windows Liquid Glass — control window + D3D11 glass window
#ifndef UNICODE
#define UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "LiquidGlass.h"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <cstdio>
#include <chrono>
#include <algorithm>
#pragma comment(lib,"comctl32.lib")

// ============================================================================
// Logging — outputs to both console (wprintf) and VS debug window (OutputDebugStringW)
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
// Control IDs
// ============================================================================
enum CID {
    ID_BLUR=100, ID_REFR, ID_REFRH, ID_RADIUS, ID_SAT,
    ID_DISP, ID_DEPTH, ID_WHITE, ID_IMAGE, ID_CONSOLE, ID_RESET,
    ID_COLOR0, ID_COLOR9=ID_COLOR0+9
};
static const wchar_t* kIdNames[] = {
    nullptr, nullptr, nullptr, nullptr, nullptr,  // 0-4 unused
    nullptr, nullptr, nullptr, nullptr, nullptr,  // 5-9 unused
    // ... up to 100
};
// Initialize on first use
static const wchar_t* GetIdName(int id) {
    switch (id) {
    case ID_BLUR:   return L"BLUR";
    case ID_REFR:   return L"REFR";
    case ID_REFRH:  return L"REFRH";
    case ID_RADIUS: return L"RADIUS";
    case ID_SAT:    return L"SAT";
    case ID_DISP:   return L"DISP";
    case ID_DEPTH:  return L"DEPTH";
    case ID_WHITE:  return L"WHITE";
    case ID_IMAGE:  return L"IMAGE";
    case ID_CONSOLE:return L"CONSOLE";
    case ID_RESET:  return L"RESET";
    default:
        if (id>=ID_COLOR0 && id<=ID_COLOR9) {
            static wchar_t buf[16];
            swprintf_s(buf, L"COLOR%d", id-ID_COLOR0);
            return buf;
        }
        return L"???";
    }
}

// ============================================================================
// Globals
// ============================================================================
static LiquidGlass::Renderer     gR;
static LiquidGlass::GlassConfig  gCfg;
static float    gGX, gGY, gGW, gGH;    // glass position & size
static HWND     gMainWnd, gCtrlWnd;
static HWND     gBlur, gRefr, gRefrH, gRadius, gSat, gDisp, gDepth;
static HWND     gValBlur, gValRefr, gValRefrH, gValRadius, gValSat, gValDisp;
static HWND     gWhite, gImg, gConsoleBtn, gReset;
static HWND     gColorBtns[10];
static int      gW, gH, gMx, gMy;
static bool     gDrag, gConVis = true;
static float    gDsx, gDsy, gDcx, gDcy;

static const float kColors[10][3] = {
    {1,.71f,.68f}, {1,.70f,.75f}, {.92f,.71f,.93f}, {.83f,.74f,.99f}, {.73f,.76f,1},
    {.63f,.79f,.99f}, {.51f,.83f,.89f}, {.83f,.78f,.44f}, {.56f,.81f,.95f}, {1,.70f,.73f}
};
static const wchar_t* kNames[10] = {
    L"Red", L"Pink", L"Purple", L"DkPurple", L"Indigo",
    L"Blue", L"Cyan", L"Yellow", L"Sky", L"Sakura"
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
// Helpers
// ============================================================================
static float GetTB(HWND tb, float mn, float mx) {
    int p = (int)SendMessageW(tb, TBM_GETPOS, 0, 0);
    return mn + (mx - mn) * p / 1000.0f;
}
static void SetTB(HWND tb, float v, float mn, float mx) {
    SendMessageW(tb, TBM_SETPOS, TRUE, (LPARAM)((v - mn) / (mx - mn) * 1000.0f));
}
static void SetVal(HWND lbl, float v, const wchar_t* fmt = L"%.1f") { wchar_t b[16]; swprintf_s(b, fmt, v); SetWindowTextW(lbl, b); }
static void SetValI(HWND lbl, float v) { wchar_t b[16]; swprintf_s(b, L"%.0f", v); SetWindowTextW(lbl, b); }

static void UpdUI() {
    SetTB(gBlur,    gCfg.blurSigma,         0.1f, 30.0f);  SetVal(gValBlur,   gCfg.blurSigma);
    SetTB(gRefr,    gCfg.refractionAmount,  4.0f,  120.0f); SetValI(gValRefr,  gCfg.refractionAmount);
    SetTB(gRefrH,   gCfg.refractionHeight,  4.0f,  60.0f);  SetValI(gValRefrH, gCfg.refractionHeight);
    SetTB(gRadius,  gCfg.cornerRadius,      0.0f,  80.0f);  SetValI(gValRadius,gCfg.cornerRadius);
    SetTB(gSat,     gCfg.saturation,        1.0f,  2.0f);   SetVal(gValSat,    gCfg.saturation, L"%.2f");
    SetTB(gDisp,    gCfg.dispersion,        0.0f,  1.0f);   SetVal(gValDisp,   gCfg.dispersion, L"%.2f");
    SendMessageW(gDepth, BM_SETCHECK, gCfg.depthEffect ? BST_CHECKED : BST_UNCHECKED, 0);
}

// ============================================================================
// Control window procedure
// ============================================================================
LRESULT CALLBACK CtrlWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        APP_LOG(L"CtrlWnd WM_CREATE");
        // Smaller font for color buttons
        HFONT smallFont = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        auto mkLbl = [&](const wchar_t* t, int y) {
            CreateWindowW(L"STATIC", t, WS_CHILD | WS_VISIBLE, 12, y, 75, 16, hwnd, nullptr, nullptr, nullptr);
        };
        auto mkVal = [&](int y) -> HWND {
            return CreateWindowW(L"STATIC", L"0.00", WS_CHILD | WS_VISIBLE | SS_RIGHT, 266, y, 46, 16, hwnd, nullptr, nullptr, nullptr);
        };
        auto mkTb = [&](int id, int y) -> HWND {
            HWND tb = CreateWindowW(TRACKBAR_CLASSW, nullptr,
                WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ,
                88, y, 175, 24, hwnd, (HMENU)(INT_PTR)id, nullptr, nullptr);
            SendMessageW(tb, TBM_SETRANGE, TRUE, MAKELPARAM(0, 1000));
            return tb;
        };
        int y = 10;
        mkLbl(L"Blur", y);             gBlur   = mkTb(ID_BLUR, y);   gValBlur   = mkVal(y); y += 26;
        mkLbl(L"Refr Amount", y);      gRefr   = mkTb(ID_REFR, y);   gValRefr   = mkVal(y); y += 26;
        mkLbl(L"Refr Height", y);      gRefrH  = mkTb(ID_REFRH, y);  gValRefrH  = mkVal(y); y += 26;
        mkLbl(L"Radius", y);           gRadius = mkTb(ID_RADIUS, y); gValRadius = mkVal(y); y += 26;
        mkLbl(L"Saturation", y);       gSat    = mkTb(ID_SAT, y);    gValSat    = mkVal(y); y += 26;
        mkLbl(L"Dispersion", y);       gDisp   = mkTb(ID_DISP, y);   gValDisp   = mkVal(y); y += 28;

        gDepth = CreateWindowW(L"BUTTON", L"Depth Effect",  WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 12, y, 140, 20, hwnd, (HMENU)(INT_PTR)ID_DEPTH, nullptr, nullptr); y += 28;

        for (int i = 0; i < 10; i++) {
            int cx = 12 + (i % 5) * 56, cy = y + (i / 5) * 22;
            HWND btn = CreateWindowW(L"BUTTON", kNames[i],
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, cx, cy, 50, 20,
                hwnd, (HMENU)(INT_PTR)(ID_COLOR0 + i), nullptr, nullptr);
            SendMessageW(btn, WM_SETFONT, (WPARAM)smallFont, TRUE);
            gColorBtns[i] = btn;
        }
        y += 46;

        gWhite      = CreateWindowW(L"BUTTON", L"White Background", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 12, y, 272, 26, hwnd, (HMENU)(INT_PTR)ID_WHITE,   nullptr, nullptr); y += 28;
        gImg        = CreateWindowW(L"BUTTON", L"Open Image...",    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 12, y, 272, 26, hwnd, (HMENU)(INT_PTR)ID_IMAGE,  nullptr, nullptr); y += 28;
        gConsoleBtn = CreateWindowW(L"BUTTON", L"Show Debug Window", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 12, y, 272, 26, hwnd, (HMENU)(INT_PTR)ID_CONSOLE,nullptr, nullptr); y += 28;
        gReset      = CreateWindowW(L"BUTTON", L"Reset Glass",     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 12, y, 272, 26, hwnd, (HMENU)(INT_PTR)ID_RESET,  nullptr, nullptr);

        UpdUI();
        APP_LOG(L"CtrlWnd controls created");
        return 0;
    }

    case WM_HSCROLL: {
        int id = GetDlgCtrlID((HWND)lp);
        float val = 0;
        switch (id) {
        case ID_BLUR:   val=gCfg.blurSigma=GetTB(gBlur,0.1f,30.0f); SetVal(gValBlur,val); break;
        case ID_REFR:   val=gCfg.refractionAmount=GetTB(gRefr,4.0f,120.0f); SetValI(gValRefr,val); break;
        case ID_REFRH:  val=gCfg.refractionHeight=GetTB(gRefrH,4.0f,60.0f); SetValI(gValRefrH,val); break;
        case ID_RADIUS: val=gCfg.cornerRadius=GetTB(gRadius,0.0f,80.0f); SetValI(gValRadius,val); break;
        case ID_SAT:    val=gCfg.saturation=GetTB(gSat,1.0f,2.0f); SetVal(gValSat,val,L"%.2f"); break;
        case ID_DISP:   val=gCfg.dispersion=GetTB(gDisp,0.0f,1.0f); SetVal(gValDisp,val,L"%.2f"); break;

        }
        APP_LOG(L"Slider %s = %.2f", GetIdName(id), val);
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        APP_LOG(L"WM_COMMAND id=%d (%s) code=%d", id, GetIdName(id), HIWORD(wp));
        if (id >= ID_COLOR0 && id <= ID_COLOR9) {
            int i = id - ID_COLOR0;
            APP_LOG(L"  -> SetBackgroundColor %s (%.2f,%.2f,%.2f)", kNames[i], kColors[i][0], kColors[i][1], kColors[i][2]);
            gR.SetBackgroundColor(kColors[i][0], kColors[i][1], kColors[i][2]);
        }
        switch (id) {
        case ID_DEPTH:
            gCfg.depthEffect = (SendMessageW(gDepth, BM_GETCHECK, 0, 0) == BST_CHECKED);
            APP_LOG(L"  -> depthEffect = %d", gCfg.depthEffect);
            break;
        case ID_WHITE:
            APP_LOG(L"  -> SetBackgroundColor white");
            gR.SetBackgroundColor(1, 1, 1);
            break;
        case ID_IMAGE: {
            wchar_t path[MAX_PATH] = {};
            OPENFILENAMEW ofn = {sizeof(ofn), gMainWnd};
            ofn.lpstrFile = path; ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"Images\0*.jpg;*.jpeg;*.png;*.bmp\0All\0*.*\0";
            ofn.Flags = OFN_FILEMUSTEXIST;
            if (GetOpenFileNameW(&ofn)) {
                APP_LOG(L"  -> LoadBackgroundImage: %s", path);
                bool ok = gR.LoadBackgroundImage(path);
                APP_LOG(L"  -> Result: %s", ok ? L"OK" : L"FAILED");
            }
            break;
        }
        case ID_CONSOLE:
            gConVis = !gConVis;
            ShowWindow(GetConsoleWindow(), gConVis ? SW_SHOW : SW_HIDE);
            SetWindowTextW(gConsoleBtn, gConVis ? L"Hide Debug Window" : L"Show Debug Window");
            APP_LOG(L"  -> Console %s", gConVis ? L"shown" : L"hidden");
            break;
        case ID_RESET:
            gGW = gGH = 220;
            gGX = (gW - 220.0f) * 0.5f; gGY = (gH - 220.0f) * 0.42f;
            UpdUI();
            APP_LOG(L"  -> Reset pos=(%.0f,%.0f) size=%.0f", gGX, gGY, gGW);
            break;
        }
        return 0;
    }

    case WM_CLOSE:
        APP_LOG(L"CtrlWnd WM_CLOSE -> hide");
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ============================================================================
// Main window procedure
// ============================================================================
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
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

    case WM_MOUSEWHEEL: {
        float d = (float)GET_WHEEL_DELTA_WPARAM(wp) / 120.0f;
        float cx = gGX + gGW * 0.5f, cy = gGY + gGH * 0.5f;
        gGW = gGH = std::max(60.0f, std::min(400.0f, gGW + d * 15.0f));
        gGX = cx - gGW * 0.5f; gGY = cy - gGH * 0.5f;
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
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
    PrintSystemInfo();

    ShowWindow(GetConsoleWindow(), SW_HIDE);  // Hidden by default
    gConVis = false;
    InitCommonControls();

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
    APP_LOG(L"GlassConfig: blur=%.1f refrA=%.0f refrH=%.0f r=%.0f sat=%.2f disp=%.2f depth=%d",
        gCfg.blurSigma, gCfg.refractionAmount, gCfg.refractionHeight, gCfg.cornerRadius,
        gCfg.saturation, gCfg.dispersion, gCfg.depthEffect);

    // Control window
    WNDCLASSEXW cwc = {sizeof(cwc), 0, CtrlWndProc, 0, 0, hInst,
        nullptr, nullptr, (HBRUSH)(COLOR_BTNFACE + 1), nullptr, L"WLGCtrl", nullptr};
    if (!RegisterClassExW(&cwc)) { APP_ERR(L"RegisterClass CtrlWnd FAILED"); return 1; }
    gCtrlWnd = CreateWindowExW(0, L"WLGCtrl", L"Controls",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        0, 0, 320, 560, gMainWnd, nullptr, hInst, nullptr);
    if (!gCtrlWnd) { APP_ERR(L"CreateWindow CtrlWnd FAILED"); return 1; }
    APP_LOG(L"CtrlWnd created: hwnd=%p", gCtrlWnd);
    SetWindowPos(gCtrlWnd, nullptr, sw - 340, sh / 2 - 280, 320, 560, SWP_NOZORDER);

    UpdUI();

    ShowWindow(gMainWnd, nShow);
    UpdateWindow(gMainWnd);
    APP_LOG(L"========================================");
    APP_LOG(L"  Entering main loop");
    APP_LOG(L"========================================");

    auto prev = std::chrono::high_resolution_clock::now();
    auto fpsT = prev;
    int frames = 0;
    MSG msg;

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
        float dt = std::min(0.1f, std::chrono::duration<float>(now - prev).count());
        prev = now;

        gR.BeginFrame();
        gR.RenderGlass(gGX, gGY, gGW, gGH, gCfg);
        gR.EndFrame();

        frames++;
        if (std::chrono::duration<float>(now - fpsT).count() >= 1.0f) {
            float fps = (float)frames / std::chrono::duration<float>(now - fpsT).count();
            wchar_t title[128];
            swprintf_s(title, L"WLG | %.0f FPS", fps);
            SetWindowTextW(gMainWnd, title);
            frames = 0;
            fpsT = now;
        }
    }

done:
    APP_LOG(L"Shutting down...");
    CoUninitialize();
    return 0;
}
