# Update Log

## 2026-07-26

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
