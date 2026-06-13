#include "clickinjector.h"

// ─────────────────────────────────────────────────────────────
//  WINDOWS
// ─────────────────────────────────────────────────────────────
#if defined(PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static void sendMouseEvent(DWORD flags, int x, int y, DWORD data = 0)
{
    INPUT in = {};
    in.type           = INPUT_MOUSE;
    in.mi.dx          = static_cast<LONG>((x * 65536) / GetSystemMetrics(SM_CXVIRTUALSCREEN));
    in.mi.dy          = static_cast<LONG>((y * 65536) / GetSystemMetrics(SM_CYVIRTUALSCREEN));
    in.mi.mouseData   = data;
    in.mi.dwFlags     = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | flags;
    SendInput(1, &in, sizeof(INPUT));
}

static void sendKeyEvent(WORD vk, bool down)
{
    INPUT in = {};
    in.type       = INPUT_KEYBOARD;
    in.ki.wVk     = vk;
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof(INPUT));
}

void ClickInjector::pressModifiers(int mods)
{
    if (mods & ModCtrl)  sendKeyEvent(VK_CONTROL, true);
    if (mods & ModAlt)   sendKeyEvent(VK_MENU,    true);
    if (mods & ModShift) sendKeyEvent(VK_SHIFT,   true);
}

void ClickInjector::releaseModifiers(int mods)
{
    if (mods & ModShift) sendKeyEvent(VK_SHIFT,   false);
    if (mods & ModAlt)   sendKeyEvent(VK_MENU,    false);
    if (mods & ModCtrl)  sendKeyEvent(VK_CONTROL, false);
}

void ClickInjector::moveCursor(QPoint pos)
{
    sendMouseEvent(MOUSEEVENTF_MOVE, pos.x(), pos.y());
}

void ClickInjector::performClick(ClickType type, QPoint pos, int mods)
{
    // Move first
    sendMouseEvent(MOUSEEVENTF_MOVE, pos.x(), pos.y());
    pressModifiers(mods);

    switch (type) {
    case ClickType::LeftClick:
        sendMouseEvent(MOUSEEVENTF_LEFTDOWN,  pos.x(), pos.y());
        sendMouseEvent(MOUSEEVENTF_LEFTUP,    pos.x(), pos.y());
        break;
    case ClickType::LeftDoubleClick:
        sendMouseEvent(MOUSEEVENTF_LEFTDOWN,  pos.x(), pos.y());
        sendMouseEvent(MOUSEEVENTF_LEFTUP,    pos.x(), pos.y());
        sendMouseEvent(MOUSEEVENTF_LEFTDOWN,  pos.x(), pos.y());
        sendMouseEvent(MOUSEEVENTF_LEFTUP,    pos.x(), pos.y());
        break;
    case ClickType::LeftDown:
        sendMouseEvent(MOUSEEVENTF_LEFTDOWN,  pos.x(), pos.y());
        break;
    case ClickType::LeftUp:
        sendMouseEvent(MOUSEEVENTF_LEFTUP,    pos.x(), pos.y());
        break;
    case ClickType::RightClick:
        sendMouseEvent(MOUSEEVENTF_RIGHTDOWN, pos.x(), pos.y());
        sendMouseEvent(MOUSEEVENTF_RIGHTUP,   pos.x(), pos.y());
        break;
    case ClickType::RightDoubleClick:
        sendMouseEvent(MOUSEEVENTF_RIGHTDOWN, pos.x(), pos.y());
        sendMouseEvent(MOUSEEVENTF_RIGHTUP,   pos.x(), pos.y());
        sendMouseEvent(MOUSEEVENTF_RIGHTDOWN, pos.x(), pos.y());
        sendMouseEvent(MOUSEEVENTF_RIGHTUP,   pos.x(), pos.y());
        break;
    case ClickType::RightDown:
        sendMouseEvent(MOUSEEVENTF_RIGHTDOWN, pos.x(), pos.y());
        break;
    case ClickType::RightUp:
        sendMouseEvent(MOUSEEVENTF_RIGHTUP,   pos.x(), pos.y());
        break;
    case ClickType::MiddleClick:
        sendMouseEvent(MOUSEEVENTF_MIDDLEDOWN, pos.x(), pos.y());
        sendMouseEvent(MOUSEEVENTF_MIDDLEUP,   pos.x(), pos.y());
        break;
    case ClickType::MiddleDoubleClick:
        sendMouseEvent(MOUSEEVENTF_MIDDLEDOWN, pos.x(), pos.y());
        sendMouseEvent(MOUSEEVENTF_MIDDLEUP,   pos.x(), pos.y());
        sendMouseEvent(MOUSEEVENTF_MIDDLEDOWN, pos.x(), pos.y());
        sendMouseEvent(MOUSEEVENTF_MIDDLEUP,   pos.x(), pos.y());
        break;
    case ClickType::ScrollUp:
        sendMouseEvent(MOUSEEVENTF_WHEEL, pos.x(), pos.y(), static_cast<DWORD>(WHEEL_DELTA));
        break;
    case ClickType::ScrollDown:
        sendMouseEvent(MOUSEEVENTF_WHEEL, pos.x(), pos.y(), static_cast<DWORD>(-WHEEL_DELTA));
        break;
    case ClickType::ScrollLeft:
        sendMouseEvent(MOUSEEVENTF_HWHEEL, pos.x(), pos.y(), static_cast<DWORD>(-WHEEL_DELTA));
        break;
    case ClickType::ScrollRight:
        sendMouseEvent(MOUSEEVENTF_HWHEEL, pos.x(), pos.y(), static_cast<DWORD>(WHEEL_DELTA));
        break;
    default: break;
    }

    releaseModifiers(mods);
}

// ─────────────────────────────────────────────────────────────
//  macOS
// ─────────────────────────────────────────────────────────────
#elif defined(PLATFORM_MACOS)
#include <ApplicationServices/ApplicationServices.h>

static CGEventRef makeMouseEvent(CGEventType evType, CGMouseButton btn, CGPoint pt)
{
    return CGEventCreateMouseEvent(nullptr, evType, pt, btn);
}

static void postMouse(CGEventType evType, CGMouseButton btn, QPoint pos)
{
    CGPoint pt{ static_cast<CGFloat>(pos.x()), static_cast<CGFloat>(pos.y()) };
    CGEventRef ev = makeMouseEvent(evType, btn, pt);
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);
}

static void postKey(CGKeyCode key, bool down, CGEventFlags flags = 0)
{
    CGEventRef ev = CGEventCreateKeyboardEvent(nullptr, key, down);
    if (flags) CGEventSetFlags(ev, flags);
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);
}

static CGEventFlags modsToCGFlags(int mods)
{
    CGEventFlags f = 0;
    if (mods & ModCtrl)  f |= kCGEventFlagMaskControl;
    if (mods & ModAlt)   f |= kCGEventFlagMaskAlternate;
    if (mods & ModShift) f |= kCGEventFlagMaskShift;
    return f;
}

void ClickInjector::pressModifiers(int) {}   // handled via CGEventFlags
void ClickInjector::releaseModifiers(int) {}

void ClickInjector::moveCursor(QPoint pos)
{
    CGPoint pt{ static_cast<CGFloat>(pos.x()), static_cast<CGFloat>(pos.y()) };
    CGEventRef ev = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, pt, kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);
}

void ClickInjector::performClick(ClickType type, QPoint pos, int mods)
{
    CGEventFlags flags = modsToCGFlags(mods);
    CGPoint pt{ static_cast<CGFloat>(pos.x()), static_cast<CGFloat>(pos.y()) };

    auto post = [&](CGEventType evType, CGMouseButton btn, int clickCount = 1) {
        CGEventRef ev = CGEventCreateMouseEvent(nullptr, evType, pt, btn);
        CGEventSetIntegerValueField(ev, kCGMouseEventClickState, clickCount);
        if (flags) CGEventSetFlags(ev, flags);
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
    };

    switch (type) {
    case ClickType::LeftClick:
        post(kCGEventLeftMouseDown,  kCGMouseButtonLeft, 1);
        post(kCGEventLeftMouseUp,    kCGMouseButtonLeft, 1);
        break;
    case ClickType::LeftDoubleClick:
        post(kCGEventLeftMouseDown,  kCGMouseButtonLeft, 2);
        post(kCGEventLeftMouseUp,    kCGMouseButtonLeft, 2);
        break;
    case ClickType::LeftDown:
        post(kCGEventLeftMouseDown,  kCGMouseButtonLeft);
        break;
    case ClickType::LeftUp:
        post(kCGEventLeftMouseUp,    kCGMouseButtonLeft);
        break;
    case ClickType::RightClick:
        post(kCGEventRightMouseDown, kCGMouseButtonRight, 1);
        post(kCGEventRightMouseUp,   kCGMouseButtonRight, 1);
        break;
    case ClickType::RightDoubleClick:
        post(kCGEventRightMouseDown, kCGMouseButtonRight, 2);
        post(kCGEventRightMouseUp,   kCGMouseButtonRight, 2);
        break;
    case ClickType::RightDown:
        post(kCGEventRightMouseDown, kCGMouseButtonRight);
        break;
    case ClickType::RightUp:
        post(kCGEventRightMouseUp,   kCGMouseButtonRight);
        break;
    case ClickType::MiddleClick:
        post(kCGEventOtherMouseDown, kCGMouseButtonCenter, 1);
        post(kCGEventOtherMouseUp,   kCGMouseButtonCenter, 1);
        break;
    case ClickType::MiddleDoubleClick:
        post(kCGEventOtherMouseDown, kCGMouseButtonCenter, 2);
        post(kCGEventOtherMouseUp,   kCGMouseButtonCenter, 2);
        break;
    case ClickType::ScrollUp:
    case ClickType::ScrollDown:
    case ClickType::ScrollLeft:
    case ClickType::ScrollRight: {
        int vert = 0, horiz = 0;
        if (type == ClickType::ScrollUp)    vert  =  10;
        if (type == ClickType::ScrollDown)  vert  = -10;
        if (type == ClickType::ScrollLeft)  horiz = -10;
        if (type == ClickType::ScrollRight) horiz =  10;
        CGEventRef ev = CGEventCreateScrollWheelEvent(
            nullptr, kCGScrollEventUnitLine, 2, vert, horiz);
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
        break;
    }
    default: break;
    }
}

// ─────────────────────────────────────────────────────────────
//  Linux / X11
// ─────────────────────────────────────────────────────────────
#elif defined(PLATFORM_LINUX)
#include <QGuiApplication>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

static Display* getDisplay()
{
    static Display* dpy = XOpenDisplay(nullptr);
    return dpy;
}

static void fakeButton(int button, bool press, int mods = 0)
{
    Display* dpy = getDisplay();
    if (!dpy) return;
    // Modifiers
    if (press) {
        if (mods & ModCtrl)  XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Control_L), True,  0);
        if (mods & ModAlt)   XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Alt_L),     True,  0);
        if (mods & ModShift) XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Shift_L),   True,  0);
    }
    XTestFakeButtonEvent(dpy, button, press ? True : False, 0);
    if (!press) {
        if (mods & ModShift) XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Shift_L),   False, 0);
        if (mods & ModAlt)   XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Alt_L),     False, 0);
        if (mods & ModCtrl)  XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Control_L), False, 0);
    }
    XFlush(dpy);
}

static void click(int button, int mods = 0)
{
    fakeButton(button, true,  mods);
    fakeButton(button, false, mods);
}

static void doubleClick(int button, int mods = 0)
{
    click(button, mods);
    click(button, mods);
}

void ClickInjector::pressModifiers(int) {}
void ClickInjector::releaseModifiers(int) {}

void ClickInjector::moveCursor(QPoint pos)
{
    Display* dpy = getDisplay();
    if (!dpy) return;
    XTestFakeMotionEvent(dpy, -1, pos.x(), pos.y(), 0);
    XFlush(dpy);
}

void ClickInjector::performClick(ClickType type, QPoint pos, int mods)
{
    Display* dpy = getDisplay();
    if (!dpy) return;
    XTestFakeMotionEvent(dpy, -1, pos.x(), pos.y(), 0);
    XFlush(dpy);

    // X11 button numbers: 1=left, 2=middle, 3=right, 4=scroll-up, 5=scroll-down, 6=scroll-left, 7=scroll-right
    switch (type) {
    case ClickType::LeftClick:        click(1, mods); break;
    case ClickType::LeftDoubleClick:  doubleClick(1, mods); break;
    case ClickType::LeftDown:         fakeButton(1, true,  mods); break;
    case ClickType::LeftUp:           fakeButton(1, false, mods); break;
    case ClickType::RightClick:       click(3, mods); break;
    case ClickType::RightDoubleClick: doubleClick(3, mods); break;
    case ClickType::RightDown:        fakeButton(3, true,  mods); break;
    case ClickType::RightUp:          fakeButton(3, false, mods); break;
    case ClickType::MiddleClick:      click(2, mods); break;
    case ClickType::MiddleDoubleClick:doubleClick(2, mods); break;
    case ClickType::ScrollUp:         click(4); break;
    case ClickType::ScrollDown:       click(5); break;
    case ClickType::ScrollLeft:       click(6); break;
    case ClickType::ScrollRight:      click(7); break;
    default: break;
    }
}

// ─────────────────────────────────────────────────────────────
//  Fallback (unsupported platform — no-op)
// ─────────────────────────────────────────────────────────────
#else
#include <QDebug>

void ClickInjector::pressModifiers(int) {}
void ClickInjector::releaseModifiers(int) {}
void ClickInjector::moveCursor(QPoint) {}

void ClickInjector::performClick(ClickType, QPoint, int)
{
    qWarning() << "ClickInjector: unsupported platform — click not sent";
}
#endif
