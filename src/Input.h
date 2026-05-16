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
