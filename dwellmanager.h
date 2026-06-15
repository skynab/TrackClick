#pragma once
#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QPoint>
#include <functional>
#include "clickinjector.h"

// DwellManager polls the cursor position and fires a click after the cursor
// has been stationary for `dwellMs` milliseconds within `sensitivityPx` pixels.
// After firing it waits for the cursor to move before arming again.
class DwellManager : public QObject
{
    Q_OBJECT
public:
    explicit DwellManager(QObject* parent = nullptr);

    // Arms the dwell mechanism with the given click type + modifiers.
    // Calling again with a new type replaces the pending type.
    void arm(ClickType type, int modifiers = ModNone);

    // Disarms (cancels) pending dwell.
    void disarm();

    bool isArmed() const { return m_armed; }

    // Settings
    void setDwellMs(int ms)          { m_dwellMs = ms; }
    void setSensitivityPx(int px)    { m_sensitivityPx = px; }
    void setScrollRepeat(int n)      { m_scrollRepeat = n; }
    void setPollIntervalMs(int ms)   { m_pollTimer.setInterval(ms); }
    // Update modifier mask without resetting the current dwell countdown.
    void setModifiers(int mods)      { m_modifiers = mods; }

    int  dwellMs()       const { return m_dwellMs; }
    int  sensitivityPx() const { return m_sensitivityPx; }

signals:
    // Emitted just before the click fires (can show visual feedback).
    void dwellAboutToFire(QPoint pos, ClickType type);
    // Emitted after the click has been injected.
    void dwellFired(QPoint pos, ClickType type);
    // Progress 0.0–1.0 while hovering.
    void dwellProgress(float fraction);

private slots:
    void onPoll();

private:
    QTimer  m_pollTimer;
    QTimer  m_dwellTimer;

    bool      m_armed        = false;
    bool      m_waiting      = false; // waiting for cursor to move before re-arming
    bool      m_hovering     = false; // cursor has been still long enough to start countdown
    bool      m_dragActive   = false; // LeftDown/RightDown was fired; next fire should be the Up

    ClickType m_clickType    = ClickType::LeftClick;
    int       m_modifiers    = ModNone;

    QPoint    m_anchorPos;
    QPoint    m_lastPos;

    int       m_dwellMs        = 1000;
    int       m_sensitivityPx  = 5;
    int       m_scrollRepeat   = 3;

    qint64    m_hoverStartMs   = 0;
    qint64    m_waitStartMs    = 0; // when m_waiting began (for timeout fallback)
    qint64    m_dragStartMs    = 0; // when LeftDown/RightDown fired (for safety release)

    // Injectable seams — default to real implementations; tests override these
    // to drive the state machine without a display or platform input layer.
    std::function<QPoint()>                    m_cursorPosFn = &ClickInjector::cursorPos;
    std::function<void(ClickType, QPoint, int)> m_clickFn    = &ClickInjector::performClick;
    std::function<qint64()>                    m_nowFn       = []{ return QDateTime::currentMSecsSinceEpoch(); };

#ifdef TRACKCLICK_TESTING
public:
    void setTestCursorPosFn(std::function<QPoint()> fn)                    { m_cursorPosFn = std::move(fn); }
    void setTestClickFn(std::function<void(ClickType, QPoint, int)> fn)    { m_clickFn     = std::move(fn); }
    void setTestNowFn(std::function<qint64()> fn)                          { m_nowFn       = std::move(fn); }
    void triggerPoll()                                                      { onPoll(); }
#endif
};
