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
static bool g_initialized = false;

static int64_t g_resultStartTime_ = 0;

static std::vector<Button> g_currentButtons;
static std::vector<UI::TextField> g_settingsFields;
static int g_focusedField = -1;
static bool g_capturingKey = false;

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
            g_input.startCapture([](TriggerKeyType type, unsigned code) {
                g_config.triggerKeyType = type;
                g_config.triggerKeyCode = code;
                g_capturingKey = false;
                g_session.transitionTo(AppState::Settings);
            });
        }));
    g_currentButtons.push_back(
        makeButton(L"Save", btnCx, btnY + 55.0f, 220.0f, 40.0f, []() {
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
    result.pollingRate = 0;
    result.fullscreen = g_renderer.isFullscreen();
    appendHistory(getHistoryPath(), result);
}

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
        break;
    case AppState::Foul:
        break;
    case AppState::Result:
        g_resultStartTime_ = Timer::now();
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

        if (s == AppState::Summary) {
            saveCurrentResults();
            g_session.transitionTo(AppState::Idle);
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

        if (s == AppState::Summary) {
            saveCurrentResults();
            g_session.transitionTo(AppState::Idle);
            return 0;
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
            } else if ((ch >= L'0' && ch <= L'9') || ch == L'.') {
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
        if (wParam == VK_SPACE && g_session.state() == AppState::Summary) {
            saveCurrentResults();
            g_session.transitionTo(AppState::Idle);
        }
        if (wParam == VK_F11) {
            if (g_renderer.isFullscreen()) {
                g_renderer.toggleFullscreen(hwnd);
                SetWindowLongPtrW(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
                ShowWindow(hwnd, SW_SHOWNORMAL);
                RECT rc = { 0, 0, 1280, 720 };
                AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
                SetWindowPos(hwnd, nullptr, 0, 0,
                             rc.right - rc.left, rc.bottom - rc.top,
                             SWP_NOZORDER | SWP_FRAMECHANGED);
            } else {
                int sw = GetSystemMetrics(SM_CXSCREEN);
                int sh = GetSystemMetrics(SM_CYSCREEN);
                SetWindowLongPtrW(hwnd, GWL_STYLE, WS_POPUP);
                SetWindowPos(hwnd, nullptr, 0, 0, sw, sh,
                             SWP_NOZORDER | SWP_FRAMECHANGED);
                g_renderer.toggleFullscreen(hwnd);
            }
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

    g_config = loadConfig(getConfigPath());

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ReactionTimer";
    RegisterClassExW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    DWORD style = g_config.fullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    g_hwnd = CreateWindowExW(0, L"ReactionTimer", L"Reaction Timer",
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

    g_session.setDisplayLatency(g_config.displayLatencyMs);
    g_session.setMouseLatency(g_config.mouseLatencyMs);
    g_session.setStateChangedCallback(onStateChanged);
    g_session.transitionTo(AppState::Idle);
    setupIdleButtons();

    g_initialized = true;

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
            if (Timer::elapsedMs(g_resultStartTime_) >= 1500.0) {
                g_session.proceedToNextRound();
            }
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
