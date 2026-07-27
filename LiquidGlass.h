// ============================================================================
// LiquidGlass - Real-time frosted glass rendering for Windows (D3D11)
// ============================================================================
// Usage:
//   glass.Init(hwnd, w, h);
//   glass.Blur(16).Radius(40).Saturation(1.5f);
//   glass.BeginFrame();
//   glass.RenderGlass(x, y, w, h);
//   glass.EndFrame();
// ============================================================================
#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <cstdint>
#include <cmath>

namespace LiquidGlass {

// ============================================================================
// GlassConfig — 批量设置所有参数
// ============================================================================
struct GlassConfig {
    float blurSigma         = 12.0f;  // 模糊量
    float saturation        = 1.5f;   // 饱和度
    float refractionHeight  = 0.20f;  // 折射高度（glassSize 比例）
    float refractionCorrect = 0.15f;  // 正向折射（凸透镜，glassSize 比例）
    float refractionNegative= 0.00f;  // 反向折射（凹透镜，glassSize 比例）
    float cornerRadius      = 40.0f;  // 圆角半径
    float dispersion        = 1.0f;   // 色散强度 0.0~1.0
    bool  depthEffect        = true;  // 深度效果
    float highlightMouseX   = 0.0f;   // 高光鼠标 X
    float highlightMouseY   = 0.0f;   // 高光鼠标 Y
    float spotRadius        = 80.0f;  // 聚光灯半径（像素）
    float highlightAlpha    = 0.20f;  // 高光透明度 0.0~1.0
    float darkening         = 0.92f;  // 玻璃暗化系数（0=全黑, 1=不变）
    float shadowOffsetX     = 0.0f;   // 阴影 X 偏移（像素）
    float shadowOffsetY     = 0.0f;   // 阴影 Y 偏移（像素）
    float shadowAlpha       = 0.20f;  // 阴影不透明度 0.0~1.0
    float glassTintR        = 1.0f;   // 玻璃染色 R
    float glassTintG        = 1.0f;   // 玻璃染色 G
    float glassTintB        = 1.0f;   // 玻璃染色 B
    float glassTintA        = 0.0f;   // 玻璃染色强度 0.0~1.0
};

// ============================================================================
// Renderer — 核心渲染引擎
// ============================================================================
class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&& other) noexcept;
    Renderer& operator=(Renderer&& other) noexcept;

    // ---- 生命周期 ----
    bool Init(HWND hwnd, int width, int height);
    void Resize(int width, int height);
    void Shutdown();  // 手动释放 COM 资源（CoUninitialize 前调用）
    void BeginFrame();
    void EndFrame();

    // ---- 背景 ----
    void SetBackgroundColor(float r, float g, float b);
    bool LoadBackgroundImage(const wchar_t* filePath);
    void ClearBackground();

    // ---- 玻璃参数（链式调用） ----
    Renderer& Blur(float sigma);                          // 模糊 0.1~30
    Renderer& Saturation(float s);                        // 饱和度 1.0~2.0
    Renderer& RefractionHeight(float h);                  // 折射高度（glassSize比例）
    Renderer& RefrAmountCorrect(float a);                 // 正向折射（凸透镜，≥0）
    Renderer& RefrAmountNegative(float a);                // 反向折射（凹透镜，≥0）
    Renderer& HighlightAlpha(float alpha);                // 高光透明度（0=禁用）
    Renderer& Darkening(float v);                         // 暗化系数（0.0~1.0）
    Renderer& ShadowOffset(float x, float y);             // 阴影偏移（像素）
    Renderer& ShadowAlpha(float alpha);                   // 阴影不透明度 0.0~1.0
    Renderer& GlassTint(float r, float g, float b, float a); // 玻璃染色
    Renderer& Radius(float r);                            // 圆角 0~80
    Renderer& Dispersion(float intensity);                // 色散强度 0.0~1.0
    Renderer& Depth(bool on);                             // 深度效果
    Renderer& Config(const GlassConfig& cfg);             // 批量设置

    // ---- 渲染 ----
    void RenderGlass(float x, float y, float w, float h);                    // 使用已设置的参数
    void RenderGlass(float x, float y, float w, float h, const GlassConfig&); // 指定参数

    // ---- D3D11 访问 ----
    ID3D11Device*        GetDevice()  const;
    ID3D11DeviceContext* GetContext() const;
    int Width()  const;
    int Height() const;
    bool HasBackgroundColor() const;
    void GetBackgroundColor(float& r, float& g, float& b) const;
    void DumpDebugMessages();

private:
    struct Impl;
    Impl* m;
};

} // namespace LiquidGlass
