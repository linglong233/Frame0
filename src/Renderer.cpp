#include "Renderer.h"
#include "Timer.h"
#include <dxgi1_5.h>
#include <cassert>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

static D2D1_COLOR_F toD2D(const Color& c) {
    return D2D1::ColorF(c.r, c.g, c.b, c.a);
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
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
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

    // Check tearing support
    ComPtr<IDXGIFactory5> factory5;
    factory.As(&factory5);
    if (factory5) {
        BOOL allowTearing = FALSE;
        HRESULT hr = factory5->CheckFeatureSupport(
            DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
        if (SUCCEEDED(hr) && allowTearing) {
            tearingSupported_ = true;
        }
    }

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width = width_;
    scDesc.Height = height_;
    scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.SampleDesc.Count = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2;
    scDesc.Scaling = DXGI_SCALING_STRETCH;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    if (tearingSupported_) {
        scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    } else {
        scDesc.Flags = 0;
    }

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc = {};
    fsDesc.Windowed = !fullscreen_;

    ComPtr<IDXGISwapChain1> sc1;
    hr = factory->CreateSwapChainForHwnd(
        device_.Get(), hwnd, &scDesc,
        fullscreen_ ? &fsDesc : nullptr, nullptr, &sc1);
    if (FAILED(hr)) return false;

    sc1.As(&swapChain_);

    if (fullscreen_) {
        hr = swapChain_->SetFullscreenState(TRUE, nullptr);
        if (FAILED(hr)) {
            fullscreen_ = false;
        }
    }

    // Detect refresh rate
    ComPtr<IDXGIOutput> output;
    swapChain_->GetContainingOutput(&output);
    DXGI_OUTPUT_DESC outputDesc;
    output->GetDesc(&outputDesc);
    DEVMODEW dm = {};
    dm.dmSize = sizeof(dm);
    EnumDisplaySettingsW(outputDesc.DeviceName, ENUM_CURRENT_SETTINGS, &dm);
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
    if (!d2dFactory_) {
        D2D1_FACTORY_OPTIONS factoryOpts = {};
#ifdef _DEBUG
        factoryOpts.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
        HRESULT hr = D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED,
            __uuidof(ID2D1Factory),
            &factoryOpts,
            reinterpret_cast<void**>(d2dFactory_.GetAddressOf()));
        if (FAILED(hr)) return false;
    }

    if (!dwFactory_) {
        HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                 __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown**>(dwFactory_.GetAddressOf()));
        if (FAILED(hr)) return false;
    }

    ComPtr<IDXGISurface> surface;
    HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&surface));
    if (FAILED(hr)) return false;

    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));

    hr = d2dFactory_->CreateDxgiSurfaceRenderTarget(surface.Get(), rtProps, &d2dTarget_);
    return SUCCEEDED(hr);
}

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

// Stimulus path: clear to the set color, present, record QPC
void Renderer::presentStimulus() {
    float color[4] = { clearColor_.r, clearColor_.g, clearColor_.b, clearColor_.a };
    context_->ClearRenderTargetView(rtv_.Get(), color);
    UINT syncInterval = fullscreen_ ? 1 : 0;
    UINT flags = (!fullscreen_ && tearingSupported_) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    swapChain_->Present(syncInterval, flags);
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    lastPresentQPC_ = li.QuadPart;
    // Recreate D2D target after flip; the back buffer has rotated.
    d2dTarget_.Reset();
    rtv_.Reset();
    createRenderTarget();
    createD2DResources();
}

int64_t Renderer::getLastPresentTimeQPC() const {
    DXGI_FRAME_STATISTICS stats = {};
    HRESULT hr = swapChain_->GetFrameStatistics(&stats);
    if (SUCCEEDED(hr) && stats.SyncQPCTime.QuadPart != 0) {
        return stats.SyncQPCTime.QuadPart;
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
    UINT flags = (!fullscreen_ && tearingSupported_) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    swapChain_->Present(syncInterval, flags);
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    lastPresentQPC_ = li.QuadPart;
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

    UINT resizeFlags = tearingSupported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, resizeFlags);
    createRenderTarget();
    createD2DResources();
}

bool Renderer::setFullscreen(HWND hwnd, bool fullscreen) {
    d2dTarget_.Reset();
    rtv_.Reset();

    HRESULT hr = swapChain_->SetFullscreenState(fullscreen ? TRUE : FALSE, nullptr);
    if (FAILED(hr)) {
        syncFullscreenState();
        createRenderTarget();
        createD2DResources();
        return false;
    }

    fullscreen_ = fullscreen;
    UINT resizeFlags = tearingSupported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    hr = swapChain_->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, resizeFlags);
    if (FAILED(hr)) {
        syncFullscreenState();
        createRenderTarget();
        createD2DResources();
        return false;
    }

    DXGI_SWAP_CHAIN_DESC desc;
    swapChain_->GetDesc(&desc);
    width_ = static_cast<int>(desc.BufferDesc.Width);
    height_ = static_cast<int>(desc.BufferDesc.Height);
    return createRenderTarget() && createD2DResources();
}

void Renderer::toggleFullscreen(HWND hwnd) {
    setFullscreen(hwnd, !fullscreen_);
}

bool Renderer::restoreFullscreen(HWND hwnd) {
    return setFullscreen(hwnd, true);
}

bool Renderer::isFullscreen() const { return fullscreen_; }

bool Renderer::syncFullscreenState() {
    if (swapChain_) {
        BOOL fs = FALSE;
        HRESULT hr = swapChain_->GetFullscreenState(&fs, nullptr);
        if (SUCCEEDED(hr)) {
            fullscreen_ = (fs == TRUE);
        }
    }
    return fullscreen_;
}

int Renderer::refreshRate() const { return refreshRate_; }
int Renderer::width() const { return width_; }
int Renderer::height() const { return height_; }
