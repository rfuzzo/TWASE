#pragma once

#include "../Image.hpp"

#include <dobby.h>

// Inline hook on a game function, aAddress is unslid (as shown in Binary Ninja).
// Calling the hook object calls the original function.
// The constructor is constexpr so global hooks are constant-initialized: the hooks are attached from a dylib constructor,
// which can run before a file's dynamic initializers.
template<typename T>
class Hook
{
public:
    constexpr Hook(uint64_t aAddress, T aDetour)
        : m_isAttached(false)
        , m_address(aAddress)
        , m_detour(aDetour)
        , m_original(nullptr)
    {
    }

    operator T() const
    {
        return m_original;
    }

    uint64_t GetAddress() const
    {
        return m_address;
    }

    // returns 0 on success, like Dobby
    int32_t Attach()
    {
        if (m_isAttached)
        {
            return 0;
        }

        auto target = reinterpret_cast<void*>(Image::Get()->Resolve(m_address));
        auto result = DobbyHook(target, reinterpret_cast<dobby_dummy_func_t>(m_detour),
                                reinterpret_cast<dobby_dummy_func_t*>(&m_original));
        m_isAttached = result == 0;

        return result;
    }

    int32_t Detach()
    {
        if (!m_isAttached)
        {
            return 0;
        }

        auto result = DobbyDestroy(reinterpret_cast<void*>(Image::Get()->Resolve(m_address)));
        m_isAttached = result != 0;

        return result;
    }

private:
    bool m_isAttached;
    uint64_t m_address;
    T m_detour;
    T m_original;
};
