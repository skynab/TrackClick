#include "macos_utils.h"
#ifdef Q_OS_MAC

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

void applyMacOSWindowBehavior(quintptr winId)
{
    // winId() on macOS is an NSView*
    NSView*   view   = reinterpret_cast<NSView*>(winId);
    NSWindow* window = [view window];
    if (!window) return;

    // Stationary  — stays visible and doesn't zoom out in Mission Control / Exposé
    // CanJoinAllSpaces — appears on every virtual desktop / Space
    window.collectionBehavior =
        NSWindowCollectionBehaviorStationary |
        NSWindowCollectionBehaviorCanJoinAllSpaces;
}

bool macAccessibilityTrusted(bool promptIfNeeded)
{
    if (!promptIfNeeded)
        return AXIsProcessTrusted();

    // Passing kAXTrustedCheckOptionPrompt registers TrackClick in the
    // Accessibility list and, if not yet trusted, shows the system prompt.
    const void* keys[]   = { kAXTrustedCheckOptionPrompt };
    const void* values[] = { kCFBooleanTrue };
    CFDictionaryRef opts = CFDictionaryCreate(
        kCFAllocatorDefault, keys, values, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const bool trusted = AXIsProcessTrustedWithOptions(opts);
    CFRelease(opts);
    return trusted;
}

#endif // Q_OS_MAC
