# Frame0

精确人体视觉-运动反应时间测量工具。基于 DirectX 11 + QPC（<100ns 分辨率）+ Raw Input，计时链路的系统偏差按确定性模型自动补偿，残余误差为亚毫秒级抖动加鼠标轮询量化；显示器面板延迟等硬件环节可按实测值在设置中扣除。

## 为什么比网页版准

网页端测试的延迟链叠加后系统开销可达 15-60ms：

- 浏览器渲染管线 8-16ms + VSync 等待
- `performance.now()` 精度被安全策略降低
- 无法获取帧实际扫描输出时间或硬件级输入时间戳

本工具通过底层 API 突破这些限制：

- **起点**：`Present()` 返回即刻的 QPC + 自动整帧补偿 — Waiting 的 VSync 节奏使"提交→扫描输出"残余确定为约 1 个刷新周期，直接按刷新率补偿掉（不依赖驱动相关的 `SyncQPCTime`）
- **终点**：Raw Input（`WM_INPUT`）入口即刻 QPC — 刺激帧只呈现一次，消息泵泊在输入等待上，点击入队即被派发，无 Present 阻塞插队
- **补偿**：自动帧补偿 + 可选的显示器输入延迟、鼠标按键延迟扣除，Summary 显示补偿明细

## 功能

- 5 轮测试，统计中位数 / 均值 / 标准差
- 随机等待 2-6 秒防预判（Mersenne Twister）
- 犯规检测（绿屏前点击）
- 可配置触发键（鼠标左键 / 右键 / 任意键盘按键）
- 自动帧补偿 + 显示器/鼠标延迟可选补偿（超补偿会警示而非静默钳 0）
- 独占全屏模式；测试期间隐藏光标
- 历史记录 JSON 持久化

## 构建

需要 Visual Studio 2022、Windows SDK 10.0.19041.0+、CMake。

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

nlohmann/json 通过 CMake FetchContent 自动拉取（需要网络）。

## 使用

直接运行 `build/src/Release/Frame0.exe`。

| 操作 | 说明 |
|------|------|
| 点击 Start Test | 开始 5 轮测试 |
| 等红屏变绿后点击 | 记录反应时间 |
| ESC | 打开菜单（设置 / 退出） |
| F11 | 切换全屏 / 窗口 |
| Alt+Tab 切回 | 自动恢复全屏 |

### 设置

- **Display Input Latency**：显示器输入延迟（可从 rtings.com 查到）
- **Mouse Click Latency**：鼠标按键延迟
- **Mouse Polling Rate**：鼠标轮询率（仅记录进历史，不参与计算）
- **Bind Trigger Key**：捕获并绑定任意按键作为反应触发键

### 数据存储

```
%APPDATA%/Frame0/config.json   — 配置
%APPDATA%/Frame0/history.json  — 测试历史
```

## 精度分析

| 环节 | 误差 |
|------|------|
| QPC 分辨率 | < 100ns |
| 起点：提交→出画残余 | 确定性约 1 帧，已按刷新率自动补偿（残余为 VSync 相位抖动，亚毫秒级） |
| 终点：WM_INPUT 派发 | 泵泊在输入等待上，µs 级 |
| 鼠标轮询量化 | 1000Hz ≈ ±0.5ms，8000Hz 更低 |
| 需用户补偿项 | 显示器面板处理延迟、按键行程（设置中扣除） |

窗口无撕裂模式经 DWM 合成，残余同样按 1 帧近似补偿，精度低于独占全屏与窗口撕裂路径；结果界面会标注模式。以上为模型分析值，未经外部光电传感器校准，不作为实测误差承诺。

## 技术栈

C++17 / DirectX 11 (D3D11 + DXGI 1.5) / Direct2D + DirectWrite / Win32 Raw Input / nlohmann/json / Google Test

## License

MIT
