#include "D3D11Hook.hpp"

#include "../UI/LuaConsole.hpp"

#include <d3d11.h>
#include <dxgi.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// ============================================================================
// Types
// ============================================================================
using PFN_Present = HRESULT(WINAPI*)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);

// ============================================================================
// State
// ============================================================================
namespace
{
bool                      g_attached = false;
PFN_Present               g_OriginalPresent = nullptr;
ID3D11Device*             g_device = nullptr;
ID3D11DeviceContext*      g_context = nullptr;
ID3D11RenderTargetView*   g_rtv = nullptr;
HWND                      g_hwnd = nullptr;
WNDPROC                   g_originalWndProc = nullptr;
bool                      g_imguiInitialized = false;
} // namespace

// Forward declare the ImGui WndProc handler
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================================
// WndProc hook – forwards input to ImGui
// ============================================================================
static LRESULT WINAPI HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Toggle console with the tilde/grave key
    if (msg == WM_KEYDOWN && wParam == VK_OEM_3) // ~ key
    {
        LuaConsole::Get().Toggle();
        return 0;
    }

    // When console is open, let ImGui consume input
    if (LuaConsole::Get().IsOpen())
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return 0;

        // Block game input while console is open (mouse/keyboard)
        switch (msg)
        {
        case WM_KEYDOWN: case WM_KEYUP: case WM_CHAR:
        case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
        case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
        case WM_MOUSEWHEEL: case WM_MOUSEMOVE:
            return 0;
        }
    }

    return CallWindowProcW(g_originalWndProc, hWnd, msg, wParam, lParam);
}

// ============================================================================
// Hooked Present – initializes ImGui on first call, renders every frame
// ============================================================================
static HRESULT WINAPI HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    if (!g_imguiInitialized)
    {
        // Get the device and context from the swap chain
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_device))))
        {
            g_device->GetImmediateContext(&g_context);

            // Get the game window
            DXGI_SWAP_CHAIN_DESC desc{};
            pSwapChain->GetDesc(&desc);
            g_hwnd = desc.OutputWindow;

            // Create render target view from back buffer
            ID3D11Texture2D* backBuffer = nullptr;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
            if (backBuffer)
            {
                g_device->CreateRenderTargetView(backBuffer, nullptr, &g_rtv);
                backBuffer->Release();
            }

            // Init ImGui
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGui::StyleColorsDark();

            ImGui_ImplWin32_Init(g_hwnd);
            ImGui_ImplDX11_Init(g_device, g_context);

            // Subclass the window for input
            g_originalWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc)));

            g_imguiInitialized = true;
            spdlog::info("ImGui initialized on game window");
        }
    }

    if (g_imguiInitialized)
    {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Draw the console
        LuaConsole::Get().Draw();

        ImGui::EndFrame();
        ImGui::Render();

        g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return g_OriginalPresent(pSwapChain, SyncInterval, Flags);
}

// ============================================================================
// Attach / Detach – gets the Present vtable address via a dummy swap chain
// ============================================================================
bool Hooks::D3D11Hook::Attach()
{
    // Create a temporary device + swap chain to read the vtable
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, DefWindowProcW, 0, 0,
                       GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr,
                       L"TWASE_DX_DUMMY", nullptr };
    RegisterClassExW(&wc);
    HWND dummy = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPED,
                                 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = dummy;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* pSwapChain = nullptr;
    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &pSwapChain, &pDevice, &featureLevel, &pContext);

    if (FAILED(hr))
    {
        spdlog::error("D3D11Hook: Failed to create dummy device, HRESULT={:#x}", static_cast<unsigned>(hr));
        DestroyWindow(dummy);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    // Read Present (index 8) from the IDXGISwapChain vtable
    void** vtable = *reinterpret_cast<void***>(pSwapChain);
    g_OriginalPresent = reinterpret_cast<PFN_Present>(vtable[8]);

    spdlog::info("D3D11Hook: IDXGISwapChain::Present at {}", reinterpret_cast<void*>(g_OriginalPresent));

    // Clean up dummy objects
    pSwapChain->Release();
    pDevice->Release();
    pContext->Release();
    DestroyWindow(dummy);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    // Detour Present
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    LONG err = DetourAttach(reinterpret_cast<PVOID*>(&g_OriginalPresent),
                            static_cast<PVOID>(HookedPresent));
    if (err != NO_ERROR)
    {
        spdlog::error("D3D11Hook: DetourAttach failed with error {}", err);
        DetourTransactionAbort();
        return false;
    }

    err = DetourTransactionCommit();
    if (err != NO_ERROR)
    {
        spdlog::error("D3D11Hook: DetourTransactionCommit failed with error {}", err);
        return false;
    }

    g_attached = true;
    spdlog::info("D3D11Hook: Present hook attached");
    return true;
}

bool Hooks::D3D11Hook::Detach()
{
    if (!g_attached)
        return false;

    // Restore WndProc
    if (g_hwnd && g_originalWndProc)
    {
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWndProc));
    }

    // Tear down ImGui
    if (g_imguiInitialized)
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
    // Don't release g_device / g_context – they belong to the game

    // Detach Detour
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(reinterpret_cast<PVOID*>(&g_OriginalPresent),
                 static_cast<PVOID>(HookedPresent));
    DetourTransactionCommit();

    g_attached = false;
    spdlog::info("D3D11Hook: Present hook detached");
    return true;
}