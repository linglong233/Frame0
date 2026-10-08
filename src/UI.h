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
                           int pollingRate, bool wasFullscreen,
                           double compMs);

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
