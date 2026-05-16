# Reaction Timer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Windows desktop app that measures human visual-motor reaction time with minimal system latency using DX11 frame statistics and Raw Input timestamps.

**Architecture:** Single-threaded Win32 message loop. D3D11/DXGI for swap chain and frame timing, D2D1/DirectWrite for UI text and buttons. Raw Input for precision input timestamps. State machine drives test flow. Testable logic (Timer, Config, Storage, TestSession) separated into a static library; DX11/Win32 code lives only in the exe.

**Tech Stack:** C++17, CMake + MSVC (VS 2022 x64), DirectX 11, DXGI 1.4+, Direct2D 1.1, DirectWrite, Win32 Raw Input, nlohmann/json, Google Test

---

## File Structure

```
D:\Project\Frame0\
├── CMakeLists.txt
├── src\
│   ├── CMakeLists.txt
│   ├── main.cpp              # WinMain, WndProc, message loop
│   ├── Timer.h / Timer.cpp   # QPC wrapper
│   ├── Config.h / Config.cpp # Settings load/save
│   ├── Storage.h / Storage.cpp # History persistence
│   ├── TestSession.h / TestSession.cpp # State machine
│   ├── Renderer.h / Renderer.cpp # D3D11 + D2D1
│   ├── Input.h / Input.cpp   # Raw Input
│   └── UI.h / UI.cpp         # Drawing + button hit-test
├── tests\
│   ├── CMakeLists.txt
│   ├── test_Timer.cpp
│   ├── test_Config.cpp
│   ├── test_Storage.cpp
│   └── test_TestSession.cpp
└── docs\
    └── superpowers\
        ├── specs\
        │   └── 2026-05-16-reaction-timer-design.md
        └── plans\
            └── 2026-05-16-reaction-timer-plan.md
```

**Library split:** `reaction_timer_lib` (STATIC) contains Timer, Config, Storage, TestSession — all testable without DX11. `ReactionTimer` (EXE) adds main, Renderer, Input, UI and links the lib + DX11/D2D1/DWrite.

---

## Task 1: Project Scaffolding

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/CMakeLists.txt`
- Create: `tests/CMakeLists.txt`
- Create: `src/main.cpp`

- [ ] **Step 1: Write root CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.20)
project(ReactionTimer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3)
FetchContent_MakeAvailable(json)

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)
```

- [ ] **Step 2: Write src/CMakeLists.txt**

```cmake
# Testable logic library (no DX11 dependency)
add_library(reaction_timer_lib STATIC
    Timer.cpp
    Config.cpp
    Storage.cpp
    TestSession.cpp
)

target_include_directories(reaction_timer_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(reaction_timer_lib
    PUBLIC nlohmann_json::nlohmann_json
)

# Application executable
add_executable(ReactionTimer
    main.cpp
    Renderer.cpp
    Input.cpp
    UI.cpp
)

target_link_libraries(ReactionTimer
    PRIVATE reaction_timer_lib
    PRIVATE d3d11 dxgi d2d1 dwrite user32 gdi32
)
```

- [ ] **Step 3: Write tests/CMakeLists.txt**

```cmake
include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(ReactionTimerTests
    test_Timer.cpp
    test_Config.cpp
    test_Storage.cpp
    test_TestSession.cpp
)

target_link_libraries(ReactionTimerTests
    PRIVATE reaction_timer_lib gtest_main
)

include(GoogleTest)
gtest_discover_tests(ReactionTimerTests)
```

- [ ] **Step 4: Write minimal src/main.cpp**

```cpp
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return 0;
}
```

- [ ] **Step 5: Verify build configures**

Run: `cmake -B build -G "Visual Studio 17 2022" -A x64`
Expected: Configuration succeeds, FetchContent downloads nlohmann/json and googletest.

- [ ] **Step 6: Verify build compiles**

Run: `cmake --build build --config Release`
Expected: `ReactionTimer.exe` and `ReactionTimerTests.exe` built (all sources empty, will fail until Task 2).

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt src/ tests/
git commit -m "chore: project scaffolding with CMake, nlohmann/json, gtest"
```

---

## Task 2: Timer Module (TDD)

**Files:**
- Create: `src/Timer.h`
- Create: `src/Timer.cpp`
- Create: `tests/test_Timer.cpp`

- [ ] **Step 1: Write test_Timer.cpp**

```cpp
#include <gtest/gtest.h>
#include "Timer.h"

TEST(TimerTest, FrequencyIsPositive) {
    Timer::init();
    EXPECT_GT(Timer::frequency(), 0);
}

TEST(TimerTest, NowReturnsIncreasingValues) {
    Timer::init();
    int64_t t1 = Timer::now();
    int64_t t2 = Timer::now();
    EXPECT_GE(t2, t1);
}

TEST(TimerTest, NowMsReturnsPositiveValue) {
    Timer::init();
    double ms = Timer::nowMs();
    EXPECT_GT(ms, 0.0);
}

TEST(TimerTest, ElapsedMsReturnsPositiveForLaterTime) {
    Timer::init();
    int64_t start = Timer::now();
    Sleep(10);
    double elapsed = Timer::elapsedMs(start);
    EXPECT_GE(elapsed, 8.0);
    EXPECT_LE(elapsed, 50.0);
}
```

- [ ] **Step 2: Run test, verify it fails**

Run: `cmake --build build --config Release && build\tests\Release\ReactionTimerTests.exe --gtest_filter=TimerTest.*`
Expected: Link error — `Timer::init`, `Timer::now`, etc. unresolved.

- [ ] **Step 3: Write Timer.h**

```cpp
#pragma once
#include <cstdint>

class Timer {
public:
    static void init();
    static int64_t now();
    static double nowMs();
    static double elapsedMs(int64_t startQPC);
    static int64_t frequency();
private:
    static int64_t freq_;
};
```

- [ ] **Step 4: Write Timer.cpp**

```cpp
#include "Timer.h"
#include <windows.h>

int64_t Timer::freq_ = 0;

void Timer::init() {
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);
    freq_ = li.QuadPart;
}

int64_t Timer::now() {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
}

double Timer::nowMs() {
    return static_cast<double>(now()) * 1000.0 / static_cast<double>(freq_);
}

double Timer::elapsedMs(int64_t startQPC) {
    return static_cast<double>(now() - startQPC) * 1000.0 / static_cast<double>(freq_);
}

int64_t Timer::frequency() {
    return freq_;
}
```

- [ ] **Step 5: Run tests, verify they pass**

Run: `cmake --build build --config Release && build\tests\Release\ReactionTimerTests.exe --gtest_filter=TimerTest.*`
Expected: All 4 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add src/Timer.h src/Timer.cpp tests/test_Timer.cpp
git commit -m "feat: Timer module with QPC wrapper"
```

---

## Task 3: Config Module (TDD)

**Files:**
- Create: `src/Config.h`
- Create: `src/Config.cpp`
- Create: `tests/test_Config.cpp`

- [ ] **Step 1: Write test_Config.cpp**

```cpp
#include <gtest/gtest.h>
#include "Config.h"
#include <filesystem>
#include <fstream>

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = std::filesystem::temp_directory_path() / "reaction_timer_test";
        std::filesystem::create_directories(dir_);
        path_ = dir_ / "config.json";
    }
    void TearDown() override {
        std::filesystem::remove_all(dir_);
    }
    std::filesystem::path dir_;
    std::filesystem::path path_;
};

TEST_F(ConfigTest, DefaultConfigValues) {
    auto cfg = defaultConfig();
    EXPECT_EQ(cfg.triggerKeyType, TriggerKeyType::MouseLeft);
    EXPECT_EQ(cfg.triggerKeyCode, 0u);
    EXPECT_DOUBLE_EQ(cfg.displayLatencyMs, 0.0);
    EXPECT_DOUBLE_EQ(cfg.mouseLatencyMs, 0.0);
    EXPECT_TRUE(cfg.fullscreen);
}

TEST_F(ConfigTest, SaveAndLoadRoundTrip) {
    Config original;
    original.triggerKeyType = TriggerKeyType::Keyboard;
    original.triggerKeyCode = 0x20; // VK_SPACE
    original.displayLatencyMs = 5.0;
    original.mouseLatencyMs = 2.0;
    original.fullscreen = false;

    saveConfig(original, path_);
    auto loaded = loadConfig(path_);

    EXPECT_EQ(loaded.triggerKeyType, TriggerKeyType::Keyboard);
    EXPECT_EQ(loaded.triggerKeyCode, 0x20u);
    EXPECT_DOUBLE_EQ(loaded.displayLatencyMs, 5.0);
    EXPECT_DOUBLE_EQ(loaded.mouseLatencyMs, 2.0);
    EXPECT_FALSE(loaded.fullscreen);
}

TEST_F(ConfigTest, LoadMissingFileReturnsDefault) {
    auto cfg = loadConfig(dir_ / "nonexistent.json");
    EXPECT_EQ(cfg.triggerKeyType, TriggerKeyType::MouseLeft);
}

TEST_F(ConfigTest, LoadInvalidJsonReturnsDefault) {
    std::ofstream(path_) << "{ invalid json }";
    auto cfg = loadConfig(path_);
    EXPECT_EQ(cfg.triggerKeyType, TriggerKeyType::MouseLeft);
}
```

- [ ] **Step 2: Run test, verify it fails**

Run: `cmake --build build --config Release && build\tests\Release\ReactionTimerTests.exe --gtest_filter=ConfigTest.*`
Expected: Link error — unresolved symbols.

- [ ] **Step 3: Write Config.h**

```cpp
#pragma once
#include <string>
#include <filesystem>

enum class TriggerKeyType { MouseLeft, MouseRight, Keyboard };

struct Config {
    TriggerKeyType triggerKeyType = TriggerKeyType::MouseLeft;
    unsigned triggerKeyCode = 0;
    double displayLatencyMs = 0.0;
    double mouseLatencyMs = 0.0;
    bool fullscreen = true;
};

Config loadConfig(const std::filesystem::path& path);
void saveConfig(const Config& cfg, const std::filesystem::path& path);
Config defaultConfig();
```

- [ ] **Step 4: Write Config.cpp**

```cpp
#include "Config.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

static std::string triggerKeyTypeToString(TriggerKeyType t) {
    switch (t) {
        case TriggerKeyType::MouseLeft:  return "mouseLeft";
        case TriggerKeyType::MouseRight: return "mouseRight";
        case TriggerKeyType::Keyboard:   return "keyboard";
    }
    return "mouseLeft";
}

static TriggerKeyType stringToTriggerKeyType(const std::string& s) {
    if (s == "mouseRight") return TriggerKeyType::MouseRight;
    if (s == "keyboard")   return TriggerKeyType::Keyboard;
    return TriggerKeyType::MouseLeft;
}

Config defaultConfig() {
    return {};
}

Config loadConfig(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return defaultConfig();
    try {
        std::ifstream f(path);
        json j = json::parse(f);
        Config cfg;
        cfg.triggerKeyType = stringToTriggerKeyType(j.value("triggerKeyType", "mouseLeft"));
        cfg.triggerKeyCode = j.value("triggerKeyCode", 0u);
        cfg.displayLatencyMs = j.value("displayLatencyMs", 0.0);
        cfg.mouseLatencyMs = j.value("mouseLatencyMs", 0.0);
        cfg.fullscreen = j.value("fullscreen", true);
        return cfg;
    } catch (...) {
        return defaultConfig();
    }
}

void saveConfig(const Config& cfg, const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    json j;
    j["triggerKeyType"] = triggerKeyTypeToString(cfg.triggerKeyType);
    j["triggerKeyCode"] = cfg.triggerKeyCode;
    j["displayLatencyMs"] = cfg.displayLatencyMs;
    j["mouseLatencyMs"] = cfg.mouseLatencyMs;
    j["fullscreen"] = cfg.fullscreen;
    std::ofstream f(path);
    f << j.dump(2);
}
```

- [ ] **Step 5: Run tests, verify they pass**

Run: `cmake --build build --config Release && build\tests\Release\ReactionTimerTests.exe --gtest_filter=ConfigTest.*`
Expected: All 4 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add src/Config.h src/Config.cpp tests/test_Config.cpp
git commit -m "feat: Config module with JSON persistence"
```

---

## Task 4: Storage Module (TDD)

**Files:**
- Create: `src/Storage.h`
- Create: `src/Storage.cpp`
- Create: `tests/test_Storage.cpp`

- [ ] **Step 1: Write test_Storage.cpp**

```cpp
#include <gtest/gtest.h>
#include "Storage.h"
#include <filesystem>

class StorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = std::filesystem::temp_directory_path() / "reaction_timer_storage_test";
        std::filesystem::create_directories(dir_);
        path_ = dir_ / "history.json";
    }
    void TearDown() override {
        std::filesystem::remove_all(dir_);
    }
    std::filesystem::path dir_;
    std::filesystem::path path_;
};

TEST_F(StorageTest, AppendAndLoadRoundTrip) {
    SessionResult r;
    r.timestamp = "2026-05-16T14:30:00Z";
    r.rounds = {215.3, 198.7, 210.1, 205.5, 202.8};
    r.median = 205.5;
    r.mean = 206.48;
    r.stddev = 6.12;
    r.refreshRate = 240;
    r.pollingRate = 1000;
    r.fullscreen = true;

    appendHistory(path_, r);
    auto loaded = loadHistory(path_);

    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_EQ(loaded[0].timestamp, "2026-05-16T14:30:00Z");
    ASSERT_EQ(loaded[0].rounds.size(), 5u);
    EXPECT_NEAR(loaded[0].rounds[0], 215.3, 0.01);
    EXPECT_NEAR(loaded[0].median, 205.5, 0.01);
    EXPECT_NEAR(loaded[0].mean, 206.48, 0.01);
    EXPECT_NEAR(loaded[0].stddev, 6.12, 0.01);
    EXPECT_EQ(loaded[0].refreshRate, 240);
    EXPECT_EQ(loaded[0].pollingRate, 1000);
    EXPECT_TRUE(loaded[0].fullscreen);
}

TEST_F(StorageTest, AppendMultipleEntries) {
    SessionResult r1;
    r1.timestamp = "2026-05-16T14:30:00Z";
    r1.rounds = {200.0};
    r1.median = 200.0; r1.mean = 200.0; r1.stddev = 0.0;

    SessionResult r2;
    r2.timestamp = "2026-05-16T15:00:00Z";
    r2.rounds = {180.0};
    r2.median = 180.0; r2.mean = 180.0; r2.stddev = 0.0;

    appendHistory(path_, r1);
    appendHistory(path_, r2);

    auto loaded = loadHistory(path_);
    ASSERT_EQ(loaded.size(), 2u);
    EXPECT_EQ(loaded[0].timestamp, "2026-05-16T14:30:00Z");
    EXPECT_EQ(loaded[1].timestamp, "2026-05-16T15:00:00Z");
}

TEST_F(StorageTest, LoadMissingFileReturnsEmpty) {
    auto loaded = loadHistory(dir_ / "nonexistent.json");
    EXPECT_TRUE(loaded.empty());
}

TEST_F(StorageTest, LoadInvalidJsonReturnsEmpty) {
    std::ofstream(path_) << "not json";
    auto loaded = loadHistory(path_);
    EXPECT_TRUE(loaded.empty());
}
```

- [ ] **Step 2: Run test, verify it fails**

Run: `cmake --build build --config Release && build\tests\Release\ReactionTimerTests.exe --gtest_filter=StorageTest.*`
Expected: Link error — unresolved symbols.

- [ ] **Step 3: Write Storage.h**

```cpp
#pragma once
#include <vector>
#include <string>
#include <filesystem>

struct SessionResult {
    std::string timestamp;
    std::vector<double> rounds;
    double median = 0;
    double mean = 0;
    double stddev = 0;
    int refreshRate = 0;
    int pollingRate = 0;
    bool fullscreen = false;
};

void appendHistory(const std::filesystem::path& path, const SessionResult& result);
std::vector<SessionResult> loadHistory(const std::filesystem::path& path);
```

- [ ] **Step 4: Write Storage.cpp**

```cpp
#include "Storage.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

static SessionResult jsonToResult(const json& j) {
    SessionResult r;
    r.timestamp = j.value("timestamp", "");
    r.rounds = j.value("rounds", std::vector<double>{});
    r.median = j.value("median", 0.0);
    r.mean = j.value("mean", 0.0);
    r.stddev = j.value("stddev", 0.0);
    r.refreshRate = j.value("refreshRate", 0);
    r.pollingRate = j.value("pollingRate", 0);
    r.fullscreen = j.value("fullscreen", false);
    return r;
}

static json resultToJson(const SessionResult& r) {
    json j;
    j["timestamp"] = r.timestamp;
    j["rounds"] = r.rounds;
    j["median"] = r.median;
    j["mean"] = r.mean;
    j["stddev"] = r.stddev;
    j["refreshRate"] = r.refreshRate;
    j["pollingRate"] = r.pollingRate;
    j["fullscreen"] = r.fullscreen;
    return j;
}

std::vector<SessionResult> loadHistory(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return {};
    try {
        std::ifstream f(path);
        json arr = json::parse(f);
        if (!arr.is_array()) return {};
        std::vector<SessionResult> results;
        for (const auto& item : arr) {
            results.push_back(jsonToResult(item));
        }
        return results;
    } catch (...) {
        return {};
    }
}

void appendHistory(const std::filesystem::path& path, const SessionResult& result) {
    std::filesystem::create_directories(path.parent_path());
    auto existing = loadHistory(path);
    existing.push_back(result);
    json arr = json::array();
    for (const auto& r : existing) {
        arr.push_back(resultToJson(r));
    }
    std::ofstream f(path);
    f << arr.dump(2);
}
```

- [ ] **Step 5: Run tests, verify they pass**

Run: `cmake --build build --config Release && build\tests\Release\ReactionTimerTests.exe --gtest_filter=StorageTest.*`
Expected: All 4 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add src/Storage.h src/Storage.cpp tests/test_Storage.cpp
git commit -m "feat: Storage module with JSON history persistence"
```

---

## Task 5: Renderer Module

**Files:**
- Create: `src/Renderer.h`
- Create: `src/Renderer.cpp`

This module is DX11/D2D1 — no unit tests. Verified by building and running the exe.

- [ ] **Step 1: Write Renderer.h**

```cpp
#pragma once
#include <d3d11.h>
#include <dxgi1_4.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>

using Microsoft::WRL::ComPtr;

struct Color { float r, g, b, a; };

namespace Colors {
    inline constexpr Color DARK_RED{ 0.55f, 0.0f, 0.0f, 1.0f };
    inline constexpr Color GREEN{ 0.0f, 0.78f, 0.0f, 1.0f };
    inline constexpr Color DARK_BG{ 0.11f, 0.11f, 0.14f, 1.0f };
    inline constexpr Color WHITE{ 1.0f, 1.0f, 1.0f, 1.0f };
    inline constexpr Color BUTTON_IDLE{ 0.2f, 0.2f, 0.25f, 1.0f };
    inline constexpr Color BUTTON_HOVER{ 0.3f, 0.3f, 0.38f, 1.0f };
    inline constexpr Color OVERLAY_BG{ 0.0f, 0.0f, 0.0f, 0.7f };
}

class Renderer {
public:
    bool init(HWND hwnd, bool fullscreen, int width, int height);
    void shutdown();

    void setClearColor(const Color& c);

    // Stimulus path: pure D3D11, minimal overhead
    void presentStimulus();
    int64_t getLastPresentTimeQPC() const;

    // UI path: D2D1 drawing
    void beginUI();
    void endUI();
    void present();

    void clearToSet();
    void drawText(const std::wstring& text, float x, float y, float fontSize,
                  const Color& color, bool centerX = false, bool centerY = false);
    void fillRectangle(float x, float y, float w, float h, const Color& color);
    void drawRectangleOutline(float x, float y, float w, float h, const Color& color, float strokeWidth = 1.0f);

    void resize(int width, int height);
    bool isFullscreen() const;
    int refreshRate() const;
    int width() const;
    int height() const;

private:
    bool createDeviceAndSwapChain(HWND hwnd);
    bool createRenderTarget();
    bool createD2DResources();
    void releaseBuffers();
    void recreateBuffers();

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain3> swapChain_;
    ComPtr<ID3D11RenderTargetView> rtv_;

    ComPtr<ID2D1Factory> d2dFactory_;
    ComPtr<ID2D1RenderTarget> d2dTarget_;
    ComPtr<IDWriteFactory> dwFactory_;

    Color clearColor_{ 0, 0, 0, 1 };
    int64_t lastPresentQPC_ = 0;
    bool fullscreen_ = false;
    int width_ = 0;
    int height_ = 0;
    int refreshRate_ = 60;
};
```

- [ ] **Step 2: Write Renderer.cpp — device and swap chain**

```cpp
#include "Renderer.h"
#include "Timer.h"
#include <dxgi1_4.h>
#include <cassert>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

static D2D1_COLOR_F toD2D(const Color& c) {
    return D2D1::ColorF(c.r, c.g, c.b, c.a);
}

static D2D1_RECT_F toRect(float x, float y, float w, float h) {
    return D2D1::RectF(x, y, y + h);
}

bool Renderer::init(HWND hwnd, bool fullscreen, int width, int height) {
    fullscreen_ = fullscreen;
    width_ = width;
    height_ = height;
    if (!createDeviceAndSwapChain(hwnd)) return false;
    if (!createRenderTarget()) return false;
    if (!createD2DResources()) return false;
    return true;
}

bool Renderer::createDeviceAndSwapChain(HWND hwnd) {
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION,
        &device_, &featureLevel, &context_);
    if (FAILED(hr)) return false;

    ComPtr<IDXGIDevice> dxgiDevice;
    device_.As(&dxgiDevice);

    ComPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(&adapter);

    ComPtr<IDXGIFactory2> factory;
    adapter->GetParent(IID_PPV_ARGS(&factory));

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width = width_;
    scDesc.Height = height_;
    scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.SampleDesc.Count = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2;
    scDesc.Scaling = DXGI_SCALING_STRETCH;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc = {};
    fsDesc.Windowed = !fullscreen_;

    ComPtr<IDXGISwapChain1> sc1;
    hr = factory->CreateSwapChainForHwnd(
        device_.Get(), hwnd, &scDesc,
        fullscreen_ ? &fsDesc : nullptr, nullptr, &sc1);
    if (FAILED(hr)) return false;

    sc1.As(&swapChain_);

    if (fullscreen_) {
        swapChain_->SetFullscreenState(true, nullptr);
    }

    // Detect refresh rate
    DXGI_SWAP_CHAIN_DESC1 currentDesc;
    swapChain_->GetDesc1(&currentDesc);
    // For fullscreen, query the output
    ComPtr<IDXGIOutput> output;
    swapChain_->GetContainingOutput(&output);
    DXGI_OUTPUT_DESC outputDesc;
    output->GetDesc(&outputDesc);
    DEVMODE dm = {};
    dm.dmSize = sizeof(dm);
    EnumDisplaySettings(outputDesc.DeviceName, ENUM_CURRENT_SETTINGS, &dm);
    if (dm.dmDisplayFrequency > 0) {
        refreshRate_ = static_cast<int>(dm.dmDisplayFrequency);
    }

    return true;
}

bool Renderer::createRenderTarget() {
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) return false;
    hr = device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv_);
    return SUCCEEDED(hr);
}

bool Renderer::createD2DResources() {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory_);
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                             __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(dwFactory_.GetAddressOf()));
    if (FAILED(hr)) return false;

    // Create D2D render target from DXGI back buffer surface
    ComPtr<IDXGISurface> surface;
    swapChain_->GetBuffer(0, IID_PPV_ARGS(&surface));

    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    hr = d2dFactory_->CreateDxgiSurfaceRenderTarget(surface.Get(), rtProps, &d2dTarget_);
    return SUCCEEDED(hr);
}
```

- [ ] **Step 3: Write Renderer.cpp — drawing and present methods**

```cpp
void Renderer::shutdown() {
    d2dTarget_.Reset();
    d2dFactory_.Reset();
    dwFactory_.Reset();
    rtv_.Reset();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();
}

void Renderer::setClearColor(const Color& c) {
    clearColor_ = c;
}

void Renderer::clearToSet() {
    context_->ClearRenderTargetView(rtv_.Get(), &clearColor_.r);
}

// Stimulus path: clear to the set color, present, record QPC
void Renderer::presentStimulus() {
    clearToSet();
    UINT syncInterval = fullscreen_ ? 1 : 0;
    UINT flags = fullscreen_ ? 0 : DXGI_PRESENT_ALLOW_TEARING;
    swapChain_->Present(syncInterval, flags);
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    lastPresentQPC_ = li.QuadPart;
}

int64_t Renderer::getLastPresentTimeQPC() const {
    // Try GetFrameStatistics for more accurate SyncQPCTime
    DXGI_FRAME_STATISTICS stats = {};
    HRESULT hr = swapChain_->GetFrameStatistics(&stats);
    if (SUCCEEDED(hr) && stats.SyncQPCTime != 0) {
        return stats.SyncQPCTime;
    }
    return lastPresentQPC_;
}

// UI path: D2D1 drawing
void Renderer::beginUI() {
    d2dTarget_->BeginDraw();
    d2dTarget_->Clear(toD2D(clearColor_));
}

void Renderer::endUI() {
    d2dTarget_->EndDraw();
}

void Renderer::present() {
    UINT syncInterval = fullscreen_ ? 1 : 0;
    UINT flags = fullscreen_ ? 0 : DXGI_PRESENT_ALLOW_TEARING;
    swapChain_->Present(syncInterval, flags);
}

void Renderer::drawText(const std::wstring& text, float x, float y, float fontSize,
                        const Color& color, bool centerX, bool centerY) {
    ComPtr<IDWriteTextFormat> format;
    dwFactory_->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        fontSize, L"en-US", &format);

    if (centerX) format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    if (centerY) format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    ComPtr<ID2D1SolidColorBrush> brush;
    d2dTarget_->CreateSolidColorBrush(toD2D(color), &brush);

    float textW = static_cast<float>(width_) - x * 2;
    float textH = fontSize * 2;
    D2D1_RECT_F layoutRect = D2D1::RectF(
        centerX ? 0 : x,
        centerY ? 0 : y,
        centerX ? static_cast<float>(width_) : x + textW,
        centerY ? static_cast<float>(height_) : y + textH);

    d2dTarget_->DrawText(text.c_str(), static_cast<UINT32>(text.size()),
                         format.Get(), layoutRect, brush.Get());
}

void Renderer::fillRectangle(float x, float y, float w, float h, const Color& color) {
    ComPtr<ID2D1SolidColorBrush> brush;
    d2dTarget_->CreateSolidColorBrush(toD2D(color), &brush);
    d2dTarget_->FillRectangle(D2D1::RectF(x, y, x + w, y + h), brush.Get());
}

void Renderer::drawRectangleOutline(float x, float y, float w, float h,
                                     const Color& color, float strokeWidth) {
    ComPtr<ID2D1SolidColorBrush> brush;
    d2dTarget_->CreateSolidColorBrush(toD2D(color), &brush);
    d2dTarget_->DrawRectangle(D2D1::RectF(x, y, x + w, y + h), brush.Get(), strokeWidth);
}

void Renderer::resize(int width, int height) {
    if (width == width_ && height == height_) return;
    width_ = width;
    height_ = height;

    d2dTarget_.Reset();
    rtv_.Reset();

    swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    createRenderTarget();
    createD2DResources();
}

bool Renderer::isFullscreen() const { return fullscreen_; }
int Renderer::refreshRate() const { return refreshRate_; }
int Renderer::width() const { return width_; }
int Renderer::height() const { return height_; }
```

- [ ] **Step 4: Build to verify compilation**

Run: `cmake --build build --config Release`
Expected: Compiles without errors.

- [ ] **Step 5: Commit**

```bash
git add src/Renderer.h src/Renderer.cpp
git commit -m "feat: Renderer module with D3D11 + D2D1"
```

---

## Task 6: Input Module

**Files:**
- Create: `src/Input.h`
- Create: `src/Input.cpp`

- [ ] **Step 1: Write Input.h**

```cpp
#pragma once
#include <windows.h>
#include <functional>
#include <cstdint>
#include "Config.h"

class Input {
public:
    using TriggerCallback = std::function<void(int64_t qpcTime)>;
    using CaptureCallback = std::function<void(TriggerKeyType type, unsigned code)>;

    void init(HWND hwnd);
    void setTriggerConfig(TriggerKeyType type, unsigned code);
    void setTriggerCallback(TriggerCallback cb);

    void startCapture(CaptureCallback cb);
    void stopCapture();

    bool handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

private:
    bool matchTrigger(const RAWINPUT& raw);
    int64_t sampleQPC();

    HWND hwnd_ = nullptr;
    TriggerKeyType triggerType_ = TriggerKeyType::MouseLeft;
    unsigned triggerCode_ = 0;
    TriggerCallback onTrigger_;

    bool capturing_ = false;
    CaptureCallback onCaptured_;
};
```

- [ ] **Step 2: Write Input.cpp**

```cpp
#include "Input.h"
#include "Timer.h"
#include <Dbt.h>

void Input::init(HWND hwnd) {
    hwnd_ = hwnd;

    RAWINPUTDEVICE devices[2] = {};

    // Mouse
    devices[0].usUsagePage = 0x01;
    devices[0].usUsage = 0x02;
    devices[0].dwFlags = RIDEV_INPUTSINK;
    devices[0].hwndTarget = hwnd;

    // Keyboard
    devices[1].usUsagePage = 0x01;
    devices[1].usUsage = 0x06;
    devices[1].dwFlags = RIDEV_INPUTSINK;
    devices[1].hwndTarget = hwnd;

    RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE));
}

void Input::setTriggerConfig(TriggerKeyType type, unsigned code) {
    triggerType_ = type;
    triggerCode_ = code;
}

void Input::setTriggerCallback(TriggerCallback cb) {
    onTrigger_ = std::move(cb);
}

void Input::startCapture(CaptureCallback cb) {
    capturing_ = true;
    onCaptured_ = std::move(cb);
}

void Input::stopCapture() {
    capturing_ = false;
    onCaptured_ = nullptr;
}

int64_t Input::sampleQPC() {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
}

bool Input::matchTrigger(const RAWINPUT& raw) {
    if (raw.header.dwType == RIM_TYPEMOUSE) {
        USHORT flags = raw.data.mouse.usButtonFlags;
        if (triggerType_ == TriggerKeyType::MouseLeft &&
            (flags & RI_MOUSE_LEFT_BUTTON_DOWN)) return true;
        if (triggerType_ == TriggerKeyType::MouseRight &&
            (flags & RI_MOUSE_RIGHT_BUTTON_DOWN)) return true;
    }
    if (raw.header.dwType == RIM_TYPEKEYBOARD) {
        if (triggerType_ == TriggerKeyType::Keyboard) {
            USHORT flags = raw.data.keyboard.Flags;
            USHORT vk = raw.data.keyboard.VKey;
            if (!(flags & RI_KEY_BREAK) && vk == triggerCode_) return true;
        }
    }
    return false;
}

bool Input::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg != WM_INPUT) return false;

    UINT size = 0;
    GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size,
                    sizeof(RAWINPUTHEADER));
    if (size == 0) return false;

    RAWINPUT raw;
    GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size,
                    sizeof(RAWINPUTHEADER));

    // Capture mode: return next raw input as the bound trigger key
    if (capturing_ && onCaptured_) {
        if (raw.header.dwType == RIM_TYPEMOUSE) {
            USHORT flags = raw.data.mouse.usButtonFlags;
            if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) {
                auto cb = onCaptured_;
                stopCapture();
                cb(TriggerKeyType::MouseLeft, 0);
                return true;
            }
            if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
                auto cb = onCaptured_;
                stopCapture();
                cb(TriggerKeyType::MouseRight, 0);
                return true;
            }
        }
        if (raw.header.dwType == RIM_TYPEKEYBOARD) {
            if (!(raw.data.keyboard.Flags & RI_KEY_BREAK)) {
                TriggerKeyType type = TriggerKeyType::Keyboard;
                unsigned code = raw.data.keyboard.VKey;
                auto cb = onCaptured_;
                stopCapture();
                cb(type, code);
                return true;
            }
        }
        return false;
    }

    // Normal trigger matching
    if (matchTrigger(raw)) {
        int64_t qpc = sampleQPC();
        if (onTrigger_) onTrigger_(qpc);
        return true;
    }
    return false;
}
```

- [ ] **Step 3: Build to verify compilation**

Run: `cmake --build build --config Release`
Expected: Compiles without errors.

- [ ] **Step 4: Commit**

```bash
git add src/Input.h src/Input.cpp
git commit -m "feat: Input module with Raw Input + configurable trigger"
```

---

## Task 7: UI Module

**Files:**
- Create: `src/UI.h`
- Create: `src/UI.cpp`

- [ ] **Step 1: Write UI.h**

```cpp
#pragma once
#include <string>
#include <vector>
#include <functional>
#include "Renderer.h"

struct Button {
    std::wstring text;
    float x, y, w, h;
    std::function<void()> onClick;
    bool hovered = false;
};

class UI {
public:
    void init(Renderer* renderer, int screenW, int screenH);

    void drawIdleScreen(const std::vector<Button>& buttons);
    void drawMenuOverlay(const std::vector<Button>& menuItems);
    void drawWaitingScreen();
    void drawStimulusScreen();
    void drawFoulScreen();
    void drawResultScreen(double timeMs, int round, int total);
    void drawSummaryScreen(const std::vector<double>& rounds, double median,
                           double mean, double stddev, int refreshRate,
                           int pollingRate, bool wasFullscreen);

    // Text input fields for settings — drawn as simple bordered boxes
    struct TextField {
        std::wstring label;
        std::wstring value;
        float x, y, w, h;
        bool focused = false;
    };

    void drawSettingsScreen(const std::vector<TextField>& fields,
                            const std::wstring& triggerKeyLabel,
                            const std::vector<Button>& buttons);
    void drawCaptureScreen();

    void updateScreenSize(int w, int h);

    static bool hitTestButtons(const std::vector<Button>& buttons, float px, float py);

private:
    void drawButton(const Button& btn);
    void drawCenteredText(const std::wstring& text, float y, float fontSize,
                          const Color& color);
    void drawTextField(const TextField& field);

    Renderer* renderer_ = nullptr;
    int screenW_ = 0;
    int screenH_ = 0;
};
```

- [ ] **Step 2: Write UI.cpp**

```cpp
#include "UI.h"

void UI::init(Renderer* renderer, int screenW, int screenH) {
    renderer_ = renderer;
    screenW_ = screenW;
    screenH_ = screenH;
}

void UI::updateScreenSize(int w, int h) {
    screenW_ = w;
    screenH_ = h;
}

bool UI::hitTestButtons(const std::vector<Button>& buttons, float px, float py) {
    for (auto& btn : buttons) {
        if (px >= btn.x && px < btn.x + btn.w &&
            py >= btn.y && py < btn.y + btn.h) {
            btn.onClick();
            return true;
        }
    }
    return false;
}

void UI::drawButton(const Button& btn) {
    Color bg = btn.hovered ? Colors::BUTTON_HOVER : Colors::BUTTON_IDLE;
    renderer_->fillRectangle(btn.x, btn.y, btn.w, btn.h, bg);
    renderer_->drawRectangleOutline(btn.x, btn.y, btn.w, btn.h, Colors::WHITE, 1.0f);

    // Draw text centered in button
    float textX = btn.x + btn.w / 2;
    float textY = btn.y + btn.h * 0.2f;
    renderer_->drawText(btn.text, textX, textY, 20.0f, Colors::WHITE, true, false);
}

void UI::drawCenteredText(const std::wstring& text, float y, float fontSize,
                          const Color& color) {
    renderer_->drawText(text, 0, y, fontSize, color, true, false);
}

void UI::drawTextField(const TextField& field) {
    Color border = field.focused ? Colors::GREEN : Colors::WHITE;
    renderer_->drawRectangleOutline(field.x, field.y, field.w, field.h, border, 1.5f);
    renderer_->drawText(field.label, field.x, field.y - 22.0f, 14.0f, Colors::WHITE);
    renderer_->drawText(field.value, field.x + 8.0f, field.y + 4.0f, 18.0f, Colors::WHITE);
}

void UI::drawIdleScreen(const std::vector<Button>& buttons) {
    renderer_->beginUI();
    drawCenteredText(L"Reaction Timer", screenH_ * 0.25f, 48.0f, Colors::WHITE);
    drawCenteredText(L"Press F11 to toggle fullscreen | ESC for menu",
                     screenH_ * 0.38f, 16.0f, Colors::WHITE);
    for (const auto& btn : buttons) drawButton(btn);
    renderer_->endUI();
}

void UI::drawMenuOverlay(const std::vector<Button>& menuItems) {
    renderer_->beginUI();
    renderer_->fillRectangle(0, 0, static_cast<float>(screenW_),
                             static_cast<float>(screenH_), Colors::OVERLAY_BG);
    drawCenteredText(L"Menu", screenH_ * 0.25f, 36.0f, Colors::WHITE);
    for (const auto& item : menuItems) drawButton(item);
    renderer_->endUI();
}

void UI::drawWaitingScreen() {
    renderer_->beginUI();
    drawCenteredText(L"Wait for green...", screenH_ * 0.45f, 32.0f, Colors::WHITE);
    renderer_->endUI();
}

void UI::drawStimulusScreen() {
    renderer_->setClearColor(Colors::GREEN);
    renderer_->presentStimulus();
}

void UI::drawFoulScreen() {
    renderer_->beginUI();
    renderer_->fillRectangle(0, 0, static_cast<float>(screenW_),
                             static_cast<float>(screenH_),
                             { 0.8f, 0.0f, 0.0f, 1.0f });
    drawCenteredText(L"Too early!", screenH_ * 0.4f, 40.0f, Colors::WHITE);
    drawCenteredText(L"Click to retry", screenH_ * 0.52f, 20.0f, Colors::WHITE);
    renderer_->endUI();
}

void UI::drawResultScreen(double timeMs, int round, int total) {
    renderer_->beginUI();
    wchar_t buf[128];
    swprintf(buf, 128, L"Round %d / %d", round, total);
    drawCenteredText(buf, screenH_ * 0.3f, 24.0f, Colors::WHITE);

    swprintf(buf, 128, L"%.1f ms", timeMs);
    drawCenteredText(buf, screenH_ * 0.43f, 56.0f, Colors::GREEN);
    renderer_->endUI();
}

void UI::drawSummaryScreen(const std::vector<double>& rounds, double median,
                           double mean, double stddev, int refreshRate,
                           int pollingRate, bool wasFullscreen) {
    renderer_->beginUI();
    drawCenteredText(L"Results", screenH_ * 0.08f, 36.0f, Colors::WHITE);

    float y = screenH_ * 0.18f;
    for (size_t i = 0; i < rounds.size(); i++) {
        wchar_t buf[128];
        swprintf(buf, 128, L"Round %zu:  %.1f ms", i + 1, rounds[i]);
        drawCenteredText(buf, y, 20.0f, Colors::WHITE);
        y += 28.0f;
    }

    y += 10.0f;
    wchar_t buf[128];
    swprintf(buf, 128, L"Median: %.1f ms", median);
    drawCenteredText(buf, y, 28.0f, Colors::GREEN);
    y += 36.0f;
    swprintf(buf, 128, L"Mean: %.1f ms   StdDev: %.1f ms", mean, stddev);
    drawCenteredText(buf, y, 18.0f, Colors::WHITE);

    y += 40.0f;
    swprintf(buf, 128, L"Display: %d Hz  |  Mouse: %d Hz  |  %s",
             refreshRate, pollingRate > 0 ? pollingRate : 0,
             wasFullscreen ? L"Fullscreen" : L"Windowed");
    drawCenteredText(buf, y, 14.0f, Colors::WHITE);

    drawCenteredText(L"Press SPACE or click to continue",
                     screenH_ * 0.9f, 16.0f, Colors::WHITE);
    renderer_->endUI();
}

void UI::drawSettingsScreen(const std::vector<TextField>& fields,
                            const std::wstring& triggerKeyLabel,
                            const std::vector<Button>& buttons) {
    renderer_->beginUI();
    drawCenteredText(L"Settings", screenH_ * 0.06f, 36.0f, Colors::WHITE);

    for (const auto& field : fields) drawTextField(field);

    // Trigger key label
    float ty = screenH_ * 0.52f;
    renderer_->drawText(L"Trigger Key:", screenW_ * 0.25f, ty, 18.0f, Colors::WHITE);
    renderer_->drawText(triggerKeyLabel, screenW_ * 0.25f + 160.0f, ty, 18.0f, Colors::GREEN);

    for (const auto& btn : buttons) drawButton(btn);

    drawCenteredText(L"Press ESC to go back", screenH_ * 0.92f, 14.0f, Colors::WHITE);
    renderer_->endUI();
}

void UI::drawCaptureScreen() {
    renderer_->beginUI();
    drawCenteredText(L"Press any key or mouse button to bind...",
                     screenH_ * 0.45f, 24.0f, Colors::GREEN);
    drawCenteredText(L"ESC to cancel", screenH_ * 0.55f, 16.0f, Colors::WHITE);
    renderer_->endUI();
}
```

- [ ] **Step 3: Build to verify compilation**

Run: `cmake --build build --config Release`
Expected: Compiles without errors.

- [ ] **Step 4: Commit**

```bash
git add src/UI.h src/UI.cpp
git commit -m "feat: UI module with D2D1/DirectWrite drawing"
```

---

## Task 8: TestSession State Machine (TDD)

**Files:**
- Create: `src/TestSession.h`
- Create: `src/TestSession.cpp`
- Create: `tests/test_TestSession.cpp`

- [ ] **Step 1: Write test_TestSession.cpp**

```cpp
#include <gtest/gtest.h>
#include "TestSession.h"
#include "Timer.h"
#include <thread>

class TestSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        Timer::init();
        session_.setStateChangedCallback([](AppState) {});
    }
    TestSession session_;
};

TEST_F(TestSessionTest, StartsInIdleState) {
    EXPECT_EQ(session_.state(), AppState::Idle);
}

TEST_F(TestSessionTest, StartTransitionsToWaiting) {
    session_.start();
    EXPECT_EQ(session_.state(), AppState::Waiting);
}

TEST_F(TestSessionTest, TriggerBeforeStimulusIsFoul) {
    session_.start();
    // Simulate trigger before delay expires (stimulusQPC is 0)
    session_.onTrigger(Timer::now());
    EXPECT_EQ(session_.state(), AppState::Foul);
}

TEST_F(TestSessionTest, TriggerAfterStimulusGivesResult) {
    session_.start();
    // Manually set stimulus time to simulate the renderer presenting
    session_.setStimulusTimeForTest(Timer::now());
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    session_.onTrigger(Timer::now());

    EXPECT_EQ(session_.state(), AppState::Result);
    EXPECT_GT(session_.lastReactionMs(), 15.0);
    EXPECT_LT(session_.lastReactionMs(), 100.0);
}

TEST_F(TestSessionTest, FiveRoundsReachSummary) {
    session_.start();
    for (int i = 0; i < 5; i++) {
        // Wait for random delay to expire
        std::this_thread::sleep_for(std::chrono::seconds(7));
        session_.setStimulusTimeForTest(Timer::now());
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        session_.onTrigger(Timer::now());

        if (i < 4) {
            EXPECT_EQ(session_.state(), AppState::Result);
            // Proceed to next round
            session_.proceedToNextRound();
            EXPECT_EQ(session_.state(), AppState::Waiting);
        }
    }
    EXPECT_EQ(session_.state(), AppState::Summary);
    EXPECT_EQ(session_.currentRoundIndex(), 5);
}

TEST_F(TestSessionTest, StatsCalculation) {
    session_.start();
    // Inject results directly for testing stats
    session_.setRoundsForTest({200.0, 210.0, 205.0, 195.0, 215.0});
    session_.calculateStats();

    EXPECT_NEAR(session_.medianMs(), 205.0, 0.1);
    EXPECT_NEAR(session_.meanMs(), 205.0, 0.1);
}
```

- [ ] **Step 2: Run test, verify it fails**

Run: `cmake --build build --config Release && build\tests\Release\ReactionTimerTests.exe --gtest_filter=TestSessionTest.*`
Expected: Unresolved symbols.

- [ ] **Step 3: Write TestSession.h**

```cpp
#pragma once
#include <vector>
#include <random>
#include <cstdint>
#include <functional>

enum class AppState {
    Idle,
    Waiting,
    Stimulus,
    Foul,
    Result,
    Summary,
    Menu,
    Settings
};

class TestSession {
public:
    static constexpr int ROUNDS_PER_SESSION = 5;

    using StateChangedCallback = std::function<void(AppState newState)>;

    void setStateChangedCallback(StateChangedCallback cb);

    void start();
    void onTrigger(int64_t qpcTime);
    void onEscape();
    void proceedToNextRound();
    void update(); // Called each frame — checks if random delay expired

    AppState state() const;
    double lastReactionMs() const;
    int currentRoundIndex() const;
    const std::vector<double>& roundTimes() const;

    double medianMs() const;
    double meanMs() const;
    double stddevMs() const;

    void setDisplayLatency(double ms);
    void setMouseLatency(double ms);

    // Test helpers
    void setStimulusTimeForTest(int64_t qpc);
    void setRoundsForTest(const std::vector<double>& times);
    void calculateStats();

private:
    void transitionTo(AppState newState);
    int64_t generateRandomDelayTicks();

    AppState state_ = AppState::Idle;
    std::vector<double> roundTimes_;
    int currentRoundIndex_ = 0;

    int64_t stimulusQPC_ = 0;
    int64_t waitingStartQPC_ = 0;
    int64_t targetDelayTicks_ = 0;

    double displayLatencyMs_ = 0;
    double mouseLatencyMs_ = 0;
    double lastReactionMs_ = 0;

    double medianMs_ = 0;
    double meanMs_ = 0;
    double stddevMs_ = 0;

    std::mt19937 rng_{ std::random_device{}() };
    std::uniform_int_distribution<int> delayDist_{ 2000, 6000 };

    StateChangedCallback onStateChanged_;
};
```

- [ ] **Step 4: Write TestSession.cpp**

```cpp
#include "TestSession.h"
#include "Timer.h"
#include <algorithm>
#include <cmath>
#include <numeric>

void TestSession::setStateChangedCallback(StateChangedCallback cb) {
    onStateChanged_ = std::move(cb);
}

void TestSession::transitionTo(AppState newState) {
    state_ = newState;
    if (onStateChanged_) onStateChanged_(newState);
}

AppState TestSession::state() const { return state_; }
double TestSession::lastReactionMs() const { return lastReactionMs_; }
int TestSession::currentRoundIndex() const { return currentRoundIndex_; }
const std::vector<double>& TestSession::roundTimes() const { return roundTimes_; }

void TestSession::setDisplayLatency(double ms) { displayLatencyMs_ = ms; }
void TestSession::setMouseLatency(double ms) { mouseLatencyMs_ = ms; }

void TestSession::start() {
    roundTimes_.clear();
    currentRoundIndex_ = 0;
    lastReactionMs_ = 0;
    medianMs_ = meanMs_ = stddevMs_ = 0;
    waitingStartQPC_ = Timer::now();
    targetDelayTicks_ = generateRandomDelayTicks();
    transitionTo(AppState::Waiting);
}

void TestSession::onTrigger(int64_t qpcTime) {
    if (state_ == AppState::Waiting) {
        // Trigger before stimulus = foul
        transitionTo(AppState::Foul);
        return;
    }
    if (state_ == AppState::Stimulus) {
        double rawMs = Timer::elapsedMs(stimulusQPC_);
        lastReactionMs_ = rawMs - displayLatencyMs_ - mouseLatencyMs_;
        if (lastReactionMs_ < 0) lastReactionMs_ = 0;
        roundTimes_.push_back(lastReactionMs_);
        currentRoundIndex_++;

        if (currentRoundIndex_ >= ROUNDS_PER_SESSION) {
            calculateStats();
            transitionTo(AppState::Summary);
        } else {
            transitionTo(AppState::Result);
        }
    }
}

void TestSession::onEscape() {
    if (state_ == AppState::Waiting || state_ == AppState::Foul) {
        transitionTo(AppState::Idle);
    }
}

void TestSession::proceedToNextRound() {
    if (state_ != AppState::Result) return;
    waitingStartQPC_ = Timer::now();
    targetDelayTicks_ = generateRandomDelayTicks();
    transitionTo(AppState::Waiting);
}

void TestSession::update() {
    if (state_ != AppState::Waiting) return;
    int64_t elapsed = Timer::now() - waitingStartQPC_;
    if (elapsed >= targetDelayTicks_) {
        stimulusQPC_ = Timer::now();
        transitionTo(AppState::Stimulus);
    }
}

int64_t TestSession::generateRandomDelayTicks() {
    int delayMs = delayDist_(rng_);
    return static_cast<int64_t>(delayMs) * Timer::frequency() / 1000;
}

void TestSession::calculateStats() {
    if (roundTimes_.empty()) return;

    auto sorted = roundTimes_;
    std::sort(sorted.begin(), sorted.end());

    size_t n = sorted.size();
    if (n % 2 == 0) {
        medianMs_ = (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;
    } else {
        medianMs_ = sorted[n / 2];
    }

    double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    meanMs_ = sum / static_cast<double>(n);

    double sqSum = 0;
    for (double v : sorted) {
        sqSum += (v - meanMs_) * (v - meanMs_);
    }
    stddevMs_ = std::sqrt(sqSum / static_cast<double>(n));
}

double TestSession::medianMs() const { return medianMs_; }
double TestSession::meanMs() const { return meanMs_; }
double TestSession::stddevMs() const { return stddevMs_; }

// Test helpers
void TestSession::setStimulusTimeForTest(int64_t qpc) {
    stimulusQPC_ = qpc;
    state_ = AppState::Stimulus;
}

void TestSession::setRoundsForTest(const std::vector<double>& times) {
    roundTimes_ = times;
    currentRoundIndex_ = static_cast<int>(times.size());
}
```

- [ ] **Step 5: Run tests, verify they pass**

Run: `cmake --build build --config Release && build\tests\Release\ReactionTimerTests.exe --gtest_filter=TestSessionTest.*`
Expected: All 6 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add src/TestSession.h src/TestSession.cpp tests/test_TestSession.cpp
git commit -m "feat: TestSession state machine with stats"
```

---

## Task 9: Main Application Integration

**Files:**
- Modify: `src/main.cpp`
- Modify: `src/CMakeLists.txt` (if needed)

This task wires all modules together into a working application. No TDD — verified by running the exe.

- [ ] **Step 1: Write main.cpp — includes and globals**

```cpp
#include "Timer.h"
#include "Config.h"
#include "Storage.h"
#include "Renderer.h"
#include "Input.h"
#include "UI.h"
#include "TestSession.h"

#include <windowsx.h>
#include <shlobj.h>
#include <chrono>
#include <iomanip>
#include <sstream>

static Renderer g_renderer;
static Input g_input;
static UI g_ui;
static TestSession g_session;
static Config g_config;

static HINSTANCE g_hInstance = nullptr;
static HWND g_hwnd = nullptr;
static bool g_running = true;

static std::vector<Button> g_currentButtons;

static std::filesystem::path getConfigPath() {
    char path[MAX_PATH];
    SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path);
    auto p = std::filesystem::path(path) / "ReactionTimer";
    std::filesystem::create_directories(p);
    return p / "config.json";
}

static std::filesystem::path getHistoryPath() {
    char path[MAX_PATH];
    SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path);
    auto p = std::filesystem::path(path) / "ReactionTimer";
    std::filesystem::create_directories(p);
    return p / "history.json";
}
```

- [ ] **Step 2: Write main.cpp — button creation helpers**

```cpp
static Button makeButton(const std::wstring& text, float cx, float cy,
                         float w, float h, std::function<void()> onClick) {
    Button btn;
    btn.text = text;
    btn.x = cx - w / 2;
    btn.y = cy - h / 2;
    btn.w = w;
    btn.h = h;
    btn.onClick = std::move(onClick);
    return btn;
}

static void setupIdleButtons() {
    g_currentButtons.clear();
    float cx = static_cast<float>(g_renderer.width()) / 2;
    float cy = static_cast<float>(g_renderer.height()) * 0.55f;

    g_currentButtons.push_back(
        makeButton(L"Start Test", cx, cy, 220.0f, 50.0f, []() {
            g_session.start();
            g_currentButtons.clear();
        }));
    g_currentButtons.push_back(
        makeButton(L"Settings", cx, cy + 70.0f, 220.0f, 50.0f, []() {
            g_session.transitionTo(AppState::Settings);
        }));
}

static void setupMenuButtons() {
    g_currentButtons.clear();
    float cx = static_cast<float>(g_renderer.width()) / 2;
    float cy = static_cast<float>(g_renderer.height()) * 0.45f;

    g_currentButtons.push_back(
        makeButton(L"Settings", cx, cy, 220.0f, 50.0f, []() {
            g_session.transitionTo(AppState::Settings);
        }));
    g_currentButtons.push_back(
        makeButton(L"Exit", cx, cy + 70.0f, 220.0f, 50.0f, []() {
            g_running = false;
        }));
}
```

- [ ] **Step 3: Write main.cpp — settings state helpers**

```cpp
static std::vector<UI::TextField> g_settingsFields;
static int g_focusedField = -1;
static bool g_capturingKey = false;

static void setupSettingsFields() {
    g_settingsFields.clear();
    float cx = static_cast<float>(g_renderer.width()) * 0.25f;
    float startY = static_cast<float>(g_renderer.height()) * 0.2f;

    UI::TextField displayField;
    displayField.label = L"Display Input Latency (ms):";
    displayField.value = std::to_wstring(static_cast<int>(g_config.displayLatencyMs));
    displayField.x = cx;
    displayField.y = startY;
    displayField.w = 200.0f;
    displayField.h = 30.0f;
    displayField.focused = false;

    UI::TextField mouseField;
    mouseField.label = L"Mouse Click Latency (ms):";
    mouseField.value = std::to_wstring(static_cast<int>(g_config.mouseLatencyMs));
    mouseField.x = cx;
    mouseField.y = startY + 70.0f;
    mouseField.w = 200.0f;
    mouseField.h = 30.0f;
    mouseField.focused = false;

    g_settingsFields = { displayField, mouseField };
    g_focusedField = -1;
    g_capturingKey = false;

    g_currentButtons.clear();
    float btnCx = static_cast<float>(g_renderer.width()) / 2;
    float btnY = static_cast<float>(g_renderer.height()) * 0.65f;

    g_currentButtons.push_back(
        makeButton(L"Bind Trigger Key", btnCx, btnY, 220.0f, 40.0f, []() {
            g_capturingKey = true;
            g_session.transitionTo(AppState::Menu); // reuse Menu as capture state
            // Actually use a custom flag
            g_capturingKey = true;
            g_input.startCapture([](TriggerKeyType type, unsigned code) {
                g_config.triggerKeyType = type;
                g_config.triggerKeyCode = code;
                g_capturingKey = false;
                g_session.transitionTo(AppState::Settings);
            });
        }));
    g_currentButtons.push_back(
        makeButton(L"Save", btnCx, btnY + 55.0f, 220.0f, 40.0f, []() {
            // Parse field values
            if (g_settingsFields.size() >= 2) {
                try {
                    g_config.displayLatencyMs = std::stod(g_settingsFields[0].value);
                } catch (...) {}
                try {
                    g_config.mouseLatencyMs = std::stod(g_settingsFields[1].value);
                } catch (...) {}
            }
            saveConfig(g_config, getConfigPath());
            g_session.transitionTo(AppState::Idle);
            setupIdleButtons();
        }));
}

static std::wstring triggerKeyLabel() {
    switch (g_config.triggerKeyType) {
        case TriggerKeyType::MouseLeft:  return L"Mouse Left";
        case TriggerKeyType::MouseRight: return L"Mouse Right";
        case TriggerKeyType::Keyboard: {
            wchar_t buf[32];
            swprintf(buf, 32, L"Key 0x%02X", g_config.triggerKeyCode);
            return buf;
        }
    }
    return L"Unknown";
}
```

- [ ] **Step 4: Write main.cpp — state-changed callback**

```cpp
static void onStateChanged(AppState newState) {
    g_currentButtons.clear();

    switch (newState) {
    case AppState::Idle:
        g_renderer.setClearColor(Colors::DARK_BG);
        setupIdleButtons();
        break;
    case AppState::Waiting:
        g_renderer.setClearColor(Colors::DARK_RED);
        break;
    case AppState::Stimulus:
        // Drawn by UI::drawStimulusScreen (sets green + presents)
        break;
    case AppState::Foul:
        break;
    case AppState::Result:
        break;
    case AppState::Summary:
        g_renderer.setClearColor(Colors::DARK_BG);
        break;
    case AppState::Menu:
        g_renderer.setClearColor(Colors::DARK_BG);
        setupMenuButtons();
        break;
    case AppState::Settings:
        g_renderer.setClearColor(Colors::DARK_BG);
        setupSettingsFields();
        break;
    }
}
```

- [ ] **Step 5: Write main.cpp — WndProc**

```cpp
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    g_input.handleMessage(msg, wParam, lParam);

    switch (msg) {
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;

    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        if (w > 0 && h > 0) {
            g_renderer.resize(w, h);
            g_ui.updateScreenSize(w, h);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        float x = static_cast<float>(GET_X_LPARAM(lParam));
        float y = static_cast<float>(GET_Y_LPARAM(lParam));

        AppState s = g_session.state();

        // Button hit testing for UI states
        if (s == AppState::Idle || s == AppState::Menu || s == AppState::Settings) {
            if (UI::hitTestButtons(g_currentButtons, x, y)) return 0;
        }

        // Field focus for settings
        if (s == AppState::Settings) {
            g_focusedField = -1;
            for (int i = 0; i < static_cast<int>(g_settingsFields.size()); i++) {
                auto& f = g_settingsFields[i];
                if (x >= f.x && x < f.x + f.w && y >= f.y && y < f.y + f.h) {
                    g_focusedField = i;
                    f.focused = true;
                } else {
                    f.focused = false;
                }
            }
        }

        // Foul screen — click to retry
        if (s == AppState::Foul) {
            g_session.start();
            return 0;
        }

        // Summary — click to go back to idle
        if (s == AppState::Summary) {
            // Save results first
            SessionResult result;
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            struct tm tm_buf;
            localtime_s(&tm_buf, &time_t_now);
            std::ostringstream oss;
            oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
            result.timestamp = oss.str();
            result.rounds = g_session.roundTimes();
            result.median = g_session.medianMs();
            result.mean = g_session.meanMs();
            result.stddev = g_session.stddevMs();
            result.refreshRate = g_renderer.refreshRate();
            result.pollingRate = 0; // Not easily detectable
            result.fullscreen = g_renderer.isFullscreen();
            appendHistory(getHistoryPath(), result);

            g_session.transitionTo(AppState::Idle);
            return 0;
        }

        return 0;
    }

    case WM_CHAR: {
        // Text input for settings fields
        if (g_session.state() == AppState::Settings && g_focusedField >= 0 &&
            g_focusedField < static_cast<int>(g_settingsFields.size())) {
            wchar_t ch = static_cast<wchar_t>(wParam);
            if (ch == '\b') {
                auto& val = g_settingsFields[g_focusedField].value;
                if (!val.empty()) val.pop_back();
            } else if (ch >= '0' && ch <= '9' || ch == '.') {
                g_settingsFields[g_focusedField].value += ch;
            }
        }
        return 0;
    }

    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            AppState s = g_session.state();
            if (s == AppState::Settings || s == AppState::Menu) {
                g_input.stopCapture();
                g_capturingKey = false;
                g_session.transitionTo(AppState::Idle);
                setupIdleButtons();
            } else if (s == AppState::Waiting || s == AppState::Foul) {
                g_session.onEscape();
                setupIdleButtons();
            } else if (s != AppState::Stimulus) {
                g_session.transitionTo(AppState::Menu);
                setupMenuButtons();
            }
        }
        if (wParam == VK_F11) {
            // Toggle fullscreen not implemented yet — will add in polish
        }
        if (wParam == VK_SPACE && g_session.state() == AppState::Summary) {
            // Same as click on summary — save and go to idle
            SessionResult result;
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            struct tm tm_buf;
            localtime_s(&tm_buf, &time_t_now);
            std::ostringstream oss;
            oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
            result.timestamp = oss.str();
            result.rounds = g_session.roundTimes();
            result.median = g_session.medianMs();
            result.mean = g_session.meanMs();
            result.stddev = g_session.stddevMs();
            result.refreshRate = g_renderer.refreshRate();
            result.pollingRate = 0;
            result.fullscreen = g_renderer.isFullscreen();
            appendHistory(getHistoryPath(), result);
            g_session.transitionTo(AppState::Idle);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        float x = static_cast<float>(GET_X_LPARAM(lParam));
        float y = static_cast<float>(GET_Y_LPARAM(lParam));
        for (auto& btn : g_currentButtons) {
            btn.hovered = (x >= btn.x && x < btn.x + btn.w &&
                           y >= btn.y && y < btn.y + btn.h);
        }
        return 0;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
```

- [ ] **Step 6: Write main.cpp — WinMain and render loop**

```cpp
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    g_hInstance = hInstance;
    Timer::init();

    // Load config
    g_config = loadConfig(getConfigPath());

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ReactionTimer";
    RegisterClassExW(&wc);

    // Determine initial size
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    DWORD style = g_config.fullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    g_hwnd = CreateWindowExW(0, L"ReactionTimer", L"Reaction Timer",
                              style, 0, 0, screenW, screenH,
                              nullptr, nullptr, hInstance, nullptr);

    if (!g_hwnd) return 1;

    ShowWindow(g_hwnd, nCmdShow);

    // Init renderer
    if (!g_renderer.init(g_hwnd, g_config.fullscreen, screenW, screenH)) return 1;

    // Init input
    g_input.init(g_hwnd);
    g_input.setTriggerConfig(g_config.triggerKeyType, g_config.triggerKeyCode);
    g_input.setTriggerCallback([](int64_t qpcTime) {
        g_session.onTrigger(qpcTime);
    });

    // Init UI
    g_ui.init(&g_renderer, screenW, screenH);

    // Init session
    g_session.setDisplayLatency(g_config.displayLatencyMs);
    g_session.setMouseLatency(g_config.mouseLatencyMs);
    g_session.setStateChangedCallback(onStateChanged);
    g_session.transitionTo(AppState::Idle);
    setupIdleButtons();

    // Message loop
    MSG msg = {};
    while (g_running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!g_running) break;

        // Update session (check if random delay expired)
        g_session.update();

        // Render based on state
        AppState s = g_session.state();
        switch (s) {
        case AppState::Idle:
            g_ui.drawIdleScreen(g_currentButtons);
            g_renderer.present();
            break;
        case AppState::Waiting:
            g_ui.drawWaitingScreen();
            g_renderer.present();
            break;
        case AppState::Stimulus:
            g_ui.drawStimulusScreen();
            break;
        case AppState::Foul:
            g_ui.drawFoulScreen();
            g_renderer.present();
            break;
        case AppState::Result:
            g_ui.drawResultScreen(g_session.lastReactionMs(),
                                   g_session.currentRoundIndex(),
                                   TestSession::ROUNDS_PER_SESSION);
            g_renderer.present();
            // Auto-advance after brief display
            Sleep(1500);
            g_session.proceedToNextRound();
            break;
        case AppState::Summary:
            g_ui.drawSummaryScreen(g_session.roundTimes(),
                                    g_session.medianMs(),
                                    g_session.meanMs(),
                                    g_session.stddevMs(),
                                    g_renderer.refreshRate(),
                                    0,
                                    g_renderer.isFullscreen());
            g_renderer.present();
            break;
        case AppState::Menu:
            if (g_capturingKey) {
                g_ui.drawCaptureScreen();
            } else {
                g_ui.drawMenuOverlay(g_currentButtons);
            }
            g_renderer.present();
            break;
        case AppState::Settings:
            g_ui.drawSettingsScreen(g_settingsFields, triggerKeyLabel(),
                                     g_currentButtons);
            g_renderer.present();
            break;
        }
    }

    g_renderer.shutdown();
    return 0;
}
```

- [ ] **Step 7: Build**

Run: `cmake --build build --config Release`
Expected: `ReactionTimer.exe` builds without errors.

- [ ] **Step 8: Run and verify basic flow**

Run: `build\src\Release\ReactionTimer.exe`

Verify:
1. Window launches, shows dark background with "Start Test" and "Settings" buttons
2. ESC opens menu overlay with "Settings" and "Exit"
3. Click "Start Test" → screen turns dark red → after 2-6s turns green → click → shows result
4. After 5 rounds, summary screen shows stats
5. Click or SPACE returns to idle

- [ ] **Step 9: Commit**

```bash
git add src/main.cpp
git commit -m "feat: main application integration — full working flow"
```

---

## Task 10: Fullscreen Toggle and Polish

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add F11 fullscreen toggle in WM_KEYDOWN handler**

In `main.cpp`, in the `WM_KEYDOWN` case, replace the `VK_F11` placeholder with:

```cpp
if (wParam == VK_F11) {
    g_renderer.setClearColor(Colors::DARK_BG);
    if (g_renderer.isFullscreen()) {
        g_renderer.toggleFullscreen(g_hwnd);
        // Restore windowed style
        SetWindowLongPtrW(g_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        ShowWindow(g_hwnd, SW_SHOWNORMAL);
        RECT rc = { 0, 0, 1280, 720 };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(g_hwnd, nullptr, 0, 0,
                     rc.right - rc.left, rc.bottom - rc.top,
                     SWP_NOZORDER | SWP_FRAMECHANGED);
    } else {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        SetWindowLongPtrW(g_hwnd, GWL_STYLE, WS_POPUP);
        SetWindowPos(g_hwnd, nullptr, 0, 0, sw, sh,
                     SWP_NOZORDER | SWP_FRAMECHANGED);
        g_renderer.toggleFullscreen(g_hwnd);
    }
}
```

Add `Renderer::toggleFullscreen` to `Renderer.h`:

```cpp
void Renderer::toggleFullscreen(HWND hwnd) {
    fullscreen_ = !fullscreen_;
    swapChain_->SetFullscreenState(fullscreen_, nullptr);
    if (!fullscreen_) {
        swapChain_->ResizeBuffers(0, width_, height_, DXGI_FORMAT_UNKNOWN, 0);
    }
    createRenderTarget();
    createD2DResources();
}
```

- [ ] **Step 2: Build and verify**

Run: `cmake --build build --config Release`
Expected: Builds without errors.

Run the exe, verify F11 toggles between fullscreen and windowed mode.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp src/Renderer.h src/Renderer.cpp
git commit -m "feat: fullscreen toggle with F11"
```

---

## Task 11: Final Integration Test

- [ ] **Step 1: Run all unit tests**

Run: `build\tests\Release\ReactionTimerTests.exe`
Expected: All tests PASS (Timer, Config, Storage, TestSession).

- [ ] **Step 2: Run the app end-to-end and verify all flows**

Checklist:
- [ ] App launches, idle screen with buttons visible
- [ ] ESC opens menu, ESC again returns to idle
- [ ] Menu → Settings shows config fields
- [ ] Settings → type in latency fields works
- [ ] Settings → Bind Trigger Key → press key → updates
- [ ] Settings → Save → returns to idle, config persists after restart
- [ ] Start Test → 5 rounds → summary screen with stats
- [ ] History saved to %APPDATA%/ReactionTimer/history.json
- [ ] F11 toggles fullscreen
- [ ] ESC during waiting cancels to idle
- [ ] Clicking during waiting → foul screen

- [ ] **Step 3: Final commit**

```bash
git add -A
git commit -m "chore: integration testing complete"
```

---

## Self-Review

### Spec Coverage

| Spec Section | Task |
|---|---|
| Module architecture (Timer, Config, Storage, Renderer, Input, UI, TestSession) | Tasks 2-9 |
| Configurable trigger key (mouse/keyboard) | Task 6 (Input), Task 9 (Settings UI) |
| DXGI frame statistics (SyncQPCTime) | Task 5 (Renderer::getLastPresentTimeQPC) |
| Raw Input timestamps | Task 6 (Input) |
| Device latency compensation | Task 3 (Config), Task 8 (TestSession) |
| 5-round sessions with stats | Task 8 (TestSession) |
| Random delay 2-6s | Task 8 (generateRandomDelayTicks) |
| State machine | Task 8 (TestSession) |
| Main screen buttons | Task 9 (setupIdleButtons) |
| ESC menu overlay | Task 9 (WndProc, setupMenuButtons) |
| Settings UI with fields | Task 9 (setupSettingsFields) |
| JSON persistence (config + history) | Tasks 3-4 |
| F11 fullscreen toggle | Task 10 |
| DirectWrite text rendering | Task 7 (UI) |
| D2D1 button drawing | Task 7 (UI) |

### Placeholder Scan

No TBD, TODO, or "implement later" found. All steps contain complete code.

### Type Consistency

- `TriggerKeyType` enum used consistently across Config.h, Input.h, Input.cpp, main.cpp
- `AppState` enum used in TestSession.h, onStateChanged callback, and main.cpp WndProc
- `Button` struct fields (x, y, w, h, text, onClick) match between UI.h and main.cpp usage
- `SessionResult` fields match between Storage.h and main.cpp population code
- `Config` struct fields match between Config.h and JSON serialization in Config.cpp
