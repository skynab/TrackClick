#include "mainwindow.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  define GLOBAL_POS(ev) (ev)->globalPosition().toPoint()
#else
#  define GLOBAL_POS(ev) (ev)->globalPos()
#endif
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStyle>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QDebug>
#include <QToolTip>
#include <QAction>
#include <QMessageBox>
#include <QFont>
#include <QToolButton>
#include <QIcon>
#include <QSize>
#include <QProcess>
#include <QTemporaryFile>
#include "translations/tsparser.h"
#ifdef Q_OS_MAC
#  include "macos_utils.h"
#  include <QDesktopServices>
#  include <QUrl>
#endif

// ─────────────────────────────────────────────────────────────
//  Palette constants
// ─────────────────────────────────────────────────────────────
static const QColor COL_BG       ("#2D2D2D");
static const QColor COL_ACCENT   ("#FFA600");
static const QColor COL_BG_BTN  ("#3A3A3A");
static const QColor COL_TEXT     ("#FFFFFF");
static const QColor COL_SUBTEXT  ("#AAAAAA");
static const QColor COL_OVERLAY  (0, 0, 0, 128); // rgba(0,0,0,0.5)
static const QColor COL_DANGER   ("#CC3333");

static const char* BASE_STYLE = R"(
QWidget {
    background: #2D2D2D;
    color: #FFFFFF;
    font-family: "Segoe UI", "Helvetica Neue", Arial, sans-serif;
    font-size: 11px;
}
QProgressBar {
    border: 1px solid #FFA600;
    border-radius: 3px;
    background: rgba(0,0,0,0.5);
    text-align: center;
    color: #FFA600;
    font-size: 9px;
}
QProgressBar::chunk {
    background: #FFA600;
    border-radius: 2px;
}
QToolTip {
    background: #1A1A1A;
    color: #FFA600;
    border: 1px solid #FFA600;
    padding: 3px 6px;
}
)";

// ─────────────────────────────────────────────────────────────
//  ClickButton
// ─────────────────────────────────────────────────────────────
ClickButton::ClickButton(const QString& label, ClickType type, QWidget* parent)
    : QToolButton(parent), m_type(type)
{
    setText(label);
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setMinimumSize(32, 44);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCheckable(false);
#ifdef Q_OS_MAC
    // macOS native style applies invisible layout-item margins around QToolButton
    // that cause the grid to mis-align rows relative to Windows/Linux.
    // WA_LayoutUsesWidgetRect tells the layout to use the visual rect instead.
    setAttribute(Qt::WA_LayoutUsesWidgetRect);
#endif
    updateStyle();
    connect(this, &QToolButton::clicked, this, [this](){
        emit clickTypePressed(m_type);
    });
}

void ClickButton::setSelected(bool sel)
{
    m_selected = sel;
    updateStyle();
    updateIcon();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ClickButton::enterEvent(QEnterEvent* ev)
#else
void ClickButton::enterEvent(QEvent* ev)
#endif
{
    QToolButton::enterEvent(ev);
    emit clickTypeHovered(m_type);
}

void ClickButton::leaveEvent(QEvent* ev)
{
    QToolButton::leaveEvent(ev);
    emit clickTypeLeft();
}

void ClickButton::setButtonIcon(const QString& iconName)
{
    m_iconName = iconName;
    updateIcon();
}

void ClickButton::setLargeMode(bool large)
{
    m_large = large;
    updateStyle();
    if (!m_iconName.isEmpty()) {
        const int sz = large ? 54 : 36;
        setIconSize(QSize(sz, sz));
    }
}

void ClickButton::updateIcon()
{
    if (m_iconName.isEmpty()) return;
    const QString path = m_selected
        ? ":/icons/selected/" + m_iconName + ".svg"
        : ":/icons/"          + m_iconName + ".svg";
    setIcon(QIcon(path));
}

void ClickButton::updateStyle()
{
    const char* fs  = m_large ? "14px" : "11px";
    const char* pad = m_large ? "6px 4px" : "4px 2px";
    if (m_selected) {
        setStyleSheet(QString(
            "QToolButton {"
            "  background: #FFA600;"
            "  color: #1A1A1A;"
            "  border: 2px solid #FFB833;"
            "  border-radius: 5px;"
            "  font-weight: bold;"
            "  font-size: %1;"
            "  padding: %2;"
            "}"
            "QToolButton:hover { background: #FFB833; }"
            "QToolButton:pressed { background: #CC8400; }"
        ).arg(fs).arg(pad));
    } else {
        setStyleSheet(QString(
            "QToolButton {"
            "  background: #3A3A3A;"
            "  color: #DDDDDD;"
            "  border: 2px solid #555555;"
            "  border-radius: 5px;"
            "  font-size: %1;"
            "  padding: %2;"
            "}"
            "QToolButton:hover {"
            "  background: #4A4A4A;"
            "  border: 2px solid #FFA600;"
            "  color: #FFA600;"
            "}"
            "QToolButton:pressed { background: #2A2A2A; }"
        ).arg(fs).arg(pad));
    }
}

// ── Click confirmation overlay ────────────────────────────────────────────────
// An expanding, fading ring rendered at the cursor position immediately after
// a dwell-click or manual button-press fires.  It is a frameless, always-on-top
// tool window that is transparent to mouse input and never steals focus.
class MainWindow::ClickIndicatorOverlay : public QWidget {
public:
    explicit ClickIndicatorOverlay() : QWidget(nullptr,
        Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
        | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        // Belt-and-braces click-through: the overlay spans the whole screen, so
        // it must never intercept input meant for windows beneath it.
        setAttribute(Qt::WA_TransparentForMouseEvents);
        m_timer.setInterval(STEP_MS);
        connect(&m_timer, &QTimer::timeout, this, [this]{
            if (++m_step >= STEPS) { m_timer.stop(); hide(); }
            else update();
        });
    }

    void flash(QPoint gp) {
        // Draw the ring inside a click-through overlay that spans the ENTIRE
        // virtual desktop (all monitors), anchored at the desktop origin, and
        // paint the ring at the cursor's offset within it.
        //
        // Spanning everything — rather than sizing the overlay to the cursor's
        // monitor — is the key to landing on the right screen.  Moving a
        // top-level window onto a specific, non-primary monitor is unreliable:
        // under Wayland/XWayland the compositor controls placement and routinely
        // drops the window back onto the primary monitor, so the ring showed up
        // on the wrong screen no matter how correctly its coordinates were
        // computed.  A single window covering the whole desktop has only one
        // possible position (the desktop origin), so there is no placement
        // decision left for the compositor to get wrong; the ring then appears
        // wherever the cursor is, on any monitor.
        //
        // `gp` is the global position the click was actually injected at, passed
        // in by the caller.  It MUST NOT be re-read here from QCursor::pos():
        // under XWayland that value freezes the instant the pointer leaves an
        // XWayland surface (see docs/INTERNAL.md §2), so it lags behind to the
        // last spot the cursor hovered a TrackClick window — the ring then landed
        // between the real cursor and the UI on Ubuntu.  The click position comes
        // from ClickInjector::cursorPos() (evdev/XI2), which stays accurate, so
        // the ring now marks exactly where the click happened.
        QScreen* scr = QGuiApplication::screenAt(gp);
        if (!scr) scr = QGuiApplication::primaryScreen();
        if (!scr) return;
        const QRect  vg = scr->virtualGeometry();   // union of all monitors (logical)
        setGeometry(vg);
        m_center = gp - vg.topLeft();               // ring centre, overlay-local coords

        if (qEnvironmentVariableIntValue("TRACKCLICK_CLICK_DEBUG"))
            qWarning() << "TrackClick click-ring: cursor" << gp
                       << "| screen" << scr->name() << scr->geometry()
                       << "| virtual" << vg << "| center" << m_center;

        m_step = 0;
        update();
        show();
        raise();
        m_timer.start();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        // Leave the window fully transparent (WA_TranslucentBackground clears it
        // each frame); only the ring + dot are painted, so nothing else is
        // visible and the whole surface stays click-through.
        // One-off diagnostic (first frame only): where did the overlay window
        // ACTUALLY land?  If mapToGlobal(0,0) is not the virtual-desktop origin,
        // the compositor/WM has repositioned the window and the ring is offset by
        // that amount regardless of how correct m_center is.  devicePixelRatio
        // surfaces any fractional-scaling mismatch between the injected position
        // (physical) and the overlay's logical paint coordinates.
        if (m_step == 0 && qEnvironmentVariableIntValue("TRACKCLICK_CLICK_DEBUG"))
            qWarning() << "TrackClick click-ring PAINT: window origin (global)"
                       << mapToGlobal(QPoint(0, 0))
                       << "| geometry" << geometry()
                       << "| frameGeometry" << frameGeometry()
                       << "| dpr" << devicePixelRatioF()
                       << "| m_center" << m_center;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const float t      = float(m_step) / float(STEPS);
        const int   radius = int(MIN_R + t * (MAX_R - MIN_R));
        const int   alpha  = int(255 * (1.0f - t));

        // Outer expanding ring
        QPen pen(QColor(0xFF, 0xA6, 0x00, alpha), 3);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(m_center, radius, radius);

        // Small solid centre dot that fades quickly
        const int dotAlpha = int(255 * qMax(0.0f, 1.0f - t * 2.5f));
        if (dotAlpha > 0) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0xFF, 0xA6, 0x00, dotAlpha));
            p.drawEllipse(m_center, 4, 4);
        }
    }

private:
    QTimer m_timer;
    int    m_step = 0;
    QPoint m_center;                     // ring centre, in overlay-local coords
    static constexpr int STEPS   = 24;   // ~384 ms total
    static constexpr int STEP_MS = 16;   // ~60 fps
    static constexpr int MIN_R   = 6;
    static constexpr int MAX_R   = 42;
};

// ─────────────────────────────────────────────────────────────
//  MainWindow
// ─────────────────────────────────────────────────────────────
MainWindow::MainWindow(QTranslator* startupTranslator, QWidget* parent)
    : QWidget(parent)
    , m_persist("TrackClick", "TrackClick")
{
    // Load persisted settings
    m_settings.dwellMs       = m_persist.value("dwell/ms",        1000).toInt();
    m_settings.sensitivityPx = m_persist.value("dwell/sensitivity", 5).toInt();
    m_settings.windowOpacity = m_persist.value("window/opacity",  1.0).toDouble();
    m_settings.alwaysOnTop   = m_persist.value("window/alwaysOnTop", true).toBool();
    m_settings.showNoClick     = m_persist.value("show/noClick",      true).toBool();
    m_settings.showLeftClick   = m_persist.value("show/leftClick",   true).toBool();
    m_settings.showLeftDouble  = m_persist.value("show/leftDouble",  true).toBool();
    m_settings.showLeftDrag    = m_persist.value("show/leftDrag",    true).toBool();
    m_settings.showRightClick  = m_persist.value("show/rightClick",  true).toBool();
    m_settings.showRightDouble = m_persist.value("show/rightDouble", true).toBool();
    m_settings.showRightDrag   = m_persist.value("show/rightDrag",   true).toBool();
    // Right Double / Right Drag are disabled actions (unusual). Force them off
    // regardless of any persisted or default "on" value, so the toolbar buttons
    // never appear. They're also omitted from the Settings reorder list. To
    // re-enable, remove these two overrides and add them to that list.
    m_settings.showRightDouble = false;
    m_settings.showRightDrag   = false;
    m_settings.showMiddleClick = m_persist.value("show/middleClick", true).toBool();
    m_settings.showMiddleDouble= m_persist.value("show/middleDouble",false).toBool();
    m_settings.showScrollUp    = m_persist.value("show/scrollUp",    true).toBool();
    m_settings.showScrollDown  = m_persist.value("show/scrollDown",  true).toBool();
    m_settings.showScrollHoriz = m_persist.value("show/scrollHoriz", false).toBool();
    m_settings.showModCtrl     = m_persist.value("show/modCtrl",     true).toBool();
    m_settings.showModAlt      = m_persist.value("show/modAlt",      true).toBool();
    m_settings.showModShift    = m_persist.value("show/modShift",    true).toBool();
    m_settings.showQuitButton    = m_persist.value("show/quitButton",     true).toBool();
    m_settings.showDwellActiveBtn= m_persist.value("show/dwellActiveBtn", true).toBool();
    m_settings.startMinimized   = m_persist.value("window/startMin",         false).toBool();
    m_settings.xMinimizesApp    = m_persist.value("window/xMinimizesApp",    false).toBool();
    // Top X minimizes app is a read-only option locked off: the top X always
    // quits. Force it off regardless of any persisted value. To re-enable,
    // remove this override — see settingsdialog.cpp buildUi.
    m_settings.xMinimizesApp    = false;
    m_settings.launchOnStartup  = m_persist.value("window/launchOnStartup",  false).toBool();
    m_settings.audioFeedback       = m_persist.value("audio/enabled",          false).toBool();
    m_settings.showClickIndicator  = m_persist.value("visual/clickIndicator",  AppSettings{}.showClickIndicator).toBool();
    m_settings.audioClickEnabled   = m_persist.value("audioClick/enabled",   false).toBool();
    m_settings.audioClickThreshold = m_persist.value("audioClick/threshold",    50).toInt();
    m_settings.audioInputDevice    = m_persist.value("audioClick/device",       "").toString();
    m_settings.iconsOnly       = m_persist.value("show/iconsOnly",    false).toBool();
    m_settings.largeButtons    = m_persist.value("show/largeButtons", false).toBool();
    m_settings.buttonLayout    = static_cast<ButtonLayout>(m_persist.value("show/buttonLayout", static_cast<int>(ButtonLayout::Vertical)).toInt());
    m_settings.language        = m_persist.value("language",          "en").toString();
    m_settings.settingsFontScale = m_persist.value("settings/fontScale", 100).toInt();
    m_settings.buttonOrder       = m_persist.value("show/buttonOrder").toStringList();
    m_settings.scrollRepeat    = m_persist.value("scroll/repeat",      7).toInt();
    m_settings.repeatOnDwell   = m_persist.value("dwell/repeatOnDwell", false).toBool();
    m_settings.hoverSelectPercent = m_persist.value("dwell/hoverSelectPercent", 60).toInt();
    m_settings.edgeLock = static_cast<EdgeLock>(m_persist.value("window/edgeLock", 0).toInt());
    m_settings.edgeHide = m_persist.value("window/edgeHide", false).toBool();
    for (int i = 0; i < 3; ++i) {
        const QString base = QString("hotkey/%1/").arg(i);
        m_settings.hotkeys[i].enabled     = m_persist.value(base + "enabled", false).toBool();
        m_settings.hotkeys[i].label       = m_persist.value(base + "label",   "").toString();
        m_settings.hotkeys[i].keySequence = m_persist.value(base + "seq",     "").toString();
    }

    // Adopt any translator already installed at startup so installLanguage()
    // can remove it when the user later switches languages (e.g. back to English).
    if (startupTranslator) {
        m_translator = startupTranslator;
        m_translator->setParent(this);   // transfer ownership from QApplication
    }

    // Window flags: frameless, stays on top
    Qt::WindowFlags flags = Qt::Window | Qt::FramelessWindowHint | Qt::Tool;
    if (m_settings.alwaysOnTop) flags |= Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
    setAttribute(Qt::WA_TranslucentBackground, false);
    // On macOS, Qt::Tool creates an NSPanel that hides when another app becomes
    // active (hidesOnDeactivation = true by default).  This attribute disables
    // that behaviour so the toolbar stays visible regardless of focus.
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
#ifdef Q_OS_MAC
    // Keep the window visible during Mission Control / Exposé and on all Spaces.
    // Must be called after setWindowFlags (which can recreate the native handle).
    applyMacOSWindowBehavior(winId());
#endif
    setStyleSheet(BASE_STYLE);
    setWindowTitle(tr("TrackClick"));
    setWindowOpacity(m_settings.windowOpacity);

    m_dwell = new DwellManager(this);
    m_dwell->setDwellMs(m_settings.dwellMs);
    m_dwell->setSensitivityPx(m_settings.sensitivityPx);
    m_dwell->setScrollRepeat(m_settings.scrollRepeat);
    m_dwell->setRepeatOnDwell(m_settings.repeatOnDwell);

#ifdef HAVE_MULTIMEDIA
    m_clickSound = new QSoundEffect(this);
    m_clickSound->setSource(QUrl("qrc:/sounds/click-noise.wav"));

    // Audio click: a loud sound fires the armed action instead of the dwell
    // timer.  The listener is only started while dwell-active is on (see
    // updateAudioClick()); here we just wire the trigger.
    m_audioClick = new AudioClickListener(this);
    connect(m_audioClick, &AudioClickListener::noiseDetected, this, [this]{
        if (m_settings.audioClickEnabled && m_autoEnabled)
            m_dwell->fireNow();
    });
#endif

    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    connect(m_hoverTimer, &QTimer::timeout, this, [this](){
        if (m_hoveredHotkey >= 0) {
            onHotkeySelected(m_hoveredHotkey);
        } else if (m_hoveredType != ClickType::None) {
            setClickType(m_hoveredType);
            if (m_autoEnabled)
                m_dwell->arm(m_hoveredType, m_modifiers);
        }
    });

    connect(m_dwell, &DwellManager::dwellProgress, this, &MainWindow::onDwellProgress);
    connect(m_dwell, &DwellManager::dwellFired,    this, &MainWindow::onDwellFired);

    m_clickIndicator = new ClickIndicatorOverlay();

    buildUi();
    buildTray();
    loadWindowSettings();

    // Swallow tooltip pop-ups over the toolbar (see eventFilter).
    qApp->installEventFilter(this);

    // If launch-on-startup is enabled, make sure the OS registration still
    // exists and points at the current executable (self-heals after updates).
    syncLaunchOnStartup();

    // Put the dwell manager into audio-trigger mode if the persisted setting
    // asks for it (the microphone only opens once dwell-active is turned on).
    updateAudioClick();
}

void MainWindow::promptForInputAccessIfNeeded()
{
#ifdef Q_OS_LINUX
    if (ClickInjector::hasInputDeviceAccess())
        return;

    // Respect a previous "Don't ask again" choice.
    if (m_persist.value("linux/skipInputAccessPrompt", false).toBool())
        return;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Enable cursor tracking"));
    box.setText(tr("TrackClick needs permission to read mouse movement."));
    box.setInformativeText(tr(
        "Without it, dwell-clicking only works while the cursor is over the "
        "TrackClick window. Granting permission installs a small system rule "
        "and shows a password prompt — no terminal required."));
    QPushButton* grant = box.addButton(tr("Grant Permission…"), QMessageBox::AcceptRole);
    box.addButton(tr("Not Now"), QMessageBox::RejectRole);
    QPushButton* never = box.addButton(tr("Don't Ask Again"), QMessageBox::ActionRole);
    box.setDefaultButton(grant);
    box.exec();

    if (box.clickedButton() == never)
        m_persist.setValue("linux/skipInputAccessPrompt", true);
    // Only proceed on an explicit Grant; "Not Now", "Don't Ask Again" and
    // dismissing the dialog all leave permissions unchanged.
    if (box.clickedButton() != grant)
        return;

    // Grant: extract the bundled udev rule, then install it as root via pkexec
    // (graphical polkit auth), reload the rules and re-tag input devices so the
    // uaccess ACL applies to the current session.
    QString ruleText;
    {
        QFile res(":/linux/71-trackclick-input.rules");
        if (res.open(QIODevice::ReadOnly | QIODevice::Text))
            ruleText = QString::fromUtf8(res.readAll());
    }
    if (ruleText.isEmpty()) {
        QMessageBox::warning(this, tr("TrackClick"),
            tr("Internal error: the permission rule could not be loaded."));
        return;
    }

    QString tmpPath;
    {
        QTemporaryFile tmp(QDir::tempPath() + "/trackclick-XXXXXX.rules");
        tmp.setAutoRemove(false);
        if (!tmp.open()) {
            QMessageBox::warning(this, tr("TrackClick"),
                tr("Could not create a temporary file for the permission rule."));
            return;
        }
        tmp.write(ruleText.toUtf8());
        tmp.flush();
        tmpPath = tmp.fileName();
    }

    const QString dest = QStringLiteral("/etc/udev/rules.d/71-trackclick-input.rules");
    const QString script = QStringLiteral(
        "cp '%1' '%2' && chmod 0644 '%2' && "
        "udevadm control --reload-rules && "
        "udevadm trigger --subsystem-match=input --action=change")
        .arg(tmpPath, dest);

    QProcess proc;
    proc.start(QStringLiteral("pkexec"), {QStringLiteral("/bin/sh"),
                                          QStringLiteral("-c"), script});
    const bool started = proc.waitForStarted(5000);
    bool finished = false;
    if (started)
        finished = proc.waitForFinished(120000);  // allow time at the password prompt

    QFile::remove(tmpPath);

    if (!started) {
        // pkexec unavailable — fall back to copyable manual instructions.
        QMessageBox::information(this, tr("TrackClick"),
            tr("Could not launch the graphical authentication helper (pkexec).\n\n"
               "To enable full cursor tracking, install this file as root:\n  %1\n\n"
               "with the following contents:\n\n%2").arg(dest, ruleText));
        return;
    }
    if (finished && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
        QMessageBox::information(this, tr("TrackClick"),
            tr("Permission granted. Please restart TrackClick to enable cursor "
               "tracking across all windows."));
    } else {
        QMessageBox::warning(this, tr("TrackClick"),
            tr("Permission was not granted. TrackClick will ask again next time "
               "it starts. (On an X11/Xorg session this permission is not needed.)"));
    }
#elif defined(Q_OS_MAC)
    if (ClickInjector::hasInputDeviceAccess())
        return;   // already trusted for Accessibility

    // Respect a previous "Don't ask again" choice.
    if (m_persist.value("mac/skipAccessibilityPrompt", false).toBool())
        return;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Enable clicking"));
    box.setText(tr("TrackClick needs Accessibility permission to move and click the mouse."));
    box.setInformativeText(tr(
        "Until it's granted in System Settings ▸ Privacy & Security ▸ Accessibility, "
        "the dwell countdown runs but no click is performed. You can also reach this "
        "later from the Settings dialog."));
    QPushButton* grant = box.addButton(tr("Open Accessibility Settings…"), QMessageBox::AcceptRole);
    box.addButton(tr("Not Now"), QMessageBox::RejectRole);
    QPushButton* never = box.addButton(tr("Don't Ask Again"), QMessageBox::ActionRole);
    box.setDefaultButton(grant);
    box.exec();

    if (box.clickedButton() == never)
        m_persist.setValue("mac/skipAccessibilityPrompt", true);
    if (box.clickedButton() != grant)
        return;

    // Register TrackClick in the Accessibility list and trigger macOS's own
    // prompt, then open the pane so the user can flip the switch. The grant
    // only takes effect after a relaunch.
    macAccessibilityTrusted(/*promptIfNeeded=*/true);
    QDesktopServices::openUrl(QUrl(
        "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"));
    QMessageBox::information(this, tr("TrackClick"),
        tr("Enable TrackClick in the Accessibility list, then restart the app."));
#endif
}

void MainWindow::buildUi()
{
    setMouseTracking(true);   // needed for cursor updates without a button held

    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    // ── Title bar ─────────────────────────────────────────────
    m_titleBar = new QWidget;
    m_titleBar->setObjectName("titleBar");
    m_titleBar->setFixedHeight(30);
    m_titleBar->setStyleSheet(
        "#titleBar { background: #1A1A1A; border-bottom: 2px solid #FFA600; }"
        "QLabel    { color: #FFA600; font-weight: bold; font-size: 12px;"
        "            background: transparent; border: none; }"
    );
    auto* tbLayout = new QHBoxLayout(m_titleBar);
    tbLayout->setContentsMargins(8, 0, 4, 0);
    tbLayout->setSpacing(4);

    m_titleIcon = new QLabel;
    m_titleIcon->setPixmap(QIcon(":/icons/app.svg").pixmap(16, 16));
    m_titleIcon->setFixedSize(20, 20);
    m_titleIcon->setAlignment(Qt::AlignCenter);
    tbLayout->addWidget(m_titleIcon);

    m_titleLabel = new QLabel(tr("TrackClick"));
    m_titleLabel->setMinimumWidth(0);  // let it clip rather than force window wider
    tbLayout->addWidget(m_titleLabel);
    tbLayout->addStretch(1);  // always pushes the action buttons to the right

    // Auto button
    m_autoBtn = new QPushButton;
    m_autoBtn->setIcon(QIcon(":/icons/auto.svg"));
    m_autoBtn->setIconSize(QSize(18, 18));
    m_autoBtn->setCheckable(true);
    m_autoBtn->setFixedSize(38, 22);
    m_autoBtn->setToolTip(tr("Toggle AutoMouse dwell-clicking"));
    m_autoBtn->setStyleSheet(
        "QPushButton { background:#3A3A3A; color:#DDDDDD; border:1px solid #555; border-radius:3px; font-size:10px; font-weight:bold; }"
        "QPushButton:checked { background:#FFA028; color:#1A1A1A; border:1px solid #FFB040; }"
        "QPushButton:hover { border:1px solid #FFA028; }"
    );
    connect(m_autoBtn, &QPushButton::toggled, this, &MainWindow::onAutoToggled);
    tbLayout->addWidget(m_autoBtn);

    // Settings button
    m_settingsBtn = new QPushButton;
    m_settingsBtn->setIcon(QIcon(":/icons/settings.svg"));
    m_settingsBtn->setIconSize(QSize(16, 16));
    m_settingsBtn->setFixedSize(22, 22);
    m_settingsBtn->setToolTip(tr("Settings"));
    m_settingsBtn->setStyleSheet(
        "QPushButton { background:#3A3A3A; color:#CCC; border:1px solid #555; border-radius:3px; font-size:14px; }"
        "QPushButton:hover { background:#4A4A4A; color:#FFA600; border:1px solid #FFA600; }"
        "QPushButton:pressed { background:#2A2A2A; }"
    );
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    tbLayout->addWidget(m_settingsBtn);

    // Close/hide button
    m_exitBtn = new QPushButton;
    m_exitBtn->setIcon(QIcon(":/icons/close.svg"));
    m_exitBtn->setIconSize(QSize(14, 14));
    m_exitBtn->setFixedSize(22, 22);
    m_exitBtn->setToolTip(tr("Hide to tray (right-click tray icon to quit)"));
    m_exitBtn->setStyleSheet(
        "QPushButton { background:#3A3A3A; color:#CCC; border:1px solid #555; border-radius:3px; font-size:12px; }"
        "QPushButton:hover { background:#CC3333; color:#FFF; border:1px solid #CC3333; }"
        "QPushButton:pressed { background:#991111; }"
    );
    connect(m_exitBtn, &QPushButton::clicked, this, &MainWindow::onExitClicked);
    tbLayout->addWidget(m_exitBtn);

    root->addWidget(m_titleBar);

    // ── Button area ───────────────────────────────────────────
    m_btnArea = new QWidget;
    m_btnArea->setContentsMargins(6, 6, 6, 6);
    root->addWidget(m_btnArea);
    rebuildButtons();

    // ── Dwell progress bar ────────────────────────────────────
    m_dwellBar = new QProgressBar;
    m_dwellBar->setRange(0, 100);
    m_dwellBar->setValue(0);
    m_dwellBar->setFixedHeight(8);
    m_dwellBar->setTextVisible(false);
    m_dwellBar->setMinimumWidth(0);
    m_dwellBar->setVisible(false);
    root->addWidget(m_dwellBar);

    // ── Audio level meter ─────────────────────────────────────
    // Same footprint as the dwell bar but a distinct (green) colour, shown in its
    // place while audio-click mode is active so the two modes read differently.
    m_audioMeter = new QProgressBar;
    m_audioMeter->setRange(0, 100);
    m_audioMeter->setValue(0);
    m_audioMeter->setFixedHeight(8);
    m_audioMeter->setTextVisible(false);
    m_audioMeter->setMinimumWidth(0);
    // Cyan (vs the dwell bar's orange) so the active mode is obvious at a glance,
    // and matched to the settings calibration meter for cross-screen correlation.
    m_audioMeter->setStyleSheet(
        "QProgressBar { border: 1px solid #00A5B8; border-radius: 3px;"
        " background: rgba(0,0,0,0.5); }"
        "QProgressBar::chunk { background: #00EBFF; border-radius: 2px; }");
    m_audioMeter->setVisible(false);
    root->addWidget(m_audioMeter);
#ifdef HAVE_MULTIMEDIA
    if (m_audioClick) {
        connect(m_audioClick, &AudioClickListener::level, this, [this](double v){
            if (m_audioMeter->isVisible())
                m_audioMeter->setValue(static_cast<int>(qBound(0.0, v, 1.0) * 100));
        });
    }
#endif

    // ── Status label ──────────────────────────────────────────
    m_statusLabel = new QLabel("Ready — hover to dwell-click");
    m_statusLabel->setStyleSheet(
        "QLabel { color: #888888; font-size: 9px; padding: 2px 6px; "
        "background: #1A1A1A; border-top: 1px solid #3A3A3A; }"
    );
    m_statusLabel->setFixedHeight(18);
    m_statusLabel->setMinimumWidth(0);
    m_statusLabel->setVisible(false);
    root->addWidget(m_statusLabel);

    adjustSize();
}

// Attaches hover-toggle behaviour to a modifier QPushButton: the button toggles
// once the cursor has rested on it for the hover-select interval — the same
// timing used to switch the active click type (see hoverSelectMs()).  The
// interval is supplied as a callback so it is read fresh on each hover.
// Parented to the button so it is deleted automatically with it.
class ModHoverFilter : public QObject {
public:
    explicit ModHoverFilter(QPushButton* btn, std::function<int()> intervalFn)
        : QObject(btn), m_intervalFn(std::move(intervalFn))
    {
        m_timer = new QTimer(this);
        m_timer->setSingleShot(true);
        QObject::connect(m_timer, &QTimer::timeout, btn, [btn](){ btn->toggle(); });
        btn->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject*, QEvent* ev) override
    {
        if (ev->type() == QEvent::Enter)
            m_timer->start(m_intervalFn());
        else if (ev->type() == QEvent::Leave)
            m_timer->stop();
        return false;
    }

private:
    QTimer*              m_timer;
    std::function<int()> m_intervalFn;
};

// Forwards Enter/Leave events for a hotkey button to lambdas so the shared
// hover-select timer can select the hotkey just like a ClickButton selection.
class HotkeyHoverFilter : public QObject {
public:
    HotkeyHoverFilter(QWidget* btn,
                      std::function<void()> onEnter,
                      std::function<void()> onLeave)
        : QObject(btn), m_onEnter(std::move(onEnter)), m_onLeave(std::move(onLeave))
    { btn->installEventFilter(this); }

protected:
    bool eventFilter(QObject*, QEvent* ev) override
    {
        if      (ev->type() == QEvent::Enter) m_onEnter();
        else if (ev->type() == QEvent::Leave) m_onLeave();
        return false;
    }

private:
    std::function<void()> m_onEnter, m_onLeave;
};

// Returns the stylesheet for a modifier/hotkey button in its selected or
// unselected state.  Used by rebuildButtons, setClickType, and onHotkeySelected.
static QString modBtnStyle(bool selected, bool large)
{
    const char* fs  = large ? "14px" : "11px";
    const char* pad = large ? "4px"  : "2px";
    return selected
        ? QString("QPushButton { background:#FFA600; color:#1A1A1A; border:2px solid #FFB833; "
                  "border-radius:4px; font-weight:bold; font-size:%1; padding:%2; }"
                  "QPushButton:hover { background:#FFB833; }").arg(fs).arg(pad)
        : QString("QPushButton { background:#3A3A3A; color:#DDDDDD; border:1px solid #555; "
                  "border-radius:4px; font-size:%1; padding:%2; }"
                  "QPushButton:hover { background:#4A4A4A; border:1px solid #FFA600; color:#FFA600; }").arg(fs).arg(pad);
}

// Uniform minimum cell size for every toolbar button, so click buttons,
// modifiers, Dwell Active, Quit and hotkeys share the same footprint and line
// up in the grid regardless of which are shown.  Width is 0 in the vertical
// modes (the single/double columns stretch); fixed otherwise.
static QSize toolbarButtonMinSize(ButtonLayout layout, bool large)
{
    switch (layout) {
        case ButtonLayout::Horizontal: return QSize(60, large ? 72 : 54);
        case ButtonLayout::Rectangle:  return QSize(48, large ? 64 : 48);
        case ButtonLayout::Vertical:
        case ButtonLayout::VerticalTwo:
        default:                       return QSize(0,  large ? 54 : 36);
    }
}

void MainWindow::rebuildButtons()
{
    // Clear existing
    if (m_btnArea->layout()) {
        while (m_btnArea->layout()->count()) {
            auto* item = m_btnArea->layout()->takeAt(0);
            if (item->widget()) { item->widget()->deleteLater(); }
            delete item;
        }
        delete m_btnArea->layout();
    }
    m_clickButtons.clear();
    m_ctrlBtn = m_altBtn = m_shiftBtn = m_dwellActiveBtn = nullptr;
    m_hotkeyBtns[0] = m_hotkeyBtns[1] = m_hotkeyBtns[2] = nullptr;

    auto* grid = new QGridLayout(m_btnArea);
    grid->setSpacing(4);
    // Vertical modes: total horizontal margin (1+1+1+1 = 4 px) equals the column
    // spacing (4 px), so VerticalOne = exactly half the width of VerticalTwo.
    const bool isVerticalMode = (m_settings.buttonLayout == ButtonLayout::Vertical ||
                                  m_settings.buttonLayout == ButtonLayout::VerticalTwo);
    const bool vertOne = (m_settings.buttonLayout == ButtonLayout::Vertical);

    // In single-column vertical mode, collapse the title label and shrink the
    // auto button to match the settings/exit buttons — this halves the window
    // width relative to every other layout mode.
    m_titleLabel->setVisible(!vertOne);
    m_autoBtn->setFixedSize(vertOne ? 22 : 38, 22);

    if (isVerticalMode) {
        m_btnArea->setContentsMargins(1, 4, 1, 4);
        grid->setContentsMargins(1, 4, 1, 4);
    } else {
        m_btnArea->setContentsMargins(6, 6, 6, 6);
        grid->setContentsMargins(4, 4, 4, 4);
    }

    int row = 0, col = 0;
    const int COLS = (m_settings.buttonLayout == ButtonLayout::Vertical)    ? 1
                   : (m_settings.buttonLayout == ButtonLayout::VerticalTwo)  ? 2
                   : (m_settings.buttonLayout == ButtonLayout::Horizontal)   ? 99
                   :                                                            3;

    // Factory: create a click button (signals + m_clickButtons), no placement.
    auto makeClickButton = [&](const QString& lbl, const QString& tip, ClickType t, const QString& icon) -> QWidget* {
        auto* btn = makeButton(lbl, tip, t, icon);
        m_clickButtons.append(btn);
        connect(btn, &ClickButton::clickTypePressed, this, &MainWindow::onClickButtonPressed);
        connect(btn, &ClickButton::clickTypeHovered, this, [this](ClickType type){
            m_hoveredType = type;
            m_hoverTimer->start(hoverSelectMs());
        });
        connect(btn, &ClickButton::clickTypeLeft, this, [this](){
            m_hoverTimer->stop();
            m_hoveredType = ClickType::None;
        });
        return btn;
    };

    // Shared look/sizing for the modifier, Dwell Active, Quit and hotkey buttons
    // — the same footprint as the click buttons so everything aligns in the grid.
    const bool  large   = m_settings.largeButtons;
    auto modStyle = [large](bool on) -> QString { return modBtnStyle(on, large); };
    const QSize modSize = toolbarButtonMinSize(m_settings.buttonLayout, large);

    // Factory: a checkable modifier button (Ctrl/Alt/Shift).  Assigns the member
    // pointer and toggles the given modifier bit.
    auto makeModifier = [&](QPushButton*& member, const QString& text,
                            const QString& tip, int flag) -> QWidget* {
        auto* btn = new QPushButton(text, m_btnArea);
        member = btn;
        btn->setCheckable(true);
        btn->setMinimumSize(modSize);
        btn->setToolTip(tip);
        btn->setStyleSheet(modStyle(false));
        connect(btn, &QPushButton::toggled, this, [this, btn, modStyle, flag](bool on){
            if (on) m_modifiers |= flag; else m_modifiers &= ~flag;
            btn->setStyleSheet(modStyle(on));
            if (m_autoEnabled) m_dwell->setModifiers(m_modifiers);
        });
        new ModHoverFilter(btn, [this]{ return hoverSelectMs(); });
        return btn;
    };

    // Factory: the Dwell Active toggle (mirrors the title-bar Auto button).
    auto makeDwellActive = [&]() -> QWidget* {
        auto dwellActiveStyle = [large](bool on) -> QString {
            const char* fs  = large ? "14px" : "11px";
            const char* pad = large ? "4px"  : "2px";
            return on
                ? QString("QPushButton { background:#FFA028; color:#1A1A1A; border:2px solid #FFB040; "
                          "border-radius:4px; font-weight:bold; font-size:%1; padding:%2; }"
                          "QPushButton:hover { background:#FFB040; }").arg(fs).arg(pad)
                : QString("QPushButton { background:#3A3A3A; color:#DDDDDD; border:1px solid #555; "
                          "border-radius:4px; font-size:%1; padding:%2; }"
                          "QPushButton:hover { background:#4A4A4A; border:1px solid #FFA028; color:#FFA028; }").arg(fs).arg(pad);
        };
        auto* btn = new QPushButton(tr("Dwell Active"), m_btnArea);
        m_dwellActiveBtn = btn;
        btn->setCheckable(true);
        btn->setMinimumSize(modSize);
        btn->setToolTip(tr("Enable dwell-clicking (same as the Auto button)"));
        btn->setChecked(m_autoEnabled);
        btn->setStyleSheet(dwellActiveStyle(m_autoEnabled));
        connect(btn, &QPushButton::toggled, this, [this, dwellActiveStyle](bool on){
            m_dwellActiveBtn->setStyleSheet(dwellActiveStyle(on));
            m_autoBtn->setChecked(on);   // drive the canonical Auto button
        });
        new ModHoverFilter(btn, [this]{ return hoverSelectMs(); });
        return btn;
    };

    // Factory: the red Quit button.
    auto makeQuit = [&]() -> QWidget* {
        auto* quitBtn = new QPushButton(tr("Quit Program"), m_btnArea);
        quitBtn->setStyleSheet(
            QString("QPushButton { background:#3A1A1A; color:#FF6B6B; border:1px solid #7A3333; "
                    "border-radius:4px; font-size:%1; padding:%2; }"
                    "QPushButton:hover { background:#5C2222; border-color:#FF6B6B; }")
                .arg(large ? "13px" : "11px")
                .arg(large ? "4px"  : "2px"));
        quitBtn->setMinimumSize(modSize);
        connect(quitBtn, &QPushButton::clicked, qApp, &QApplication::quit);
        return quitBtn;
    };

    // Factory: a custom-hotkey button for slot i, or nullptr if that slot is off
    // or has no key assigned.  Flows inline with the other reorderable buttons.
    auto makeHotkey = [&](int i) -> QWidget* {
        const auto& slot = m_settings.hotkeys[i];
        if (!slot.enabled || slot.keySequence.isEmpty()) return nullptr;
        QKeySequence seq(slot.keySequence, QKeySequence::PortableText);
        const QString displayLabel = slot.label.isEmpty()
            ? seq.toString(QKeySequence::NativeText) : slot.label;
        if (displayLabel.isEmpty()) return nullptr;
        auto* btn = new QPushButton(displayLabel, m_btnArea);
        btn->setMinimumSize(modSize);
        btn->setToolTip(seq.toString(QKeySequence::NativeText));
        btn->setStyleSheet(modBtnStyle(m_selectedHotkey == i, large));
        connect(btn, &QPushButton::clicked, this, [this, i]{ onHotkeySelected(i); });
        new HotkeyHoverFilter(btn,
            [this, i]{ m_hoveredHotkey = i; m_hoverTimer->start(hoverSelectMs()); },
            [this]   { m_hoveredHotkey = -1; m_hoverTimer->stop(); });
        m_hotkeyBtns[i] = btn;
        return btn;
    };

    // Placement: column-flow with wrap (Horizontal has COLS large, so one row).
    auto place = [&](QWidget* w){
        grid->addWidget(w, row, col++);
        if (col >= COLS) { col = 0; row++; }
    };

    // Click-button metadata (label/tooltip/type/icon + visibility).
    struct BtnMeta { bool show; QString lbl; QString tip; ClickType type; QString icon; };
    const QHash<QString, BtnMeta> meta = {
        { "no_click",      { m_settings.showNoClick,     tr("No Click"), tr("No action — dwell without clicking"), ClickType::NoClick,          "no_click" } },
        { "left_click",    { m_settings.showLeftClick,   tr("L Click"),  tr("Left Click"),         ClickType::LeftClick,        "left_click" } },
        { "left_double",   { m_settings.showLeftDouble,  tr("L Dbl"),    tr("Left Double-Click"),  ClickType::LeftDoubleClick,  "left_double" } },
        { "left_drag",     { m_settings.showLeftDrag,    tr("L Drag"),   tr("Left Drag — dwell to grab, dwell again to release"),   ClickType::LeftDown,         "left_drag" } },
        { "right_click",   { m_settings.showRightClick,  tr("R Click"),  tr("Right Click"),        ClickType::RightClick,       "right_click" } },
        { "right_double",  { m_settings.showRightDouble, tr("R Dbl"),    tr("Right Double-Click"), ClickType::RightDoubleClick, "right_double" } },
        { "right_drag",    { m_settings.showRightDrag,   tr("R Drag"),   tr("Right Drag — dwell to grab, dwell again to release"),  ClickType::RightDown,        "right_drag" } },
        { "middle_click",  { m_settings.showMiddleClick, tr("M Click"),  tr("Middle Click"),       ClickType::MiddleClick,      "middle_click" } },
        { "middle_double", { m_settings.showMiddleDouble,tr("M Dbl"),    tr("Middle Double-Click"),ClickType::MiddleDoubleClick,"middle_double" } },
        { "scroll_up",     { m_settings.showScrollUp,    tr("Scroll ▲"), tr("Scroll Up"),          ClickType::ScrollUp,         "scroll_up" } },
        { "scroll_down",   { m_settings.showScrollDown,  tr("Scroll ▼"), tr("Scroll Down"),        ClickType::ScrollDown,       "scroll_down" } },
    };

    // Single ordered pass — place every enabled reorderable button inline in the
    // user's saved order.  Click buttons, modifiers, Dwell Active and Quit all
    // flow together; "scroll_horiz" expands to the two horizontal-scroll buttons.
    for (const QString& id : orderedClickButtonIds(m_settings)) {
        if (id == QLatin1String("scroll_horiz")) {
            if (m_settings.showScrollHoriz) {
                place(makeClickButton(tr("Scroll ◄"), tr("Scroll Left"),  ClickType::ScrollLeft,  "scroll_left"));
                place(makeClickButton(tr("Scroll ►"), tr("Scroll Right"), ClickType::ScrollRight, "scroll_right"));
            }
        } else if (id == QLatin1String("ctrl")) {
            if (m_settings.showModCtrl)        place(makeModifier(m_ctrlBtn,  QStringLiteral("Ctrl"),  tr("Hold Ctrl modifier for next click"),  ModCtrl));
        } else if (id == QLatin1String("alt")) {
            if (m_settings.showModAlt)         place(makeModifier(m_altBtn,   QStringLiteral("Alt"),   tr("Hold Alt modifier for next click"),   ModAlt));
        } else if (id == QLatin1String("shift")) {
            if (m_settings.showModShift)       place(makeModifier(m_shiftBtn, QStringLiteral("Shift"), tr("Hold Shift modifier for next click"), ModShift));
        } else if (id == QLatin1String("dwell_active")) {
            if (m_settings.showDwellActiveBtn) place(makeDwellActive());
        } else if (id == QLatin1String("quit")) {
            if (m_settings.showQuitButton)     place(makeQuit());
        } else if (id.startsWith(QLatin1String("hotkey_"))) {
            bool ok = false;
            const int i = id.mid(7).toInt(&ok);
            if (ok && i >= 0 && i < 3)
                if (QWidget* w = makeHotkey(i)) place(w);
        } else {
            auto it = meta.find(id);
            if (it != meta.end() && it->show)
                place(makeClickButton(it->lbl, it->tip, it->type, it->icon));
        }
    }

    // Horizontal mode: give every occupied column equal stretch so buttons fill
    // the window width.
    if (m_settings.buttonLayout == ButtonLayout::Horizontal) {
        for (int c = 0; c < col; ++c)
            grid->setColumnStretch(c, 1);
    }

    // Click buttons, modifiers, Dwell Active, Quit and the custom hotkey buttons
    // are all created by the factories and placed inline in the ordered pass
    // above, so there's nothing more to lay out here.

    // Update selection highlight — suppress ClickButton highlight when a hotkey is selected
    const ClickType selForBtns = (m_selectedHotkey >= 0) ? ClickType::None : m_selectedType;
    for (auto* b : m_clickButtons)
        b->setSelected(b->clickType() == selForBtns);
    for (int i = 0; i < 3; ++i) {
        if (m_hotkeyBtns[i])
            m_hotkeyBtns[i]->setStyleSheet(modBtnStyle(m_selectedHotkey == i, large));
    }

    // macOS: a stylesheet-styled QPushButton reports a stale sizeHint until it is
    // polished — which otherwise only happens on first interaction — so the
    // modifier / hotkey / Quit buttons render misaligned and the window resizes
    // on the first click.  Force a re-polish + geometry refresh now so the layout
    // (and adjustSize below) uses their correct size from the start.  Click
    // buttons are QToolButtons and unaffected, so this only touches QPushButtons.
    for (QPushButton* b : m_btnArea->findChildren<QPushButton*>()) {
        b->style()->unpolish(b);
        b->style()->polish(b);
        b->updateGeometry();
    }
    if (auto* lay = m_btnArea->layout())
        lay->activate();

    adjustSize();
}

ClickButton* MainWindow::makeButton(const QString& label, const QString& tooltip, ClickType type, const QString& iconName)
{
    auto* btn = new ClickButton(label, type, m_btnArea);
    btn->setToolTip(tooltip);
    btn->setToolButtonStyle(m_settings.iconsOnly ? Qt::ToolButtonIconOnly
                                                  : Qt::ToolButtonTextUnderIcon);
    btn->setButtonIcon(iconName);
    btn->setLargeMode(m_settings.largeButtons);   // scales style, font, icon size

    const bool large = m_settings.largeButtons;
    btn->setMinimumSize(toolbarButtonMinSize(m_settings.buttonLayout, large));
    // Icon size is click-button specific (labels sit under the icon).
    if (m_settings.buttonLayout == ButtonLayout::Horizontal)
        btn->setIconSize(QSize(large ? 42 : 30, large ? 42 : 30));
    else
        btn->setIconSize(QSize(large ? 36 : 24, large ? 36 : 24));
    return btn;
}

void MainWindow::buildTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;

    m_tray = new QSystemTrayIcon(QIcon(":/icons/app.svg"), this);
    m_tray->setToolTip(tr("TrackClick Virtual Mouse"));

    m_trayMenu = new QMenu(this);
    m_trayMenu->setStyleSheet(
        "QMenu { background:#2D2D2D; color:#FFF; border:1px solid #FFA600; }"
        "QMenu::item:selected { background:#FFA600; color:#1A1A1A; }"
    );
    m_showAct = m_trayMenu->addAction(tr("Show / Hide"));
    m_trayMenu->addSeparator();
    m_quitAct = m_trayMenu->addAction(tr("Quit TrackClick"));

    connect(m_showAct, &QAction::triggered, this, [this](){
        setVisible(!isVisible());
        if (isVisible()) raise();
    });
    connect(m_quitAct, &QAction::triggered, qApp, &QApplication::quit);
    connect(m_tray, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);

    m_tray->setContextMenu(m_trayMenu);
    m_tray->show();
}

// ─── Resize / drag the frameless window ──────────────────────────────────

MainWindow::ResizeEdge MainWindow::edgeAt(QPoint p) const
{
    const QRect  r = rect();
    const int    m = RESIZE_MARGIN;
    const bool onL = p.x() <= m;
    const bool onR = p.x() >= r.width()  - m;
    const bool onT = p.y() <= m;
    const bool onB = p.y() >= r.height() - m;
    if (onT && onL) return ResizeEdge::TopLeft;
    if (onT && onR) return ResizeEdge::TopRight;
    if (onB && onL) return ResizeEdge::BottomLeft;
    if (onB && onR) return ResizeEdge::BottomRight;
    if (onL)        return ResizeEdge::Left;
    if (onR)        return ResizeEdge::Right;
    if (onT)        return ResizeEdge::Top;
    if (onB)        return ResizeEdge::Bottom;
    return ResizeEdge::None;
}

Qt::CursorShape MainWindow::cursorForEdge(ResizeEdge e)
{
    switch (e) {
        case ResizeEdge::Left:  case ResizeEdge::Right:        return Qt::SizeHorCursor;
        case ResizeEdge::Top:   case ResizeEdge::Bottom:       return Qt::SizeVerCursor;
        case ResizeEdge::TopLeft: case ResizeEdge::BottomRight:return Qt::SizeFDiagCursor;
        case ResizeEdge::TopRight:case ResizeEdge::BottomLeft: return Qt::SizeBDiagCursor;
        default:                                               return Qt::ArrowCursor;
    }
}

void MainWindow::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) return;

    const ResizeEdge edge = edgeAt(ev->pos());
    if (edge != ResizeEdge::None) {
        m_resizeEdge  = edge;
        m_resizeStart = GLOBAL_POS(ev);
        m_resizeGeo   = frameGeometry();
        return;
    }

    if (m_titleBar && m_titleBar->geometry().contains(ev->pos())) {
        m_dragging   = true;
        m_dragOffset = GLOBAL_POS(ev) - frameGeometry().topLeft();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_resizeEdge != ResizeEdge::None) {
        const QPoint delta = GLOBAL_POS(ev) - m_resizeStart;
        QRect geo = m_resizeGeo;
        switch (m_resizeEdge) {
            case ResizeEdge::Left:        geo.setLeft(geo.left()   + delta.x()); break;
            case ResizeEdge::Right:       geo.setRight(geo.right() + delta.x()); break;
            case ResizeEdge::Top:         geo.setTop(geo.top()     + delta.y()); break;
            case ResizeEdge::Bottom:      geo.setBottom(geo.bottom()+ delta.y());break;
            case ResizeEdge::TopLeft:     geo.setTopLeft(geo.topLeft()         + delta); break;
            case ResizeEdge::TopRight:    geo.setTopRight(geo.topRight()       + delta); break;
            case ResizeEdge::BottomLeft:  geo.setBottomLeft(geo.bottomLeft()   + delta); break;
            case ResizeEdge::BottomRight: geo.setBottomRight(geo.bottomRight() + delta); break;
            default: break;
        }
        setGeometry(geo.normalized());
        return;
    }

    if (m_dragging) {
        QPoint newPos = GLOBAL_POS(ev) - m_dragOffset;
        if (m_settings.edgeLock != EdgeLock::None) {
            QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
            newPos.setX(m_settings.edgeLock == EdgeLock::Left
                ? avail.left()
                : avail.right() - width() + 1);
        }
        move(newPos);
        return;
    }

    // Hover — update cursor to hint at available resize edges
    setCursor(cursorForEdge(edgeAt(ev->pos())));
}

void MainWindow::mouseReleaseEvent(QMouseEvent*)
{
    m_resizeEdge = ResizeEdge::None;
    m_dragging   = false;
    saveWindowSettings();
}

void MainWindow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // Dark background
    p.fillRect(rect(), COL_BG);
    // Orange border
    p.setPen(QPen(COL_ACCENT, 2));
    p.drawRect(rect().adjusted(1,1,-1,-1));
}

void MainWindow::closeEvent(QCloseEvent* ev)
{
    if (m_settings.xMinimizesApp) {
        ev->ignore();
        hide();
    } else {
        ev->accept();
    }
}

void MainWindow::changeEvent(QEvent* ev)
{
    if (ev->type() == QEvent::WindowStateChange && isMinimized()) {
        hide();
    }
    QWidget::changeEvent(ev);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    // Drop tooltip requests for any widget inside this window. isAncestorOf is
    // false for widgets in other top-level windows (e.g. the Settings dialog),
    // so those keep their tooltips.
    if (ev->type() == QEvent::ToolTip) {
        auto* w = qobject_cast<QWidget*>(obj);
        if (w && (w == this || isAncestorOf(w)))
            return true;   // handled — no tooltip is shown
    }
    return QWidget::eventFilter(obj, ev);
}

void MainWindow::showEvent(QShowEvent* ev)
{
    QWidget::showEvent(ev);
    // When restored from tray (or any show), always snap to the visible edge position
    // so the window never appears in its partially-hidden state.
    if (m_settings.edgeLock != EdgeLock::None) {
        if (m_edgeAnim) m_edgeAnim->stop();
        QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
        int shownX = (m_settings.edgeLock == EdgeLock::Left)
            ? avail.left()
            : avail.right() - width() + 1;
        if (x() != shownX)
            move(shownX, y());
        m_edgeShown       = true;
        m_edgeHideCountMs = 0;
    }
}

// ── Edge lock / slide-hide ────────────────────────────────────────────────────

static constexpr int k_edgePeekPx      = 8;   // pixels visible when hidden
static constexpr int k_edgeHideDelayMs = 800;  // idle time before sliding away
static constexpr int k_edgePollMs      = 80;   // poll interval
static constexpr int k_edgeAnimMs      = 180;  // slide animation duration

void MainWindow::applyEdgeLock()
{
    if (m_edgePollTimer) m_edgePollTimer->stop();
    if (m_edgeAnim)      m_edgeAnim->stop();

    m_edgeShown       = true;
    m_edgeHideCountMs = 0;

    const EdgeLock lock = m_settings.edgeLock;
    if (lock == EdgeLock::None) return;

    QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
    int shownX  = (lock == EdgeLock::Left)
        ? avail.left()
        : avail.right() - width() + 1;
    move(shownX, qBound(avail.top(), y(), avail.bottom() - height()));

    if (m_settings.edgeHide) {
        if (!m_edgePollTimer) {
            m_edgePollTimer = new QTimer(this);
            connect(m_edgePollTimer, &QTimer::timeout, this, &MainWindow::onEdgePoll);
        }
        m_edgePollTimer->start(k_edgePollMs);
    }
}

void MainWindow::onEdgePoll()
{
    if (!isVisible()) {
        m_edgeShown = true;
        m_edgeHideCountMs = 0;
        return;
    }

    const EdgeLock lock = m_settings.edgeLock;
    if (lock == EdgeLock::None) return;

    QRect  avail  = QGuiApplication::primaryScreen()->availableGeometry();
    // Use ClickInjector::cursorPos() rather than QCursor::pos(): on Wayland the
    // latter goes stale as soon as the pointer leaves the window, so the
    // slide-away/peek detection never saw the cursor move and the feature
    // appeared dead on Ubuntu.  cursorPos() tracks the cursor globally (evdev /
    // XQueryPointer), the same source the dwell timer uses.
    QPoint cursor = ClickInjector::cursorPos();

    const int shownX  = (lock == EdgeLock::Left)
        ? avail.left()
        : avail.right() - width() + 1;
    const int hiddenX = (lock == EdgeLock::Left)
        ? avail.left() - width() + k_edgePeekPx
        : avail.right() - k_edgePeekPx + 1;

    if (m_edgeShown) {
        bool over = geometry().contains(cursor);
        if (over) {
            m_edgeHideCountMs = 0;
        } else {
            m_edgeHideCountMs += k_edgePollMs;
            if (m_edgeHideCountMs >= k_edgeHideDelayMs) {
                m_edgeShown       = false;
                m_edgeHideCountMs = 0;
                animateEdgeTo(QPoint(hiddenX, y()));
            }
        }
    } else {
        // Show when cursor enters the visible sliver at the screen edge
        bool inPeek = (lock == EdgeLock::Left)
            ? (cursor.x() <= avail.left() + k_edgePeekPx - 1
               && cursor.y() >= y() && cursor.y() < y() + height())
            : (cursor.x() >= avail.right() - k_edgePeekPx + 1
               && cursor.y() >= y() && cursor.y() < y() + height());

        if (inPeek) {
            m_edgeShown       = true;
            m_edgeHideCountMs = 0;
            animateEdgeTo(QPoint(shownX, y()));
        }
    }
}

void MainWindow::animateEdgeTo(QPoint target)
{
    if (!m_edgeAnim) {
        m_edgeAnim = new QPropertyAnimation(this, "pos", this);
        m_edgeAnim->setEasingCurve(QEasingCurve::OutCubic);
        m_edgeAnim->setDuration(k_edgeAnimMs);
    }
    m_edgeAnim->stop();
    m_edgeAnim->setStartValue(pos());
    m_edgeAnim->setEndValue(target);
    m_edgeAnim->start();
}

// ─── Slots ────────────────────────────────────────────────────────────────
void MainWindow::onClickButtonPressed(ClickType type)
{
    setClickType(type);

    if (m_autoEnabled) {
        // In auto mode, selecting a button arms the dwell manager
        m_dwell->arm(type, m_modifiers);
    } else {
        // Manual mode: inject the click at the current cursor position
        QPoint pos = QCursor::pos();
        bool isScroll = (type == ClickType::ScrollUp   || type == ClickType::ScrollDown ||
                         type == ClickType::ScrollLeft  || type == ClickType::ScrollRight);
        int reps = isScroll ? m_settings.scrollRepeat : 1;
        for (int i = 0; i < reps; ++i)
            ClickInjector::performClick(type, pos, m_modifiers);
        // "No Click" injects no action (like a paused dwell), so it gets no
        // visual or audio feedback — unlike every other click type.
        if (type != ClickType::NoClick) {
            if (m_settings.showClickIndicator)
                m_clickIndicator->flash(pos);
#ifdef HAVE_MULTIMEDIA
            if (m_settings.audioFeedback) m_clickSound->play();
#endif
        }

        // Clear modifiers after use (one-shot)
        m_modifiers = ModNone;
        if (m_ctrlBtn)  { m_ctrlBtn->setChecked(false); }
        if (m_altBtn)   { m_altBtn->setChecked(false); }
        if (m_shiftBtn) { m_shiftBtn->setChecked(false); }
    }
}

void MainWindow::setClickType(ClickType t)
{
    // Deselect any hotkey button that was previously selected
    if (m_selectedHotkey >= 0) {
        if (m_hotkeyBtns[m_selectedHotkey])
            m_hotkeyBtns[m_selectedHotkey]->setStyleSheet(
                modBtnStyle(false, m_settings.largeButtons));
        m_selectedHotkey = -1;
    }

    m_selectedType = t;
    for (auto* b : m_clickButtons) {
        b->setSelected(b->clickType() == t);
    }

    // Update status — not static so tr() reflects the active language
    const QHash<ClickType,QString> names{
        {ClickType::NoClick,          tr("No action")},
        {ClickType::LeftClick,        tr("Left Click")},
        {ClickType::LeftDoubleClick,  tr("Left Double-Click")},
        {ClickType::LeftDown,         tr("Left Drag")},
        {ClickType::RightClick,       tr("Right Click")},
        {ClickType::RightDoubleClick, tr("Right Double-Click")},
        {ClickType::RightDown,        tr("Right Drag")},
        {ClickType::MiddleClick,      tr("Middle Click")},
        {ClickType::MiddleDoubleClick,tr("Middle Double-Click")},
        {ClickType::ScrollUp,         tr("Scroll Up")},
        {ClickType::ScrollDown,       tr("Scroll Down")},
        {ClickType::ScrollLeft,       tr("Scroll Left")},
        {ClickType::ScrollRight,      tr("Scroll Right")},
    };
    m_selectionName = names.value(t, "?");
    updateStatusLabel();
}

void MainWindow::onHotkeySelected(int i)
{
    // Deselect all click buttons
    for (auto* b : m_clickButtons)
        b->setSelected(false);

    // Deselect previously selected hotkey button
    if (m_selectedHotkey >= 0 && m_selectedHotkey < 3 && m_hotkeyBtns[m_selectedHotkey])
        m_hotkeyBtns[m_selectedHotkey]->setStyleSheet(modBtnStyle(false, m_settings.largeButtons));

    m_selectedHotkey = i;
    if (m_hotkeyBtns[i])
        m_hotkeyBtns[i]->setStyleSheet(modBtnStyle(true, m_settings.largeButtons));

    // Update status label with hotkey display name
    const auto& slot = m_settings.hotkeys[i];
    const QKeySequence seq(slot.keySequence, QKeySequence::PortableText);
    const QString name = slot.label.isEmpty()
        ? seq.toString(QKeySequence::NativeText) : slot.label;
    m_selectionName = name;
    updateStatusLabel();

    if (m_autoEnabled) {
        // Armed with NoClick so DwellManager performs no mouse action;
        // the actual key injection happens in onDwellFired.
        m_dwell->arm(ClickType::NoClick, m_modifiers);
    } else {
        // Manual mode: inject immediately, same as clicking a mouse-action button
        ClickInjector::injectKeySequence(seq);
        if (m_settings.showClickIndicator)
            m_clickIndicator->flash(ClickInjector::cursorPos());
#ifdef HAVE_MULTIMEDIA
        if (m_settings.audioFeedback && m_clickSound) m_clickSound->play();
#endif
    }
}

void MainWindow::onAutoToggled(bool on)
{
    m_autoEnabled = on;

    // Keep the in-panel Dwell Active button in sync without re-triggering this slot.
    if (m_dwellActiveBtn && m_dwellActiveBtn->isChecked() != on) {
        QSignalBlocker b(m_dwellActiveBtn);
        m_dwellActiveBtn->setChecked(on);
        // Re-apply style by toggling the stylesheet manually.
        // The style lambda lives inside rebuildButtons so we reproduce the
        // active/inactive colours here.
        const bool large = m_settings.largeButtons;
        const char* fs  = large ? "14px" : "11px";
        const char* pad = large ? "4px"  : "2px";
        m_dwellActiveBtn->setStyleSheet(on
            ? QString("QPushButton { background:#FFA028; color:#1A1A1A; border:2px solid #FFB040; "
                      "border-radius:4px; font-weight:bold; font-size:%1; padding:%2; }"
                      "QPushButton:hover { background:#FFB040; }").arg(fs).arg(pad)
            : QString("QPushButton { background:#3A3A3A; color:#DDDDDD; border:1px solid #555; "
                      "border-radius:4px; font-size:%1; padding:%2; }"
                      "QPushButton:hover { background:#4A4A4A; border:1px solid #FFA028; color:#FFA028; }").arg(fs).arg(pad));
    }

    // Show the dwell bar or the audio level meter (whichever matches the mode)
    // and the status label, all gated on dwell-active being on.
    updateActivityFeedback();

    if (on) {
        if (m_selectedHotkey >= 0)
            m_dwell->arm(ClickType::NoClick, m_modifiers);
        else
            m_dwell->arm(m_selectedType, m_modifiers);
    } else {
        m_dwell->disarm();
        m_dwellBar->setValue(0);
    }
    // Start/stop the microphone listener to match the new dwell-active state.
    updateAudioClick();
    // Adjust height only — keeps the window width stable so the dwell bar
    // simply fills whatever width the window already has.
    resize(width(), sizeHint().height());
}

void MainWindow::onDwellProgress(float frac)
{
    if (m_dwellBar->isVisible()) {
        m_dwellBar->setValue(static_cast<int>(frac * 100));
    }
}

void MainWindow::onDwellFired(QPoint pos, ClickType type)
{
    // Hotkey selected: DwellManager fired NoClick (no mouse action); inject key now.
    bool hotkeyAction = false;
    if (m_selectedHotkey >= 0 && m_selectedHotkey < 3) {
        hotkeyAction = true;
        const auto& slot = m_settings.hotkeys[m_selectedHotkey];
        if (!slot.keySequence.isEmpty()) {
            QKeySequence seq(slot.keySequence, QKeySequence::PortableText);
            ClickInjector::injectKeySequence(seq);
        }
    }

    // "No Click" performs no action (like a paused dwell), so it gets no visual
    // or audio feedback. A hotkey arms NoClick internally but does inject a key,
    // so it still counts as an action and keeps its feedback.
    if (!hotkeyAction && type == ClickType::NoClick)
        return;

    if (m_settings.showClickIndicator)
        m_clickIndicator->flash(pos);   // accurate injected position (evdev on Wayland)
#ifdef HAVE_MULTIMEDIA
    if (m_settings.audioFeedback) m_clickSound->play();
#endif
}

void MainWindow::updateAudioClick()
{
#ifdef HAVE_MULTIMEDIA
    const bool wantAudio = m_settings.audioClickEnabled;
    // In audio mode the dwell timer never fires on its own — fireNow() (driven
    // by the microphone) does instead.
    m_dwell->setAudioTriggerMode(wantAudio);

    if (m_audioClick) {
        m_audioClick->setThreshold(m_settings.audioClickThreshold / 100.0);
        m_audioClick->setPreferredDeviceId(m_settings.audioInputDevice);
        // Only hold the microphone open while it can actually be used: audio
        // click enabled AND dwell-active on.
        const bool shouldListen = wantAudio && m_autoEnabled;
        if (shouldListen) {
            // Restart if already running so a changed device/threshold applies.
            if (m_audioClick->isRunning()) m_audioClick->stop();
            m_audioClick->start();
        } else if (m_audioClick->isRunning()) {
            m_audioClick->stop();
        }
    }
#else
    // No audio support: make sure the dwell timer keeps working normally.
    m_dwell->setAudioTriggerMode(false);
#endif
}

void MainWindow::updateActivityFeedback()
{
    // May be called before the status widgets exist (early init); no-op then.
    if (!m_dwellBar || !m_audioMeter || !m_statusLabel) return;
#ifdef HAVE_MULTIMEDIA
    const bool audioMode = m_settings.audioClickEnabled;
#else
    const bool audioMode = false;   // feature compiled out → always dwell
#endif
    m_dwellBar->setVisible(m_autoEnabled && !audioMode);
    m_audioMeter->setVisible(m_autoEnabled && audioMode);
    if (!m_audioMeter->isVisible())
        m_audioMeter->setValue(0);
    m_statusLabel->setVisible(m_autoEnabled);
    updateStatusLabel();
}

void MainWindow::updateStatusLabel()
{
    if (!m_statusLabel) return;
#ifdef HAVE_MULTIMEDIA
    const bool audioMode = m_settings.audioClickEnabled;
#else
    const bool audioMode = false;
#endif
    m_statusLabel->setText(audioMode
        ? tr("Audio click active: %1").arg(m_selectionName)
        : tr("Selected: %1").arg(m_selectionName));
}

void MainWindow::onSettingsClicked()
{
    SettingsDialog dlg(m_settings, m_translator, this);
    if (dlg.exec() == QDialog::Accepted) {
        applySettings(dlg.settings());
    }
}

// ── Launch-on-startup helpers (platform-specific) ─────────────────────────

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static void setLaunchOnStartup(bool enable)
{
    const wchar_t* runKey =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const wchar_t* approvedKey =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
    const wchar_t* name = L"TrackClick";

    HKEY hKey;

    if (enable) {
        // Write quoted exe path as REG_SZ under the Run key
        const std::wstring exe = QString("\"%1\"")
            .arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()))
            .toStdWString();

        if (RegCreateKeyExW(HKEY_CURRENT_USER, runKey, 0, nullptr,
                            REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
                            nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, name, 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(exe.c_str()),
                           static_cast<DWORD>((exe.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);
        }

        // Remove any stale "disabled" entry from StartupApproved so Windows
        // treats the Run key entry as approved (absent = enabled by default).
        if (RegOpenKeyExW(HKEY_CURRENT_USER, approvedKey, 0,
                          KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, name);
            RegCloseKey(hKey);
        }
    } else {
        if (RegOpenKeyExW(HKEY_CURRENT_USER, runKey, 0,
                          KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, name);
            RegCloseKey(hKey);
        }
        if (RegOpenKeyExW(HKEY_CURRENT_USER, approvedKey, 0,
                          KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, name);
            RegCloseKey(hKey);
        }
    }
}

#elif defined(Q_OS_MACOS)
static void setLaunchOnStartup(bool enable)
{
    const QString plist =
        QDir::homePath() + "/Library/LaunchAgents/com.optitrack.trackclick.plist";
    if (enable) {
        QDir().mkpath(QFileInfo(plist).absolutePath());
        QFile f(plist);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\""
                   " \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                   "<plist version=\"1.0\">\n"
                   "<dict>\n"
                   "    <key>Label</key>\n"
                   "    <string>com.optitrack.trackclick</string>\n"
                   "    <key>ProgramArguments</key>\n"
                   "    <array>\n"
                   "        <string>" << QCoreApplication::applicationFilePath() << "</string>\n"
                   "    </array>\n"
                   "    <key>RunAtLoad</key>\n"
                   "    <true/>\n"
                   "</dict>\n"
                   "</plist>\n";
        }
    } else {
        QFile::remove(plist);
    }
}

#elif defined(Q_OS_LINUX)
static void setLaunchOnStartup(bool enable)
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
    const QString path = dir + "/trackclick.desktop";
    if (enable) {
        QDir().mkpath(dir);
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out << "[Desktop Entry]\n"
                   "Type=Application\n"
                   "Name=TrackClick\n"
                   "Exec=" << QCoreApplication::applicationFilePath() << "\n"
                   "Hidden=false\n"
                   "X-GNOME-Autostart-enabled=true\n";
        }
    } else {
        QFile::remove(path);
    }
}

#else
static void setLaunchOnStartup(bool) {}
#endif

void MainWindow::syncLaunchOnStartup()
{
    // Only re-assert when enabled; disabling is handled in applySettings so we
    // never touch the OS registration on launch for users who opted out.
    if (m_settings.launchOnStartup)
        setLaunchOnStartup(true);
}

// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::applySettings(const AppSettings& s)
{
    const QString oldLanguage = m_settings.language;
    m_settings = s;

    m_dwell->setDwellMs(s.dwellMs);
    m_dwell->setSensitivityPx(s.sensitivityPx);
    m_dwell->setScrollRepeat(s.scrollRepeat);
    m_dwell->setRepeatOnDwell(s.repeatOnDwell);
    setWindowOpacity(s.windowOpacity);

    Qt::WindowFlags flags = Qt::Window | Qt::FramelessWindowHint | Qt::Tool;
    if (s.alwaysOnTop) flags |= Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
#ifdef Q_OS_MAC
    applyMacOSWindowBehavior(winId());
#endif
    show();

    if (s.language != oldLanguage) {
        installLanguage(s.language);  // also calls retranslateUi → rebuildButtons
    } else {
        rebuildButtons();
    }

    if (m_exitBtn) m_exitBtn->setToolTip(s.xMinimizesApp
        ? tr("Hide to tray (right-click tray icon to quit)")
        : tr("Close application"));

    // Persist
    m_persist.setValue("dwell/ms",           s.dwellMs);
    m_persist.setValue("dwell/sensitivity",  s.sensitivityPx);
    m_persist.setValue("window/opacity",     s.windowOpacity);
    m_persist.setValue("window/alwaysOnTop", s.alwaysOnTop);
    m_persist.setValue("window/startMin",      s.startMinimized);
    m_persist.setValue("window/xMinimizesApp", s.xMinimizesApp);
    m_persist.setValue("window/launchOnStartup", s.launchOnStartup);
    // Reconcile unconditionally: the registry/autostart entry must match the
    // setting even when it is unchanged here but was lost externally, never
    // written by an older build, or points at a stale executable path.
    setLaunchOnStartup(s.launchOnStartup);
    m_persist.setValue("audio/enabled",         s.audioFeedback);
    m_persist.setValue("visual/clickIndicator", s.showClickIndicator);
    m_persist.setValue("audioClick/enabled",   s.audioClickEnabled);
    m_persist.setValue("audioClick/threshold", s.audioClickThreshold);
    m_persist.setValue("audioClick/device",    s.audioInputDevice);
    m_persist.setValue("show/noClick",        s.showNoClick);
    m_persist.setValue("show/leftClick",     s.showLeftClick);
    m_persist.setValue("show/leftDouble",    s.showLeftDouble);
    m_persist.setValue("show/leftDrag",      s.showLeftDrag);
    m_persist.setValue("show/rightClick",    s.showRightClick);
    m_persist.setValue("show/rightDouble",   s.showRightDouble);
    m_persist.setValue("show/rightDrag",     s.showRightDrag);
    m_persist.setValue("show/middleClick",   s.showMiddleClick);
    m_persist.setValue("show/middleDouble",  s.showMiddleDouble);
    m_persist.setValue("show/scrollUp",      s.showScrollUp);
    m_persist.setValue("show/scrollDown",    s.showScrollDown);
    m_persist.setValue("show/scrollHoriz",   s.showScrollHoriz);
    m_persist.setValue("show/modCtrl",       s.showModCtrl);
    m_persist.setValue("show/modAlt",        s.showModAlt);
    m_persist.setValue("show/modShift",      s.showModShift);
    m_persist.setValue("show/quitButton",     s.showQuitButton);
    m_persist.setValue("show/dwellActiveBtn", s.showDwellActiveBtn);
    m_persist.setValue("show/iconsOnly",     s.iconsOnly);
    m_persist.setValue("show/largeButtons",  s.largeButtons);
    m_persist.setValue("show/buttonLayout",  static_cast<int>(s.buttonLayout));
    m_persist.setValue("language",           s.language);
    m_persist.setValue("settings/fontScale", s.settingsFontScale);
    m_persist.setValue("show/buttonOrder",   s.buttonOrder);
    m_persist.setValue("scroll/repeat",      s.scrollRepeat);
    m_persist.setValue("dwell/repeatOnDwell", s.repeatOnDwell);
    m_persist.setValue("dwell/hoverSelectPercent", s.hoverSelectPercent);
    m_persist.setValue("window/edgeLock",    static_cast<int>(s.edgeLock));
    m_persist.setValue("window/edgeHide",    s.edgeHide);
    for (int i = 0; i < 3; ++i) {
        const QString base = QString("hotkey/%1/").arg(i);
        m_persist.setValue(base + "enabled", s.hotkeys[i].enabled);
        m_persist.setValue(base + "label",   s.hotkeys[i].label);
        m_persist.setValue(base + "seq",     s.hotkeys[i].keySequence);
    }
    applyEdgeLock();
    updateAudioClick();
    // Reflect any change to audio-click mode in the meter/dwell-bar + status text.
    updateActivityFeedback();
}

void MainWindow::onExitClicked()
{
    if (m_settings.xMinimizesApp) {
        hide();
        if (m_tray) {
            m_tray->showMessage(tr("TrackClick"),
                tr("Running in the system tray. Right-click the tray icon to quit."),
                QSystemTrayIcon::Information, 2000);
        }
    } else {
        qApp->quit();
    }
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        setVisible(!isVisible());
        if (isVisible()) { raise(); activateWindow(); }
    }
}

void MainWindow::saveWindowSettings()
{
    m_persist.setValue("window/pos", pos());
}

void MainWindow::loadWindowSettings()
{
    QPoint savedPos = m_persist.value("window/pos", QPoint(-1,-1)).toPoint();
    if (savedPos.x() >= 0) {
        move(savedPos);
    } else {
        // Default: top-right of primary screen
        QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
        move(screen.right() - width() - 20, screen.top() + 40);
    }
    applyEdgeLock();
}

// ─── Translation ──────────────────────────────────────────────────────────────
void MainWindow::installLanguage(const QString& lang)
{
    if (m_translator) {
        qApp->removeTranslator(m_translator);
        delete m_translator;
        m_translator = nullptr;
    }

    if (lang != "en") {
        m_translator = loadBestTranslator(lang, this);
        if (m_translator)
            qApp->installTranslator(m_translator);
    }

    retranslateUi();
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("TrackClick"));
    if (m_titleLabel)
        m_titleLabel->setText(tr("TrackClick"));
    if (m_autoBtn)
        m_autoBtn->setToolTip(tr("Toggle AutoMouse dwell-clicking"));
    if (m_settingsBtn) m_settingsBtn->setToolTip(tr("Settings"));
    if (m_exitBtn)     m_exitBtn->setToolTip(m_settings.xMinimizesApp
                           ? tr("Hide to tray (right-click tray icon to quit)")
                           : tr("Close application"));
    if (m_tray)        m_tray->setToolTip(tr("TrackClick Virtual Mouse"));
    if (m_showAct)     m_showAct->setText(tr("Show / Hide"));
    if (m_quitAct)     m_quitAct->setText(tr("Quit TrackClick"));

    rebuildButtons();          // recreates click/modifier buttons with active language
    setClickType(m_selectedType); // refreshes the status label
}
