#include "stdafx.hpp"

#include "WindowState.hpp"

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

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
        const NSRect content = window.contentView ? window.contentView.bounds : NSZeroRect;
        state += fmt::format("; window \"{}\" frame {}x{} at {},{} content {}x{} scale {}{}{}{}", window.title.UTF8String ?: "",
                             frame.size.width, frame.size.height, frame.origin.x, frame.origin.y, content.size.width,
                             content.size.height, window.backingScaleFactor,
                             (window.styleMask & NSWindowStyleMaskFullScreen) ? " fullscreen" : "",
                             window.isMiniaturized ? " minimised" : "", window.isKeyWindow ? " key" : "");
        if (CAMetalLayer* layer = FindMetalLayer(window.contentView.layer))
        {
            state += fmt::format(" metal {}x{} layer {}x{}", layer.drawableSize.width, layer.drawableSize.height,
                                 layer.bounds.size.width, layer.bounds.size.height);
        }
    }

    if (state != lastState)
    {
        spdlog::info("[Window] {}", state);
        lastState = std::move(state);
    }
}
