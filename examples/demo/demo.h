#pragma once
#include "../../LiquidGlass.h"
#include "imgui.h"
#include <chrono>

class DemoApp {
public:
    bool Init(HINSTANCE hInst, int nShow);
    int  Run();
    void Shutdown();

private:
    HWND m_hwnd = nullptr;
    int  m_w = 0, m_h = 0;
    LiquidGlass::Renderer m_renderer;
    LiquidGlass::GlassConfig m_cfg;
    bool m_imguiInit = false;
    bool m_consoleVisible = true;

    float m_cardX = 0, m_cardY = 0, m_cardSize = 220;
    bool m_dragging = false;
    float m_dragSX = 0, m_dragSY = 0, m_dragCX = 0, m_dragCY = 0;
    int m_mx = 0, m_my = 0;
    int m_frameCount = 0;

    static DemoApp* s_inst;
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT MsgHandler(UINT msg, WPARAM wp, LPARAM lp);

    void Layout();
    void DrawUI();
    void RenderFrame();
    static void PrintSystemInfo();
    static void PrintProcessInfo();
};
