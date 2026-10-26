#include "stdafx.hpp"

#include "Integrity.hpp"

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
    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC), 60 * NSEC_PER_SEC,
                              NSEC_PER_SEC);
    dispatch_source_set_event_handler_f(timer, [](void*) { CheckValidity(); });
    dispatch_resume(timer);
}
