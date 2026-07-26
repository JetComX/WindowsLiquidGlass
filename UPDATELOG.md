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

### Controls 面板
- Dispersion 从复选框 → 滑块（0.0~1.0）
- 新增 Refraction Height 滑块（4~60）
- Refraction 标签改为 Refr Amount
- 移除 Dispersion 复选框

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
