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
    float blurSigma         = 12.0f;
    float saturation        = 1.5f;
    float refractionHeight  = 30.0f;
    float refractionAmount  = 50.0f;
    float cornerRadius      = 40.0f;
    bool  chromaticAberration = true;
    bool  depthEffect        = true;
};

// ============================================================================
// Renderer — 核心渲染引擎
// ============================================================================
class Renderer {
public:
    Renderer();
    ~Renderer();

    // ---- 生命周期 ----
    bool Init(HWND hwnd, int width, int height);
    void Resize(int width, int height);
    void BeginFrame();
    void EndFrame();

    // ---- 背景 ----
    void SetBackgroundColor(float r, float g, float b);
    bool LoadBackgroundImage(const wchar_t* filePath);
    void ClearBackground();

    // ---- 玻璃参数（链式调用） ----
    Renderer& Blur(float sigma);               // 模糊 0.1~30
    Renderer& Saturation(float s);             // 饱和度 1.0~2.0
    Renderer& Refraction(float amount);        // 折射强度 4~120
    Renderer& Radius(float r);                 // 圆角 0~80
    Renderer& Dispersion(bool on);             // 色散
    Renderer& Depth(bool on);                  // 深度效果
    Renderer& Config(const GlassConfig& cfg);  // 批量设置

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
