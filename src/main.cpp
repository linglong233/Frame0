#include "Timer.h"
#include "Config.h"
#include "Storage.h"
#include "Renderer.h"
#include "Input.h"
#include "UI.h"
#include "TestSession.h"
#include "FullscreenRecovery.h"

#include <windowsx.h>
#include <shlobj.h>
#include <avrt.h>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "avrt.lib")

static Renderer g_renderer;
static Input g_input;
static UI g_ui;
static TestSession g_session;
static Config g_config;
static FullscreenRecovery g_fullscreenRecovery;

static HINSTANCE g_hInstance = nullptr;
static HWND g_hwnd = nullptr;
static bool g_running = true;
static bool g_initialized = false;
static int64_t g_summaryStartTime_ = 0;

static int64_t g_resultStartTime_ = 0;

static int64_t g_foulStartTime_ = 0;
// Win32 retry/escape actions in Foul are accepted only after the foul screen
// has been up this long. The legacy message of the SAME physical input that
// triggered the foul (WM_INPUT is queued before WM_LBUTTONDOWN/WM_KEYDOWN)
// arrives microseconds after the Foul transition and must not dismiss the
// screen before the user ever sees it. Message ordering across categories is
// not guaranteed by the OS, so the guard is time-based, not order-based.
static constexpr double kFoulInputGuardMs = 300.0;

// Guards saveCurrentResults() against re-entry: onStateChanged(Summary) is a
// single transition, but the flag makes the save idempotent and resilient to
// future refactors. Reset whenever a fresh session starts.
static bool g_resultsSaved = false;

static std::vector<Button> g_currentButtons;
static std::vector<UI::TextField> g_settingsFields;
static int g_focusedField = -1;
static bool g_capturingKey = false;

static std::filesystem::path getAppDataDir() {
    std::filesystem::path base;
    PWSTR wpath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &wpath)) && wpath) {
        base = wpath;
        CoTaskMemFree(wpath);
    } else {
        if (wpath) CoTaskMemFree(wpath);
        char path[MAX_PATH];
        SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path);
        base = path;
    }
    auto dir = base / "Frame0";
    std::filesystem::create_directories(dir);
    return dir;
}

static std::filesystem::path getConfigPath() {
    return getAppDataDir() / "config.json";
}

static std::filesystem::path getHistoryPath() {
    return getAppDataDir() / "history.json";
}

// Formats a latency value for the settings field: integers show without a
// decimal point, fractional values keep one decimal. Avoids the silent
// precision loss that static_cast<int> caused (e.g. 4.5 -> "4").
static std::wstring formatLatency(double ms) {
    wchar_t buf[32];
    if (ms == std::floor(ms)) {
        swprintf(buf, 32, L"%d", static_cast<int>(ms));
    } else {
        swprintf(buf, 32, L"%.1f", ms);
    }
    return buf;
}

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
            // Session start is the composition point for latency constants:
            // user latencies come from the current config (settings may have
            // changed since startup), the scanout residual from the current
            // display mode. Fixed per session, like a calibrated rig.
            g_session.setDisplayLatency(g_config.displayLatencyMs);
            g_session.setMouseLatency(g_config.mouseLatencyMs);
            g_session.setScanoutCompensationMs(g_renderer.scanoutCompensationMs());
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

static void setupSettingsFields() {
    g_settingsFields.clear();
    float cx = static_cast<float>(g_renderer.width()) * 0.25f;
    float startY = static_cast<float>(g_renderer.height()) * 0.2f;

    UI::TextField displayField;
    displayField.label = L"Display Input Latency (ms):";
    displayField.value = formatLatency(g_config.displayLatencyMs);
    displayField.x = cx;
    displayField.y = startY;
    displayField.w = 200.0f;
    displayField.h = 30.0f;
    displayField.focused = false;

    UI::TextField mouseField;
    mouseField.label = L"Mouse Click Latency (ms):";
    mouseField.value = formatLatency(g_config.mouseLatencyMs);
    mouseField.x = cx;
    mouseField.y = startY + 70.0f;
    mouseField.w = 200.0f;
    mouseField.h = 30.0f;
    mouseField.focused = false;

    UI::TextField pollingField;
    pollingField.label = L"Mouse Polling Rate (Hz):";
    pollingField.value = std::to_wstring(g_config.pollingRate);
    pollingField.x = cx;
    pollingField.y = startY + 140.0f;
    pollingField.w = 200.0f;
    pollingField.h = 30.0f;
    pollingField.focused = false;

    g_settingsFields = { displayField, mouseField, pollingField };
    g_focusedField = -1;
    g_capturingKey = false;

    g_currentButtons.clear();
    float btnCx = static_cast<float>(g_renderer.width()) / 2;
    float btnY = static_cast<float>(g_renderer.height()) * 0.68f;

    g_currentButtons.push_back(
        makeButton(L"Bind Trigger Key", btnCx, btnY, 220.0f, 40.0f, []() {
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
            if (g_settingsFields.size() >= 3) {
                try {
                    g_config.displayLatencyMs = std::stod(g_settingsFields[0].value);
                } catch (...) {}
                try {
                    g_config.mouseLatencyMs = std::stod(g_settingsFields[1].value);
                } catch (...) {}
                try {
                    g_config.pollingRate = std::stoi(g_settingsFields[2].value);
                } catch (...) {}
            }
            saveConfig(g_config, getConfigPath());
            g_session.transitionTo(AppState::Idle);
            setupIdleButtons();
        }));
}

static void saveCurrentResults() {
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
    result.pollingRate = g_config.pollingRate;
    result.fullscreen = g_renderer.isFullscreen();
    appendHistory(getHistoryPath(), result);
}

// Cursor is hidden across the measured states (Waiting/Stimulus/Foul).
// ShowCursor uses a display counter, so the guard keeps hide/show calls
// strictly balanced across arbitrary state transitions.
static bool g_cursorHidden = false;
static void setCursorHiddenForTest(bool hidden) {
    if (hidden == g_cursorHidden) return;
    ShowCursor(hidden ? FALSE : TRUE);
    g_cursorHidden = hidden;
}

static void onStateChanged(AppState newState) {
    g_currentButtons.clear();

    switch (newState) {
    case AppState::Idle:
        g_renderer.setClearColor(Colors::DARK_BG);
        setCursorHiddenForTest(false);
        setupIdleButtons();
        break;
    case AppState::Waiting:
        g_renderer.setClearColor(Colors::DARK_RED);
        setCursorHiddenForTest(true);
        g_resultsSaved = false;  // fresh round/session: allow next Summary to save
        break;
    case AppState::Stimulus:
        setCursorHiddenForTest(true);
        break;
    case AppState::Foul:
        g_renderer.setClearColor({ 0.8f, 0.0f, 0.0f, 1.0f });
        setCursorHiddenForTest(true);
        g_foulStartTime_ = Timer::now();
        break;
    case AppState::Result:
        g_renderer.setClearColor(Colors::DARK_BG);
        setCursorHiddenForTest(false);
        g_resultStartTime_ = Timer::now();
        break;
    case AppState::Summary:
        g_renderer.setClearColor(Colors::DARK_BG);
        setCursorHiddenForTest(false);
        if (!g_resultsSaved) {
            saveCurrentResults();
            g_resultsSaved = true;
        }
        g_summaryStartTime_ = Timer::now();
        break;
    case AppState::Menu:
        g_renderer.setClearColor(Colors::DARK_BG);
        setCursorHiddenForTest(false);
        setupMenuButtons();
        break;
    case AppState::Settings:
        g_renderer.setClearColor(Colors::DARK_BG);
        setCursorHiddenForTest(false);
        setupSettingsFields();
        break;
    }
}

static void syncClientSizeFromWindow(HWND hwnd) {
    if (!g_initialized) return;

    RECT client{};
    if (!GetClientRect(hwnd, &client)) return;

    int w = client.right - client.left;
    int h = client.bottom - client.top;
    if (w <= 0 || h <= 0) return;

    g_renderer.resize(w, h);
    g_ui.updateScreenSize(w, h);
}

static void applyFullscreenWindowStyle(HWND hwnd) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    SetWindowLongPtrW(hwnd, GWL_STYLE, WS_POPUP);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, sw, sh,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
}

static void applyWindowedStyle(HWND hwnd) {
    SetWindowLongPtrW(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
    ShowWindow(hwnd, SW_SHOWNORMAL);

    RECT rc = { 0, 0, 1280, 720 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    SetWindowPos(hwnd, nullptr, 0, 0,
                 rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
}

static void applyFullscreenAction(FullscreenRecoveryAction action) {
    if (!g_initialized || !g_hwnd) return;

    switch (action) {
    case FullscreenRecoveryAction::RestoreFullscreen:
        applyFullscreenWindowStyle(g_hwnd);
        if (g_renderer.restoreFullscreen(g_hwnd)) {
            g_ui.updateScreenSize(g_renderer.width(), g_renderer.height());
        } else {
            syncClientSizeFromWindow(g_hwnd);
        }
        break;
    case FullscreenRecoveryAction::ExitFullscreen:
        g_renderer.setFullscreen(g_hwnd, false);
        applyWindowedStyle(g_hwnd);
        syncClientSizeFromWindow(g_hwnd);
        break;
    case FullscreenRecoveryAction::None:
        break;
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    g_input.handleMessage(msg, wParam, lParam);

    switch (msg) {
    case WM_DESTROY:
        setCursorHiddenForTest(false);
        g_running = false;
        PostQuitMessage(0);
        return 0;

    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        // Always process WM_SIZE; DXGI needs this to complete mode switches.
        if (w > 0 && h > 0 && g_initialized) {
            g_renderer.resize(w, h);
            g_ui.updateScreenSize(w, h);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        float x = static_cast<float>(GET_X_LPARAM(lParam));
        float y = static_cast<float>(GET_Y_LPARAM(lParam));
        AppState s = g_session.state();

        if (s == AppState::Idle || s == AppState::Menu || s == AppState::Settings) {
            if (UI::hitTestButtons(g_currentButtons, x, y)) return 0;
        }

        if (s == AppState::Summary && Timer::elapsedMs(g_summaryStartTime_) >= 1000.0) {
            g_session.transitionTo(AppState::Idle);
            return 0;
        }

        // Foul retry goes through Win32 (UI interaction), not Raw Input. The
        // time guard keeps the foul screen visible despite the legacy message
        // of the same click that triggered the foul (see kFoulInputGuardMs).
        if (s == AppState::Foul) {
            if (Timer::elapsedMs(g_foulStartTime_) >= kFoulInputGuardMs) {
                g_session.retryRound();
            }
            return 0;
        }

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

        return 0;
    }

    case WM_CHAR: {
        if (g_session.state() == AppState::Settings && g_focusedField >= 0 &&
            g_focusedField < static_cast<int>(g_settingsFields.size())) {
            wchar_t ch = static_cast<wchar_t>(wParam);
            if (ch == L'\b') {
                auto& val = g_settingsFields[g_focusedField].value;
                if (!val.empty()) val.pop_back();
            } else if (ch >= L'0' && ch <= L'9') {
                g_settingsFields[g_focusedField].value += ch;
            } else if (ch == L'.') {
                auto& val = g_settingsFields[g_focusedField].value;
                if (val.find(L'.') == std::wstring::npos) val += ch;
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
            } else if (s == AppState::Waiting) {
                g_session.onEscape();
                setupIdleButtons();
            } else if (s == AppState::Foul) {
                // Guarded: if ESC is the trigger key, the same-press KEYDOWN
                // must not abort the session before the foul screen is seen.
                if (Timer::elapsedMs(g_foulStartTime_) >= kFoulInputGuardMs) {
                    g_session.onEscape();
                    setupIdleButtons();
                }
            } else if (s != AppState::Stimulus) {
                g_session.transitionTo(AppState::Menu);
                setupMenuButtons();
            }
        }
        if (wParam == VK_SPACE || wParam == VK_RETURN) {
            AppState s = g_session.state();
            if (s == AppState::Foul) {
                // Same guard as the click retry (SPACE/RETURN may be the
                // trigger key; their own KEYDOWN would dismiss the foul).
                if (Timer::elapsedMs(g_foulStartTime_) >= kFoulInputGuardMs) {
                    g_session.retryRound();
                }
            } else if (s == AppState::Summary &&
                       Timer::elapsedMs(g_summaryStartTime_) >= 1000.0) {
                g_session.transitionTo(AppState::Idle);
            }
        }
        if (wParam == VK_F11) {
            bool actualFullscreen = g_renderer.syncFullscreenState();
            bool desiredFullscreen = !g_fullscreenRecovery.desiredFullscreen();
            auto action = g_fullscreenRecovery.setDesiredFullscreen(
                desiredFullscreen, actualFullscreen);
            applyFullscreenAction(action);
            if (!desiredFullscreen && action == FullscreenRecoveryAction::None) {
                applyWindowedStyle(hwnd);
                syncClientSizeFromWindow(hwnd);
            }
        }
        return 0;
    }

    case WM_ACTIVATEAPP: {
        bool active = (wParam != 0);
        if (g_initialized) {
            bool actualFullscreen = active ? g_renderer.syncFullscreenState()
                                           : g_renderer.isFullscreen();
            auto action = g_fullscreenRecovery.onActivationChanged(
                active, actualFullscreen);
            applyFullscreenAction(action);
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    g_hInstance = hInstance;
    Timer::init();

    // Scheduling guard for the measured window: keep the render/input thread
    // from being preempted while a reaction is in flight. Best effort — a
    // failure just falls back to normal scheduling. Restored on exit below.
    DWORD savedPriorityClass = GetPriorityClass(GetCurrentProcess());
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    DWORD mmcssIndex = 0;
    HANDLE mmcssTask = AvSetMmThreadCharacteristicsW(L"Games", &mmcssIndex);

    g_config = loadConfig(getConfigPath());
    g_fullscreenRecovery.setDesiredFullscreen(g_config.fullscreen,
                                              g_config.fullscreen);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"Frame0";
    RegisterClassExW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    DWORD style = g_config.fullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    g_hwnd = CreateWindowExW(0, L"Frame0", L"Frame0",
                              style, 0, 0, screenW, screenH,
                              nullptr, nullptr, hInstance, nullptr);

    if (!g_hwnd) {
        MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_OK);
        return 1;
    }

    ShowWindow(g_hwnd, nCmdShow);

    if (!g_renderer.init(g_hwnd, g_config.fullscreen, screenW, screenH)) {
        MessageBoxW(nullptr, L"Failed to initialize DirectX 11 renderer.\nMake sure your GPU supports D3D11.", L"Error", MB_OK);
        return 1;
    }

    g_input.init(g_hwnd);
    g_input.setTriggerConfig(g_config.triggerKeyType, g_config.triggerKeyCode);
    g_input.setTriggerCallback([](int64_t qpcTime) {
        g_session.onTrigger(qpcTime);
    });

    g_ui.init(&g_renderer, screenW, screenH);

    g_session.setStateChangedCallback(onStateChanged);
    g_session.transitionTo(AppState::Idle);
    setupIdleButtons();

    g_initialized = true;
    applyFullscreenAction(g_fullscreenRecovery.setDesiredFullscreen(
        g_config.fullscreen, g_renderer.syncFullscreenState()));

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

        if (!g_fullscreenRecovery.isAppActive()) {
            Sleep(50);
            continue;
        }

        // Skip rendering if minimized
        if (IsIconic(g_hwnd)) {
            Sleep(50);
            continue;
        }

        g_session.update();

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
            // Present the green frame exactly once — the swap chain retains it
            // until the next state presents. Re-presenting each iteration would
            // block this thread in Present(1) until the next VBlank, delaying
            // WM_INPUT dispatch by up to a refresh period into the measured
            // reaction time.
            if (g_session.needsStimulusTime()) {
                g_ui.drawStimulusScreen();
                g_session.setStimulusTime(g_renderer.getLastPresentTimeQPC());
            } else {
                // Frame already on screen: park until input arrives so WM_INPUT
                // is dispatched the moment it is queued. The short timeout is a
                // belt-and-braces wake; input wakes immediately regardless.
                MsgWaitForMultipleObjectsEx(0, nullptr, 20, QS_ALLINPUT,
                                            MWMO_INPUTAVAILABLE);
            }
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
            if (Timer::elapsedMs(g_resultStartTime_) >= 3000.0) {
                g_session.proceedToNextRound();
            }
            break;
        case AppState::Summary:
            g_ui.drawSummaryScreen(g_session.roundTimes(),
                                    g_session.medianMs(),
                                    g_session.meanMs(),
                                    g_session.stddevMs(),
                                    g_renderer.refreshRate(),
                                    g_config.pollingRate,
                                    g_renderer.isFullscreen(),
                                    g_session.appliedCompensationMs(),
                                    g_session.clampedRounds());
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
    if (mmcssTask) AvRevertMmThreadCharacteristics(mmcssTask);
    if (savedPriorityClass != 0) {
        SetPriorityClass(GetCurrentProcess(), savedPriorityClass);
    }
    return 0;
}
