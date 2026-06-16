#pragma once
#include <QPoint>

// All click types supported by TrackClick
enum class ClickType {
    None,
    NoClick,         // selected but performs no action
    LeftClick,
    LeftDoubleClick,
    LeftDown,       // begin drag
    LeftUp,         // end drag
    RightClick,
    RightDoubleClick,
    RightDown,
    RightUp,
    MiddleClick,
    MiddleDoubleClick,
    ScrollUp,
    ScrollDown,
    ScrollLeft,
    ScrollRight,
};

// Modifier flags (OR-able)
enum ModifierFlag {
    ModNone  = 0,
    ModCtrl  = 1 << 0,
    ModAlt   = 1 << 1,
    ModShift = 1 << 2,
};

class ClickInjector
{
public:
    // Perform a click at the given screen position with optional modifiers.
    // Modifiers is a bitmask of ModifierFlag values.
    static void performClick(ClickType type, QPoint pos, int modifiers = ModNone);

    // Move the system cursor to pos without clicking.
    static void moveCursor(QPoint pos);

    // Returns the real global cursor position.
    // On Linux this uses XQueryPointer so it stays accurate on Wayland even
    // when the cursor is outside the application window (QCursor::pos() goes
    // stale as soon as the pointer leaves the window on Wayland).
    static QPoint cursorPos();

    // Whether the injector can read kernel pointer-motion devices.  On Linux
    // this reflects whether any /dev/input/event* node could be opened — which
    // is required for the dwell timer to track the cursor over windows other
    // than TrackClick's own on Wayland.  Always true on other platforms.
    static bool hasInputDeviceAccess();

private:
    static void pressModifiers(int modifiers);
    static void releaseModifiers(int modifiers);
};
