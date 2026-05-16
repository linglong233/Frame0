# 精确人体反应速度测量工具 — 设计文档

## 1. 项目概述

Windows 本地桌面应用，精确测量人体视觉-运动反应时间。通过操作系统底层 API 获取帧呈现时间戳和硬件输入时间戳，剔除渲染管线、输入管线等系统延迟，使测量结果接近真实的神经-肌肉反应时间。

目标平台 Windows 10/11，C++17 开发，基于 DirectX 11 渲染，预期系统测量误差控制在 5ms 以内。

## 2. 背景与动机

网页端反应速度测试存在不可忽视的延迟链：浏览器渲染管线（8-16ms）、VSync 等待、显示器物理延迟（5-30ms）、鼠标硬件传输（1-15ms）以及 OS/浏览器事件分发（1-5ms），叠加后系统开销可达 15-60ms。

网页端 JavaScript 受安全策略限制，`performance.now()` 精度被降低，且无法获取帧实际扫描输出时间或鼠标硬件级时间戳。本地程序通过 DXGI 和 Raw Input 等底层接口突破这些限制。

## 3. 延迟链模型

完整延迟链：

```
[CPU 设置颜色] → [渲染管线] → [GPU 提交帧] → [VSync 等待]
→ [显示器信号处理] → [面板像素响应] → [光子到达视网膜]
→ [神经传导 + 大脑处理 + 运动指令] ← 目标测量区间
→ [手指按下] → [鼠标控制器采样] → [USB/蓝牙传输]
→ [OS 驱动接收] → [OS 消息分发] → [应用层记录时间戳]
```

起点推到"帧被显示器实际扫描的时刻"，终点推到"OS 收到硬件中断的时刻"。中间无法消除的显示器物理延迟和鼠标硬件延迟，通过校准阶段的设备参数进行可选补偿。

## 4. 精确起点：帧呈现时间戳

使用 DXGI 的 `IDXGISwapChain::GetFrameStatistics` 获取 `DXGI_FRAME_STATISTICS`，其中 `SyncQPCTime` 是以 `QueryPerformanceCounter` 单位表示的帧被 VSync 信号送出的精确时间。

SwapChain 在独占全屏模式下创建，或 Windows 10 1803+ 上使用 `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` + `DXGI_PRESENT_ALLOW_TEARING` 实现低延迟窗口模式。窗口模式下回退到 Present 调用时刻的 QPC，精度下降，结果界面会标注"窗口模式，精度较低"。

## 5. 精确终点：Raw Input 硬件时间戳

使用 Raw Input API 同时注册原始鼠标和键盘输入。在 `WM_INPUT` 消息中通过 `GetRawInputData` 获取 `RAWINPUT` 结构体，消息处理入口处立即调用 `QueryPerformanceCounter` 获取高精度时间戳。

Raw Input 绕过 Windows 高层消息队列合并与分发机制，延迟更低且更稳定。根据配置的触发键类型过滤：
- 鼠标左键：`RI_MOUSE_LEFT_BUTTON_DOWN`
- 鼠标右键：`RI_MOUSE_RIGHT_BUTTON_DOWN`
- 键盘按键：`RI_KEY_MAKE` + 具体虚拟键码匹配

新增 `captureNextInput()` 方法用于设置界面的按键捕获——调用后下一条 Raw Input 消息即作为用户绑定的触发键返回。

## 6. 设备延迟补偿

显示器物理延迟（信号处理 + 面板响应）和鼠标硬件延迟（采样 + 传输）是纯硬件开销，软件无法直接测量。设置界面允许用户填入显示器标称输入延迟（可从 rtings.com 等评测网站查到）和鼠标 click latency，计算时可选扣除。默认不开启。

## 7. 状态机

```
IDLE（主界面）→ WAITING（用户点击"开始测试"按钮）
WAITING → STIMULUS（随机延迟到期，渲染绿屏）
WAITING → FOUL（用户在绿屏前点击）
STIMULUS → RESULT（用户按下触发键，计算反应时间）
RESULT → WAITING（进入下一轮）
RESULT → SUMMARY（五轮结束）
MENU（Escape 触发，覆盖层）→ SETTINGS / EXIT
SETTINGS → 返回上一层（Escape）
```

### 各状态界面

- **IDLE（主界面）**：深色背景，居中显示可点击的「开始测试」「设置」按钮。
- **MENU（菜单覆盖层）**：半透明遮罩，显示「设置」「退出」选项。Escape 关闭。
- **WAITING（等待刺激）**：深红色全屏 + 提示文字（如"等待绿色出现后按下"）。随机延迟 2-6 秒（均匀分布，Mersenne Twister）。Escape 取消本轮回到 IDLE。
- **STIMULUS（刺激呈现）**：亮绿色全屏。DXGI 帧统计记录 T_start。
- **FOUL（犯规）**：显示"过早点击"提示，按任意键重试本轮。
- **RESULT（单轮结果）**：显示本轮反应时间，自动进入下一轮或汇总。
- **SUMMARY（汇总）**：五轮统计（每轮数据、中位数、均值、标准差、设备信息）。Escape 打开菜单。
- **SETTINGS（设置）**：设备延迟补偿输入框、触发键配置（含按键捕获 UI）、自动检测到的设备信息。Escape 返回上一层。

## 8. 防作弊

等待时间采用均匀随机分布 2000-6000ms，由 `<random>` 的 Mersenne Twister 引擎生成。连续测试的多轮之间也引入随机间隔，防止基于固定节奏的预判。

## 9. 结果展示与统计

每组五轮完成后展示：每轮反应时间、中位数、平均值、标准差、系统环境信息（显示器刷新率、鼠标轮询率等，自动检测）。

历史记录保存在本地 JSON 文件，包含时间戳、每轮数据和设备信息，方便追踪趋势。

## 10. 触发键配置

设置界面提供"配置触发键"功能，点击后进入捕获模式，按下任意键（鼠标左键/右键/键盘任意键）即绑定为反应触发键。配置保存在 config.json 中，默认鼠标左键。

## 11. 模块架构

### Timer

封装 `QueryPerformanceCounter`，初始化时缓存 `QueryPerformanceFrequency`，提供 `now()` 和 `elapsed()` 方法。

### Renderer

D3D11 设备和交换链的创建、屏幕颜色切换、帧呈现时间戳获取。渲染极简——每帧 ClearRenderTargetView + Present。`getLastPresentTime()` 封装 `GetFrameStatistics`。

### Input

注册 Raw Input 同时监听鼠标和键盘（`HID_USAGE_PAGE_GENERIC`），`WM_INPUT` 中根据 `TriggerConfig` 过滤触发键，匹配时触发回调传入 QPC 时间戳。`captureNextInput()` 用于按键捕获。

主界面的按钮交互使用普通 Win32 消息（`WM_LBUTTONDOWN`），仅测试中的精度计时使用 Raw Input。两套机制分离。

### TestSession

管理测试状态机（IDLE → WAITING → STIMULUS → RESULT → SUMMARY），协调 Renderer 和 Input，计算反应时间。

### UI

DirectWrite 文字渲染 + 简单按钮控件（矩形区域 + 文字 + hit-testing）。需要绘制的界面：主界面（标题 + 按钮）、菜单覆盖层、设置界面、等待/刺激/犯规/结果/汇总界面。

文字对比度保持至少 4.5:1。按钮 hover 和 press 有明确视觉状态变化。每个界面状态让用户明确知道当前位置和下一步操作。

### Config

封装配置读写。内容：触发键类型和键码、显示器延迟补偿值、鼠标延迟补偿值、是否默认全屏。启动时从 config.json 加载，设置界面修改后即时保存。

### Storage

nlohmann/json 头文件库。文件路径 `%APPDATA%/ReactionTimer/`，存储 `history.json`（测试历史）和 `config.json`（设置）。

## 12. 数据格式

### config.json

```json
{
  "triggerKeyType": "mouseLeft",
  "triggerKeyCode": 0,
  "displayLatencyMs": 0,
  "mouseLatencyMs": 0,
  "fullscreen": true
}
```

`triggerKeyType` 枚举值：`"mouseLeft"` / `"mouseRight"` / `"keyboard"`。
`triggerKeyCode` 仅 keyboard 模式有效，存储虚拟键码。

### history.json

```json
[
  {
    "timestamp": "2026-05-16T14:30:00Z",
    "rounds": [215.3, 198.7, 210.1, 205.5, 202.8],
    "median": 205.5,
    "mean": 206.48,
    "stddev": 6.12,
    "refreshRate": 240,
    "pollingRate": 1000,
    "fullscreen": true
  }
]
```

## 13. 线程模型

主线程运行 Win32 消息循环，处理窗口消息（包括 WM_INPUT）和渲染。渲染负载极低，无需单独渲染线程。所有时间关键路径在主线程同步执行，避免线程间通信引入不确定性延迟。

## 14. 精度分析

- **QPC 分辨率**：现代 Windows 上基于 TSC 或 HPET，通常 100ns 以内。
- **SyncQPCTime 量化误差**：与 VSync 对齐，60Hz ±16.7ms，240Hz ±4.2ms。高刷新率显示器同时减少延迟和时间戳量化误差。
- **WM_INPUT 处理延迟**：程序渲染负载极低，消息循环近乎空闲，延迟可忽略（<1ms）。
- **综合预估**：240Hz + 1000Hz 有线鼠标 ±3-5ms；60Hz + 125Hz 鼠标 ±10-15ms。

## 15. 技术选型

- 语言：C++17
- 构建：CMake + MSVC（Visual Studio 2022，x64）
- 图形：DirectX 11（D3D11 + DXGI 1.4+）
- 窗口/输入：Win32 API（CreateWindowEx + Raw Input）
- 文字渲染：DirectWrite
- JSON：nlohmann/json（CMake FetchContent 自动拉取）
- 高精度计时：QueryPerformanceCounter / QueryPerformanceFrequency

选择 DX11 而非 DX12 是因为帧统计接口更成熟且实现更简单，极简渲染需求下 DX11 完全足够。

## 16. 依赖与构建

### 编译依赖

- Windows SDK 10.0.19041.0 或更高版本
- nlohmann/json（CMake FetchContent 自动拉取）

### 运行依赖

- Windows 10 1803 或更高版本
- 支持 D3D11 的显卡和驱动

### 构建步骤

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### 运行方式

直接运行 ReactionTimer.exe。Escape 打开菜单，F11 切换窗口/全屏模式。

## 17. 关键代码路径（伪代码）

### 刺激呈现

```cpp
renderer.setClearColor(COLOR_GREEN);
renderer.present();

DXGI_FRAME_STATISTICS stats;
swapChain->GetFrameStatistics(&stats);
T_start = stats.SyncQPCTime;
```

### 输入记录

```cpp
case WM_INPUT: {
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);

    RAWINPUT raw;
    GetRawInputData(hRawInput, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));

    if (input.matchesTrigger(raw)) {
        T_end = qpc.QuadPart;
        testSession.onTrigger(T_end);
    }
    break;
}
```

### 反应时间计算

```cpp
double reactionTimeMs =
    (T_end - T_start) * 1000.0 / qpcFrequency
    - config.displayLatencyMs
    - config.mouseLatencyMs;
```

## 18. 未来扩展

- 声音刺激模式（WASAPI 独占模式低延迟音频）
- 多种刺激模式（选择反应、Go/No-Go）
- 硬件校准向导（光敏传感器 + Arduino 实测端到端延迟）
- 跨平台移植（Linux: Vulkan + libinput）
