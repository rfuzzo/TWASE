#pragma once

template<typename T>
class Hook
{
public:
    Hook(T aAddress, T aDetour)
        : m_isAttached(false)
        , m_address(aAddress)
        , m_detour(aDetour)
    {
    }

    Hook(std::uint32_t aAddress, T aDetour)
        : Hook(reinterpret_cast<T>(aAddress), aDetour)
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
        DWORD moduleBase = App::Get()->GetEmpireDllAddr();
		DWORD rva = reinterpret_cast<DWORD>(m_address);

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
    mutable T m_address;
    T m_detour;
};
