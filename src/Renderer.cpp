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
    // All D3D/D2D calls happen on the main thread; SINGLETHREADED drops the
    // runtime's internal locking.
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT
               | D3D11_CREATE_DEVICE_SINGLETHREADED;
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

// Creates the D2D device/context and the shared brush. These are device-scoped
// resources: they survive back-buffer rotation, resize, and fullscreen flips,
// so this is only called once during init (and cheaply re-asserted afterward).
bool Renderer::createD2DResources() {
    if (!d2dFactory_) {
        D2D1_FACTORY_OPTIONS factoryOpts = {};
#ifdef _DEBUG
        factoryOpts.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
        HRESULT hr = D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED,
            __uuidof(ID2D1Factory1),
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

    // D2D device shares the D3D11 device via DXGI; create once.
    if (!d2dDevice_) {
        ComPtr<IDXGIDevice> dxgiDevice;
        device_.As(&dxgiDevice);
        HRESULT hr = d2dFactory_->CreateDevice(dxgiDevice.Get(), &d2dDevice_);
        if (FAILED(hr)) return false;
        hr = d2dDevice_->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2dContext_);
        if (FAILED(hr)) return false;
    }

    // Brush is bound to the context, not the target bitmap, so it persists.
    if (!brush_ && d2dContext_) {
        HRESULT hr = d2dContext_->CreateSolidColorBrush(
            D2D1::ColorF(1, 1, 1, 1), &brush_);
        if (FAILED(hr)) return false;
    }

    // The back-buffer bitmap target is (re)bound lazily by beginUI().
    d2dBitmapStale_ = true;
    return true;
}

// Binds the current back buffer as the D2D render target. Cheap: a bitmap view,
// not a full render-target recreation. Called after every flip / resize.
void Renderer::refreshD2DTarget() {
    if (!d2dContext_ || !swapChain_) return;

    ComPtr<IDXGISurface> surface;
    HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&surface));
    if (FAILED(hr)) return;

    D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    ComPtr<ID2D1Bitmap1> bitmap;
    hr = d2dContext_->CreateBitmapFromDxgiSurface(surface.Get(), bp, &bitmap);
    if (FAILED(hr)) return;

    d2dBitmap_ = bitmap;
    d2dContext_->SetTarget(d2dBitmap_.Get());
    d2dBitmapStale_ = false;
}

IDWriteTextFormat* Renderer::getTextFormat(float fontSize) {
    if (!dwFactory_) return nullptr;
    int key = static_cast<int>(fontSize * 10.0f + 0.5f);
    auto it = textFormats_.find(key);
    if (it != textFormats_.end()) return it->second.Get();

    ComPtr<IDWriteTextFormat> format;
    HRESULT hr = dwFactory_->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        fontSize, L"en-US", &format);
    if (FAILED(hr)) return nullptr;

    IDWriteTextFormat* raw = format.Get();
    textFormats_[key] = std::move(format);
    return raw;
}

void Renderer::shutdown() {
    brush_.Reset();
    textFormats_.clear();
    d2dBitmap_.Reset();
    d2dContext_.Reset();
    d2dDevice_.Reset();
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

// Stimulus path: clear to the set color, present, record QPC. Pure D3D11 —
// no D2D work on this hot path. The stale D2D bitmap is refreshed lazily by the
// next beginUI(), well away from the stimulus->input measurement window.
void Renderer::presentStimulus() {
    float color[4] = { clearColor_.r, clearColor_.g, clearColor_.b, clearColor_.a };
    context_->ClearRenderTargetView(rtv_.Get(), color);
    UINT syncInterval = fullscreen_ ? 1 : 0;
    UINT flags = (!fullscreen_ && tearingSupported_) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    swapChain_->Present(syncInterval, flags);
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    lastPresentQPC_ = li.QuadPart;
    // Back buffer rotated after flip: refresh D3D11 RTV for the next clear.
    rtv_.Reset();
    createRenderTarget();
    d2dBitmapStale_ = true;
}

int64_t Renderer::getLastPresentTimeQPC() const {
    // Return the QPC captured immediately after Present returns. This is the
    // deterministic reference for the stimulus timestamp: it is always >= the
    // submit time and the residual (submit -> scanout) is what displayLatency
    // compensates for.
    //
    // DXGI_FRAME_STATISTICS::SyncQPCTime is intentionally NOT used here: when
    // sampled right after Present it reflects the PREVIOUS VBlank (the stimulus
    // frame has not scanned out yet), which is stale and driver-dependent, and
    // would inflate the measured reaction time by up to a frame.
    return lastPresentQPC_;
}

double Renderer::scanoutCompensationMs() const {
    if (!fullscreen_ && tearingSupported_) return 0.0;
    return refreshRate_ > 0 ? 1000.0 / static_cast<double>(refreshRate_) : 0.0;
}

// UI path: D2D1 drawing
void Renderer::beginUI() {
    if (d2dBitmapStale_) refreshD2DTarget();
    d2dContext_->BeginDraw();
    d2dContext_->Clear(toD2D(clearColor_));
}

void Renderer::endUI() {
    d2dContext_->EndDraw();
}

// UI present: always VSync-capped so windowed mode does not spin the CPU.
void Renderer::present() {
    swapChain_->Present(1, 0);
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    lastPresentQPC_ = li.QuadPart;
    d2dBitmapStale_ = true;  // back buffer rotated
}

void Renderer::drawText(const std::wstring& text, float x, float y, float fontSize,
                        const Color& color, bool centerX, bool centerY) {
    IDWriteTextFormat* format = getTextFormat(fontSize);
    if (!format || !brush_) return;

    // Alignment is reset every call: cached formats retain state between uses.
    format->SetTextAlignment(centerX ? DWRITE_TEXT_ALIGNMENT_CENTER
                                     : DWRITE_TEXT_ALIGNMENT_LEADING);
    format->SetParagraphAlignment(centerY ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER
                                          : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    brush_->SetColor(toD2D(color));

    float textW = static_cast<float>(width_) - x * 2;
    float textH = fontSize * 2;
    D2D1_RECT_F layoutRect = D2D1::RectF(
        centerX ? 0 : x,
        centerY ? 0 : y,
        centerX ? static_cast<float>(width_) : x + textW,
        centerY ? static_cast<float>(height_) : y + textH);

    d2dContext_->DrawText(text.c_str(), static_cast<UINT32>(text.size()),
                          format, layoutRect, brush_.Get());
}

void Renderer::fillRectangle(float x, float y, float w, float h, const Color& color) {
    if (!brush_) return;
    brush_->SetColor(toD2D(color));
    d2dContext_->FillRectangle(D2D1::RectF(x, y, x + w, y + h), brush_.Get());
}

void Renderer::drawRectangleOutline(float x, float y, float w, float h,
                                     const Color& color, float strokeWidth) {
    if (!brush_) return;
    brush_->SetColor(toD2D(color));
    d2dContext_->DrawRectangle(D2D1::RectF(x, y, x + w, y + h), brush_.Get(), strokeWidth);
}

void Renderer::resize(int width, int height) {
    if (width == width_ && height == height_) return;
    width_ = width;
    height_ = height;

    // Release all references to the back buffer before ResizeBuffers.
    d2dBitmap_.Reset();
    rtv_.Reset();

    UINT resizeFlags = tearingSupported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, resizeFlags);
    createRenderTarget();
    createD2DResources();  // device/context/brush persist; marks bitmap stale
}

bool Renderer::setFullscreen(HWND hwnd, bool fullscreen) {
    d2dBitmap_.Reset();
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
