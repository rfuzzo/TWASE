#pragma once

#include "../../sdk/Attila/Build.hpp"

// Detours hook on an empire.retail.dll RVA. Calling the hook object calls the original function.
template<typename T>
class Hook
{
public:
    // A build-selected RVA. Keeps a reference: global hooks are initialized before the build is selected.
    Hook(const sdk::Attila::BuildAddress& aAddress, T aDetour)
        : m_isAttached(false)
        , m_buildAddress(&aAddress)
        , m_rva(0)
        , m_address(nullptr)
        , m_detour(aDetour)
    {
    }

    // An RVA found at run time.
    Hook(uint32_t aRva, T aDetour)
        : m_isAttached(false)
        , m_buildAddress(nullptr)
        , m_rva(aRva)
        , m_address(nullptr)
        , m_detour(aDetour)
    {
    }

    operator T() const
    {
        return m_address;
    }

    uintptr_t GetAddress() const
    {
        return reinterpret_cast<uintptr_t>(m_address);
    }

    int32_t Attach()
    {
        if (m_isAttached)
        {
            return 0;
        }

        // resolve from RVA to absolute address
        const DWORD moduleBase = App::Get()->GetEmpireDllAddr();
        const uint32_t rva = m_buildAddress ? static_cast<uint32_t>(*m_buildAddress) : m_rva;

        m_address = reinterpret_cast<T>(moduleBase + rva);

        auto result = DetourAttach(&m_address, m_detour);
        m_isAttached = result == NO_ERROR;

        return result;
    }

    int32_t Detach()
    {
        if (!m_isAttached)
        {
            return 0;
        }

        auto result = DetourDetach(&m_address, m_detour);
        m_isAttached = result == NO_ERROR;

        return result;
    }

private:
    bool m_isAttached;
    const sdk::Attila::BuildAddress* m_buildAddress;
    uint32_t m_rva;
    mutable T m_address;
    T m_detour;
};
