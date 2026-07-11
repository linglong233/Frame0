# AGENTS.md

> **核心约束：所有功能设计不得影响测试计时精度。** 任何改动不能增加刺激呈现到输入采集之间的延迟，不能引入额外的 VSync 等待、消息队列延迟或资源重建开销。`presentStimulus()` 到 `WM_INPUT` 回调这条热路径禁止被打断或加锁。

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Build & Test Commands

Uses VS 2022 Developer Shell (MSVC x64). From `build/` directory:

```bash
# Configure (first time or after CMakeLists changes)
cmake .. -G "Visual Studio 17 2022" -A x64

# Build exe
msbuild Frame0.sln /p:Configuration=Release /p:Platform=x64 /t:Frame0

# Rebuild (incremental build may miss changes — use Rebuild if stale)
msbuild Frame0.sln /p:Configuration=Release /p:Platform=x64 /t:Frame0:Rebuild

# Build and run tests
msbuild Frame0.sln /p:Configuration=Release /p:Platform=x64 /t:Frame0Tests
Release\tests\Frame0Tests.exe

# Run a single test
Release\tests\Frame0Tests.exe --gtest_filter=TestSessionTest.FiveRounds
```

Output: `build/src/Release/Frame0.exe`, `build/Release/tests/Frame0Tests.exe`

FetchContent (nlohmann/json, Google Test) requires network access — user in China may need proxy at `127.0.0.1:1080`.

## Architecture

**Split: testable lib vs DX11-dependent app**

- `frame0_lib` (static lib) — Timer, Config, Storage, TestSession. No DirectX, unit-testable.
- `Frame0` (exe) — main, Renderer, Input, UI. Links DX11/DXGI/D2D1/DWrite/Win32.

**State machine** (`TestSession`): `Idle → Waiting → Stimulus → Result → Summary` (5 rounds), with `Foul` on early clicks. `Menu`/`Settings` via ESC. `onStateChanged` callback drives UI transitions in `main.cpp`.

**Dual input paths**:
- Raw Input (`WM_INPUT`) — low-latency timing for test triggers (mouse/keyboard). `Input` class filters by configured trigger key.
- Win32 messages (`WM_LBUTTONDOWN`) — UI button clicks in Idle/Menu/Settings states. Never mixed.

**Timing flow**:
- Start: `Renderer::presentStimulus()` → DXGI `GetFrameStatistics::SyncQPCTime` (falls back to QPC at Present time if unavailable)
- End: `WM_INPUT` handler → `QueryPerformanceCounter` immediately
- Calculation: `(T_end - T_start) / QPC_freq * 1000 - displayLatency - mouseLatency`

**Fullscreen handling**: Exclusive fullscreen via `IDXGISwapChain::SetFullscreenState`. Alt+Tab triggers DXGI fullscreen exit → render loop detects via `syncFullscreenState()` (queries actual DXGI state) → degrades to windowed → `WM_ACTIVATEAPP` auto-restores on return.

**Data paths**: `%APPDATA%/Frame0/config.json` and `history.json`.

## Key Files

- `src/main.cpp` — WinMain, WndProc, globals, render loop, state-driven rendering, Alt+Tab recovery
- `src/Renderer.h/cpp` — D3D11 device/swap chain, DXGI frame stats, D2D1/DirectWrite UI rendering
- `src/TestSession.h/cpp` — State machine, timing, statistics (median/mean/stddev)
- `src/Input.h/cpp` — Raw Input registration, trigger matching, key capture mode
- `src/UI.h/cpp` — All screen drawing (uses Renderer's D2D methods)
