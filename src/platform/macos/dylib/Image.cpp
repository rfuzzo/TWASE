#include "stdafx.hpp"

#include "Image.hpp"

#include "../sdk/Attila/Addresses.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>

namespace
{
constexpr std::string_view AttilaBundleId = "com.feralinteractive.attilatw";

std::string ToString(CFStringRef aString)
{
    if (!aString)
    {
        return {};
    }

    char buffer[256];
    if (!CFStringGetCString(aString, buffer, sizeof(buffer), kCFStringEncodingUTF8))
    {
        return {};
    }

    return buffer;
}

// reads the raw Info.plist dictionary, CFBundleGetValueForInfoDictionaryKey would look up the localized one, which
// goes through Foundation and recurses forever while Foundation isn't initialized yet (we run before main)
std::string GetBundleString(CFBundleRef aBundle, CFStringRef aKey)
{
    auto info = CFBundleGetInfoDictionary(aBundle);
    if (!info)
    {
        return {};
    }

    auto value = CFDictionaryGetValue(info, aKey);
    if (!value || CFGetTypeID(value) != CFStringGetTypeID())
    {
        return {};
    }

    return ToString(static_cast<CFStringRef>(value));
}

// with DYLD_INSERT_LIBRARIES our dylib can come before the executable in the image list, so search for it
std::optional<uint32_t> FindMainImage()
{
    for (uint32_t i = 0; i < _dyld_image_count(); i++)
    {
        if (_dyld_get_image_header(i)->filetype == MH_EXECUTE)
        {
            return i;
        }
    }

    return std::nullopt;
}

std::string ReadUUID(const mach_header_64* aHeader)
{
    auto cmd = reinterpret_cast<const load_command*>(aHeader + 1);
    for (uint32_t i = 0; i < aHeader->ncmds; i++)
    {
        if (cmd->cmd == LC_UUID)
        {
            const auto* u = reinterpret_cast<const uuid_command*>(cmd)->uuid;
            return fmt::format("{:02X}{:02X}{:02X}{:02X}-{:02X}{:02X}-{:02X}{:02X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}",
                               u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], u[8], u[9], u[10], u[11], u[12], u[13],
                               u[14], u[15]);
        }

        cmd = reinterpret_cast<const load_command*>(reinterpret_cast<const uint8_t*>(cmd) + cmd->cmdsize);
    }

    return {};
}
} // namespace

Image::Image()
    : m_isAttila(false)
    , m_slide(0)
{
    auto bundle = CFBundleGetMainBundle();
    if (bundle)
    {
        m_isAttila = GetBundleString(bundle, kCFBundleIdentifierKey) == AttilaBundleId;
        m_version = GetBundleString(bundle, CFSTR("CFBundleShortVersionString"));
        m_build = GetBundleString(bundle, kCFBundleVersionKey);
    }

    auto index = FindMainImage();
    if (index)
    {
        m_slide = static_cast<uintptr_t>(_dyld_get_image_vmaddr_slide(*index));
        m_uuid = ReadUUID(reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(*index)));
    }
}

Image* Image::Get()
{
    static Image image;
    return &image;
}

bool Image::IsAttila() const
{
    return m_isAttila;
}

bool Image::IsSupported() const
{
    return m_uuid == sdk::Attila::Addresses::SupportedUUID;
}

const std::string& Image::GetUUID() const
{
    return m_uuid;
}

const std::string& Image::GetVersion() const
{
    return m_version;
}

const std::string& Image::GetBuild() const
{
    return m_build;
}

uintptr_t Image::GetSlide() const
{
    return m_slide;
}

uintptr_t Image::Resolve(uint64_t aAddress) const
{
    return static_cast<uintptr_t>(aAddress) + m_slide;
}
