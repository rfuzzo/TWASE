#include "DInput8Hook.hpp"
#include "../UI/LuaConsole.hpp"

#include <initguid.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

// GetDeviceState signature: HRESULT(this*, DWORD cbData, LPVOID lpvData)
using GetDeviceState_t = HRESULT(WINAPI*)(IDirectInputDevice8W*, DWORD, LPVOID);

namespace
{
bool             g_attached               = false;
GetDeviceState_t g_OriginalGetDeviceState = nullptr;

HRESULT WINAPI HookedGetDeviceState(IDirectInputDevice8W* pDevice, DWORD cbData, LPVOID lpvData)
{
    HRESULT hr = g_OriginalGetDeviceState(pDevice, cbData, lpvData);
    if (SUCCEEDED(hr) && LuaConsole::Get().IsOpen() && lpvData)
        ZeroMemory(lpvData, cbData);
    return hr;
}
} // namespace

bool Hooks::DInput8Hook::Attach()
{
    // Spin up a temporary IDirectInput8 + keyboard device to read the vtable —
    // same pattern as D3D11Hook reading the IDXGISwapChain vtable.
    IDirectInput8W* pDI = nullptr;
    HRESULT hr = DirectInput8Create(
        GetModuleHandleW(nullptr),
        DIRECTINPUT_VERSION,
        IID_IDirectInput8W,
        reinterpret_cast<void**>(&pDI),
        nullptr);

    if (FAILED(hr))
    {
        spdlog::error("DInput8Hook: DirectInput8Create failed, HRESULT={:#x}", static_cast<unsigned>(hr));
        return false;
    }

    IDirectInputDevice8W* pDevice = nullptr;
    hr = pDI->CreateDevice(GUID_SysKeyboard, &pDevice, nullptr);
    pDI->Release();

    if (FAILED(hr) || !pDevice)
    {
        spdlog::error("DInput8Hook: CreateDevice(GUID_SysKeyboard) failed, HRESULT={:#x}", static_cast<unsigned>(hr));
        return false;
    }

    // vtable[9] = GetDeviceState
    void** vtable = *reinterpret_cast<void***>(pDevice);
    g_OriginalGetDeviceState = reinterpret_cast<GetDeviceState_t>(vtable[9]);
    pDevice->Release();

    spdlog::info("DInput8Hook: IDirectInputDevice8::GetDeviceState at {}", reinterpret_cast<void*>(g_OriginalGetDeviceState));

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    LONG err = DetourAttach(reinterpret_cast<PVOID*>(&g_OriginalGetDeviceState), HookedGetDeviceState);
    if (err != NO_ERROR)
    {
        spdlog::error("DInput8Hook: DetourAttach failed with error {}", err);
        DetourTransactionAbort();
        return false;
    }

    err = DetourTransactionCommit();
    if (err != NO_ERROR)
    {
        spdlog::error("DInput8Hook: DetourTransactionCommit failed with error {}", err);
        return false;
    }

    g_attached = true;
    spdlog::info("DInput8Hook: GetDeviceState hook attached");
    return true;
}

bool Hooks::DInput8Hook::Detach()
{
    if (!g_attached)
        return false;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(reinterpret_cast<PVOID*>(&g_OriginalGetDeviceState), HookedGetDeviceState);
    DetourTransactionCommit();

    g_attached = false;
    spdlog::info("DInput8Hook: GetDeviceState hook detached");
    return true;
}
