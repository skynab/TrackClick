#pragma once
#include <QPoint>

// All click types supported by TrackClick
enum class ClickType {
    None,
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

private:
    static void pressModifiers(int modifiers);
    static void releaseModifiers(int modifiers);
};
