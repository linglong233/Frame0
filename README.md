# Frame0

精确人体视觉-运动反应时间测量工具。基于 DirectX 11 + DXGI 帧统计 + Raw Input 硬件时间戳，测量误差可控制在 ±5ms 以内（高刷新率显示器 + 高轮询率鼠标）。

## 为什么比网页版准

网页端测试的延迟链叠加后系统开销可达 15-60ms：

- 浏览器渲染管线 8-16ms + VSync 等待
- `performance.now()` 精度被安全策略降低
- 无法获取帧实际扫描输出时间或硬件级输入时间戳

本工具通过底层 API 突破这些限制：

- **起点**：DXGI `GetFrameStatistics` 的 `SyncQPCTime` — 帧被 VSync 送出的精确时刻
- **终点**：Raw Input 消息入口的 `QueryPerformanceCounter` — OS 收到硬件中断的时刻
- **补偿**：可选扣除显示器输入延迟和鼠标硬件延迟

## 功能

- 5 轮测试，统计中位数 / 均值 / 标准差
- 随机等待 2-6 秒防预判（Mersenne Twister）
- 犯规检测（绿屏前点击）
- 可配置触发键（鼠标左键 / 右键 / 任意键盘按键）
- 显示器延迟和鼠标延迟可选补偿
- 独占全屏模式
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
- **Bind Trigger Key**：捕获并绑定任意按键作为反应触发键

### 数据存储

```
%APPDATA%/Frame0/config.json   — 配置
%APPDATA%/Frame0/history.json  — 测试历史
```

## 精度分析

| 环节 | 延迟 |
|------|------|
| QPC 分辨率 | < 100ns |
| SyncQPCTime 量化（240Hz） | ±4.2ms |
| SyncQPCTime 量化（60Hz） | ±16.7ms |
| WM_INPUT 处理 | < 1ms |
| 综合（240Hz + 1000Hz 鼠标） | ±3-5ms |
| 综合（60Hz + 125Hz 鼠标） | ±10-15ms |

窗口模式下 `SyncQPCTime` 可能不可用，回退到 Present 时刻的 QPC，精度略低，结果界面会标注模式。

## 技术栈

C++17 / DirectX 11 (D3D11 + DXGI 1.5) / Direct2D + DirectWrite / Win32 Raw Input / nlohmann/json / Google Test

## License

MIT
