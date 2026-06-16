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

QPoint ClickInjector::cursorPos()
{
    POINT p{};
    GetCursorPos(&p);
    return QPoint(p.x, p.y);
}

bool ClickInjector::hasInputDeviceAccess() { return true; }

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
#include <QCursor>

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

QPoint ClickInjector::cursorPos()
{
    return QCursor::pos();
}

bool ClickInjector::hasInputDeviceAccess() { return true; }

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
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

// ── XTest (X11 fallback) ──────────────────────────────────────
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

// ── XInput2 raw-motion (Wayland stale-position fix) ──────────
#ifdef HAVE_XI2
#include <X11/extensions/XInput2.h>
#endif

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
        for (int b : {BTN_LEFT, BTN_RIGHT, BTN_MIDDLE})
            ioctl(fd, UI_SET_KEYBIT, b);

        struct uinput_setup us{};
        us.id.bustype = BUS_USB;
        us.id.vendor  = 0x1234;
        us.id.product = 0x5678;
        std::strncpy(us.name, "TrackClick Virtual Mouse", UINPUT_MAX_NAME_SIZE - 1);

        if (ioctl(fd, UI_DEV_SETUP, &us) < 0) { destroy(); return false; }
        if (ioctl(fd, UI_DEV_CREATE)     < 0) { destroy(); return false; }
        return true;
    }

    void destroy()
    {
        if (fd >= 0) { ioctl(fd, UI_DEV_DESTROY); ::close(fd); fd = -1; }
    }

    bool isOpen() const { return fd >= 0; }

    void send(uint16_t type, uint16_t code, int32_t val) const
    {
        struct input_event ev{};
        ev.type  = type;
        ev.code  = code;
        ev.value = val;
        ::write(fd, &ev, sizeof(ev));
    }

    void syn() const { send(EV_SYN, SYN_REPORT, 0); }
};

// Separate keyboard-only device so libinput classifies it as a keyboard and
// routes KEY_LEFTCTRL/ALT/SHIFT to the compositor's modifier state.  A pointer
// device (EV_REL) is NOT classified as a keyboard by libinput even if it
// declares EV_KEY modifier keys, so modifier events from the mouse device are
// silently ignored on Wayland.
struct UInputKeyDev {
    int fd = -1;

    bool tryOpen()
    {
        fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (fd < 0) return false;

        ioctl(fd, UI_SET_EVBIT, EV_KEY);
        ioctl(fd, UI_SET_EVBIT, EV_SYN);
        for (int k : {KEY_LEFTCTRL, KEY_LEFTALT, KEY_LEFTSHIFT})
            ioctl(fd, UI_SET_KEYBIT, k);

        struct uinput_setup us{};
        us.id.bustype = BUS_USB;
        us.id.vendor  = 0x1234;
        us.id.product = 0x5679;
        std::strncpy(us.name, "TrackClick Keyboard", UINPUT_MAX_NAME_SIZE - 1);
        if (ioctl(fd, UI_DEV_SETUP, &us) < 0) { destroy(); return false; }
        if (ioctl(fd, UI_DEV_CREATE)     < 0) { destroy(); return false; }
        // No extra sleep — the mouse device's 200 ms window covers both.
        return true;
    }

    void destroy()
    {
        if (fd >= 0) { ioctl(fd, UI_DEV_DESTROY); ::close(fd); fd = -1; }
    }

    bool isOpen() const { return fd >= 0; }

    void sendKey(int code, bool down) const
    {
        struct input_event ev{};
        ev.type  = EV_KEY;
        ev.code  = static_cast<uint16_t>(code);
        ev.value = down ? 1 : 0;
        ::write(fd, &ev, sizeof(ev));
        struct input_event syn{};
        syn.type  = EV_SYN;
        syn.code  = SYN_REPORT;
        ::write(fd, &syn, sizeof(syn));
    }
};

UInputKeyDev& ukeydev()
{
    static UInputKeyDev dev;
    static bool tried = false;
    if (!tried) { tried = true; dev.tryOpen(); }
    return dev;
}

// Initialised on first use — tries uinput, falls back to XTest if denied.
// Both the mouse and keyboard devices are created before sleeping so one
// 200 ms compositor window covers both.
UInputDev& udev()
{
    static UInputDev dev;
    static bool tried = false;
    if (!tried) {
        tried = true;
        if (dev.tryOpen()) {
            ukeydev();  // create keyboard device in the same wake-up window
            usleep(200'000);
        }
    }
    return dev;
}

// ── XTest fallback helpers ────────────────────────────────────

Display* getDisplay()
{
    static Display* dpy = XOpenDisplay(nullptr);
    return dpy;
}

// ── Compositor-independent pointer motion via evdev ──────────────────────────
// XQueryPointer — and XWayland's XI2 raw motion — only report movement while the
// cursor is over an XWayland surface.  Over native Wayland surfaces (a GTK
// browser, the file manager, …) they freeze, so the DwellManager never sees the
// cursor move and the dwell countdown fails to reset.  Reading relative/absolute
// motion straight from the kernel's evdev nodes works no matter which compositor
// owns the pointer or which surface it is over.  Requires read access to
// /dev/input/event* — the same "input" group access class that grants
// /dev/uinput — and falls back gracefully (to XI2, then XQueryPointer) when the
// nodes cannot be opened.
struct EvdevMotion {
    struct Dev { int fd; int lastX; int lastY; bool haveLast; };
    std::vector<Dev> devs;
    bool opened = false;

    void openAll()
    {
        opened = true;
        int denied = 0, examined = 0;
        for (int i = 0; i < 64; ++i) {
            char path[32];
            std::snprintf(path, sizeof(path), "/dev/input/event%d", i);
            int fd = ::open(path, O_RDONLY | O_NONBLOCK);
            if (fd < 0) {
                if (errno == EACCES || errno == EPERM) ++denied;
                continue;
            }
            ++examined;

            unsigned long relBits = 0, absBits = 0;
            ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relBits)), &relBits);
            ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), &absBits);
            const bool hasRel = (relBits & (1UL << REL_X)) && (relBits & (1UL << REL_Y));
            const bool hasAbs = (absBits & (1UL << ABS_X)) && (absBits & (1UL << ABS_Y));
            if (hasRel || hasAbs)
                devs.push_back({fd, 0, 0, false});   // mouse / trackball / touchpad / tablet
            else
                ::close(fd);
        }

        // One-time diagnostic — without a readable pointer device the dwell
        // timer cannot detect motion over non-XWayland surfaces and will keep
        // counting down regardless of how the cursor moves.
        if (devs.empty()) {
            qWarning("TrackClick: evdev motion tracking DISABLED — no readable "
                     "pointer device under /dev/input/event* (%d examined, %d "
                     "permission-denied). On Wayland the dwell timer can only "
                     "track motion while the cursor is over the TrackClick "
                     "window. Grant read access (e.g. add your user to the "
                     "'input' group) to track motion everywhere.", examined, denied);
        } else {
            qInfo("TrackClick: evdev motion tracking active on %d pointer "
                  "device(s).", static_cast<int>(devs.size()));
        }
    }

    bool isOpen() const { return !devs.empty(); }

    // Accumulated motion since the previous call, in device units.  The exact
    // scale is irrelevant — the DwellManager only needs to know the cursor
    // moved.  REL devices contribute their deltas directly; ABS devices
    // contribute the change in absolute axis value.
    QPoint drain()
    {
        int dx = 0, dy = 0;
        struct input_event ev{};
        for (auto& d : devs) {
            while (::read(d.fd, &ev, sizeof(ev)) == static_cast<ssize_t>(sizeof(ev))) {
                if (ev.type == EV_REL) {
                    if      (ev.code == REL_X) dx += ev.value;
                    else if (ev.code == REL_Y) dy += ev.value;
                } else if (ev.type == EV_ABS) {
                    if (ev.code == ABS_X) {
                        if (d.haveLast) dx += ev.value - d.lastX;
                        d.lastX = ev.value;
                        d.haveLast = true;
                    } else if (ev.code == ABS_Y) {
                        if (d.haveLast) dy += ev.value - d.lastY;
                        d.lastY = ev.value;
                        d.haveLast = true;
                    }
                }
            }
        }
        return QPoint(dx, dy);
    }
};

EvdevMotion& evdev()
{
    static EvdevMotion m;
    if (!m.opened) m.openAll();
    return m;
}

// ── XInput2 raw-motion (fallback when evdev nodes are not readable) ──────────
// Provides the same root-window hardware deltas as evdev for X11 sessions and
// XWayland surfaces.  Only used when /dev/input/event* cannot be opened.
#ifdef HAVE_XI2
int  g_xi2Opcode     = -1;
bool g_xi2Subscribed = false;

void xi2Subscribe(Display* dpy)
{
    int event, error;
    if (!XQueryExtension(dpy, "XInputExtension", &g_xi2Opcode, &event, &error))
        return;
    int major = 2, minor = 0;
    if (XIQueryVersion(dpy, &major, &minor) != Success)
        return;
    unsigned char bits[XIMaskLen(XI_RawMotion)] = {};
    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask     = bits;
    mask.mask_len = sizeof(bits);
    XISetMask(bits, XI_RawMotion);
    XISelectEvents(dpy, DefaultRootWindow(dpy), &mask, 1);
    XFlush(dpy);
    g_xi2Subscribed = true;
}

// Accumulated raw motion (dx, dy) since the previous call.
QPoint xi2DrainDelta(Display* dpy)
{
    int dx = 0, dy = 0;
    if (!g_xi2Subscribed || g_xi2Opcode < 0) return QPoint(0, 0);
    while (XPending(dpy) > 0) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (ev.type != GenericEvent || ev.xcookie.extension != g_xi2Opcode) continue;
        if (!XGetEventData(dpy, &ev.xcookie)) continue;
        if (ev.xcookie.evtype == XI_RawMotion) {
            const XIRawEvent* raw = static_cast<const XIRawEvent*>(ev.xcookie.data);
            int idx = 0;
            for (int a = 0; a < raw->valuators.mask_len * 8; ++a) {
                if (XIMaskIsSet(raw->valuators.mask, a)) {
                    if (a == 0) dx += static_cast<int>(raw->raw_values[idx]);
                    if (a == 1) dy += static_cast<int>(raw->raw_values[idx]);
                    ++idx;
                }
            }
        }
        XFreeEventData(dpy, &ev.xcookie);
    }
    return QPoint(dx, dy);
}
#endif // HAVE_XI2

void xtestFakeButton(int button, bool press, int mods = 0)
{
    Display* dpy = getDisplay();
    if (!dpy) return;
    // Never send a motion event before the button event — on Wayland the
    // compositor overrides the pointer position from the motion, causing
    // snap-back.  The cursor is already at the right location.
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

QPoint ClickInjector::cursorPos()
{
    Display* dpy = getDisplay();
    if (!dpy) return QCursor::pos();

#ifdef HAVE_XI2
    if (!g_xi2Subscribed) xi2Subscribe(dpy);
#endif

    // Pull any pending motion from the compositor-independent source (evdev),
    // falling back to XInput2 raw motion when the evdev nodes are not readable.
    // estPos tracks the cursor across native Wayland surfaces where
    // XQueryPointer freezes; it is re-anchored to XQueryPointer whenever that
    // reading advances (cursor over an XWayland surface, or any X11 session).
    static bool   haveEst   = false;
    static QPoint estPos;
    static QPoint lastXQuery;
    static bool   gotXQuery = false;

    bool   haveMotionSrc = false;
    QPoint delta(0, 0);
    if (evdev().isOpen()) {
        delta = evdev().drain();
        haveMotionSrc = true;
    }
#ifdef HAVE_XI2
    else {
        delta = xi2DrainDelta(dpy);
        haveMotionSrc = true;
    }
#endif
    if (haveEst && haveMotionSrc)
        estPos += delta;

    Window root = DefaultRootWindow(dpy);
    Window root_ret, child_ret;
    int rx, ry, wx, wy;
    unsigned int mask;
    if (!XQueryPointer(dpy, root, &root_ret, &child_ret, &rx, &ry, &wx, &wy, &mask))
        return haveEst ? estPos : QCursor::pos();

    const QPoint absPos(rx, ry);
    const bool   fresh = !gotXQuery || (absPos != lastXQuery);
    lastXQuery = absPos;
    gotXQuery  = true;

    // Without a kernel-level motion source we can only trust XQueryPointer.  On
    // X11 that is always accurate; on Wayland it is accurate while the cursor is
    // over an XWayland surface — the best obtainable without evdev/XI2.
    if (!haveMotionSrc)
        return absPos;

    if (fresh) {
        // XQueryPointer advanced — authoritative reading (cursor over an
        // XWayland surface, or any surface on X11).  Re-anchor, clearing drift.
        estPos  = absPos;
        haveEst = true;
        return absPos;
    }

    // XQueryPointer is frozen — cursor is over a native Wayland surface.  Use the
    // motion-tracked estimate so the DwellManager still sees movement and resets.
    if (!haveEst) { estPos = absPos; haveEst = true; }
    return estPos;
}

bool ClickInjector::hasInputDeviceAccess() { return evdev().isOpen(); }

void ClickInjector::moveCursor(QPoint pos)
{
    if (udev().isOpen()) {
        QPoint cur = ClickInjector::cursorPos();
        int dx = pos.x() - cur.x();
        int dy = pos.y() - cur.y();
        if (dx || dy) {
            udev().send(EV_REL, REL_X, dx);
            udev().send(EV_REL, REL_Y, dy);
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
    // Do not move the cursor before clicking. For a dwell clicker the cursor
    // is already at the target; moving it causes snap-back on Wayland because
    // QCursor::pos() is unreliable there and the computed delta is wrong.
    Q_UNUSED(pos)

    if (udev().isOpen()) {
        auto& d = udev();

        // Route modifier keys through the separate keyboard device so libinput
        // classifies them correctly on Wayland.  Each modifier gets its own
        // SYN_REPORT so the compositor sees it before the mouse button event.
        auto key = [&](int code, bool down) {
            if (ukeydev().isOpen()) {
                ukeydev().sendKey(code, down);
            } else {
                d.send(EV_KEY, static_cast<uint16_t>(code), down ? 1 : 0);
            }
        };
        auto btn = [&](int code, bool down) {
            if (down && (mods & ModCtrl))  key(KEY_LEFTCTRL,  true);
            if (down && (mods & ModAlt))   key(KEY_LEFTALT,   true);
            if (down && (mods & ModShift)) key(KEY_LEFTSHIFT, true);
            d.send(EV_KEY, static_cast<uint16_t>(code), down ? 1 : 0);
            d.syn();
            if (!down && (mods & ModShift)) key(KEY_LEFTSHIFT, false);
            if (!down && (mods & ModAlt))   key(KEY_LEFTALT,   false);
            if (!down && (mods & ModCtrl))  key(KEY_LEFTCTRL,  false);
            if (!down && !ukeydev().isOpen()) d.syn();
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
            d.send(EV_REL, REL_WHEEL,   1); d.syn(); break;
        case ClickType::ScrollDown:
            d.send(EV_REL, REL_WHEEL,  -1); d.syn(); break;
        case ClickType::ScrollLeft:
            d.send(EV_REL, REL_HWHEEL, -1); d.syn(); break;
        case ClickType::ScrollRight:
            d.send(EV_REL, REL_HWHEEL,  1); d.syn(); break;
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
QPoint ClickInjector::cursorPos() { return QCursor::pos(); }
bool ClickInjector::hasInputDeviceAccess() { return true; }

void ClickInjector::performClick(ClickType, QPoint, int)
{
    qWarning() << "ClickInjector: unsupported platform — click not sent";
}
#endif
