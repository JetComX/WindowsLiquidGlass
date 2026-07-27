<a id="english"></a>
# Update Log

[English](#english) · [中文](#chinese)

## v0.0.3 (2026-07-27)

### Highlight System (Mouse Spotlight)
- `HighlightPS` rewritten: circular spotlight at cursor `intensity = 1 - smoothstep(0, spotRadius, dist(pixel, mouse))`
- `GlassConfig` added `highlightMouseX/Y`, `spotRadius` (80px), `highlightAlpha` (0.20)
- API: `HighlightAlpha(float)`, Control Panel slider

### Refraction Parameter Refactor (Breaking Change)
- `refractionHeight` / `refractionAmount` from absolute pixels → glassSize-proportional
- Split into `refractionCorrect` (convex) + `refractionNegative` (concave)
- API: `RefrAmountCorrect(float)` / `RefrAmountNegative(float)`
- Control Panel: `Refr Mode` Combo, slider range 0.00~0.30

### Configurable Darkening
- `GlassConfig::darkening` (default 0.92), passed via `GlassCB.c3.w`
- API: `Darkening(float)`, Control Panel slider (0.50~1.00)

### Shadow System Refactor
- `GlassConfig` added `shadowOffsetX/Y`, `shadowAlpha`
- ShadowPS: `discard` inside glass + fixed 12px soft edge
- Removed `shadowBlur` parameter
- API: `ShadowOffset(x,y)`, `ShadowAlpha(alpha)`

### Glass Tint System
- `GlassConfig` added `glassTintR/G/B/A`, `GlassCB` extended to c4
- 10 color buttons now tint glass instead of changing background
- API: `GlassTint(r,g,b,a)`

### Scissor Rect Optimization
- Added `rasterScissor` state, clips shadow/glass/highlight passes
- GPU skips pixel shader outside glass — only 14.4% of pixels processed

### Anti-Game-Detection
- `DXGI_SWAP_EFFECT_DISCARD` → `FLIP_SEQUENTIAL` + 60 FPS frame cap

### Crash Fix + Engineering
- `Renderer::Shutdown()` for COM cleanup before `CoUninitialize`
- Removed `static` locals (`lastMode`, `callCount` → Impl members)
- `Renderer`: deleted copy, implemented move

### Logging Enhancements
- All setters log via `LG_LOG`; exit prints frame count, memory, 16-frame stack trace
- Console X button removed (`DeleteMenu(SC_CLOSE)`); Ctrl+C intercepted

### Build + Docs
- vcxproj ImGui paths now relative (`..\..\imgui\`), removed `imgui_demo.cpp`
- `README.md`: bilingual, badges, pipeline table, API overview

---

## v0.0.2 (2026-07-26)

### API Changes
- Dispersion: `bool` → `float` (0.0~1.0)
- Refraction split: `RefractionHeight` + `RefractionAmount`

### Controls Panel Refactor
- ImGui replaced raw Win32 controls (~180 lines removed)

### Bug Fixes
- NaN white lines on glass resize: `sign(0)=0` → zero gradient → NaN
- `normalize((0,0))` NaN at glass center fixed with epsilon

### Performance
- Removed `imgui_demo.cpp` from build
- WIC Factory cached after first load

### README
- Bilingual rewrite with badges, pipeline table, demo section

---

## 2026-07-25

### Initial Refactor
- Deleted broken old `src/glass_renderer.*` implementation
- Unified `LiquidGlass::Renderer` API
- Fixed D3D11 SRV/RTV binding conflict
- SDF edge anti-aliasing via `smoothstep(-1,1,sd)`
- Fluent setter API: `Blur()`/`Saturation()`/`Radius()` etc.
- Removed `WDA_EXCLUDEDFROMCAPTURE`
- Fixed `PassthroughPS` composite shader

---

<a id="chinese"></a>
# 更新日志

[English](#english) · [中文](#chinese)

## v0.0.3 (2026-07-27)

### 高光系统（鼠标聚光灯）
- `HighlightPS` 重写：鼠标位置圆形聚光灯 `intensity = 1 - smoothstep(0, spotRadius, dist(pixel, mouse))`
- `GlassConfig` 新增 `highlightMouseX/Y`、`spotRadius`（80px）、`highlightAlpha`（0.20）
- `Renderer` 新增 `HighlightAlpha(float)` setter，Control Panel 新增 Highlight 滑块

### 折射参数体系重构（Breaking Change）
- `refractionHeight` / `refractionAmount` 从绝对像素 → glassSize 比例值
  - `refrH = fraction × glassMin × 0.5`，`refrA = (correct - negative) × glassMin`
- `refractionAmount` 拆分为 `refractionCorrect`（凸透镜）+ `refractionNegative`（凹透镜）
- API：`RefrAmountCorrect(float)` / `RefrAmountNegative(float)`
- Control Panel：`Refr Mode` Combo 下拉切换，滑块范围 0.00~0.30

### 暗化系数可配置
- `GlassConfig::darkening`（默认 0.92），`GlassCB.c3.w` 传着色器
- `c.rgb = saturate(c.rgb * darkening)` 替代硬编码 0.92
- `Renderer::Darkening(float)` setter + Control Panel 滑块（0.50~1.00）

### 阴影系统重构
- `GlassConfig` 新增 `shadowOffsetX/Y`、`shadowAlpha`
- 默认偏移 (0,0)，均匀光晕；默认不透明度 0.20
- ShadowPS：`discard` 玻璃内部 + 固定 12px 柔和边缘
- 移除 `shadowBlur` 参数（改固定值）
- API：`ShadowOffset(x,y)`、`ShadowAlpha(alpha)`
- Control Panel：Shadow Alpha 滑块（0.00~0.35）

### 玻璃染色系统
- `GlassConfig` 新增 `glassTintR/G/B/A`，`GlassCB` 扩展 c4
- 10 色按钮：背景色 → 玻璃染色（`lerp` 混入色调，0.6 强度）
- "Clear Tint" 按钮恢复无染色状态
- 着色器：`c.rgb = lerp(c.rgb, glassTint.rgb, glassTint.a)`
- API：`GlassTint(r,g,b,a)`

### 渲染性能优化（Scissor Rect）
- 新增 `rasterScissor` 状态（`ScissorEnable=TRUE`）
- 阴影、玻璃体、高光 3 Pass 前设置 scissor rect
- GPU 硬件跳过玻璃外像素着色器，220×220 玻璃仅 14.4% 像素需处理

### 反游戏检测
- `DXGI_SWAP_EFFECT_DISCARD` → `FLIP_SEQUENTIAL` + 60 FPS 帧率上限

### 崩溃修复 + 工程化
- `Renderer::Shutdown()` — 手动释放 COM 资源，避免 `CoUninitialize` 后析构崩溃
- 消除 `static` 局部变量（`lastMode`、`callCount` 移入 Impl）
- `Renderer` 禁 copy + 实现 move

### 日志系统增强
- 所有 setter 加 `LG_LOG`，GlassCB 上传日志含 tint/darkening 值
- 退出时打印帧数、内存占用（WorkingSet/Peak）、16 层调用堆栈
- 控制台 X 按钮移除（`DeleteMenu(SC_CLOSE)`），防误关
- Ctrl+C 拦截，关闭控制台提示用 Control Panel 的 Hide Console

### 构建 + 文档
- vcxproj ImGui 路径绝对 → 相对（`..\..\imgui\`），移除 `imgui_demo.cpp`
- `README.md`：中英双语，shields.io 徽章、渲染管线表、Demo 章节、API 速览
- 着色器注释、CB 结构体注释、Impl 字段分组

---

## v0.0.2 (2026-07-26)

### API 变更
- **Dispersion 参数**：从 `bool` 改为 `float`（0.0~1.0），改为滑块控制色散强度
  - `Dispersion(bool on)` → `Dispersion(float intensity)`
  - Shader 中色散偏移量乘以 `dispersion` 参数缩放
  - 当 `dispersion == 0` 时使用无色散着色器（性能优化）
- **Refraction 拆分**：折射参数拆为两个独立字段
  - 新增 `RefractionHeight(float h)` — 折射高度（像素，4~60）
  - `RefractionAmount(float a)` — 折射量/扭曲强度（4~120）
  - 常量缓冲区 `GlassCB` 新增 `disp` 字段传递色散强度

### Controls 面板重构
- **ImGui 替换 Win32 控件**：删除 `CtrlWndProc`、`CID` 枚举、所有 Trackbar/Button HWND 全局变量及辅助函数（~180 行）
- **ImGui 界面**：`ImGui::SliderFloat` 6 个滑块、`ImGui::ColorButton` 10 个颜色按钮、`ImGui::Checkbox`、操作按钮
- **构建配置**：`WindowsLiquidGlass.vcxproj` 新增 7 个 ImGui 源文件和 2 个包含路径
- **消息路由**：`MainWndProc` 添加 `ImGui_ImplWin32_WndProcHandler`，滑块/按钮操作时用 `WantCaptureMouse` 阻止玻璃交互
- **GitHub 链接**：Control Panel 底部添加可点击链接跳转 `github.com/JetComX/WindowsLiquidGlass` + Star 呼吁文字

### 性能优化
- **移除 imgui_demo.cpp**：~9000 行死代码不再编译（项目无 `ShowDemoWindow()` 调用），减编译时间
- **WIC Factory 复用**：`IWICImagingFactory` 首次创建后缓存，消除每次 LoadImg 的 `CoCreateInstance` 开销

### README 重写
- 全新设计：居中标题区 + shields.io 徽章 + 为什么要做这个项目 + 5 分钟快速开始 + 核心特性
- 新增渲染管线表、着色器说明、路线图、Star History 图表
- 修复暗化值 85%→92%，补充 `HasBackgroundColor`/`GetBackgroundColor` API

### Bug 修复
- **缩放时玻璃内竖白线**（奇数尺寸 NaN）：`gradSdRoundedRect` 两处零向量 → `normalize` NaN
  - corner 分支：`cornerCoord.x==0` 时 `max(cornerCoord,0)=(0,0)` → `normalize((0,0))` NaN。条件 `>=` → `>`，`==0` 时回落 interior 代码路径
  - interior 分支：`sign(0)=0` 导致零向量。`sign()` → `sx/sy`（`>=0?1:-1`）
  - `normalize(cc)` 玻璃中心 `cc=(0,0)` NaN。`normalize(cc)` → `cc/(length(cc)+1e-6)`
  - `c.rgb *= 0.92` → `saturate(c.rgb * 0.92)` 兜底


### 日志增强
- 每个 setter 调用时打印参数值
- RenderGlass 前 3 帧打印分步管线详情（模糊参数、阴影参数、玻璃参数、背景状态等）

### README & 项目
- 删除"最小示例"章节
- 添加 ⚠️ 实验阶段警告（中英双语）
- 添加 Demo 截图
- 许可证从 MIT 改为 Apache-2.0
- 创建 `LICENSE`、`.gitignore`、`UPDATELOG.md`
- 推送 `v0.0.1` 标签至 GitHub

---

## 2026-07-25

### 初始重构
- 删除 `src/glass_renderer.*` 和 `src/shaders.h`（破损的旧实现）
- 统一为 `LiquidGlass::Renderer` API
- 修复 D3D11 SRV/RTV 绑定冲突（`SetRT` 调用顺序）
- 修复 `glassRT` 清屏 alpha 值（1→0，恢复背景可见）
- 修复 `CreateBlendState` alpha 通道未初始化 D3D11 警告

### 圆角打磨
- 玻璃着色器添加 `smoothstep(-1,1,sd)` SDF 边缘抗锯齿

### 链式 API
- 新增 fluent setter：`Blur()`/`Saturation()`/`Radius()`/`Dispersion()`/`Depth()`/`Config()`
- 新增无参 `RenderGlass(x,y,w,h)` 使用内部存储的参数

### 测试项目 (WIndowsLiquidGlassTest)
- 删除过时库文件副本，改为引用主项目相对路径
- `main.cpp` 精简为 65 行最小示例
- 链式调用：`glass.Blur(12).Radius(40).Saturation(1.5f)`

### 渲染管线
- 移除 Highlight 高光通道及 `GlassConfig::highlightAlpha`

### 日志系统
- 库代码：`LG_LOG`/`LG_WARN`/`LG_ERR` 宏（带帧号前缀）
- Demo：`APP_LOG`/`APP_WARN`/`APP_ERR` 宏
- 双重输出：控制台 + `OutputDebugStringA/W`
- 减少重复日志（仅模式切换时打印）

### Controls 面板
- 放大窗口 240×500 → 300×520
- 颜色按钮使用 13pt Segoe UI 小字体
- 控制台按钮改为 "Show Debug Window"，默认隐藏

### Bug 修复
- 移除 `WDA_EXCLUDEDFROMCAPTURE`（截图时窗口消失）
- 修复 `PassthroughPS` 合成着色器（原 `ImageCopyPS` 缺少常量缓冲区）

---
