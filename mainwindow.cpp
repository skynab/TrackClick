#include "mainwindow.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QToolTip>
#include <QAction>
#include <QMessageBox>
#include <QFont>

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
    : QPushButton(label, parent), m_type(type)
{
    setMinimumSize(64, 44);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCheckable(false);
    updateStyle();
    connect(this, &QPushButton::clicked, this, [this](){
        emit clickTypePressed(m_type);
    });
}

void ClickButton::setSelected(bool sel)
{
    m_selected = sel;
    updateStyle();
}

void ClickButton::updateStyle()
{
    if (m_selected) {
        setStyleSheet(
            "QPushButton {"
            "  background: #FFA600;"
            "  color: #1A1A1A;"
            "  border: 2px solid #FFB833;"
            "  border-radius: 5px;"
            "  font-weight: bold;"
            "  font-size: 11px;"
            "  padding: 4px 2px;"
            "}"
            "QPushButton:hover { background: #FFB833; }"
            "QPushButton:pressed { background: #CC8400; }"
        );
    } else {
        setStyleSheet(
            "QPushButton {"
            "  background: #3A3A3A;"
            "  color: #DDDDDD;"
            "  border: 1px solid #555555;"
            "  border-radius: 5px;"
            "  font-size: 11px;"
            "  padding: 4px 2px;"
            "}"
            "QPushButton:hover {"
            "  background: #4A4A4A;"
            "  border: 1px solid #FFA600;"
            "  color: #FFA600;"
            "}"
            "QPushButton:pressed { background: #2A2A2A; }"
        );
    }
}

// ─────────────────────────────────────────────────────────────
//  MainWindow
// ─────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
    , m_persist("PointNClickQt", "PointNClick")
{
    // Load persisted settings
    m_settings.dwellMs       = m_persist.value("dwell/ms",        1000).toInt();
    m_settings.sensitivityPx = m_persist.value("dwell/sensitivity", 5).toInt();
    m_settings.windowOpacity = m_persist.value("window/opacity",  1.0).toDouble();
    m_settings.alwaysOnTop   = m_persist.value("window/alwaysOnTop", true).toBool();
    m_settings.showLeftClick   = m_persist.value("show/leftClick",   true).toBool();
    m_settings.showLeftDouble  = m_persist.value("show/leftDouble",  true).toBool();
    m_settings.showLeftDrag    = m_persist.value("show/leftDrag",    true).toBool();
    m_settings.showRightClick  = m_persist.value("show/rightClick",  true).toBool();
    m_settings.showRightDouble = m_persist.value("show/rightDouble", true).toBool();
    m_settings.showRightDrag   = m_persist.value("show/rightDrag",   true).toBool();
    m_settings.showMiddleClick = m_persist.value("show/middleClick", true).toBool();
    m_settings.showMiddleDouble= m_persist.value("show/middleDouble",false).toBool();
    m_settings.showScrollUp    = m_persist.value("show/scrollUp",    true).toBool();
    m_settings.showScrollDown  = m_persist.value("show/scrollDown",  true).toBool();
    m_settings.showScrollHoriz = m_persist.value("show/scrollHoriz", false).toBool();
    m_settings.showModCtrl     = m_persist.value("show/modCtrl",     true).toBool();
    m_settings.showModAlt      = m_persist.value("show/modAlt",      true).toBool();
    m_settings.showModShift    = m_persist.value("show/modShift",    true).toBool();
    m_settings.showExitButton  = m_persist.value("show/exit",        true).toBool();
    m_settings.startMinimized  = m_persist.value("window/startMin",  false).toBool();
    m_settings.audioFeedback   = m_persist.value("audio/enabled",    false).toBool();

    // Window flags: frameless, stays on top
    Qt::WindowFlags flags = Qt::Window | Qt::FramelessWindowHint | Qt::Tool;
    if (m_settings.alwaysOnTop) flags |= Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setStyleSheet(BASE_STYLE);
    setWindowTitle("Point-N-Click");
    setWindowOpacity(m_settings.windowOpacity);

    m_dwell = new DwellManager(this);
    m_dwell->setDwellMs(m_settings.dwellMs);
    m_dwell->setSensitivityPx(m_settings.sensitivityPx);

    connect(m_dwell, &DwellManager::dwellProgress, this, &MainWindow::onDwellProgress);
    connect(m_dwell, &DwellManager::dwellFired,    this, &MainWindow::onDwellFired);

    buildUi();
    buildTray();
    loadWindowSettings();

    if (m_settings.startMinimized) {
        hide();
    }
}

void MainWindow::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    // ── Title bar ─────────────────────────────────────────────
    m_titleBar = new QWidget;
    m_titleBar->setFixedHeight(30);
    m_titleBar->setStyleSheet(
        "QWidget { background: #1A1A1A; border-bottom: 2px solid #FFA600; }"
        "QLabel  { color: #FFA600; font-weight: bold; font-size: 12px; background: transparent; }"
    );
    auto* tbLayout = new QHBoxLayout(m_titleBar);
    tbLayout->setContentsMargins(8, 0, 4, 0);
    tbLayout->setSpacing(4);

    m_titleLabel = new QLabel("⬤  Point-N-Click");
    tbLayout->addWidget(m_titleLabel, 1);

    // Auto button
    m_autoBtn = new QPushButton("AUTO");
    m_autoBtn->setCheckable(true);
    m_autoBtn->setFixedSize(48, 22);
    m_autoBtn->setToolTip("Toggle AutoMouse dwell-clicking");
    m_autoBtn->setStyleSheet(
        "QPushButton { background:#3A3A3A; color:#AAA; border:1px solid #555; border-radius:3px; font-size:10px; font-weight:bold; }"
        "QPushButton:checked { background:#FFA600; color:#1A1A1A; border:1px solid #FFB833; }"
        "QPushButton:hover { border:1px solid #FFA600; }"
    );
    connect(m_autoBtn, &QPushButton::toggled, this, &MainWindow::onAutoToggled);
    tbLayout->addWidget(m_autoBtn);

    // Settings button
    m_settingsBtn = new QPushButton("⚙");
    m_settingsBtn->setFixedSize(26, 22);
    m_settingsBtn->setToolTip("Settings");
    m_settingsBtn->setStyleSheet(
        "QPushButton { background:#3A3A3A; color:#CCC; border:1px solid #555; border-radius:3px; font-size:14px; }"
        "QPushButton:hover { background:#4A4A4A; color:#FFA600; border:1px solid #FFA600; }"
        "QPushButton:pressed { background:#2A2A2A; }"
    );
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    tbLayout->addWidget(m_settingsBtn);

    // Close/hide button
    m_exitBtn = new QPushButton("✕");
    m_exitBtn->setFixedSize(26, 22);
    m_exitBtn->setToolTip("Hide to tray (right-click tray icon to quit)");
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
    m_dwellBar->setVisible(false);
    root->addWidget(m_dwellBar);

    // ── Status label ──────────────────────────────────────────
    m_statusLabel = new QLabel("Ready — hover to dwell-click");
    m_statusLabel->setStyleSheet(
        "QLabel { color: #888888; font-size: 9px; padding: 2px 6px; "
        "background: #1A1A1A; border-top: 1px solid #3A3A3A; }"
    );
    m_statusLabel->setFixedHeight(18);
    m_statusLabel->setVisible(false);
    root->addWidget(m_statusLabel);

    adjustSize();
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
    m_ctrlBtn = m_altBtn = m_shiftBtn = nullptr;

    auto* grid = new QGridLayout(m_btnArea);
    grid->setSpacing(4);
    grid->setContentsMargins(4, 4, 4, 4);

    int row = 0, col = 0;
    const int COLS = 3;

    auto add = [&](ClickButton* btn){
        m_clickButtons.append(btn);
        grid->addWidget(btn, row, col);
        col++;
        if (col >= COLS) { col = 0; row++; }
        connect(btn, &ClickButton::clickTypePressed, this, &MainWindow::onClickButtonPressed);
    };

    auto addIf = [&](bool show, const QString& lbl, const QString& tip, ClickType t){
        if (show) {
            auto* b = makeButton(lbl, tip, t);
            add(b);
        }
    };

    addIf(m_settings.showLeftClick,   "L Click\n🖱",    "Left Click",         ClickType::LeftClick);
    addIf(m_settings.showLeftDouble,  "L Dbl\n🖱🖱",   "Left Double-Click",  ClickType::LeftDoubleClick);
    addIf(m_settings.showLeftDrag,    "L Drag\n↔",     "Left Drag (hold)",   ClickType::LeftDown);
    addIf(m_settings.showRightClick,  "R Click\n🖱",   "Right Click",        ClickType::RightClick);
    addIf(m_settings.showRightDouble, "R Dbl\n🖱🖱",  "Right Double-Click", ClickType::RightDoubleClick);
    addIf(m_settings.showRightDrag,   "R Drag\n↔",    "Right Drag (hold)",  ClickType::RightDown);
    addIf(m_settings.showMiddleClick, "M Click\n⚬",   "Middle Click",       ClickType::MiddleClick);
    addIf(m_settings.showMiddleDouble,"M Dbl\n⚬⚬",   "Middle Double-Click",ClickType::MiddleDoubleClick);
    addIf(m_settings.showScrollUp,    "Scroll\n▲",    "Scroll Up",          ClickType::ScrollUp);
    addIf(m_settings.showScrollDown,  "Scroll\n▼",    "Scroll Down",        ClickType::ScrollDown);

    if (m_settings.showScrollHoriz) {
        addIf(true, "Scroll\n◄", "Scroll Left",  ClickType::ScrollLeft);
        addIf(true, "Scroll\n►", "Scroll Right", ClickType::ScrollRight);
    }

    // Fill to next row boundary if needed
    if (col > 0) {
        while (col < COLS) {
            auto* spacer = new QWidget;
            spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            grid->addWidget(spacer, row, col);
            col++;
        }
        row++;
    }

    // ── Modifier row ──────────────────────────────────────────
    auto modStyle = [](bool on) -> QString {
        return on
            ? "QPushButton { background:#FFA600; color:#1A1A1A; border:2px solid #FFB833; "
              "border-radius:4px; font-weight:bold; font-size:11px; padding:2px; }"
              "QPushButton:hover { background:#FFB833; }"
            : "QPushButton { background:#3A3A3A; color:#AAA; border:1px solid #555; "
              "border-radius:4px; font-size:11px; padding:2px; }"
              "QPushButton:hover { background:#4A4A4A; border:1px solid #FFA600; color:#FFA600; }";
    };

    col = 0;
    if (m_settings.showModCtrl) {
        m_ctrlBtn = new QPushButton("Ctrl");
        m_ctrlBtn->setCheckable(true);
        m_ctrlBtn->setMinimumSize(64, 28);
        m_ctrlBtn->setToolTip("Hold Ctrl modifier for next click");
        m_ctrlBtn->setStyleSheet(modStyle(false));
        connect(m_ctrlBtn, &QPushButton::toggled, this, [this, modStyle](bool on){
            if (on) m_modifiers |= ModCtrl; else m_modifiers &= ~ModCtrl;
            m_ctrlBtn->setStyleSheet(modStyle(on));
        });
        grid->addWidget(m_ctrlBtn, row, col++);
    }
    if (m_settings.showModAlt) {
        m_altBtn = new QPushButton("Alt");
        m_altBtn->setCheckable(true);
        m_altBtn->setMinimumSize(64, 28);
        m_altBtn->setToolTip("Hold Alt modifier for next click");
        m_altBtn->setStyleSheet(modStyle(false));
        connect(m_altBtn, &QPushButton::toggled, this, [this, modStyle](bool on){
            if (on) m_modifiers |= ModAlt; else m_modifiers &= ~ModAlt;
            m_altBtn->setStyleSheet(modStyle(on));
        });
        grid->addWidget(m_altBtn, row, col++);
    }
    if (m_settings.showModShift) {
        m_shiftBtn = new QPushButton("Shift");
        m_shiftBtn->setCheckable(true);
        m_shiftBtn->setMinimumSize(64, 28);
        m_shiftBtn->setToolTip("Hold Shift modifier for next click");
        m_shiftBtn->setStyleSheet(modStyle(false));
        connect(m_shiftBtn, &QPushButton::toggled, this, [this, modStyle](bool on){
            if (on) m_modifiers |= ModShift; else m_modifiers &= ~ModShift;
            m_shiftBtn->setStyleSheet(modStyle(on));
        });
        grid->addWidget(m_shiftBtn, row, col++);
    }

    // Update selection highlight
    for (auto* b : m_clickButtons) {
        b->setSelected(b->clickType() == m_selectedType);
    }

    adjustSize();
}

ClickButton* MainWindow::makeButton(const QString& label, const QString& tooltip, ClickType type)
{
    auto* btn = new ClickButton(label, type, m_btnArea);
    btn->setToolTip(tooltip);
    return btn;
}

void MainWindow::buildTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;

    // Build a simple colored icon programmatically
    QPixmap pix(22, 22);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor("#FFA600"));
    p.setPen(Qt::NoPen);
    p.drawEllipse(2, 2, 18, 18);
    p.setBrush(QColor("#2D2D2D"));
    p.drawEllipse(6, 6, 10, 10);
    p.end();

    m_tray = new QSystemTrayIcon(QIcon(pix), this);
    m_tray->setToolTip("Point-N-Click Virtual Mouse");

    m_trayMenu = new QMenu(this);
    m_trayMenu->setStyleSheet(
        "QMenu { background:#2D2D2D; color:#FFF; border:1px solid #FFA600; }"
        "QMenu::item:selected { background:#FFA600; color:#1A1A1A; }"
    );
    auto* showAct = m_trayMenu->addAction("Show / Hide");
    m_trayMenu->addSeparator();
    auto* quitAct = m_trayMenu->addAction("Quit Point-N-Click");

    connect(showAct, &QAction::triggered, this, [this](){
        setVisible(!isVisible());
        if (isVisible()) raise();
    });
    connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);
    connect(m_tray, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);

    m_tray->setContextMenu(m_trayMenu);
    m_tray->show();
}

// ─── Drag the frameless window ────────────────────────────────────────────
void MainWindow::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton) {
        // Only drag from title bar area
        if (m_titleBar && m_titleBar->geometry().contains(ev->pos())) {
            m_dragging  = true;
            m_dragOffset = ev->globalPosition().toPoint() - frameGeometry().topLeft();
        }
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_dragging) {
        move(ev->globalPosition().toPoint() - m_dragOffset);
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent*)
{
    m_dragging = false;
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
    // Hide to tray instead of quitting
    ev->ignore();
    hide();
}

void MainWindow::changeEvent(QEvent* ev)
{
    if (ev->type() == QEvent::WindowStateChange && isMinimized()) {
        hide();
    }
    QWidget::changeEvent(ev);
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
        ClickInjector::performClick(type, pos, m_modifiers);

        // Clear modifiers after use (one-shot)
        m_modifiers = ModNone;
        if (m_ctrlBtn)  { m_ctrlBtn->setChecked(false); }
        if (m_altBtn)   { m_altBtn->setChecked(false); }
        if (m_shiftBtn) { m_shiftBtn->setChecked(false); }
    }
}

void MainWindow::setClickType(ClickType t)
{
    m_selectedType = t;
    for (auto* b : m_clickButtons) {
        b->setSelected(b->clickType() == t);
    }

    // Update status
    static const QHash<ClickType,QString> names{
        {ClickType::LeftClick,        "Left Click"},
        {ClickType::LeftDoubleClick,  "Left Double-Click"},
        {ClickType::LeftDown,         "Left Drag (hold)"},
        {ClickType::RightClick,       "Right Click"},
        {ClickType::RightDoubleClick, "Right Double-Click"},
        {ClickType::RightDown,        "Right Drag (hold)"},
        {ClickType::MiddleClick,      "Middle Click"},
        {ClickType::MiddleDoubleClick,"Middle Double-Click"},
        {ClickType::ScrollUp,         "Scroll Up"},
        {ClickType::ScrollDown,       "Scroll Down"},
        {ClickType::ScrollLeft,       "Scroll Left"},
        {ClickType::ScrollRight,      "Scroll Right"},
    };
    m_statusLabel->setText("Selected: " + names.value(t, "?"));
}

void MainWindow::onAutoToggled(bool on)
{
    m_autoEnabled = on;
    m_dwellBar->setVisible(on);
    m_statusLabel->setVisible(on);

    if (on) {
        m_dwell->arm(m_selectedType, m_modifiers);
        m_autoBtn->setText("AUTO ●");
        m_titleLabel->setText("⬤  Point-N-Click  [AUTO]");
    } else {
        m_dwell->disarm();
        m_autoBtn->setText("AUTO");
        m_titleLabel->setText("⬤  Point-N-Click");
        m_dwellBar->setValue(0);
    }
    adjustSize();
}

void MainWindow::onDwellProgress(float frac)
{
    if (m_dwellBar->isVisible()) {
        m_dwellBar->setValue(static_cast<int>(frac * 100));
    }
}

void MainWindow::onDwellFired(QPoint /*pos*/, ClickType type)
{
    // Re-arm for continuous clicking
    if (m_autoEnabled) {
        m_dwell->arm(type, m_modifiers);
    }
}

void MainWindow::onSettingsClicked()
{
    SettingsDialog dlg(m_settings, this);
    if (dlg.exec() == QDialog::Accepted) {
        applySettings(dlg.settings());
    }
}

void MainWindow::applySettings(const AppSettings& s)
{
    m_settings = s;

    m_dwell->setDwellMs(s.dwellMs);
    m_dwell->setSensitivityPx(s.sensitivityPx);
    setWindowOpacity(s.windowOpacity);

    Qt::WindowFlags flags = Qt::Window | Qt::FramelessWindowHint | Qt::Tool;
    if (s.alwaysOnTop) flags |= Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
    show();

    rebuildButtons();

    // Persist
    m_persist.setValue("dwell/ms",           s.dwellMs);
    m_persist.setValue("dwell/sensitivity",  s.sensitivityPx);
    m_persist.setValue("window/opacity",     s.windowOpacity);
    m_persist.setValue("window/alwaysOnTop", s.alwaysOnTop);
    m_persist.setValue("window/startMin",    s.startMinimized);
    m_persist.setValue("audio/enabled",      s.audioFeedback);
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
    m_persist.setValue("show/exit",          s.showExitButton);
}

void MainWindow::onExitClicked()
{
    hide();
    if (m_tray) {
        m_tray->showMessage("Point-N-Click",
            "Running in the system tray. Right-click the tray icon to quit.",
            QSystemTrayIcon::Information, 2000);
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
}
