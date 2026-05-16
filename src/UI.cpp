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
    renderer_->beginUI();
    renderer_->endUI();
    renderer_->present();
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
