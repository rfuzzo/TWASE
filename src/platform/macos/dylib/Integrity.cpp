#include "stdafx.hpp"

#include "Integrity.hpp"
#include "Image.hpp"
#include "Hooks/WindowState.hpp"

#include <Security/Security.h>
#include <dispatch/dispatch.h>
#include <unistd.h>

// <sys/codesign.h> isn't in the public SDK
extern "C" int csops(pid_t pid, unsigned int ops, void* useraddr, size_t usersize);

namespace
{
constexpr unsigned int CS_OPS_STATUS = 0;

std::string FlagNames(uint32_t aFlags)
{
    constexpr std::pair<uint32_t, const char*> names[] = {
        {0x1, "VALID"},           {0x2, "ADHOC"},     {0x4, "GET_TASK_ALLOW"}, {0x20, "INVALID_ALLOWED"},
        {0x100, "HARD"},          {0x200, "KILL"},    {0x800, "RESTRICT"},     {0x1000, "ENFORCEMENT"},
        {0x2000, "REQUIRE_LV"},   {0x10000, "RUNTIME"}, {0x10000000, "DEBUGGED"},
    };
    std::string text;
    for (const auto& [flag, name] : names)
    {
        if (aFlags & flag)
        {
            text += text.empty() ? "" : " ";
            text += name;
        }
    }
    return text;
}

uint32_t lastFlags = 0xFFFFFFFF;
OSStatus lastValidity = 1;
bool hasValidity = false;

int ticks = 0;
int64_t lastTamperCounter = -1;

// Feral's hidden tamper counter (docs/macos/integrity-and-quirks.md): AntiTamper_InitHiddenCounter stores a pointer to
// a zeroed heap int at 0x105019040; the code guards add their hash mismatches to it. 0 means no guard has fired.
void LogTamperCounter()
{
    constexpr uint64_t CounterHolder = 0x105019040;

    if (!Image::Get()->IsSupported())
    {
        return;
    }

    const auto* counter = *Image::Get()->Resolve<const int32_t*>(CounterHolder);
    if (!counter)
    {
        return;
    }

    const int64_t value = *counter;
    if (value != lastTamperCounter)
    {
        spdlog::info("[Integrity] Feral tamper counter: {} ({:#x})", value, static_cast<uint32_t>(value));
        lastTamperCounter = value;
    }
}

void CheckValidity()
{
    SecCodeRef self = nullptr;
    OSStatus status = SecCodeCopySelf(kSecCSDefaultFlags, &self);
    if (status == errSecSuccess && self)
    {
        status = SecCodeCheckValidity(self, kSecCSDefaultFlags, nullptr);
        CFRelease(self);
    }

    if (!hasValidity || status != lastValidity)
    {
        if (status == errSecSuccess)
        {
            spdlog::info("[Integrity] SecCodeCheckValidity on the game: valid");
        }
        else
        {
            spdlog::warn("[Integrity] SecCodeCheckValidity on the game: OSStatus {} (the game would see this)", status);
        }
        hasValidity = true;
        lastValidity = status;
    }
    Integrity::LogStatus("periodic check");
}
} // namespace

void Integrity::LogStatus(std::string_view aStep)
{
    uint32_t flags = 0;
    if (csops(getpid(), CS_OPS_STATUS, &flags, sizeof(flags)) != 0)
    {
        spdlog::warn("[Integrity] {}: csops failed (errno {})", aStep, errno);
        return;
    }
    if (flags != lastFlags)
    {
        spdlog::info("[Integrity] {}: code signing flags {:#x} ({})", aStep, flags, FlagNames(flags));
        lastFlags = flags;
    }
}

void Integrity::StartPeriodicCheck()
{
    auto* timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    // every 5 s the window state and Feral's tamper counter (cheap, logged on change), every minute the signature
    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC), 5 * NSEC_PER_SEC,
                              NSEC_PER_SEC / 2);
    dispatch_source_set_event_handler_f(timer,
                                        [](void*)
                                        {
                                            WindowState::LogIfChanged();
                                            LogTamperCounter();
                                            if (ticks++ % 12 == 0)
                                            {
                                                CheckValidity();
                                            }
                                        });
    dispatch_resume(timer);
}
