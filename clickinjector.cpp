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
//  Linux — uinput (Wayland + X11) with XTest fallback (X11 only)
// ─────────────────────────────────────────────────────────────
#elif defined(PLATFORM_LINUX)

// Qt headers must come before X11 headers (X11 defines Bool/Status macros)
#include <QCursor>
#include <QGuiApplication>

// ── uinput ────────────────────────────────────────────────────
#include <linux/input.h>
#include <linux/uinput.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

// ── XTest (X11 fallback) ──────────────────────────────────────
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

// ── uinput virtual pointer device ────────────────────────────
namespace {

struct UInputDev {
    int fd = -1;

    bool tryOpen()
    {
        fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (fd < 0) return false;

        ioctl(fd, UI_SET_EVBIT, EV_KEY);
        ioctl(fd, UI_SET_EVBIT, EV_REL);
        ioctl(fd, UI_SET_EVBIT, EV_SYN);

        for (int b : {REL_X, REL_Y, REL_WHEEL, REL_HWHEEL})
            ioctl(fd, UI_SET_RELBIT, b);
        for (int b : {BTN_LEFT, BTN_RIGHT, BTN_MIDDLE,
                      KEY_LEFTCTRL, KEY_LEFTALT, KEY_LEFTSHIFT})
            ioctl(fd, UI_SET_KEYBIT, b);

        struct uinput_setup us{};
        us.id.bustype = BUS_USB;
        us.id.vendor  = 0x1234;
        us.id.product = 0x5678;
        std::strncpy(us.name, "TrackClick Virtual Mouse", UINPUT_MAX_NAME_SIZE - 1);

        if (ioctl(fd, UI_DEV_SETUP, &us) < 0) { destroy(); return false; }
        if (ioctl(fd, UI_DEV_CREATE)     < 0) { destroy(); return false; }

        // Give the compositor time to recognise the new device.
        usleep(200'000);
        return true;
    }

    void destroy()
    {
        if (fd >= 0) { ioctl(fd, UI_DEV_DESTROY); ::close(fd); fd = -1; }
    }

    bool isOpen() const { return fd >= 0; }

    void emit(uint16_t type, uint16_t code, int32_t val) const
    {
        struct input_event ev{};
        ev.type  = type;
        ev.code  = code;
        ev.value = val;
        ::write(fd, &ev, sizeof(ev));
    }

    void syn() const { emit(EV_SYN, SYN_REPORT, 0); }
};

// Initialised on first use — tries uinput, falls back to XTest if denied.
UInputDev& udev()
{
    static UInputDev dev;
    static bool tried = false;
    if (!tried) { tried = true; dev.tryOpen(); }
    return dev;
}

// ── XTest fallback helpers ────────────────────────────────────

Display* getDisplay()
{
    static Display* dpy = XOpenDisplay(nullptr);
    return dpy;
}

void xtestFakeButton(int button, bool press, int mods = 0)
{
    Display* dpy = getDisplay();
    if (!dpy) return;
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

void xtestClick(int button, int mods = 0)
{
    xtestFakeButton(button, true,  mods);
    xtestFakeButton(button, false, mods);
}

} // namespace

// ── ClickInjector ─────────────────────────────────────────────

void ClickInjector::pressModifiers(int) {}
void ClickInjector::releaseModifiers(int) {}

void ClickInjector::moveCursor(QPoint pos)
{
    if (udev().isOpen()) {
        QPoint cur = QCursor::pos();
        int dx = pos.x() - cur.x();
        int dy = pos.y() - cur.y();
        if (dx || dy) {
            udev().emit(EV_REL, REL_X, dx);
            udev().emit(EV_REL, REL_Y, dy);
            udev().syn();
        }
    } else {
        Display* dpy = getDisplay();
        if (!dpy) return;
        XTestFakeMotionEvent(dpy, -1, pos.x(), pos.y(), 0);
        XFlush(dpy);
    }
}

void ClickInjector::performClick(ClickType type, QPoint pos, int mods)
{
    // Move to target position first.
    moveCursor(pos);

    if (udev().isOpen()) {
        auto& d = udev();

        auto key = [&](int code, bool down) {
            d.emit(EV_KEY, static_cast<uint16_t>(code), down ? 1 : 0);
        };
        auto btn = [&](int code, bool down) {
            if (down && (mods & ModCtrl))  key(KEY_LEFTCTRL,  true);
            if (down && (mods & ModAlt))   key(KEY_LEFTALT,   true);
            if (down && (mods & ModShift)) key(KEY_LEFTSHIFT, true);
            d.emit(EV_KEY, static_cast<uint16_t>(code), down ? 1 : 0);
            d.syn();
            if (!down && (mods & ModShift)) key(KEY_LEFTSHIFT, false);
            if (!down && (mods & ModAlt))   key(KEY_LEFTALT,   false);
            if (!down && (mods & ModCtrl))  key(KEY_LEFTCTRL,  false);
            if (!down) d.syn();
        };
        auto click = [&](int code) { btn(code, true); btn(code, false); };
        auto dbl   = [&](int code) { click(code); click(code); };

        switch (type) {
        case ClickType::LeftClick:         click(BTN_LEFT);   break;
        case ClickType::LeftDoubleClick:   dbl(BTN_LEFT);     break;
        case ClickType::LeftDown:          btn(BTN_LEFT,  true);  break;
        case ClickType::LeftUp:            btn(BTN_LEFT,  false); break;
        case ClickType::RightClick:        click(BTN_RIGHT);  break;
        case ClickType::RightDoubleClick:  dbl(BTN_RIGHT);    break;
        case ClickType::RightDown:         btn(BTN_RIGHT, true);  break;
        case ClickType::RightUp:           btn(BTN_RIGHT, false); break;
        case ClickType::MiddleClick:       click(BTN_MIDDLE); break;
        case ClickType::MiddleDoubleClick: dbl(BTN_MIDDLE);   break;
        case ClickType::ScrollUp:
            d.emit(EV_REL, REL_WHEEL,   1); d.syn(); break;
        case ClickType::ScrollDown:
            d.emit(EV_REL, REL_WHEEL,  -1); d.syn(); break;
        case ClickType::ScrollLeft:
            d.emit(EV_REL, REL_HWHEEL, -1); d.syn(); break;
        case ClickType::ScrollRight:
            d.emit(EV_REL, REL_HWHEEL,  1); d.syn(); break;
        default: break;
        }
    } else {
        // XTest fallback (X11 sessions without uinput access)
        // X11 button map: 1=left 2=middle 3=right 4=scroll↑ 5=scroll↓ 6=scroll← 7=scroll→
        auto click = [&](int b) { xtestClick(b, mods); };
        auto dbl   = [&](int b) { xtestClick(b, mods); xtestClick(b, mods); };

        switch (type) {
        case ClickType::LeftClick:         click(1); break;
        case ClickType::LeftDoubleClick:   dbl(1);   break;
        case ClickType::LeftDown:          xtestFakeButton(1, true,  mods); break;
        case ClickType::LeftUp:            xtestFakeButton(1, false, mods); break;
        case ClickType::RightClick:        click(3); break;
        case ClickType::RightDoubleClick:  dbl(3);   break;
        case ClickType::RightDown:         xtestFakeButton(3, true,  mods); break;
        case ClickType::RightUp:           xtestFakeButton(3, false, mods); break;
        case ClickType::MiddleClick:       click(2); break;
        case ClickType::MiddleDoubleClick: dbl(2);   break;
        case ClickType::ScrollUp:          click(4); break;
        case ClickType::ScrollDown:        click(5); break;
        case ClickType::ScrollLeft:        click(6); break;
        case ClickType::ScrollRight:       click(7); break;
        default: break;
        }
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
