#include "dwellmanager.h"
#include "clickinjector.h"
#include <QCursor>
#include <QDateTime>
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
        ClickInjector::performClick(m_clickType, ClickInjector::cursorPos(), m_modifiers);
        m_dragActive = false;
    }

    m_clickType  = type;
    m_modifiers  = modifiers;
    m_armed      = true;
    m_waiting    = false;
    m_hovering   = false;
    m_dragActive = false;
    m_anchorPos  = ClickInjector::cursorPos();
    m_lastPos    = m_anchorPos;
    m_hoverStartMs = QDateTime::currentMSecsSinceEpoch();
}

void DwellManager::disarm()
{
    // If a drag Down was fired but not released, release it to avoid stuck buttons.
    if (m_dragActive) {
        ClickInjector::performClick(m_clickType, ClickInjector::cursorPos(), m_modifiers);
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

    QPoint cur = ClickInjector::cursorPos();

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
        qint64 waitedMs = QDateTime::currentMSecsSinceEpoch() - m_waitStartMs;
        if (dist(cur, m_anchorPos) > m_sensitivityPx * 2 || waitedMs >= m_dwellMs) {
            m_waiting      = false;
            m_hovering     = false;
            m_anchorPos    = cur;
            m_lastPos      = cur;
            m_hoverStartMs = QDateTime::currentMSecsSinceEpoch();
        }
        return;
    }

    if (dist(cur, m_anchorPos) > m_sensitivityPx) {
        // Cursor moved — reset hover countdown
        m_anchorPos    = cur;
        m_lastPos      = cur;
        m_hovering     = false;
        m_hoverStartMs = QDateTime::currentMSecsSinceEpoch();
        emit dwellProgress(0.0f);
        return;
    }

    // Cursor is still within sensitivity radius
    if (!m_hovering) {
        m_hovering     = true;
        m_hoverStartMs = QDateTime::currentMSecsSinceEpoch();
    }

    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_hoverStartMs;
    float  frac    = static_cast<float>(elapsed) / static_cast<float>(m_dwellMs);
    frac = qBound(0.0f, frac, 1.0f);
    emit dwellProgress(frac);

    if (elapsed >= m_dwellMs) {
        emit dwellAboutToFire(cur, m_clickType);
        ClickInjector::performClick(m_clickType, cur, m_modifiers);
        emit dwellFired(cur, m_clickType);

        // For drag actions (Down/Up toggle): alternate between Down and Up so the
        // second dwell releases rather than pressing again (which locks up Linux).
        if (m_clickType == ClickType::LeftDown) {
            m_clickType  = ClickType::LeftUp;
            m_dragActive = true;
        } else if (m_clickType == ClickType::LeftUp && m_dragActive) {
            m_clickType  = ClickType::LeftDown;
            m_dragActive = false;
        } else if (m_clickType == ClickType::RightDown) {
            m_clickType  = ClickType::RightUp;
            m_dragActive = true;
        } else if (m_clickType == ClickType::RightUp && m_dragActive) {
            m_clickType  = ClickType::RightDown;
            m_dragActive = false;
        }

        // After firing, wait for cursor to move away before allowing next dwell
        m_anchorPos   = cur;
        m_waiting     = true;
        m_hovering    = false;
        m_waitStartMs = QDateTime::currentMSecsSinceEpoch();
        emit dwellProgress(0.0f);
    }
}
