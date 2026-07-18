#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <functional>
#include <atomic>
#include <mutex>
#include <string>

namespace vectra {
class PresentHook {
public:
    using FrameCallback = std::function<void()>;
    bool Install(FrameCallback callback);
    void Shutdown();
    HWND Window() const { return window_; }
    bool Ready() const { return imguiReady_; }
    const std::wstring& Status() const { return status_; }
    void RequestScreenshot();
    std::wstring ScreenshotStatus() const;
private:
    using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
    using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    static HRESULT __stdcall Detour(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);
    static HRESULT __stdcall ResizeDetour(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT format, UINT swapChainFlags);
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    bool InitializeRenderer(IDXGISwapChain* swapChain);
    bool CreateRenderTarget();
    void ReleaseRenderTarget();
    bool IsGameSwapChain(IDXGISwapChain* swapChain, DXGI_SWAP_CHAIN_DESC& desc) const;
    bool CreateDummyTargets(void** presentTarget, void** resizeTarget);
    bool CaptureScreenshot(std::wstring& savedPath, std::wstring& error);
    PresentFn original_{};
    ResizeBuffersFn originalResize_{};
    WNDPROC originalWindowProc_{};
    HWND window_{};
    IDXGISwapChain* swapChain_{};
    ID3D11Device* device_{};
    ID3D11DeviceContext* context_{};
    ID3D11RenderTargetView* renderTargetView_{};
    FrameCallback callback_;
    bool hookInstalled_{};
    bool imguiReady_{};
    std::atomic_bool screenshotRequested_{};
    mutable std::mutex screenshotMutex_;
    std::wstring screenshotStatus_{L"F2: no screenshot captured"};
    std::wstring status_{L"Not installed"};
    static PresentHook* instance_;
};
}
