#include "stdafx.hpp"

#include "InputHook.hpp"

#include "../UI/LuaConsole.hpp"

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>

#include <imgui.h>

namespace
{
struct QueuedEvent
{
    enum class Type { Key, Text, MousePos, MouseButton, Scroll, Modifiers } type;

    ImGuiKey key = ImGuiKey_None;
    bool down = false;
    int button = 0;
    float x = 0.0f;
    float y = 0.0f;
    NSEventModifierFlags modifiers = 0;
    std::string text;
};

std::atomic<bool> isAttaching = false;
id monitor = nil;

std::mutex queueMutex;
std::vector<QueuedEvent> queue;
// size of the view the events are relative to, in points
double viewWidth = 0.0;
double viewHeight = 0.0;

unsigned short consoleKeyCode = kVK_ANSI_Grave;

ImGuiKey KeyCodeToImGuiKey(unsigned short aKeyCode)
{
    switch (aKeyCode)
    {
    case kVK_ANSI_A: return ImGuiKey_A;
    case kVK_ANSI_S: return ImGuiKey_S;
    case kVK_ANSI_D: return ImGuiKey_D;
    case kVK_ANSI_F: return ImGuiKey_F;
    case kVK_ANSI_H: return ImGuiKey_H;
    case kVK_ANSI_G: return ImGuiKey_G;
    case kVK_ANSI_Z: return ImGuiKey_Z;
    case kVK_ANSI_X: return ImGuiKey_X;
    case kVK_ANSI_C: return ImGuiKey_C;
    case kVK_ANSI_V: return ImGuiKey_V;
    case kVK_ANSI_B: return ImGuiKey_B;
    case kVK_ANSI_Q: return ImGuiKey_Q;
    case kVK_ANSI_W: return ImGuiKey_W;
    case kVK_ANSI_E: return ImGuiKey_E;
    case kVK_ANSI_R: return ImGuiKey_R;
    case kVK_ANSI_Y: return ImGuiKey_Y;
    case kVK_ANSI_T: return ImGuiKey_T;
    case kVK_ANSI_1: return ImGuiKey_1;
    case kVK_ANSI_2: return ImGuiKey_2;
    case kVK_ANSI_3: return ImGuiKey_3;
    case kVK_ANSI_4: return ImGuiKey_4;
    case kVK_ANSI_6: return ImGuiKey_6;
    case kVK_ANSI_5: return ImGuiKey_5;
    case kVK_ANSI_Equal: return ImGuiKey_Equal;
    case kVK_ANSI_9: return ImGuiKey_9;
    case kVK_ANSI_7: return ImGuiKey_7;
    case kVK_ANSI_Minus: return ImGuiKey_Minus;
    case kVK_ANSI_8: return ImGuiKey_8;
    case kVK_ANSI_0: return ImGuiKey_0;
    case kVK_ANSI_RightBracket: return ImGuiKey_RightBracket;
    case kVK_ANSI_O: return ImGuiKey_O;
    case kVK_ANSI_U: return ImGuiKey_U;
    case kVK_ANSI_LeftBracket: return ImGuiKey_LeftBracket;
    case kVK_ANSI_I: return ImGuiKey_I;
    case kVK_ANSI_P: return ImGuiKey_P;
    case kVK_ANSI_L: return ImGuiKey_L;
    case kVK_ANSI_J: return ImGuiKey_J;
    case kVK_ANSI_Quote: return ImGuiKey_Apostrophe;
    case kVK_ANSI_K: return ImGuiKey_K;
    case kVK_ANSI_Semicolon: return ImGuiKey_Semicolon;
    case kVK_ANSI_Backslash: return ImGuiKey_Backslash;
    case kVK_ANSI_Comma: return ImGuiKey_Comma;
    case kVK_ANSI_Slash: return ImGuiKey_Slash;
    case kVK_ANSI_N: return ImGuiKey_N;
    case kVK_ANSI_M: return ImGuiKey_M;
    case kVK_ANSI_Period: return ImGuiKey_Period;
    case kVK_ANSI_Grave: return ImGuiKey_GraveAccent;
    case kVK_ANSI_KeypadDecimal: return ImGuiKey_KeypadDecimal;
    case kVK_ANSI_KeypadMultiply: return ImGuiKey_KeypadMultiply;
    case kVK_ANSI_KeypadPlus: return ImGuiKey_KeypadAdd;
    case kVK_ANSI_KeypadClear: return ImGuiKey_NumLock;
    case kVK_ANSI_KeypadDivide: return ImGuiKey_KeypadDivide;
    case kVK_ANSI_KeypadEnter: return ImGuiKey_KeypadEnter;
    case kVK_ANSI_KeypadMinus: return ImGuiKey_KeypadSubtract;
    case kVK_ANSI_KeypadEquals: return ImGuiKey_KeypadEqual;
    case kVK_ANSI_Keypad0: return ImGuiKey_Keypad0;
    case kVK_ANSI_Keypad1: return ImGuiKey_Keypad1;
    case kVK_ANSI_Keypad2: return ImGuiKey_Keypad2;
    case kVK_ANSI_Keypad3: return ImGuiKey_Keypad3;
    case kVK_ANSI_Keypad4: return ImGuiKey_Keypad4;
    case kVK_ANSI_Keypad5: return ImGuiKey_Keypad5;
    case kVK_ANSI_Keypad6: return ImGuiKey_Keypad6;
    case kVK_ANSI_Keypad7: return ImGuiKey_Keypad7;
    case kVK_ANSI_Keypad8: return ImGuiKey_Keypad8;
    case kVK_ANSI_Keypad9: return ImGuiKey_Keypad9;
    case kVK_Return: return ImGuiKey_Enter;
    case kVK_Tab: return ImGuiKey_Tab;
    case kVK_Space: return ImGuiKey_Space;
    case kVK_Delete: return ImGuiKey_Backspace;
    case kVK_Escape: return ImGuiKey_Escape;
    case kVK_CapsLock: return ImGuiKey_CapsLock;
    case kVK_Control: return ImGuiKey_LeftCtrl;
    case kVK_Shift: return ImGuiKey_LeftShift;
    case kVK_Option: return ImGuiKey_LeftAlt;
    case kVK_Command: return ImGuiKey_LeftSuper;
    case kVK_RightControl: return ImGuiKey_RightCtrl;
    case kVK_RightShift: return ImGuiKey_RightShift;
    case kVK_RightOption: return ImGuiKey_RightAlt;
    case kVK_RightCommand: return ImGuiKey_RightSuper;
    case kVK_F1: return ImGuiKey_F1;
    case kVK_F2: return ImGuiKey_F2;
    case kVK_F3: return ImGuiKey_F3;
    case kVK_F4: return ImGuiKey_F4;
    case kVK_F5: return ImGuiKey_F5;
    case kVK_F6: return ImGuiKey_F6;
    case kVK_F7: return ImGuiKey_F7;
    case kVK_F8: return ImGuiKey_F8;
    case kVK_F9: return ImGuiKey_F9;
    case kVK_F10: return ImGuiKey_F10;
    case kVK_F11: return ImGuiKey_F11;
    case kVK_F12: return ImGuiKey_F12;
    case kVK_Help: return ImGuiKey_Insert;
    case kVK_Home: return ImGuiKey_Home;
    case kVK_PageUp: return ImGuiKey_PageUp;
    case kVK_ForwardDelete: return ImGuiKey_Delete;
    case kVK_End: return ImGuiKey_End;
    case kVK_PageDown: return ImGuiKey_PageDown;
    case kVK_LeftArrow: return ImGuiKey_LeftArrow;
    case kVK_RightArrow: return ImGuiKey_RightArrow;
    case kVK_DownArrow: return ImGuiKey_DownArrow;
    case kVK_UpArrow: return ImGuiKey_UpArrow;
    default: return ImGuiKey_None;
    }
}

void Push(QueuedEvent aEvent)
{
    std::scoped_lock lock(queueMutex);
    queue.push_back(std::move(aEvent));
}

// keyDown characters, without function keys (private use area) and control characters
std::string TypedText(NSEvent* aEvent)
{
    if (aEvent.modifierFlags & NSEventModifierFlagCommand)
    {
        return {};
    }

    std::string result;
    NSString* characters = aEvent.characters;
    for (NSUInteger i = 0; i < characters.length; i++)
    {
        unichar c = [characters characterAtIndex:i];
        if (c < 0x20 || c == 0x7F || (c >= 0xF700 && c <= 0xF8FF))
        {
            return {};
        }
    }

    const char* utf8 = characters.UTF8String;
    return utf8 ? utf8 : "";
}

// returns nil to swallow the event
NSEvent* HandleEvent(NSEvent* aEvent)
{

    auto& console = LuaConsole::Get();
    const auto type = aEvent.type;

    // the key below Esc toggles the console, never pass it to the game or the input box
    if ((type == NSEventTypeKeyDown || type == NSEventTypeKeyUp) && aEvent.keyCode == consoleKeyCode)
    {
        if (type == NSEventTypeKeyDown && !aEvent.isARepeat)
        {
            console.Toggle();
            spdlog::debug("Console {}", console.IsOpen() ? "opened" : "closed");
        }

        return nil;
    }

    if (!console.IsOpen())
    {
        return aEvent;
    }

    // mouse positions relative to the window's content view, ImGui wants top left origin in points
    NSView* view = aEvent.window.contentView;
    if (view)
    {
        std::scoped_lock lock(queueMutex);
        viewWidth = view.bounds.size.width;
        viewHeight = view.bounds.size.height;
    }

    switch (type)
    {
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
    {
        const bool down = type == NSEventTypeKeyDown;
        Push({.type = QueuedEvent::Type::Modifiers, .modifiers = aEvent.modifierFlags});
        Push({.type = QueuedEvent::Type::Key, .key = KeyCodeToImGuiKey(aEvent.keyCode), .down = down});

        if (down)
        {
            auto text = TypedText(aEvent);
            if (!text.empty())
            {
                Push({.type = QueuedEvent::Type::Text, .text = std::move(text)});
            }
        }
        return nil;
    }
    case NSEventTypeFlagsChanged:
        Push({.type = QueuedEvent::Type::Modifiers, .modifiers = aEvent.modifierFlags});
        return nil;
    case NSEventTypeMouseMoved:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
    {
        if (view)
        {
            auto location = [view convertPoint:aEvent.locationInWindow fromView:nil];
            auto y = view.isFlipped ? location.y : view.bounds.size.height - location.y;
            Push({.type = QueuedEvent::Type::MousePos, .x = static_cast<float>(location.x), .y = static_cast<float>(y)});
        }

        bool isDown = type == NSEventTypeLeftMouseDown || type == NSEventTypeRightMouseDown ||
                      type == NSEventTypeOtherMouseDown;
        bool isUp = type == NSEventTypeLeftMouseUp || type == NSEventTypeRightMouseUp || type == NSEventTypeOtherMouseUp;
        if (isDown || isUp)
        {
            // ImGui: 0 left, 1 right, 2 middle
            auto button = static_cast<int>(aEvent.buttonNumber);
            if (button < ImGuiMouseButton_COUNT)
            {
                Push({.type = QueuedEvent::Type::MouseButton, .down = isDown, .button = button});
            }
        }
        return nil;
    }
    case NSEventTypeScrollWheel:
    {
        double dx = aEvent.scrollingDeltaX;
        double dy = aEvent.scrollingDeltaY;
        if (aEvent.hasPreciseScrollingDeltas)
        {
            dx *= 0.1;
            dy *= 0.1;
        }
        Push({.type = QueuedEvent::Type::Scroll, .x = static_cast<float>(dx), .y = static_cast<float>(dy)});
        return nil;
    }
    default:
        return aEvent;
    }
}

void InstallMonitor()
{
    if (monitor)
    {
        return;
    }

    // on ISO keyboards (e.g. German) the key below Esc reports kVK_ISO_Section, on ANSI keyboards kVK_ANSI_Grave
    const bool isISO = KBGetLayoutType(LMGetKbdType()) == kKeyboardISO;
    consoleKeyCode = isISO ? static_cast<unsigned short>(kVK_ISO_Section) : static_cast<unsigned short>(kVK_ANSI_Grave);

    constexpr NSEventMask mask = NSEventMaskKeyDown | NSEventMaskKeyUp | NSEventMaskFlagsChanged |
                                 NSEventMaskMouseMoved | NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp |
                                 NSEventMaskRightMouseDown | NSEventMaskRightMouseUp | NSEventMaskOtherMouseDown |
                                 NSEventMaskOtherMouseUp | NSEventMaskLeftMouseDragged |
                                 NSEventMaskRightMouseDragged | NSEventMaskOtherMouseDragged | NSEventMaskScrollWheel;

    monitor = [NSEvent addLocalMonitorForEventsMatchingMask:mask handler:^NSEvent*(NSEvent* event) {
      return HandleEvent(event);
    }];

    spdlog::info("Input monitor installed (console key code {:#x})", consoleKeyCode);
}

void SetModifiers(ImGuiIO& aIO, NSEventModifierFlags aFlags)
{
    aIO.AddKeyEvent(ImGuiMod_Ctrl, (aFlags & NSEventModifierFlagControl) != 0);
    aIO.AddKeyEvent(ImGuiMod_Shift, (aFlags & NSEventModifierFlagShift) != 0);
    aIO.AddKeyEvent(ImGuiMod_Alt, (aFlags & NSEventModifierFlagOption) != 0);
    aIO.AddKeyEvent(ImGuiMod_Super, (aFlags & NSEventModifierFlagCommand) != 0);
}

const char* GetClipboardText(ImGuiContext*)
{
    static std::string text;
    NSString* string = [NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString];
    text = string ? string.UTF8String : "";
    return text.c_str();
}

void SetClipboardText(ImGuiContext*, const char* aText)
{
    [NSPasteboard.generalPasteboard clearContents];
    [NSPasteboard.generalPasteboard setString:[NSString stringWithUTF8String:aText] forType:NSPasteboardTypeString];
}
} // namespace

void Hooks::InputHook::Attach()
{
    if (monitor || isAttaching.exchange(true))
    {
        return;
    }

    if ([NSThread isMainThread])
    {
        InstallMonitor();
    }
    else
    {
        dispatch_async(dispatch_get_main_queue(), ^{
          InstallMonitor();
        });
    }
}

void Hooks::InputHook::Detach()
{
    if (monitor)
    {
        [NSEvent removeMonitor:monitor];
        monitor = nil;
    }
}

void Hooks::InputHook::NewFrame(ImGuiIO& aIO, double aFramebufferWidth, double aFramebufferHeight)
{
    std::vector<QueuedEvent> events;
    double width;
    double height;
    {
        std::scoped_lock lock(queueMutex);
        events.swap(queue);
        width = viewWidth;
        height = viewHeight;
    }

    // until we know the view size, assume the drawable is 1:1 with points
    if (width <= 0.0 || height <= 0.0)
    {
        width = aFramebufferWidth;
        height = aFramebufferHeight;
    }

    aIO.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    aIO.DisplayFramebufferScale = ImVec2(static_cast<float>(aFramebufferWidth / width),
                                         static_cast<float>(aFramebufferHeight / height));

    auto& platformIO = ImGui::GetPlatformIO();
    platformIO.Platform_GetClipboardTextFn = GetClipboardText;
    platformIO.Platform_SetClipboardTextFn = SetClipboardText;

    for (const auto& event : events)
    {
        switch (event.type)
        {
        case QueuedEvent::Type::Key:
            if (event.key != ImGuiKey_None)
            {
                aIO.AddKeyEvent(event.key, event.down);
            }
            break;
        case QueuedEvent::Type::Text: aIO.AddInputCharactersUTF8(event.text.c_str()); break;
        case QueuedEvent::Type::MousePos: aIO.AddMousePosEvent(event.x, event.y); break;
        case QueuedEvent::Type::MouseButton: aIO.AddMouseButtonEvent(event.button, event.down); break;
        case QueuedEvent::Type::Scroll: aIO.AddMouseWheelEvent(event.x, event.y); break;
        case QueuedEvent::Type::Modifiers: SetModifiers(aIO, event.modifiers); break;
        }
    }
}
