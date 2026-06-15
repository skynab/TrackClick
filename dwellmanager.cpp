#include "dwellmanager.h"
#include "clickinjector.h"
#include <QCursor>
#include <cmath>

DwellManager::DwellManager(QObject* parent)
    : QObject(parent)
{
    m_pollTimer.setInterval(50);   // 20 Hz
    connect(&m_pollTimer, &QTimer::timeout, this, &DwellManager::onPoll);
    m_pollTimer.start();
}

void DwellManager::arm(ClickType type, int modifiers)
{
    // If a drag Down was fired but not yet released, release it now before
    // switching to a different action — otherwise the button stays held.
    if (m_dragActive) {
        m_clickFn(m_clickType, m_cursorPosFn(), m_modifiers);
        m_dragActive = false;
    }

    m_clickType    = type;
    m_modifiers    = modifiers;
    m_armed        = true;
    m_waiting      = false;
    m_hovering     = false;
    m_dragActive   = false;
    m_anchorPos    = m_cursorPosFn();
    m_lastPos      = m_anchorPos;
    m_hoverStartMs = m_nowFn();
}

void DwellManager::disarm()
{
    // If a drag Down was fired but not released, release it to avoid stuck buttons.
    if (m_dragActive) {
        m_clickFn(m_clickType, m_cursorPosFn(), m_modifiers);
        m_dragActive = false;
    }
    m_armed    = false;
    m_waiting  = false;
    m_hovering = false;
    emit dwellProgress(0.0f);
}

void DwellManager::onPoll()
{
    if (!m_armed) return;

    QPoint cur = m_cursorPosFn();

    auto dist = [](QPoint a, QPoint b) -> double {
        double dx = a.x() - b.x();
        double dy = a.y() - b.y();
        return std::sqrt(dx*dx + dy*dy);
    };

    if (m_waiting) {
        // Exit waiting when the cursor moves far enough away from the fire point,
        // OR after one full dwell period — whichever comes first.  The timeout
        // handles Wayland compositor pointer grabs (e.g. right-click context menus)
        // that freeze XQueryPointer so movement is never detected.
        qint64 waitedMs  = m_nowFn() - m_waitStartMs;
        bool   movedAway = dist(cur, m_anchorPos) > m_sensitivityPx * 2;
        bool   timedOut  = waitedMs >= m_dwellMs;

        if (movedAway || timedOut) {
            m_waiting      = false;
            m_hovering     = false;
            m_anchorPos    = cur;
            m_lastPos      = cur;
            m_hoverStartMs = m_nowFn();

            if (timedOut && !movedAway) {
                // Cursor position appeared frozen throughout the entire wait —
                // most likely the pointer is outside the XWayland session on
                // Wayland, so XQueryPointer returns stale data and movement
                // cannot be detected.  Disarm to prevent repeated unintended
                // clicks; the user re-arms by hovering over a button again.
                m_armed = false;
                emit dwellProgress(0.0f);
            }
        }
        return;
    }

    // Safety release: if a drag button has been held for more than 10 seconds
    // (e.g. XQueryPointer glitches keep resetting the hover countdown), force
    // the Up event so the user is never permanently locked out.
    if (m_dragActive) {
        qint64 heldMs = m_nowFn() - m_dragStartMs;
        if (heldMs >= 10000) {
            m_clickFn(m_clickType, cur, m_modifiers);
            m_clickType   = (m_clickType == ClickType::LeftUp) ? ClickType::LeftDown : ClickType::RightDown;
            m_dragActive  = false;
            m_waiting     = true;
            m_hovering    = false;
            m_waitStartMs = m_nowFn();
            emit dwellProgress(0.0f);
            return;
        }
    }

    if (dist(cur, m_anchorPos) > m_sensitivityPx) {
        // Cursor moved — reset hover countdown
        m_anchorPos    = cur;
        m_lastPos      = cur;
        m_hovering     = false;
        m_hoverStartMs = m_nowFn();
        emit dwellProgress(0.0f);
        return;
    }

    // Cursor is still within sensitivity radius
    if (!m_hovering) {
        m_hovering     = true;
        m_hoverStartMs = m_nowFn();
    }

    qint64 elapsed = m_nowFn() - m_hoverStartMs;
    float  frac    = static_cast<float>(elapsed) / static_cast<float>(m_dwellMs);
    frac = qBound(0.0f, frac, 1.0f);
    emit dwellProgress(frac);

    if (elapsed >= m_dwellMs) {
        emit dwellAboutToFire(cur, m_clickType);
        bool isScroll = (m_clickType == ClickType::ScrollUp   ||
                         m_clickType == ClickType::ScrollDown  ||
                         m_clickType == ClickType::ScrollLeft  ||
                         m_clickType == ClickType::ScrollRight);
        int reps = isScroll ? m_scrollRepeat : 1;
        for (int i = 0; i < reps; ++i)
            m_clickFn(m_clickType, cur, m_modifiers);
        emit dwellFired(cur, m_clickType);

        // For drag (Down) events: skip the movement-wait entirely and go straight
        // into the next hover countdown for the paired Up event.  This is critical
        // on Linux where the OS button grab can freeze the cursor position, making
        // movement-based re-arm impossible.  The user drags to a destination and
        // dwells to release; if the cursor IS frozen the countdown still fires Up.
        if (m_clickType == ClickType::LeftDown) {
            m_clickType   = ClickType::LeftUp;
            m_dragActive  = true;
            m_dragStartMs = m_nowFn();
            m_anchorPos   = cur;
            m_lastPos     = cur;
            m_hovering    = false;
            emit dwellProgress(0.0f);
            return;
        }
        if (m_clickType == ClickType::RightDown) {
            m_clickType   = ClickType::RightUp;
            m_dragActive  = true;
            m_dragStartMs = m_nowFn();
            m_anchorPos   = cur;
            m_lastPos     = cur;
            m_hovering    = false;
            emit dwellProgress(0.0f);
            return;
        }

        // For Up events (completing a drag), restore the original Down type and
        // use normal waiting so an accidental re-trigger is prevented.
        if (m_clickType == ClickType::LeftUp && m_dragActive) {
            m_clickType  = ClickType::LeftDown;
            m_dragActive = false;
        } else if (m_clickType == ClickType::RightUp && m_dragActive) {
            m_clickType  = ClickType::RightDown;
            m_dragActive = false;
        }

        // Normal post-fire: wait for cursor to move before allowing next dwell.
        m_anchorPos   = cur;
        m_waiting     = true;
        m_hovering    = false;
        m_waitStartMs = m_nowFn();
        emit dwellProgress(0.0f);
    }
}
