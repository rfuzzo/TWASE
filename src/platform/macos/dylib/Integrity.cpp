#include "stdafx.hpp"

#include "Integrity.hpp"
#include "App.hpp"
#include "Image.hpp"
#include "Hooks/WindowState.hpp"
#include "Hooking/Memory.hpp"
#include "TamperWatch.hpp"

#include <mach/mach_vm.h>
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
// a zeroed heap int at 0x105019040. The libsteam_api re-check, the module watcher and a few probes add to it; the code
// guards don't. It stayed 0 in every run so far.
const int32_t* GetTamperCounter()
{
    constexpr uint64_t CounterHolder = 0x105019040;

    if (!Image::Get()->IsSupported())
    {
        return nullptr;
    }

    return *Image::Get()->Resolve<const int32_t*>(CounterHolder);
}

void LogTamperCounter()
{
    const auto* counter = GetTamperCounter();
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

// Feral's hidden input clock, kept by the central event dispatcher (FeralEventSource_Dispatch 0x10056c1c4) on every input
// event: *start (pointer at 0x1055d8158) = the time of the first event, *elapsed (pointer behind the masked holder
// 0x1055d8198/0x1055d81a0/0x1055d817c) = event time - start, in whole seconds of system uptime (NSEvent.timestamp).
// [dev] reset_input_clock moves start to now, so elapsed stays near 0; [dev] input_clock_offset = N moves start N seconds
// back once, to reach a given elapsed time quickly (docs/macos/integrity-and-quirks.md).
void InputClock(bool aReset, int32_t aOffset)
{
    if (!Image::Get()->IsSupported())
    {
        return;
    }

    // value behind a masked holder, read without trusting the decode
    const auto readHidden = [](uint64_t aHolder) -> std::string
    {
        uint64_t slot = 0;
        int32_t value = 0;
        mach_vm_size_t size = 0;
        if (aHolder == 0 ||
            mach_vm_read_overwrite(mach_task_self(), aHolder, sizeof(slot), reinterpret_cast<mach_vm_address_t>(&slot),
                                   &size) != KERN_SUCCESS ||
            slot == 0 ||
            mach_vm_read_overwrite(mach_task_self(), slot, sizeof(value), reinterpret_cast<mach_vm_address_t>(&value),
                                   &size) != KERN_SUCCESS)
        {
            return "?";
        }
        return std::to_string(value);
    };
    const auto resolve = [](uint64_t aAddress) { return Image::Get()->Resolve(aAddress); };
    const auto i32 = [](uint64_t aAddress) { return static_cast<int64_t>(*Image::Get()->Resolve<const int32_t>(aAddress)); };
    const auto u64 = [](uint64_t aAddress) { return *Image::Get()->Resolve<const uint64_t>(aAddress); };

    auto* start = *Image::Get()->Resolve<int32_t*>(0x1055d8158);
    const auto holder = ((resolve(0x1055d8198) ^ ~u64(0x1055d81a0)) - i32(0x1055d817c)) << 3;
    const auto* elapsed = holder ? *reinterpret_cast<int32_t* const*>(holder) : nullptr;
    if (!start || !elapsed)
    {
        return;
    }

    // other hidden values the two readers of *elapsed load next to it (sub_100cc2268, sub_10056c700)
    const auto readerA = ((resolve(0x1055e2948) ^ ~u64(0x1055e2950)) + i32(0x1055e2920)) << 4;
    const auto readerB = ((u64(0x1055e2930) ^ resolve(0x1055e2928)) - i32(0x1055e2918)) << 1;

    const auto now = static_cast<uint32_t>(clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / NSEC_PER_SEC);
    spdlog::debug("[InputClock] start {} elapsed {} (now {}), cc2268 value {}, 56c700 value {}{}",
                  static_cast<uint32_t>(*start), static_cast<uint32_t>(*elapsed), now, readHidden(readerA),
                  readHidden(readerB), aReset ? ", resetting start" : "");
    if (aReset)
    {
        *start = static_cast<int32_t>(now);
    }

    static bool offsetApplied = false;
    if (aOffset != 0 && !offsetApplied)
    {
        offsetApplied = true;
        *start = static_cast<int32_t>(now) - aOffset;
        spdlog::info("[InputClock] moved start {} s back (input_clock_offset)", aOffset);
    }
}

// [dev] run_tamper_guards: calls Feral's run-time code guards (AntiTamper_TextRangeGuard_*, normally started by the
// game's obfuscated scheduler) with the code as loaded and again with one never-executed __text byte flipped, logging
// the counter and every changed __DATA word after each call and dumping __DATA (data-*.bin in the log folder) for
// offline emulation.
void RunTamperGuards()
{
    static constexpr uint64_t Guards[] = {0x100257c8c, 0x10042b434, 0x100aa56c0, 0x100bbd54c, 0x100c0c318, 0x100f81860};
    // a byte of the "Cryptic Apps" string in __text, never executed
    constexpr uint64_t FlipAddress = 0x10025ccc8;

    const auto* counter = GetTamperCounter();
    if (!counter)
    {
        spdlog::warn("[Integrity] run_tamper_guards: the tamper counter isn't set up");
        return;
    }

    // __DATA, __thread_*, __bss and __common of the game; the guards keep their state there (other threads write some
    // of it too, so compare the same guard with and without the flipped byte)
    static constexpr uint64_t DataStart = 0x104d90000;
    static constexpr uint64_t DataEnd = 0x1056d8000;
    const auto* data = Image::Get()->Resolve<const uint64_t>(DataStart);
    static constexpr size_t DataWords = (DataEnd - DataStart) / sizeof(uint64_t);
    std::vector<uint64_t> before(DataWords);

    // raw dumps for offline emulation (docs/macos/re-tooling.md), written to the log folder
    const auto dump = [data](std::string_view aName)
    {
        const auto path = App::Get()->GetPaths()->GetLogsDir() / fmt::format("data-{}.bin", aName);
        if (auto* file = std::fopen(path.c_str(), "wb"))
        {
            std::fwrite(data, sizeof(uint64_t), DataWords, file);
            std::fclose(file);
            spdlog::info("[Integrity] run_tamper_guards: dumped __DATA ({:#x}-{:#x}, slide {:#x}) to {}", DataStart,
                         DataEnd, Image::Get()->GetSlide(), path.string());
        }
    };

    const auto runAll = [counter, data, &before](std::string_view aPhase)
    {
        spdlog::info("[Integrity] run_tamper_guards ({}): counter before: {:#x}", aPhase,
                     static_cast<uint32_t>(*counter));
        for (auto address : Guards)
        {
            std::memcpy(before.data(), data, DataWords * sizeof(uint64_t));
            Image::Get()->Resolve<void()>(address)();

            std::string changes;
            size_t changeCount = 0;
            for (size_t i = 0; i < DataWords; i++)
            {
                if (data[i] != before[i])
                {
                    if (changeCount++ < 48)
                    {
                        changes += fmt::format(" {:#x}:{:#x}->{:#x}", DataStart + i * sizeof(uint64_t), before[i],
                                               data[i]);
                    }
                }
            }
            spdlog::info("[Integrity] run_tamper_guards ({}): after {:#x}: counter {:#x}, {} changed words:{}", aPhase,
                         address, static_cast<uint32_t>(*counter), changeCount, changes);
        }
    };

    dump("before");
    runAll("code as loaded");
    dump("clean");

    // then a change made after the game's startup guards ran, undone afterwards
    const auto flip = Image::Get()->Resolve(FlipAddress);
    const uint8_t original = *reinterpret_cast<const uint8_t*>(flip);
    const uint8_t flipped = original ^ 0x01;
    if (!Memory::WriteCode(flip, &flipped, 1))
    {
        spdlog::warn("[Integrity] run_tamper_guards: could not flip the byte at {:#x}", FlipAddress);
        return;
    }
    runAll("one byte flipped at runtime");
    dump("flipped");
    Memory::WriteCode(flip, &original, 1);
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
                                            const auto start = std::chrono::steady_clock::now();
                                            const bool checkValidity = ticks % 12 == 0;
                                            WindowState::LogIfChanged();
                                            LogTamperCounter();
                                            InputClock(App::Get()->GetConfig()->GetDev().resetInputClock,
                                                       App::Get()->GetConfig()->GetDev().inputClockOffset);
                                            if (App::Get()->GetConfig()->GetDev().watchTamperState)
                                            {
                                                TamperWatch::Arm();
                                            }
                                            if (ticks == 6 && App::Get()->GetConfig()->GetDev().runTamperGuards)
                                            {
                                                RunTamperGuards();
                                            }
                                            if (ticks++ % 12 == 0)
                                            {
                                                CheckValidity();
                                            }
                                            const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                std::chrono::steady_clock::now() - start);
                                            if (took.count() >= 20)
                                            {
                                                spdlog::debug("[Stall] TWASE's periodic check took {} ms on the main "
                                                              "thread{}",
                                                              took.count(), checkValidity ? " (with SecCodeCheckValidity)" : "");
                                            }
                                        });
    dispatch_resume(timer);

    // watchdog: how long the main thread takes to run a block posted to it every 100 ms; long waits mean events (clicks)
    // aren't being handled either
    std::thread(
        []
        {
            pthread_setname_np("TWASE main thread watchdog");
            while (true)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                auto* done = dispatch_semaphore_create(0);
                const auto posted = std::chrono::steady_clock::now();
                dispatch_async_f(dispatch_get_main_queue(), done,
                                 [](void* aDone) { dispatch_semaphore_signal(static_cast<dispatch_semaphore_t>(aDone)); });
                dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
                dispatch_release(done);
                const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - posted);
                if (waited.count() >= 150)
                {
                    spdlog::debug("[Stall] main thread blocked for {} ms", waited.count());
                }
            }
        })
        .detach();
}
