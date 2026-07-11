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
    if (size > sizeof(RAWINPUT)) size = sizeof(RAWINPUT);

    RAWINPUT raw{};
    UINT copied = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT,
                                  &raw, &size, sizeof(RAWINPUTHEADER));
    if (copied == 0 || copied == static_cast<UINT>(-1)) return false;

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
