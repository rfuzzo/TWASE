#include "stdafx.hpp"

#include "WindowState.hpp"

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>

namespace
{
std::string lastState;

CAMetalLayer* FindMetalLayer(CALayer* aLayer)
{
    if (!aLayer)
    {
        return nil;
    }
    if ([aLayer isKindOfClass:[CAMetalLayer class]])
    {
        return (CAMetalLayer*)aLayer;
    }
    for (CALayer* sublayer in aLayer.sublayers)
    {
        if (CAMetalLayer* found = FindMetalLayer(sublayer))
        {
            return found;
        }
    }
    return nil;
}

// an integer or bool instance variable of a Feral window class by name ("-" if the class has none)
std::string IvarText(NSWindow* aWindow, const char* aName)
{
    Ivar ivar = class_getInstanceVariable(object_getClass(aWindow), aName);
    if (!ivar)
    {
        return "-";
    }
    const auto* field = reinterpret_cast<const uint8_t*>((__bridge void*)aWindow) + ivar_getOffset(ivar);
    switch (ivar_getTypeEncoding(ivar)[0])
    {
    case 'i':
    case 'I':
        return std::to_string(*reinterpret_cast<const int32_t*>(field));
    case 'B':
    case 'c':
    case 'C':
        return std::to_string(*field);
    default:
        return fmt::format("?{}", ivar_getTypeEncoding(ivar));
    }
}
} // namespace

void WindowState::LogIfChanged()
{
    std::string state = fmt::format("app {}", NSApp.isActive ? "active" : "inactive");
    for (NSWindow* window in NSApp.windows)
    {
        if (!window.isVisible && !window.isMiniaturized)
        {
            continue;
        }
        const NSRect frame = window.frame;
        const NSRect content = window.contentView ? window.contentView.frame : NSZeroRect;
        state += fmt::format("; window \"{}\" frame {}x{} at {},{} content {}x{} at {},{} scale {}{}{}{}",
                             window.title.UTF8String ?: "", frame.size.width, frame.size.height, frame.origin.x,
                             frame.origin.y, content.size.width, content.size.height, content.origin.x,
                             content.origin.y, window.backingScaleFactor,
                             (window.styleMask & NSWindowStyleMaskFullScreen) ? " fullscreen" : "",
                             window.isMiniaturized ? " minimised" : "", window.isKeyWindow ? " key" : "");
        if (CAMetalLayer* layer = FindMetalLayer(window.contentView.layer))
        {
            state += fmt::format(" metal {}x{} layer {}x{}", layer.drawableSize.width, layer.drawableSize.height,
                                 layer.bounds.size.width, layer.bounds.size.height);
        }
        // Feral's CFeralNSWindow: mFrameState 2 = live drag (title bar click), see docs/macos/integrity-and-quirks.md
        if (class_getInstanceVariable(object_getClass(window), "mFrameState"))
        {
            state += fmt::format(" frameState {} grabInput {} scheduledDrag {}", IvarText(window, "mFrameState"),
                                 IvarText(window, "mGrabInputFocus"), IvarText(window, "mScheduledLiveDrag"));
        }
    }

    if (state != lastState)
    {
        spdlog::info("[Window] {}", state);
        lastState = std::move(state);
    }
}
