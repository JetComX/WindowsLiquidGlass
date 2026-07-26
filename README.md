<div align="center">

<div id="english"></div>
# WindowsLiquidGlass

**Real-time frosted glass for Windows desktop apps — 3 lines of code, pure GPU rendering**

[English](#english) · [中文](#chinese)

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/JetComX/WindowsLiquidGlass)
[![Version](https://img.shields.io/badge/version-v0.0.2-blue)](https://github.com/JetComX/WindowsLiquidGlass/releases)
[![License](https://img.shields.io/badge/license-Apache%202.0-orange)](https://github.com/JetComX/WindowsLiquidGlass/blob/main/LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blueviolet)](https://github.com/JetComX/WindowsLiquidGlass)
[![GPU](https://img.shields.io/badge/GPU-D3D11%20FL%2011.0%2B-red)](https://github.com/JetComX/WindowsLiquidGlass)

</div>

---

![Screenshot](demopicture/demo.png)

<p align="center"><a href="https://raw.githubusercontent.com/JetComX/WindowsLiquidGlass/main/demopicture/demo_preview.gif"> Watch Animated Demo (GIF)</a></p>

---

## Why This Project

Implementing macOS-style frosted glass in native Windows desktop apps has always been painful. You either lock into UWP/WinUI, or pull in heavy frameworks like Qt/Electron. For vanilla Win32 + D3D11 — there's nothing off the shelf.

WindowsLiquidGlass fills that gap — **pure D3D11 + HLSL, zero external dependencies**. Embed real-time blurred refraction glass into any Win32 window with just 3 lines of code. No UWP, no WinUI, no third-party libraries.

---

## Quick Start

> **Requirements**: Windows 10/11 · Visual Studio 2022 (v145+) · GPU with D3D11 FL 11.0+

### Run the Demo in 5 Minutes

```bash
# 1. Clone
git clone https://github.com/JetComX/WindowsLiquidGlass.git
cd WindowsLiquidGlass

# 2. Open in Visual Studio
start WindowsLiquidGlass.vcxproj

# 3. Press F5 — build and run
```

You'll see a live frosted glass overlay with an ImGui control panel for tweaking blur, refraction, dispersion, and more in real time.

### Integrate into Your Project

**Step 1** — Copy 3 files:
```
LiquidGlass.h          # Public API
LiquidGlass.cpp        # Implementation
LiquidGlassShaders.h   # HLSL shaders
```

**Step 2** — Add to `vcxproj`:
```xml
<ClCompile Include="LiquidGlass.cpp" />
<ClInclude Include="LiquidGlass.h" />
<ClInclude Include="LiquidGlassShaders.h" />
```

**Step 3** — Write code:
```cpp
#include "LiquidGlass.h"
using namespace LiquidGlass;

Renderer glass;

// Init on WM_CREATE
glass.Init(hwnd, width, height);
glass.Blur(12).Radius(40).Saturation(1.5f);

// Per frame
glass.BeginFrame();
glass.RenderGlass(x, y, width, height);
glass.EndFrame();

// Handle WM_SIZE
glass.Resize(newWidth, newHeight);
```

Linker dependencies are handled automatically via `#pragma comment(lib, ...)` — no manual configuration needed.

---

## Core Features

- **Zero Dependencies** — Pure D3D11 + HLSL. No UWP/WinUI/Qt/third-party libs
- **3-Line Integration** — `Init()` → `RenderGlass()` → `EndFrame()`, fluent API for parameters
- **31-Tap Gaussian Blur** — CPU-precomputed weights, eliminates 62M GPU `exp()` calls per frame
- **SDF Rounded Corners** — Signed distance field anti-aliasing for sharp, smooth edges
- **Chromatic Dispersion** — 7-sample RGB channel split, realistic glass aberration
- **Depth Refraction** — `circleMap` + SDF gradient driven, near-to-far intensity falloff
- **Drop Shadow** — SDF-based soft shadow with smoothstep, adds depth perception
- **Live Control Panel** — ImGui 6 sliders + 10 color presets + image loader, real-time feedback
- **Pimpl Pattern** — Public header exposes zero D3D11 details, safe for library distribution
- **Debug-Ready** — Debug builds auto-enable D3D11 debug layer, shader `packoffset` locked layout

---

## API Overview

```cpp
Renderer glass;

// Fluent parameter chaining
glass.Init(hwnd, 1920, 1080)
     .Blur(12.0f)                // 0.1 ~ 30
     .Radius(40.0f)              // 0 ~ 80
     .Saturation(1.5f)           // 1.0 ~ 2.0
     .RefractionAmount(50.0f)    // 4 ~ 120
     .RefractionHeight(30.0f)    // 4 ~ 60
     .Dispersion(1.0f)           // 0.0 ~ 1.0
     .Depth(true);

// Background
glass.SetBackgroundColor(0.2f, 0.2f, 0.3f);
glass.LoadBackgroundImage(L"wallpaper.png");

// Render loop
glass.BeginFrame();
glass.RenderGlass(100, 100, 400, 300);           // uses internal params
glass.RenderGlass(100, 100, 400, 300, customCfg); // explicit config
glass.EndFrame();
```

Full API reference in [LiquidGlass.h](LiquidGlass.h).

---

## Render Pipeline

```
ApplyBg(backbuffer + bgRT)
  → Blur bgRT → blurHRT(horizontal) → blurVRT(vertical)
  → Shadow(SDF,alpha=0.30) → glassRT
  → GlassBody(refraction saturate+circleMap,dispersion 7-sample) → glassRT
  → Composite(PassthroughPS alphaBlend) → backbuffer
```

| Shader | Purpose |
|--------|---------|
| `BlurH_PS` / `BlurV_PS` | 31-tap separable Gaussian blur |
| `ShadowPS` | SDF drop shadow (offset {0,6}, blur 20px) |
| `GlassRefractionPS` | Glass body + refraction + saturation |
| `GlassDispersionPS` | Glass body + chromatic dispersion |
| `PassthroughPS` | Composite (glassRT alpha blend → backbuffer) |
| `ImageCopyPS` | Background image cover-fill scaling |

---

## Contributing

Issues and Pull Requests welcome!

1. **Fork** this repo
2. Create a feature branch: `git checkout -b feat/amazing-feature`
3. Commit with `Co-Authored-By: Claude <noreply@anthropic.com>`
4. Push: `git push origin feat/amazing-feature`
5. Open a **Pull Request**

When modifying shaders: all constant buffers must use `packoffset(cN.x)` to lock layout. See [UPDATELOG.md](UPDATELOG.md).

---

## Roadmap

- [ ] Shadow params in `GlassConfig` (configurable offset/blur/alpha)
- [ ] Downsize render targets (glass region + padding, 5~30× pixel reduction)
- [ ] Multi-glass-instance support (eliminate static variable limitation)
- [ ] Standalone ImGui Demo build target
- [ ] CMake build support

---

## License

[Apache License 2.0](LICENSE). Free to use, modify, and distribute — including commercial use.

---

## Star History

If this project helps you, please give it a Star ⭐ — it means the world to open source authors!

[![Star History Chart](https://api.star-history.com/svg?repos=JetComX/WindowsLiquidGlass&type=Date)](https://star-history.com/#JetComX/WindowsLiquidGlass&Date)

---

<div align="center">

<div id="chinese"></div>
# WindowsLiquidGlass

**让 Windows 桌面应用拥有原生毛玻璃效果 —— 3 行代码，纯 GPU 渲染**

[English](#english) · [中文](#chinese)

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/JetComX/WindowsLiquidGlass)
[![Version](https://img.shields.io/badge/version-v0.0.2-blue)](https://github.com/JetComX/WindowsLiquidGlass/releases)
[![License](https://img.shields.io/badge/license-Apache%202.0-orange)](https://github.com/JetComX/WindowsLiquidGlass/blob/main/LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blueviolet)](https://github.com/JetComX/WindowsLiquidGlass)
[![GPU](https://img.shields.io/badge/GPU-D3D11%20FL%2011.0%2B-red)](https://github.com/JetComX/WindowsLiquidGlass)

</div>

---

![Screenshot](demopicture/demo.png)

<p align="center"><a href="https://raw.githubusercontent.com/JetComX/WindowsLiquidGlass/main/demopicture/demo_preview.gif"> Watch Animated Demo (GIF)</a></p>

---

## 为什么要做这个项目

Windows 桌面开发中，想要实现 macOS 那样的毛玻璃（Frosted Glass）效果，要么依赖 UWP/WinUI 框架，要么引入 Qt/Electron 等重型方案。原生 Win32 + D3D11 的组合几乎没有开箱即用的选择。

WindowsLiquidGlass 填补了这个空白 —— **纯 D3D11 + HLSL，零外部依赖**。只用 3 行代码就能把实时模糊折射玻璃效果嵌入任何 Win32 窗口。它不依赖 UWP、不依赖 WinUI、不依赖任何第三方库。

---

## 快速开始

> **环境要求**: Windows 10/11 · Visual Studio 2022 (v145+) · 支持 D3D11 FL 11.0+ 的 GPU

### 5 分钟跑起来

```bash
# 1. 克隆仓库
git clone https://github.com/JetComX/WindowsLiquidGlass.git
cd WindowsLiquidGlass

# 2. 用 Visual Studio 打开项目
start WindowsLiquidGlass.vcxproj

# 3. 按 F5 —— 编译并运行 Demo
```

你会看到一个带有实时毛玻璃效果的窗口，右侧是 ImGui 控制面板，可以实时调节模糊、折射、色散等参数。

### 集成到你的项目

**第一步** —— 复制 3 个文件到你的项目：
```
LiquidGlass.h          # 公开 API
LiquidGlass.cpp        # 实现
LiquidGlassShaders.h   # HLSL 着色器
```

**第二步** —— 在 `vcxproj` 中添加：
```xml
<ClCompile Include="LiquidGlass.cpp" />
<ClInclude Include="LiquidGlass.h" />
<ClInclude Include="LiquidGlassShaders.h" />
```

**第三步** —— 写代码：
```cpp
#include "LiquidGlass.h"
using namespace LiquidGlass;

Renderer glass;

// WM_CREATE 中初始化
glass.Init(hwnd, width, height);
glass.Blur(12).Radius(40).Saturation(1.5f);

// 每帧渲染
glass.BeginFrame();
glass.RenderGlass(x, y, width, height);
glass.EndFrame();

// WM_SIZE 中处理缩放
glass.Resize(newWidth, newHeight);
```

链接器依赖由 `#pragma comment(lib, ...)` 自动处理，无需手动配置。

---

## 核心特性

- **零依赖** —— 纯 D3D11 + HLSL，不依赖 UWP/WinUI/Qt/任何第三方库
- **3 行代码集成** —— `Init()` → `RenderGlass()` → `EndFrame()`，链式 API 设置参数
- **31-tap 高斯模糊** —— CPU 预计算权重，每帧消除 6200 万 GPU `exp()` 调用
- **SDF 圆角** —— 基于有符号距离场的抗锯齿圆角矩形，边缘锐利平滑
- **色散效果** —— 7 采样 RGB 通道分离，模拟真实玻璃的 chromatic aberration
- **折射深度** —— `circleMap` 映射 + SDF 梯度驱动，远近折射强度渐变
- **投影** —— SDF 柔化阴影，smoothstep 抗锯齿，增强立体感
- **实时控制面板** —— ImGui 6 滑块 + 10 色按钮 + 图片加载，所见即所得
- **Pimpl 模式** —— 公开头文件零 D3D11 细节，适合作为库分发
- **Debug 友好** —— Debug 构建自动启用 D3D11 调试层，着色器 `packoffset` 锁定布局

---

## API 速览

```cpp
Renderer glass;

// 链式设置参数（全部返回 Renderer&，支持链式调用）
glass.Init(hwnd, 1920, 1080)
     .Blur(12.0f)                // 模糊 0.1~30
     .Radius(40.0f)              // 圆角 0~80
     .Saturation(1.5f)           // 饱和度 1.0~2.0
     .RefractionAmount(50.0f)    // 折射扭曲 4~120
     .RefractionHeight(30.0f)    // 折射厚度 4~60
     .Dispersion(1.0f)           // 色散 0.0~1.0
     .Depth(true);               // 深度效果

// 背景
glass.SetBackgroundColor(0.2f, 0.2f, 0.3f);
// 或加载图片
glass.LoadBackgroundImage(L"wallpaper.png");

// 每帧渲染
glass.BeginFrame();
glass.RenderGlass(100, 100, 400, 300);           // 使用内部参数
glass.RenderGlass(100, 100, 400, 300, customCfg); // 指定参数
glass.EndFrame();
```

完整 API 参考请查看 [LiquidGlass.h](LiquidGlass.h) 或下方文档。

---

## 渲染管线

```
ApplyBg(backbuffer + bgRT)
  → Blur bgRT → blurHRT(水平) → blurVRT(垂直)
  → Shadow(SDF投影,alpha=0.30) → glassRT
  → GlassBody(折射saturate+circleMap,色散7采样) → glassRT
  → Composite(PassthroughPS alphaBlend) → backbuffer
```

| 着色器 | 用途 |
|--------|------|
| `BlurH_PS` / `BlurV_PS` | 31-tap 可分离高斯模糊 |
| `ShadowPS` | SDF 投影（偏移 {0,6}, 模糊 20px） |
| `GlassRefractionPS` | 玻璃主体 + 折射 + 饱和度 |
| `GlassDispersionPS` | 玻璃主体 + 色散（dispersion > 0 时启用） |
| `PassthroughPS` | 合成通道（glassRT α混合到 backbuffer） |
| `ImageCopyPS` | 背景图片 cover-fill 缩放 |

---

## 贡献

欢迎提交 Issue 和 Pull Request！贡献前请阅读以下指引：

1. **Fork** 本仓库
2. 创建特性分支：`git checkout -b feat/amazing-feature`
3. 提交更改，Commit 末尾加 `Co-Authored-By: Claude <noreply@anthropic.com>`
4. 推送分支：`git push origin feat/amazing-feature`
5. 提交 **Pull Request**

修改着色器时请注意：常量缓冲区必须使用 `packoffset(cN.x)` 锁定布局，详见 [UPDATELOG.md](UPDATELOG.md)。

---

## 路线图

- [ ] 阴影参数纳入 `GlassConfig`（offset/blur/alpha 可调）
- [ ] 渲染目标降尺寸（玻璃区域 + padding，减少像素浪费 5~30×）
- [ ] 多玻璃实例支持（消除 static 变量限制）
- [ ] ImGui Demo 独立编译选项
- [ ] CMake 构建支持

---

## 许可证

本项目采用 [Apache License 2.0](LICENSE)。你可以自由使用、修改、分发，包括商业用途。

---

## Star 历史

如果这个项目对你有帮助，请给一个 Star ⭐ —— 这是对开源作者最大的鼓励！

[![Star History Chart](https://api.star-history.com/svg?repos=JetComX/WindowsLiquidGlass&type=Date)](https://star-history.com/#JetComX/WindowsLiquidGlass&Date)

---

