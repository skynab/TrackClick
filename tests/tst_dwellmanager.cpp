#include <QtTest>
#include "dwellmanager.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

struct ClickRecord {
    ClickType type;
    QPoint    pos;
    int       mods;
};

class DwellManagerTest : public QObject
{
    Q_OBJECT

    // Per-test mutable state — reset in init()
    QPoint                  m_cursor{100, 100};
    qint64                  m_now{0};
    QList<ClickRecord>      m_clicks;

    // Build a DwellManager wired to the fake cursor / time / click functions.
    std::unique_ptr<DwellManager> make(int dwellMs = 500, int sensitivityPx = 5)
    {
        auto dm = std::make_unique<DwellManager>();
        dm->setDwellMs(dwellMs);
        dm->setSensitivityPx(sensitivityPx);
        dm->setTestCursorPosFn([this]{ return m_cursor; });
        dm->setTestNowFn([this]{ return m_now; });
        dm->setTestClickFn([this](ClickType t, QPoint p, int mods){
            m_clicks.append({t, p, mods});
        });
        return dm;
    }

    // Advance virtual time by dt ms and trigger one poll.
    void poll(DwellManager* dm, qint64 dt = 0)
    {
        m_now += dt;
        dm->triggerPoll();
    }

private slots:
    void init()
    {
        m_cursor = {100, 100};
        m_now    = 0;
        m_clicks.clear();
    }

    // -----------------------------------------------------------------------
    // 1. Basic dwell: click fires after the cursor holds still for dwellMs.
    // -----------------------------------------------------------------------
    void test_basicDwellFires()
    {
        auto dm = make(/*dwellMs=*/500);
        dm->arm(ClickType::LeftClick);

        poll(dm.get());        // t=0   — hover starts
        poll(dm.get(), 499);   // t=499 — elapsed=499, no fire yet
        QCOMPARE(m_clicks.size(), 0);

        poll(dm.get(), 1);     // t=500 — elapsed=500, fires
        QCOMPARE(m_clicks.size(), 1);
        QCOMPARE(m_clicks[0].type, ClickType::LeftClick);
        QCOMPARE(m_clicks[0].pos,  QPoint(100, 100));
    }

    // -----------------------------------------------------------------------
    // 2. Large cursor movement resets the hover countdown.
    // -----------------------------------------------------------------------
    void test_movementResetsCountdown()
    {
        auto dm = make(500, /*sensitivityPx=*/5);
        dm->arm(ClickType::LeftClick);

        poll(dm.get());        // t=0, hover starts
        poll(dm.get(), 400);   // t=400, elapsed=400

        m_cursor = {150, 150}; // move well outside sensitivity
        poll(dm.get(), 10);    // t=410, movement detected — hovering=false, hoverStart=410
        poll(dm.get());        // t=410, !hovering → hover starts at t=410, elapsed=0

        poll(dm.get(), 499);   // t=909, elapsed=499, no fire yet
        QCOMPARE(m_clicks.size(), 0);

        poll(dm.get(), 1);     // t=910, elapsed=500, fires at new position
        QCOMPARE(m_clicks.size(), 1);
        QCOMPARE(m_clicks[0].pos, QPoint(150, 150));
    }

    // -----------------------------------------------------------------------
    // 3. Sub-sensitivity jitter does NOT reset the countdown.
    // -----------------------------------------------------------------------
    void test_smallJitterDoesNotReset()
    {
        auto dm = make(500, /*sensitivityPx=*/10);
        dm->arm(ClickType::LeftClick);

        poll(dm.get());        // t=0
        poll(dm.get(), 300);   // t=300

        m_cursor = {104, 103}; // 5px move — within 10px sensitivity
        poll(dm.get(), 200);   // t=500 — still fires (countdown not reset)
        QCOMPARE(m_clicks.size(), 1);
    }

    // -----------------------------------------------------------------------
    // 4. After a click fires, no second click occurs while the cursor stays
    //    still — the manager enters a waiting phase that blocks re-fire until
    //    the cursor moves, in both one-shot and repeat modes.
    // -----------------------------------------------------------------------
    void test_noReFire_CursorStill()
    {
        auto dm = make(500);
        dm->arm(ClickType::LeftClick);

        poll(dm.get());
        poll(dm.get(), 500);  // first click fires
        QCOMPARE(m_clicks.size(), 1);

        // Cursor hasn't moved; poll repeatedly — should not fire again
        poll(dm.get(), 100);
        poll(dm.get(), 100);
        QCOMPARE(m_clicks.size(), 1);
    }

    // -----------------------------------------------------------------------
    // 5. In one-shot mode (default), the manager stays armed in a waiting
    //    phase after firing rather than disarming.  Moving the cursor (e.g.
    //    back over the app) resumes the dwell timer so a second click can fire
    //    without the user re-selecting a toolbar button.
    // -----------------------------------------------------------------------
    void test_oneShotResumesAfterMovement()
    {
        auto dm = make(500);  // repeatOnDwell defaults to false (one-shot)
        dm->arm(ClickType::LeftClick);

        poll(dm.get());
        poll(dm.get(), 500);  // first click fires; manager enters waiting
        QCOMPARE(m_clicks.size(), 1);
        QVERIFY(dm->isArmed());   // stays armed (waiting), not immediately disarmed

        // Move the cursor far enough to leave the waiting phase and re-arm.
        m_cursor = {200, 200};
        poll(dm.get(), 10);   // movement detected — waiting ends, countdown restarts

        poll(dm.get());       // hover starts at the new position
        poll(dm.get(), 500);  // dwell completes — second click fires
        QCOMPARE(m_clicks.size(), 2);
        QCOMPARE(m_clicks[1].pos, QPoint(200, 200));
    }

    // -----------------------------------------------------------------------
    // 5b. In one-shot mode, if the cursor never moves after firing, the
    //     manager disarms once a full dwell period elapses (the Wayland
    //     frozen-cursor safeguard) so it cannot fire again on its own.
    // -----------------------------------------------------------------------
    void test_oneShotDisarmsWhenCursorStaysStill()
    {
        auto dm = make(500);  // one-shot
        dm->arm(ClickType::LeftClick);

        poll(dm.get());
        poll(dm.get(), 500);  // first click fires; enters waiting
        QCOMPARE(m_clicks.size(), 1);

        // Cursor never moves; after a full dwell period the waiting phase
        // times out and disarms (no movement could be detected).
        poll(dm.get(), 500);  // waitedMs >= dwellMs, !movedAway → disarm
        QVERIFY(!dm->isArmed());

        // No second click fires with further polling.
        poll(dm.get());
        poll(dm.get(), 500);
        QCOMPARE(m_clicks.size(), 1);
    }

    // -----------------------------------------------------------------------
    // 6. LeftDown → second dwell fires LeftUp (not LeftDown again).
    //    This was the root cause of the "stuck mouse" bug on Linux.
    // -----------------------------------------------------------------------
    void test_leftDragToggle_DownThenUp()
    {
        auto dm = make(500);
        dm->arm(ClickType::LeftDown);

        poll(dm.get());
        poll(dm.get(), 500);  // LeftDown fires
        QCOMPARE(m_clicks.size(), 1);
        QCOMPARE(m_clicks[0].type, ClickType::LeftDown);

        poll(dm.get());
        poll(dm.get(), 500);  // LeftUp fires
        QCOMPARE(m_clicks.size(), 2);
        QCOMPARE(m_clicks[1].type, ClickType::LeftUp);
    }

    // -----------------------------------------------------------------------
    // 7. After LeftDown → LeftUp, the next dwell fires LeftDown again
    //    (repeat mode — drag type is preserved for continuous use).
    // -----------------------------------------------------------------------
    void test_leftDragToggle_RestoredForNextDrag()
    {
        auto dm = make(500);
        dm->setRepeatOnDwell(true);  // repeat mode: drag can fire multiple times
        dm->arm(ClickType::LeftDown);

        poll(dm.get());
        poll(dm.get(), 500);  // LeftDown
        poll(dm.get());
        poll(dm.get(), 500);  // LeftUp  (enters waiting)

        // Move cursor to escape waiting
        m_cursor = {200, 200};
        poll(dm.get(), 10);

        poll(dm.get());
        poll(dm.get(), 500);  // should fire LeftDown again
        QCOMPARE(m_clicks.size(), 3);
        QCOMPARE(m_clicks[2].type, ClickType::LeftDown);
    }

    // -----------------------------------------------------------------------
    // 8. Moving the cursor during a drag resets the LeftUp countdown so the
    //    release fires at the destination, not the source.
    // -----------------------------------------------------------------------
    void test_dragMoveResetsUpCountdown()
    {
        auto dm = make(500);
        dm->arm(ClickType::LeftDown);

        poll(dm.get());
        poll(dm.get(), 500);  // LeftDown at {100,100}; hovering=false

        // Drag cursor to destination
        m_cursor = {300, 300};
        poll(dm.get(), 10);   // t=510, movement detected — hovering=false, hoverStart=510
        poll(dm.get());       // t=510, !hovering → hover starts at t=510, elapsed=0

        // Partial wait — LeftUp should not fire yet
        poll(dm.get(), 400);  // t=910, elapsed=400
        QCOMPARE(m_clicks.size(), 1);

        poll(dm.get(), 100);  // t=1010, elapsed=500, LeftUp fires at destination
        QCOMPARE(m_clicks.size(), 2);
        QCOMPARE(m_clicks[1].type, ClickType::LeftUp);
        QCOMPARE(m_clicks[1].pos,  QPoint(300, 300));
    }

    // -----------------------------------------------------------------------
    // 9. RightDown/RightUp toggle works identically to LeftDown/LeftUp.
    // -----------------------------------------------------------------------
    void test_rightDragToggle()
    {
        auto dm = make(500);
        dm->arm(ClickType::RightDown);

        poll(dm.get());
        poll(dm.get(), 500);
        poll(dm.get());
        poll(dm.get(), 500);

        QCOMPARE(m_clicks.size(), 2);
        QCOMPARE(m_clicks[0].type, ClickType::RightDown);
        QCOMPARE(m_clicks[1].type, ClickType::RightUp);
    }

    // -----------------------------------------------------------------------
    // 10. Safety timeout: if XQueryPointer keeps reporting a moving cursor
    //     (e.g. compositor grab glitch) so LeftUp never dwells to completion,
    //     the release fires automatically after 10 seconds.
    // -----------------------------------------------------------------------
    void test_dragSafetyTimeout()
    {
        auto dm = make(500);
        dm->arm(ClickType::LeftDown);

        poll(dm.get());
        poll(dm.get(), 500);  // LeftDown fires at t=500
        QCOMPARE(m_clicks.size(), 1);

        // Simulate glitch: cursor keeps jumping each poll, preventing LeftUp dwell.
        // Advance 10 seconds total in small steps.
        for (int i = 0; i < 200; ++i) {
            m_cursor = {i * 3, i * 3};  // moves each poll — resets countdown
            poll(dm.get(), 50);
            if (m_clicks.size() >= 2) break;
        }

        QVERIFY2(m_clicks.size() >= 2, "Safety timeout should have fired LeftUp");
        QCOMPARE(m_clicks[1].type, ClickType::LeftUp);
    }

    // -----------------------------------------------------------------------
    // 11. disarm() during an active drag fires the Up event immediately.
    // -----------------------------------------------------------------------
    void test_disarmDuringDragReleasesButton()
    {
        auto dm = make(500);
        dm->arm(ClickType::LeftDown);

        poll(dm.get());
        poll(dm.get(), 500);  // LeftDown fires
        QCOMPARE(m_clicks.size(), 1);

        dm->disarm();
        QCOMPARE(m_clicks.size(), 2);
        QCOMPARE(m_clicks[1].type, ClickType::LeftUp);
        QVERIFY(!dm->isArmed());
    }

    // -----------------------------------------------------------------------
    // 12. arm() with a new type during an active drag releases the held button
    //     before switching, so the button never stays permanently held.
    // -----------------------------------------------------------------------
    void test_armDuringDragReleasesBeforeSwitching()
    {
        auto dm = make(500);
        dm->arm(ClickType::LeftDown);

        poll(dm.get());
        poll(dm.get(), 500);  // LeftDown fires
        QCOMPARE(m_clicks.size(), 1);

        dm->arm(ClickType::RightClick);  // switch while drag held

        QCOMPARE(m_clicks.size(), 2);
        QCOMPARE(m_clicks[1].type, ClickType::LeftUp);
    }

    // -----------------------------------------------------------------------
    // 13. disarm() before any click fires no spurious events.
    // -----------------------------------------------------------------------
    void test_disarmBeforeFireIsClean()
    {
        auto dm = make(500);
        dm->arm(ClickType::LeftClick);

        poll(dm.get());
        poll(dm.get(), 200);  // halfway — no click yet

        dm->disarm();
        QCOMPARE(m_clicks.size(), 0);
        QVERIFY(!dm->isArmed());
    }
};

QTEST_MAIN(DwellManagerTest)
#include "tst_dwellmanager.moc"
