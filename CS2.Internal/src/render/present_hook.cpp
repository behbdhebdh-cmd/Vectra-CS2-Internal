#include "src/render/present_hook.hpp"
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <d3d11.h>
#include <wincodec.h>
#include <shlobj.h>
#include <filesystem>
#include <vector>
#include <cstdio>
#include <utility>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace vectra {
PresentHook* PresentHook::instance_ = nullptr;
namespace { LRESULT CALLBACK DummyWindowProc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); } }

void PresentHook::RequestScreenshot() { screenshotRequested_.store(true); }
std::wstring PresentHook::ScreenshotStatus() const { std::scoped_lock lock(screenshotMutex_); return screenshotStatus_; }

bool PresentHook::CaptureScreenshot(std::wstring& savedPath, std::wstring& error) {
    if (!swapChain_ || !device_ || !context_) { error = L"renderer is not ready"; return false; }
    ID3D11Texture2D* backBuffer{};
    if (FAILED(swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer))) || !backBuffer) {
        error = L"GetBuffer failed"; return false;
    }
    D3D11_TEXTURE2D_DESC desc{};
    backBuffer->GetDesc(&desc);
    const bool rgba = desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    const bool bgra = desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    if (!rgba && !bgra) { backBuffer->Release(); error = L"unsupported back-buffer format"; return false; }

    ID3D11Texture2D* source = backBuffer;
    ID3D11Texture2D* resolved{};
    if (desc.SampleDesc.Count > 1) {
        D3D11_TEXTURE2D_DESC resolveDesc = desc;
        resolveDesc.SampleDesc = {1, 0};
        resolveDesc.Usage = D3D11_USAGE_DEFAULT;
        resolveDesc.BindFlags = 0;
        resolveDesc.CPUAccessFlags = 0;
        resolveDesc.MiscFlags = 0;
        if (FAILED(device_->CreateTexture2D(&resolveDesc, nullptr, &resolved)) || !resolved) {
            backBuffer->Release(); error = L"MSAA resolve texture failed"; return false;
        }
        context_->ResolveSubresource(resolved, 0, backBuffer, 0, desc.Format);
        source = resolved;
        desc = resolveDesc;
    }

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;
    ID3D11Texture2D* staging{};
    if (FAILED(device_->CreateTexture2D(&stagingDesc, nullptr, &staging)) || !staging) {
        if (resolved) resolved->Release(); backBuffer->Release(); error = L"staging texture failed"; return false;
    }
    context_->CopyResource(staging, source);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
        staging->Release(); if (resolved) resolved->Release(); backBuffer->Release(); error = L"back-buffer map failed"; return false;
    }
    std::vector<BYTE> pixels(static_cast<size_t>(desc.Width) * desc.Height * 4);
    for (UINT y = 0; y < desc.Height; ++y) {
        const auto* row = static_cast<const BYTE*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch;
        auto* output = pixels.data() + static_cast<size_t>(y) * desc.Width * 4;
        for (UINT x = 0; x < desc.Width; ++x) {
            const BYTE* input = row + static_cast<size_t>(x) * 4;
            output[x * 4 + 0] = rgba ? input[2] : input[0];
            output[x * 4 + 1] = input[1];
            output[x * 4 + 2] = rgba ? input[0] : input[2];
            output[x * 4 + 3] = input[3];
        }
    }
    context_->Unmap(staging, 0);
    staging->Release();
    if (resolved) resolved->Release();
    backBuffer->Release();

    PWSTR picturesRaw{};
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Pictures, KF_FLAG_CREATE, nullptr, &picturesRaw)) || !picturesRaw) {
        error = L"Pictures folder unavailable"; return false;
    }
    std::filesystem::path directory = std::filesystem::path(picturesRaw) / L"Vectra Debug";
    CoTaskMemFree(picturesRaw);
    std::error_code filesystemError;
    std::filesystem::create_directories(directory, filesystemError);
    if (filesystemError) { error = L"unable to create screenshot directory"; return false; }
    SYSTEMTIME time{}; GetLocalTime(&time);
    wchar_t filename[96]{};
    std::swprintf(filename, std::size(filename), L"vectra_%04u-%02u-%02u_%02u-%02u-%02u-%03u.png", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
    const std::filesystem::path path = directory / filename;

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(comResult);
    IWICImagingFactory* factory{}; IWICStream* stream{}; IWICBitmapEncoder* encoder{}; IWICBitmapFrameEncode* frame{};
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr)) hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    if (SUCCEEDED(hr)) hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (SUCCEEDED(hr)) hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(&frame, nullptr);
    if (SUCCEEDED(hr)) hr = frame->Initialize(nullptr);
    if (SUCCEEDED(hr)) hr = frame->SetSize(desc.Width, desc.Height);
    WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppBGRA;
    if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&pixelFormat);
    if (SUCCEEDED(hr) && pixelFormat != GUID_WICPixelFormat32bppBGRA) hr = E_FAIL;
    if (SUCCEEDED(hr)) hr = frame->WritePixels(desc.Height, desc.Width * 4, static_cast<UINT>(pixels.size()), pixels.data());
    if (SUCCEEDED(hr)) hr = frame->Commit();
    if (SUCCEEDED(hr)) hr = encoder->Commit();
    if (frame) frame->Release(); if (encoder) encoder->Release(); if (stream) stream->Release(); if (factory) factory->Release();
    if (uninitialize) CoUninitialize();
    if (FAILED(hr)) { error = L"PNG encoder failed"; return false; }
    savedPath = path.wstring();
    return true;
}

bool PresentHook::CreateDummyTargets(void** presentTarget, void** resizeTarget) {
    constexpr wchar_t klass[] = L"VectraDummyDx11";
    WNDCLASSEXW wc{sizeof(wc)}; wc.lpfnWndProc = DummyWindowProc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = klass;
    RegisterClassExW(&wc);
    HWND dummy = CreateWindowExW(0, klass, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    if (!dummy) return false;
    DXGI_SWAP_CHAIN_DESC desc{}; desc.BufferCount = 1; desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.OutputWindow = dummy; desc.SampleDesc.Count = 1; desc.Windowed = TRUE;
    ID3D11Device* device{}; ID3D11DeviceContext* context{}; IDXGISwapChain* chain{}; D3D_FEATURE_LEVEL level{};
    const HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &desc, &chain, &device, &level, &context);
    if (SUCCEEDED(hr) && chain) { void** vtable = *reinterpret_cast<void***>(chain); *presentTarget = vtable[8]; *resizeTarget = vtable[13]; }
    if (context) context->Release(); if (device) device->Release(); if (chain) chain->Release(); DestroyWindow(dummy); UnregisterClassW(klass, wc.hInstance);
    return SUCCEEDED(hr) && *presentTarget && *resizeTarget;
}
bool PresentHook::Install(FrameCallback callback) {
    if (hookInstalled_) return true;
    callback_ = std::move(callback); void* target{}; void* resizeTarget{};
    if (!CreateDummyTargets(&target, &resizeTarget)) { status_ = L"Unable to locate DX11 swap-chain methods"; return false; }
    if (MH_Initialize() != MH_OK) { status_ = L"MinHook initialization failed"; return false; }
    if (MH_CreateHook(target, &Detour, reinterpret_cast<void**>(&original_)) != MH_OK || MH_CreateHook(resizeTarget, &ResizeDetour, reinterpret_cast<void**>(&originalResize_)) != MH_OK || MH_EnableHook(target) != MH_OK || MH_EnableHook(resizeTarget) != MH_OK) { MH_Uninitialize(); status_ = L"Swap-chain hook installation failed"; return false; }
    instance_ = this; hookInstalled_ = true; status_ = L"Waiting for first Present"; return true;
}

bool PresentHook::IsGameSwapChain(IDXGISwapChain* chain, DXGI_SWAP_CHAIN_DESC& desc) const {
    if (!chain || FAILED(chain->GetDesc(&desc)) || !desc.OutputWindow || !IsWindow(desc.OutputWindow)) return false;

    DWORD ownerProcess{};
    GetWindowThreadProcessId(desc.OutputWindow, &ownerProcess);
    return ownerProcess == GetCurrentProcessId() && (GetWindowLongPtrW(desc.OutputWindow, GWL_STYLE) & WS_CHILD) == 0;
}

bool PresentHook::CreateRenderTarget() {
    ReleaseRenderTarget();
    if (!swapChain_ || !device_) return false;

    ID3D11Texture2D* backBuffer{};
    const HRESULT bufferResult = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
    if (FAILED(bufferResult) || !backBuffer) {
        status_ = L"GetBuffer(0) failed";
        return false;
    }

    const HRESULT viewResult = device_->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView_);
    backBuffer->Release();
    if (FAILED(viewResult) || !renderTargetView_) {
        status_ = L"CreateRenderTargetView failed";
        return false;
    }
    return true;
}

void PresentHook::ReleaseRenderTarget() {
    if (renderTargetView_) {
        renderTargetView_->Release();
        renderTargetView_ = nullptr;
    }
}

bool PresentHook::InitializeRenderer(IDXGISwapChain* chain) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (!IsGameSwapChain(chain, desc)) {
        status_ = L"Skipping a non-game swap chain";
        return false;
    }

    if (FAILED(chain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device_))) || !device_) {
        status_ = L"IDXGISwapChain::GetDevice failed";
        return false;
    }
    device_->GetImmediateContext(&context_);
    if (!context_) {
        device_->Release();
        device_ = nullptr;
        status_ = L"Unable to acquire the immediate context";
        return false;
    }

    swapChain_ = chain;
    swapChain_->AddRef();
    window_ = desc.OutputWindow;
    if (!CreateRenderTarget()) {
        swapChain_->Release(); swapChain_ = nullptr;
        context_->Release(); context_ = nullptr;
        device_->Release(); device_ = nullptr;
        window_ = nullptr;
        return false;
    }

    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui::GetIO().IniFilename = nullptr; ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.f;
    style.FrameRounding = 4.f;
    style.PopupRounding = 4.f;
    style.GrabRounding = 4.f;
    style.WindowPadding = {12.f, 12.f};
    style.FramePadding = {7.f, 5.f};
    style.ItemSpacing = {8.f, 7.f};
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = {0.035f, 0.055f, 0.065f, 0.96f};
    colors[ImGuiCol_TitleBg] = {0.045f, 0.105f, 0.110f, 1.f};
    colors[ImGuiCol_TitleBgActive] = {0.055f, 0.155f, 0.155f, 1.f};
    colors[ImGuiCol_Header] = {0.10f, 0.28f, 0.26f, 0.82f};
    colors[ImGuiCol_HeaderHovered] = {0.14f, 0.40f, 0.35f, 0.90f};
    colors[ImGuiCol_HeaderActive] = {0.17f, 0.50f, 0.43f, 1.f};
    colors[ImGuiCol_CheckMark] = {0.36f, 0.83f, 0.65f, 1.f};
    colors[ImGuiCol_SliderGrab] = {0.36f, 0.83f, 0.65f, 0.90f};
    colors[ImGuiCol_SliderGrabActive] = {0.48f, 0.92f, 0.72f, 1.f};
    const bool win32Ready = ImGui_ImplWin32_Init(window_);
    const bool dx11Ready = win32Ready && ImGui_ImplDX11_Init(device_, context_);
    if (!win32Ready || !dx11Ready) {
        if (dx11Ready) ImGui_ImplDX11_Shutdown();
        if (win32Ready) ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        ReleaseRenderTarget();
        swapChain_->Release(); swapChain_ = nullptr;
        context_->Release(); context_ = nullptr;
        device_->Release(); device_ = nullptr;
        window_ = nullptr;
        status_ = L"ImGui backend initialization failed";
        return false;
    }
    originalWindowProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProc)));
    imguiReady_ = true; status_ = L"Running"; return true;
}
HRESULT __stdcall PresentHook::Detour(IDXGISwapChain* chain, UINT sync, UINT flags) {
    PresentHook* self = instance_; if (!self || !self->original_) return S_OK;
    if (!self->imguiReady_) self->InitializeRenderer(chain);
    if (self->imguiReady_ && chain == self->swapChain_ && self->renderTargetView_) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        if (self->callback_) self->callback_();
        ImGui::Render();

        ID3D11RenderTargetView* previousTarget{};
        ID3D11DepthStencilView* previousDepth{};
        self->context_->OMGetRenderTargets(1, &previousTarget, &previousDepth);
        self->context_->OMSetRenderTargets(1, &self->renderTargetView_, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        if (self->screenshotRequested_.exchange(false)) {
            std::wstring path, error;
            const bool success = self->CaptureScreenshot(path, error);
            std::scoped_lock lock(self->screenshotMutex_);
            self->screenshotStatus_ = success ? L"Saved: " + path : L"Screenshot failed: " + error;
        }
        self->context_->OMSetRenderTargets(1, &previousTarget, previousDepth);
        if (previousDepth) previousDepth->Release();
        if (previousTarget) previousTarget->Release();
    }
    return self->original_(chain, sync, flags);
}
HRESULT __stdcall PresentHook::ResizeDetour(IDXGISwapChain* chain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT format, UINT swapChainFlags) {
    PresentHook* self = instance_; if (!self || !self->originalResize_) return S_OK;
    const bool ownsChain = self->imguiReady_ && chain == self->swapChain_;
    if (ownsChain) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
        self->ReleaseRenderTarget();
    }
    const HRESULT hr = self->originalResize_(chain, bufferCount, width, height, format, swapChainFlags);
    if (SUCCEEDED(hr) && ownsChain) {
        if (!self->CreateRenderTarget()) self->status_ = L"Back-buffer recreation failed after resize";
        ImGui_ImplDX11_CreateDeviceObjects();
    }
    return hr;
}
LRESULT CALLBACK PresentHook::WindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    PresentHook* self = instance_;
    if (self && self->imguiReady_ && ImGui_ImplWin32_WndProcHandler(hwnd, msg, w, l) && (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard)) return 1;
    return self && self->originalWindowProc_ ? CallWindowProcW(self->originalWindowProc_, hwnd, msg, w, l) : DefWindowProcW(hwnd, msg, w, l);
}
void PresentHook::Shutdown() {
    if (hookInstalled_) { MH_DisableHook(MH_ALL_HOOKS); MH_RemoveHook(MH_ALL_HOOKS); MH_Uninitialize(); hookInstalled_ = false; }
    if (window_ && originalWindowProc_) SetWindowLongPtrW(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalWindowProc_));
    if (imguiReady_) { ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext(); }
    ReleaseRenderTarget();
    if (swapChain_) swapChain_->Release();
    if (context_) context_->Release();
    if (device_) device_->Release();
    swapChain_ = nullptr; context_ = nullptr; device_ = nullptr;
    originalWindowProc_ = nullptr; window_ = nullptr; imguiReady_ = false; instance_ = nullptr; status_ = L"Stopped";
}
}
