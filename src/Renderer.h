#pragma once
#include <d3d11.h>
#include <dxgi1_4.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>
#include <unordered_map>

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

    void drawText(const std::wstring& text, float x, float y, float fontSize,
                  const Color& color, bool centerX = false, bool centerY = false);
    void fillRectangle(float x, float y, float w, float h, const Color& color);
    void drawRectangleOutline(float x, float y, float w, float h, const Color& color, float strokeWidth = 1.0f);

    void resize(int width, int height);
    bool setFullscreen(HWND hwnd, bool fullscreen);
    void toggleFullscreen(HWND hwnd);
    bool restoreFullscreen(HWND hwnd);
    bool isFullscreen() const;
    bool syncFullscreenState();
    int refreshRate() const;
    // Deterministic submit->scanout residual of the stimulus frame in ms.
    // The Waiting loop is vsync-paced, so the stimulus submit lands just
    // after a VBlank and the frame scans out at the next one (~1 frame).
    // Windowed + tearing presents immediately (~0); windowed without tearing
    // goes through DWM composition (~1 frame, approximation).
    double scanoutCompensationMs() const;
    int width() const;
    int height() const;

private:
    bool createDeviceAndSwapChain(HWND hwnd);
    bool createRenderTarget();
    bool createD2DResources();   // device/context/brush — created once, survive back-buffer changes
    void refreshD2DTarget();     // bind current back buffer as D2D target bitmap
    IDWriteTextFormat* getTextFormat(float fontSize);  // cached lookup/create

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain3> swapChain_;
    ComPtr<ID3D11RenderTargetView> rtv_;

    ComPtr<ID2D1Factory1> d2dFactory_;
    ComPtr<ID2D1Device> d2dDevice_;
    ComPtr<ID2D1DeviceContext> d2dContext_;   // D2D render target (derives from ID2D1RenderTarget)
    ComPtr<ID2D1Bitmap1> d2dBitmap_;          // back buffer bound as target
    ComPtr<IDWriteFactory> dwFactory_;
    ComPtr<ID2D1SolidColorBrush> brush_;      // reused via SetColor
    std::unordered_map<int, ComPtr<IDWriteTextFormat>> textFormats_;  // keyed by fontSize*10

    bool d2dBitmapStale_ = true;

    Color clearColor_{ 0, 0, 0, 1 };
    int64_t lastPresentQPC_ = 0;
    bool fullscreen_ = false;
    int width_ = 0;
    int height_ = 0;
    int refreshRate_ = 60;
    bool tearingSupported_ = false;
};
