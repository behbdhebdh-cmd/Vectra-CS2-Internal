#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

#include "core/Config.h"
#include "gui/menu/MainMenu.h"

// ─── globals ────────────────────────────────────────────────────────────────
static ID3D11Device*            g_pd3dDevice        = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext  = nullptr;
static IDXGISwapChain*          g_pSwapChain         = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;
static WNDCLASSEXW              g_wc                 = {};
static HWND                     g_hwnd               = nullptr;

// ─── forward declarations ───────────────────────────────────────────────────
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();

// ─── entry point ────────────────────────────────────────────────────────────
int main()
{
	HINSTANCE hInstance = GetModuleHandleW(nullptr);

	// create window
	g_wc = {
		sizeof(g_wc), CS_CLASSDC, WndProc, 0L, 0L,
		hInstance, LoadIcon(nullptr, IDI_APPLICATION),
		LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr,
		L"MenuExt", nullptr
	};
	RegisterClassExW(&g_wc);

	g_hwnd = CreateWindowExW(
		0, g_wc.lpszClassName, L"MenuExt",
		WS_OVERLAPPEDWINDOW, 100, 100, 1280, 720,
		nullptr, nullptr, g_wc.hInstance, nullptr
	);

	if (!CreateDeviceD3D(g_hwnd))
	{
		CleanupDeviceD3D();
		UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);
		return 1;
	}

	ShowWindow(g_hwnd, SW_SHOWDEFAULT);
	UpdateWindow(g_hwnd);

	// setup ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// load font (already in ttf/LondrinaSolidCyr.ttf)
	ImFont* font = io.Fonts->AddFontFromFileTTF("ttf/LondrinaSolidCyr.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
	if (!font) io.Fonts->AddFontDefault();

	ImGui_ImplWin32_Init(g_hwnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

	// main loop
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		Menu::Render();

		ImGui::Render();
		const ImVec4 clear = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
		g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
		g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		g_pSwapChain->Present(1, 0);
	}

	// cleanup
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();
	DestroyWindow(g_hwnd);
	UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);

	return 0;
}

// ─── WndProc ────────────────────────────────────────────────────────────────
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_KEYDOWN:
		if (wParam == VK_INSERT)
			Menu::Toggle();
		return 0;

	case WM_SIZE:
		if (g_pd3dDevice && wParam != SIZE_MINIMIZED)
		{
			CleanupRenderTarget();
			g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			CreateRenderTarget();
		}
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ─── D3D11 helpers ──────────────────────────────────────────────────────────
bool CreateDeviceD3D(HWND hWnd)
{
	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount       = 2;
	sd.BufferDesc.Width  = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags             = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow      = hWnd;
	sd.SampleDesc.Count  = 1;
	sd.Windowed          = TRUE;
	sd.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

	UINT createFlags = 0;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
	if (D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
			featureLevels, 1, D3D11_SDK_VERSION, &sd,
			&g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
		return false;

	CreateRenderTarget();
	return true;
}

void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer = nullptr;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
	pBackBuffer->Release();
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain)        { g_pSwapChain->Release();        g_pSwapChain        = nullptr; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
	if (g_pd3dDevice)        { g_pd3dDevice->Release();        g_pd3dDevice        = nullptr; }
}
