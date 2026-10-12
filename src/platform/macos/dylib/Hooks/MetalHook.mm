#include "stdafx.hpp"

#include "MetalHook.hpp"
#include "InputHook.hpp"

#include "../App.hpp"
#include "../UI/LuaConsole.hpp"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <objc/runtime.h>
#include <pthread.h>

#include <imgui.h>
#include <imgui_impl_metal.h>

namespace
{
using NextDrawable_t = id (*)(id, SEL);
using PresentDrawable_t = void (*)(id, SEL, id);
using PresentDrawableTime_t = void (*)(id, SEL, id, CFTimeInterval);
using DrawablePresent_t = void (*)(id, SEL);
using DrawablePresentTime_t = void (*)(id, SEL, CFTimeInterval);

NextDrawable_t originalNextDrawable = nullptr;
PresentDrawable_t originalPresentDrawable = nullptr;
PresentDrawableTime_t originalPresentDrawableAtTime = nullptr;
PresentDrawableTime_t originalPresentDrawableAfter = nullptr;
DrawablePresent_t originalDrawablePresent = nullptr;
DrawablePresentTime_t originalDrawablePresentAtTime = nullptr;
DrawablePresentTime_t originalDrawablePresentAfter = nullptr;

std::once_flag classesHooked;
std::mutex renderMutex;
bool imguiInitialized = false;
std::string imguiIniPath;

// swizzles aSelector on aClass, returns the previous implementation (or the inherited one)
IMP Swizzle(Class aClass, SEL aSelector, IMP aReplacement)
{
    Method method = class_getInstanceMethod(aClass, aSelector);
    if (!method)
    {
        spdlog::warn("MetalHook: {} has no method {}", class_getName(aClass), sel_getName(aSelector));
        return nullptr;
    }

    // inherited methods get an override on this class so the superclass stays untouched
    if (class_addMethod(aClass, aSelector, aReplacement, method_getTypeEncoding(method)))
    {
        return method_getImplementation(method);
    }

    return method_setImplementation(method, aReplacement);
}

// logs the first call of each present path, so we know what Feral uses and on which thread
void LogFirstCall(std::atomic<bool>& aLogged, const char* aPath)
{
    if (!aLogged.exchange(true))
    {
        uint64_t tid = 0;
        pthread_threadid_np(nullptr, &tid);
        spdlog::info("MetalHook: first {} (thread {}, main thread: {})", aPath, tid, pthread_main_np() != 0);
    }
}

void InitImGui(id<MTLDevice> aDevice)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    auto& io = ImGui::GetIO();
    imguiIniPath = (App::Get()->GetPaths()->GetTWASEDir() / "imgui.ini").string();
    io.IniFilename = imguiIniPath.c_str();
    io.BackendPlatformName = "twase_macos";

    ImGui_ImplMetal_Init(aDevice);
    imguiInitialized = true;

    spdlog::info("ImGui initialized on the game's Metal device ({})", aDevice.name.UTF8String);
}

// Encodes the overlay into aCommandBuffer, drawing on top of the drawable's texture
void RenderOverlay(id<MTLCommandBuffer> aCommandBuffer, id<CAMetalDrawable> aDrawable)
{
    if (!aCommandBuffer || !aDrawable)
    {
        return;
    }

    std::scoped_lock lock(renderMutex);

    if (!imguiInitialized)
    {
        InitImGui(aCommandBuffer.device);
    }

    id<MTLTexture> texture = aDrawable.texture;
    auto& io = ImGui::GetIO();
    Hooks::InputHook::NewFrame(io, static_cast<double>(texture.width), static_cast<double>(texture.height));

    if (!LuaConsole::Get().IsOpen())
    {
        return;
    }

    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;

    ImGui_ImplMetal_NewFrame(pass);
    ImGui::NewFrame();

    // the game hides the system cursor, draw ImGui's while the console is open
    io.MouseDrawCursor = true;

    LuaConsole::Get().Draw();

    ImGui::Render();

    auto drawData = ImGui::GetDrawData();
    if (drawData && drawData->CmdListsCount > 0)
    {
        id<MTLRenderCommandEncoder> encoder = [aCommandBuffer renderCommandEncoderWithDescriptor:pass];
        [encoder pushDebugGroup:@"TWASE"];
        ImGui_ImplMetal_RenderDrawData(drawData, aCommandBuffer, encoder);
        [encoder popDebugGroup];
        [encoder endEncoding];
    }
}

// -[MTLCommandBuffer presentDrawable:...] variants, render on the game's command buffer
void HookedPresentDrawable(id self, SEL _cmd, id drawable)
{
    static std::atomic<bool> logged = false;
    LogFirstCall(logged, "-[MTLCommandBuffer presentDrawable:]");

    RenderOverlay(self, drawable);
    originalPresentDrawable(self, _cmd, drawable);
}

void HookedPresentDrawableAtTime(id self, SEL _cmd, id drawable, CFTimeInterval time)
{
    static std::atomic<bool> logged = false;
    LogFirstCall(logged, "-[MTLCommandBuffer presentDrawable:atTime:]");

    RenderOverlay(self, drawable);
    originalPresentDrawableAtTime(self, _cmd, drawable, time);
}

void HookedPresentDrawableAfter(id self, SEL _cmd, id drawable, CFTimeInterval duration)
{
    static std::atomic<bool> logged = false;
    LogFirstCall(logged, "-[MTLCommandBuffer presentDrawable:afterMinimumDuration:]");

    RenderOverlay(self, drawable);
    originalPresentDrawableAfter(self, _cmd, drawable, duration);
}

// -[CAMetalDrawable present...] variants, only logged for now (no command buffer to render on)
void HookedDrawablePresent(id self, SEL _cmd)
{
    static std::atomic<bool> logged = false;
    LogFirstCall(logged, "-[CAMetalDrawable present]");

    originalDrawablePresent(self, _cmd);
}

void HookedDrawablePresentAtTime(id self, SEL _cmd, CFTimeInterval time)
{
    static std::atomic<bool> logged = false;
    LogFirstCall(logged, "-[CAMetalDrawable presentAtTime:]");

    originalDrawablePresentAtTime(self, _cmd, time);
}

void HookedDrawablePresentAfter(id self, SEL _cmd, CFTimeInterval duration)
{
    static std::atomic<bool> logged = false;
    LogFirstCall(logged, "-[CAMetalDrawable presentAfterMinimumDuration:]");

    originalDrawablePresentAfter(self, _cmd, duration);
}

void HookPresentClasses(CAMetalLayer* aLayer, id aDrawable)
{
    // the concrete command buffer class is private and depends on the GPU, ask the device for one
    id<MTLCommandQueue> queue = [aLayer.device newCommandQueue];
    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    Class commandBufferClass = object_getClass(commandBuffer);
    Class drawableClass = object_getClass(aDrawable);

    spdlog::info("MetalHook: layer {} ({}x{}, scale {}, pixel format {}), command buffer class {}, drawable class {}",
                 static_cast<void*>((__bridge void*)aLayer), aLayer.drawableSize.width, aLayer.drawableSize.height,
                 aLayer.contentsScale, static_cast<int>(aLayer.pixelFormat), class_getName(commandBufferClass),
                 class_getName(drawableClass));

    originalPresentDrawable = reinterpret_cast<PresentDrawable_t>(
        Swizzle(commandBufferClass, @selector(presentDrawable:), reinterpret_cast<IMP>(HookedPresentDrawable)));
    originalPresentDrawableAtTime = reinterpret_cast<PresentDrawableTime_t>(Swizzle(
        commandBufferClass, @selector(presentDrawable:atTime:), reinterpret_cast<IMP>(HookedPresentDrawableAtTime)));
    originalPresentDrawableAfter = reinterpret_cast<PresentDrawableTime_t>(
        Swizzle(commandBufferClass, @selector(presentDrawable:afterMinimumDuration:),
                reinterpret_cast<IMP>(HookedPresentDrawableAfter)));

    originalDrawablePresent = reinterpret_cast<DrawablePresent_t>(
        Swizzle(drawableClass, @selector(present), reinterpret_cast<IMP>(HookedDrawablePresent)));
    originalDrawablePresentAtTime = reinterpret_cast<DrawablePresentTime_t>(
        Swizzle(drawableClass, @selector(presentAtTime:), reinterpret_cast<IMP>(HookedDrawablePresentAtTime)));
    originalDrawablePresentAfter = reinterpret_cast<DrawablePresentTime_t>(Swizzle(
        drawableClass, @selector(presentAfterMinimumDuration:), reinterpret_cast<IMP>(HookedDrawablePresentAfter)));

    spdlog::info("MetalHook: present hooks attached");
}

id HookedNextDrawable(id self, SEL _cmd)
{
    id drawable = originalNextDrawable(self, _cmd);

    if (drawable)
    {
        std::call_once(classesHooked, [&] {
            static std::atomic<bool> logged = false;
            LogFirstCall(logged, "-[CAMetalLayer nextDrawable]");

            HookPresentClasses(self, drawable);

            // NSApp exists by now, the game is rendering
            Hooks::InputHook::Attach();
        });
    }

    return drawable;
}
} // namespace

bool Hooks::MetalHook::Attach()
{
    originalNextDrawable = reinterpret_cast<NextDrawable_t>(
        Swizzle([CAMetalLayer class], @selector(nextDrawable), reinterpret_cast<IMP>(HookedNextDrawable)));

    if (!originalNextDrawable)
    {
        spdlog::error("MetalHook: could not hook -[CAMetalLayer nextDrawable], the console will not be available");
        return false;
    }

    spdlog::info("MetalHook: -[CAMetalLayer nextDrawable] hook attached");
    return true;
}
