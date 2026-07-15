#pragma once
#include <QtGlobal>

#ifdef Q_OS_MAC
// Sets NSWindowCollectionBehavior so the window stays visible during
// Mission Control / Exposé and follows the user across all Spaces.
// Safe to call on any platform (no-op outside macOS via the ifdef).
void applyMacOSWindowBehavior(quintptr winId);

// Returns whether this process is trusted for Accessibility — required to post
// synthetic mouse events with CGEventPost. When promptIfNeeded is true and the
// process is not yet trusted, macOS adds TrackClick to the Accessibility list
// and shows its system permission prompt.
bool macAccessibilityTrusted(bool promptIfNeeded);
#endif
