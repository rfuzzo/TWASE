#pragma once

// The game's main executable (the game code is statically linked into it, there is no empire.retail.dll on macOS)
class Image
{
public:
    static Image* Get();

    bool IsAttila() const;
    // true if the executable is the exact build our addresses were made for
    bool IsSupported() const;

    const std::string& GetUUID() const;
    const std::string& GetVersion() const;
    const std::string& GetBuild() const;

    uintptr_t GetSlide() const;

    // unslid address as shown in Binary Ninja (base 0x100000000) -> runtime address
    uintptr_t Resolve(uint64_t aAddress) const;

    template<typename T>
    T* Resolve(uint64_t aAddress) const
    {
        return reinterpret_cast<T*>(Resolve(aAddress));
    }

private:
    Image();
    ~Image() = default;

    bool m_isAttila;
    std::string m_uuid;
    std::string m_version;
    std::string m_build;
    uintptr_t m_slide;
};
